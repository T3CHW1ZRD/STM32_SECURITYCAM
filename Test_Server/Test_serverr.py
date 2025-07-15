import socket
import threading
import time

HOST = '0.0.0.0'     # Listen on all interfaces
PORT = 12345         # Must match STM32 side

def handle_client(conn, addr):
    print(f"[+] Connection from {addr}")
    try:
        while True:
            data = conn.recv(1024)
            if not data:
                print("[!] STM32 disconnected")
                break
            print("[STM32] >", data.decode('utf-8').strip())

            # Send a response (can modify this to test sleep/wake)
            response = "ACK from Python\n"
            conn.sendall(response.encode('utf-8'))

            time.sleep(2)  # Optional delay before next round
    except Exception as e:
        print(f"[!] Error: {e}")
    finally:
        conn.close()

def main():
    print(f"[*] Listening on {HOST}:{PORT}")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen(1)
        conn, addr = s.accept()
        handle_client(conn, addr)

if __name__ == "__main__":
    main()
