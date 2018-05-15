# SNAP
SNAP is an arduino-compatible audio datalogger board (designed for Teensy 3.x)

More information is at http://loggerhead.com

Files:
- snap.ino: Main control and sensor recording
- cmd.ino: Reads recording settings from a script file
- wav.h: wav header

Software supports:
- duty cycle recording
- display
- button input for recording setup
- writing to microSD
- different sampling frequencies (32, 44.1 48, 96 kHz)

## Updating Firmware

1.	Get Teensy loader (Teensy.exe) from https://github.com/loggerhead-instruments/snap/tree/master/
2.	Get latest hex file from appropriate repository
	https://github.com/loggerhead-instruments/snap/tree/master/hex
	To download the file, right click on it and select Save link as
3.	Connect microUSB cable to small board on device. You may have to unscrew board.
4.	Run the Teensy Loader program.
5.	From File Menu, select Open HEX File
6.	Press the button on the board, or from Operation menu, select Program (you may have to turn off automatic mode).

To check whether the firmware has been updated, collect some data, and check the log files for the version date of the firmware.