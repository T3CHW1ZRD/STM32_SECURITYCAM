# STM32_SECURITYCAM

## Compatibility:

### Board: DISCO-L475VG-IOT01A (B-L475E-IOT01A)

### Camera: OV2640-2MP-PLUS

## HARDWARE SETUP:

Connect wires to camera.

From left to right face the camera up and the pins towards you. 

starting from CS, connect each wire to the mapping shown below

```
CN1 -> 3,4,5,6,7

CN2 -> 4 (3.3v)

CN1 -> 9,10
```

## Running instructions:

### IF JPEGS ARENT LOADING CORRECTLY, FLASH ADRUINO ONTO BOARD TO SETUP CAMERA

- Unzip ARDUCAMSETUP.zip

- Open in platformio in vscode

- Unplug and replug board

- Platformio command: Full clean

- Platformio command: Upload and monitor4

- Check serial monitor in VSCODE, if its displaying the bytes followed by a green checkmark, everything is fine. 

- To ensure JPEGS are formatted correctly, capture.py can be run to retrieve a sample JPEG

### SETUP CONFIG:

### CONFIG FILE: /STM32/proj_config.h

- Set Computer IP address and port for WIFI connection

- Set BLE discovery name 

- Set JPEG size and quality 

## START PYTHON SERVER

- python /test_server/test_serverr.py

### BUILD, UPLOAD, FLASH

- Enter Wifi SSID and Password (Might fail to connect, enter again when reprompted)

### BLUETOOTH 

- Open a BLE scanner (tested on nRF Connect app), and connect to the name in ```proj_confg.h```  (Default is SEC-CAM).

- Subscribe to the GATT service

### Trip the TOF SENSOR

- BLE scanner should display a 1 on read for 3s, then a 0

### Board sends Alarm Tripped packet to server

### Server responds with get photo

### Board sends the photo in multiple packets

### Server saves photo

### Check server logs for the name of the image saved

