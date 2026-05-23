# SMARTMATRIX

Firmware for the Raspberry Pi Pico 2W-based SmartMatrix adapter. This uses the microcontroller to convert serial input to parallel output for printing to a dot matrix printer - in my case an Epson MX-80 F/T III.

The device is intended to work in two ways:

- **USB**: If you connect to it via USB it appears as a virtual com port. You send the data over serial.
- **TCP socket**: The code on Core 1 effectively emulates an HP DirectJet 500X device available over wifi. You send data via a TCP socket connection.

If you send plain ASCII text, it gets printed. But if you send an ASCII 0x01 if goes into command mode, attempting to read what's sent next as a command, until it sees an ASCII 0x04 code.

An I2C port is configured but not yet used. The aim is to attach a small OLED display at some point.

## THE HARDWARE

The hardware device largely consists of the Raspberry Pi Pico 2W, some ICs used as level shifters and buffers/drivers, a 25-pin D socket for the printer cable and a few blinkenlights.
