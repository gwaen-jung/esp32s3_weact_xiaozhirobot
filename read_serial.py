import serial
import time
import sys

sys.stdout.reconfigure(encoding='utf-8')

try:
    s = serial.Serial('COM7', 115200, timeout=0.5)
    s.setDTR(False)
    s.setRTS(True)
    time.sleep(0.1)
    s.setRTS(False)
except Exception as e:
    print(f"Error opening COM7: {e}")
    sys.exit(1)

print("Connected to COM7. Listening for 20 seconds...")
t0 = time.time()
while time.time() - t0 < 20:
    try:
        line = s.readline().decode('utf-8', errors='ignore')
        if line:
            print(line, end='', flush=True)
    except Exception as e:
        print(f"Read error: {e}")
        break

s.close()
