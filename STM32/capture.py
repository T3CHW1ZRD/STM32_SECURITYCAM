#!/usr/bin/env python3
"""
Grab the next JPEG the board spits out and save it to disk.
"""

PORT      = "COM10"       # << change ONLY this if your COM number differs
BAUD      = 115_200
OUT_FILE  = "snapshot.jpg"

import serial, time, sys

def main() -> None:
    with serial.Serial(PORT, BAUD, timeout=1) as ser, open(OUT_FILE, "wb") as out:
        # Wait until the firmware prints “Capture done…”
        print(f"[Waiting for image on {PORT} …]")
        line = b""
        while b"Capture done" not in line:
            line = ser.readline()

        # From here on every byte is image data until the firmware prints “✔️”.
        print("[Receiving bytes…]")
        while True:
            b = ser.read(1)
            if not b:
                print("\n‼️  Serial timeout, gave up.")
                sys.exit(1)
            # 0xE2 0x9C 0x94 is UTF‑8 “✔️” (the success line begins with that)
            if b == b'\xE2':
                peek = ser.read(2)
                if peek == b'\x9C\x94':
                    print("\n✔️  Saved to", OUT_FILE)
                    break
                # otherwise it was just a random 0xE2 inside the JPEG
                out.write(b + peek)
            else:
                out.write(b)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nAborted.")
