# SmartMatrix

**WARNING: This is a work in progress. It's not yet complete. It certainly doesn't work.**

Firmware for the Raspberry Pi Pico 2W-based SmartMatrix adapter. This uses the microcontroller to convert serial or network input to parallel output for printing to a dot matrix printer - in my case an Epson MX-80 F/T III.

The device is intended to work in two ways:

- **USB**: If you connect to it via USB it appears as a virtual com port. You send the data over serial. This is intended to be used programmatically.
- **TCP socket**: The code on Core 1 effectively emulates an HP DirectJet 500X device available over wifi. You send data via a TCP socket connection to port :9100.

If you send plain ASCII text, it gets printed. But if you send an ASCII 0x01 it goes into command mode, attempting to read what's sent next as a command, until it sees an ASCII 0x04 code.

An I2C port is configured but not yet used. The aim is to attach a small OLED display at some point.

## THE HARDWARE

The hardware device largely consists of the Raspberry Pi Pico 2W, some ICs used as level shifters and buffers/drivers, a 25-pin D-sub socket for the printer cable and a few blinkenlights.

## WIFI CREDENTIALS

The code needs your wifi credentials. To get them, it includes a header file `__wifi_creds.h` which needs to have entries like:

```
#define WIFI_SSID     "SSID"
#define WIFI_PASSWORD "password"
```

An example file is included as: `wifi_creds.h`. You should edit this with your details and then either:

- Rename the file to `__wifi_creds.h`; or...
- Edit the main code to include `wifi_creds.h` instead of `__wifi_creds.h`.

## LIFE WITH A DOT MATRIX

This project is the culmination of a number of projects all based around making good use of the Epson MX80 F/T-III dot matric printer I bought in the early 1980s and which is still working. I've documented these projects in a number of articles on Machina Speculatrix (Medium subscription required):

- [**Getting to grips with the parallel interface**](https://medium.com/machina-speculatrix/getting-to-grips-with-the-parallel-interface-cfab79c8a7b8) : Putting an old printer back into use meant talking the language of its now (mostly) obsolete interface. 28/02/2025.
- [**Life with a dot matrix printer**](https://medium.com/machina-speculatrix/life-with-a-dot-matrix-printer-ae4d89153b90) : There’s something charming about old technology, especially if you can find a use for it. 05/06/2026.
- [**Networking a dot matrix printer**](https://medium.com/machina-speculatrix/networking-a-dot-matrix-printer-eeda870f5728) : Nothing adds more value to resources like printers than being able to share them. 12/06/2026.
