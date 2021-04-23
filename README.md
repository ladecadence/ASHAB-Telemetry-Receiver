# ASHAB Telemetry Receiver

Receiver code for the ASHAB LoRa-based telemetry and SSDV packets. Designed to run on ESP32+LoRa+OLED boards like the Heltec ones.

Works with the ASHAB-Telemetry desktop software (using the USB serial port) and also creates a Bluetooth-Serial device to send the telemetry data.

See ASHAB-Telemetry repository for more info: https://github.com/ladecadence/ASHAB-Telemetry 

## Compiling and uploading

Uses platform.io, so just run platformio run -t upload if using it from command line. If you are using the platform.io IDE, just upload it to the board.

You'll need to change the OLED pins depending on your board (heltec or TTGO), and also the Bluetooth device name on the #define section at the top of the main.c file.

You can use the provided OpenIventor Android App (app/ directory) to receive the telemetry on your Android device. If you want to compile it, you must have an OpenInventor account and import and build the .aia package.



