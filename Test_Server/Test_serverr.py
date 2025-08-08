#!/usr/bin/env python3
import socket
from Crypto.Cipher import AES
import time
import os

HOST     = '0.0.0.0'
PORT     = 12345
KEY_FILE = 'key.txt'
# Flags reused from MCU (CMD_SEND_PHOTO arg bits)
PHOTO_FLAG_START = 0x0001
PHOTO_FLAG_LAST  = 0x0002
PHOTO_SEQ_SHIFT  = 8
PHOTO_SEQ_MASK   = 0xFF00

def load_key():
    data = open(KEY_FILE, 'rb').read().strip()
    if len(data) == 24:
        return data
    try:
        key = bytes.fromhex(data.decode('ascii'))
        if len(key) == 24:
            return key
    except Exception:
        pass
    raise ValueError(f"{KEY_FILE} must contain 24 raw bytes or 48 hex digits")

def recv_exact(sock, n):
    buf = b''
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("Connection closed")
        buf += chunk
    return buf

def recv_exact_allow_timeouts(sock, n, deadline=None):
    buf = b''
    while len(buf) < n:
        if deadline and time.time() > deadline:
            raise TimeoutError("tail read timed out")
        try:
            chunk = sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionError("Connection closed")
            buf += chunk
        except socket.timeout:
            continue  # keep waiting until deadline
    return buf

def abort_photo(photo_rx, reason):
    if not photo_rx:
        return None
    name = getattr(photo_rx['fh'], 'name', None)
    try:
        photo_rx['fh'].close()
    except Exception:
        pass
    if name:
        try:
            os.remove(name)
            print(f"[photo] Aborted: {reason}. Deleted partial file {name}")
        except OSError:
            print(f"[photo] Aborted: {reason}. (couldn't delete {name})")
    else:
        print(f"[photo] Aborted: {reason}.")
    return None


def send_get_photo(sock, key):
    cmd_id     = 0x01                # CMD_GET_PHOTO
    arg        = 0
    data_len   = 0
    pad_len    = 4                   # 12-byte header needs 4 bytes pad
    timestamp  = 0

    header  = bytearray()
    header.append(cmd_id)
    header += arg.to_bytes(2, 'little')
    header += data_len.to_bytes(4, 'little')
    header.append(pad_len)           # ← the missing byte
    header += timestamp.to_bytes(4, 'little')   # 4 bytes

    plain = bytes(header) + b'\x00' * pad_len   # 16 bytes total
    iv    = os.urandom(16)
    cipher = AES.new(key, AES.MODE_CBC, iv).encrypt(plain)
    sock.sendall(iv + cipher)        # 32-byte packet on the wire

def main():
    key = load_key()
    print(f"Loaded AES-192 key ({len(key)} bytes): {key.hex()}\n")

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as srv:
        srv.bind((HOST, PORT))
        srv.listen(1)
        print(f"Listening on {HOST}:{PORT}…\n")

        conn, addr = srv.accept()
        with conn:
            print(f"STM32 connected from {addr}\n")

            # Handshake
            challenge = recv_exact(conn, 16)
            iv        = recv_exact(conn, 16)
            print(f"Handshake → challenge: {challenge.hex()}")
            print(f"Handshake →       IV: {iv.hex()}")
            aes = AES.new(key, AES.MODE_CBC, iv)
            resp = aes.encrypt(challenge)
            conn.sendall(resp)
            print(f"Handshake → response: {resp.hex()}\n")
            print("Entering decrypt loop (Ctrl-C to exit)…\n")
            conn.settimeout(1.0)  # 1s poll so we can enforce photo_rx deadline
            # Command loop: always read 32 bytes (IV + 1 block of ciphertext)
            photo_rx = None  # holds current receive state or None


            while True:
                try:
                    iv2 = recv_exact(conn, 16)
                    c0  = recv_exact(conn, 16)

                except socket.timeout:
                    # No data this second — enforce photo receive timeout if active
                    if photo_rx and time.time() > photo_rx['deadline']:
                        photo_rx = abort_photo(photo_rx, "session timeout waiting for next chunk")
                    continue  # go back to top of loop

                except ConnectionError:
                    print("Connection closed by STM32")
                    break

                # Decrypt first block
                aes2 = AES.new(key, AES.MODE_CBC, iv2)
                p0   = aes2.decrypt(c0)


                cmd_id    = p0[0]
                cmd_arg   = int.from_bytes(p0[1:3], 'little')
                data_len  = int.from_bytes(p0[3:7], 'little')
                pad_len   = p0[7]
                timestamp = int.from_bytes(p0[8:12], 'little')

                print(f">> Packet header:")
                print(f"   cmd_id    = {cmd_id}")
                print(f"   cmd_arg   = {cmd_arg}")
                print(f"   data_len  = {data_len}")
                print(f"   pad_len   = {pad_len}")
                print(f"   timestamp = {timestamp}")

                
                if cmd_id == 0x03:  # ALARM_TRIPPED
                    if photo_rx:
                        print("[photo] Busy (receiving photo); ignoring alarm")
                    else:
                        print("Alarm tripped command received!")
                        send_get_photo(conn, key)

                elif cmd_id == 0x02:  # CMD_SEND_PHOTO
                    # --- how many more encrypted bytes do we need? ---
                    plain_size  = 12 + data_len + pad_len
                    cipher_size = ((plain_size + 15) // 16) * 16
                    rem_cipher  = cipher_size - 16

                    pkt_deadline = time.time() + 5.0  # 5s to finish this packet
                    try:
                        enc_rest = recv_exact_allow_timeouts(conn, rem_cipher, pkt_deadline)
                    except TimeoutError:
                        photo_rx = abort_photo(photo_rx, "packet tail timeout")
                        continue
                    except ConnectionError:
                        print("Connection closed mid-packet")
                        break


                    # Decrypt full packet
                    aes_full   = AES.new(key, AES.MODE_CBC, iv2)
                    full_plain = aes_full.decrypt(c0 + enc_rest)
                    payload    = full_plain[12 : 12 + data_len]  # skip 12B header

                    # Chunk header fields encoded in cmd_arg/timestamp
                    flags = cmd_arg & 0x00FF
                    seq   = (cmd_arg & PHOTO_SEQ_MASK) >> PHOTO_SEQ_SHIFT
                    sid   = timestamp

                    if flags & PHOTO_FLAG_START:
                        total_len = int.from_bytes(payload[:4], 'little')
                        jpeg_part = payload[4:]
                        print(f"[photo] START sid={sid} total={total_len} seq={seq} len={len(jpeg_part)}")
                        if photo_rx:
                            photo_rx = abort_photo(photo_rx, "new START arrived while busy")

                        photo_rx = {
                            'sid': sid, 'total': total_len,
                            'bytes': 0, 'expect_seq': 1,
                            'fh': open(f"photo_{sid}.jpg", "wb"),
                            'deadline': time.time() + 5.0
                        }
                        photo_rx['fh'].write(jpeg_part)
                        photo_rx['bytes'] += len(jpeg_part)
                        if (flags & PHOTO_FLAG_LAST) or photo_rx['bytes'] >= total_len:
                            photo_rx['fh'].close()
                            print(f"[+] Completed {photo_rx['bytes']} bytes -> photo_{sid}.jpg")
                            photo_rx = None

                    elif photo_rx and sid == photo_rx['sid']:
                        if seq != photo_rx['expect_seq']:
                            print(f"[photo] Sequence mismatch (got {seq}, want {photo_rx['expect_seq']})")
                            photo_rx = abort_photo(photo_rx, "sequence mismatch")
                            continue
                        print(f"[photo] CHUNK sid={sid} seq={seq} len={len(payload)}")
                        photo_rx['fh'].write(payload)
                        photo_rx['bytes'] += len(payload)
                        photo_rx['expect_seq'] += 1
                        photo_rx['deadline'] = time.time() + 5.0
                        if (flags & PHOTO_FLAG_LAST) or photo_rx['bytes'] >= photo_rx['total']:
                            photo_rx['fh'].close()
                            print(f"[+] Completed {photo_rx['bytes']} bytes -> photo_{sid}.jpg")
                            photo_rx = None

                    else:
                        print("[photo] Chunk for unknown or expired session — ignoring")




    print("Server exiting.")

if __name__ == '__main__':
    main()
