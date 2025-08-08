#!/usr/bin/env python3
import socket
from Crypto.Cipher import AES
import time
import os

HOST     = '0.0.0.0'
PORT     = 12345
KEY_FILE = 'key.txt'

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

            # Command loop: always read 32 bytes (IV + 1 block of ciphertext)
            while True:
                try:
                    iv2 = recv_exact(conn, 16)
                    c0  = recv_exact(conn, 16)
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

                

                if cmd_id == 0x03:
                    print("Alarm tripped command received!\n")
                    send_get_photo(conn, key)    # new helper – see below
                    time.sleep(1)
                    print("GETTING CURRENT IMAGE: \n")
                    send_get_photo(conn, key)    # new helper – see below
                elif cmd_id == 0x02:          # CMD_SEND_PHOTO
                    # --- how many more encrypted bytes do we need? ---
                    plain_size  = 12 + data_len + pad_len       # entire plaintext
                    cipher_size = ((plain_size + 15) // 16) * 16
                    rem_cipher  = cipher_size - 16              # we've already read the first block

                    enc_rest = recv_exact(conn, rem_cipher)     # encrypted tail

                    # Decrypt the full message (first block + rest) in one shot
                    aes_full = AES.new(key, AES.MODE_CBC, iv2)
                    full_plain = aes_full.decrypt(c0 + enc_rest)

                    jpeg = full_plain[12 : 12 + data_len]       # skip 12-byte header, strip pad

                    # Sanity-check JPEG signature
                    print(f"[cam] JPEG starts: {jpeg[:2].hex(' ')}")

                    # Save to disk
                    fname = f"photo_{timestamp}.jpg"
                    with open(fname, "wb") as f:
                        f.write(jpeg)

                    print(f"[+] Saved {data_len} bytes -> {fname}\n")


    print("Server exiting.")

if __name__ == '__main__':
    main()
