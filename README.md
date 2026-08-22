# SmartMatrix

Firmware for the Raspberry Pi Pico 2W-based SmartMatrix adapter. This uses the microcontroller to convert serial or network input to parallel output for printing to a dot matrix printer - in my case an Epson MX-80 F/T III.

The device is intended to work in two ways:

- **USB**: If you connect to it via USB it appears as a virtual com port. You send the data over serial. This is intended to be used programmatically.
- **TCP socket**: The code on Core 1 effectively emulates an HP DirectJet 500X device available over wifi. You send data via a TCP socket connection to port :9100.

If you send plain ASCII text, it gets printed. But if you send an ASCII 0x01 it goes into command mode, attempting to read what's sent next as a command, until it sees an ASCII 0x04 code.

An I2C port is configured but not yet used. The aim is to attach a small OLED display at some point.

## THE HARDWARE

The SmartMatrix hardware device largely consists of the Raspberry Pi Pico 2W, some ICs used as level shifters and buffers/drivers, a 25-pin D-sub socket for the printer cable and a few blinkenlights.

### Signals

| **Pin** | **Function** | **Host** | **Def.** | **Cntl by** | **Note** |
| :---: | :---: | :---: | :---: | :---: | --- |
| 1 | /STROBE | Out —> | HIGH | Host | Pulsed low by host for 0.5-500 µsecs. |
| 2-9 | Data D0-D7 | Out —> | – | Host | |
| 10 | /ACK | <— In | HIGH | Printer | Pulsed low by printer to acknowledge receipt of data |
| 11 | BUSY | <— In | LOW | Printer | Taken high by printer when busy. |
| 12 | PE | <— In | LOW | Printer | Taken high by printer if paper out |
| 13 | SELECT | <— In | HIGH | Printer | Taken low when printer goes offline. |
| 14 | /AUTOFEED | Out —> | LOW | Host | Host pulls high to force automatic linefeed |
| 15 | /ERROR | <— In | HIGH | Printer | Taken low to indicate error. |
| 16 | /INIT | Out —> | HIGH | Host | Reset. Taken low by host to initialise/reset printer |
| 17 | /SEL-IN | Out —> | LOW | Host | Taken high by host to take printer offline. |
| 18-25 | GND | – | – | – | – |

On the MX-80, DIP switch 2-3 can be used to 'fix' the `/AUTOFEED` setting. Factory default for the switch is OFF (which is how my printer has it). In effect, this allows the host to control this function.

When `/AUTOFEED` is LOW, the printer will automatically issue a linefeed after printing each line.

The DIP switch setting is ORed with the signal on line 14. So if the DIP switch is set to OFF, the line is held high and then the host can either allow this to remain high (Autofeed disabled) or take it low (Autofeed enabled). If the DIP switch is set to ON, Autofeed is always disabled.

`/SEL-IN`: Data input to the printer is possible only when `/SEL-IN` is LOW. However, DIP switch 1-8 on the Epson makes this default to LOW anyway.

## VERSION HISTORY

Dates indicate when the dev branch code was merged into main.

### IN PROGRESS

- Changed function of user LEDs.
  - LED A flashes when data is sent to the printer.
  - LED B now indicates a successful Wifi connection.
- Added OLED functions.

### 1.0 19/08/2026

- Fixed an incorrect default signal setting.

### 0.1 14/08/2026

- Wifi works. It usually connects on the second attempt.
- Serial via USB works. Why wouldn't it?
- The direct mode of sending bytes over the serial connection and having them print immediately on the printer works.
- Nothing else has been tested.

## WIFI CREDENTIALS

The code needs your wifi credentials. To get them, it includes a header file `__wifi_creds.h` which needs to have entries like:

```
#define WIFI_SSID     "SSID"
#define WIFI_PASSWORD "password"
```

An example file is included as: `wifi_creds.h`. You should edit this with your details and then either:

- Rename the file to `__wifi_creds.h`; or...
- Edit the main code to include `wifi_creds.h` instead of `__wifi_creds.h`.

## LIFE WITH A DOT MATRIX PRINTER

This project is the culmination of a number of projects all based around making good use of the Epson MX80 F/T-III dot matric printer I bought in the early 1980s and which is still working. I've documented these projects in a number of articles on Machina Speculatrix (Medium subscription required):

- [**Getting to grips with the parallel interface**](https://medium.com/machina-speculatrix/getting-to-grips-with-the-parallel-interface-cfab79c8a7b8) : Putting an old printer back into use meant talking the language of its now (mostly) obsolete interface. 28/02/2025.
- [**Life with a dot matrix printer**](https://medium.com/machina-speculatrix/life-with-a-dot-matrix-printer-ae4d89153b90) : There’s something charming about old technology, especially if you can find a use for it. 05/06/2026.
- [**Networking a dot matrix printer**](https://medium.com/machina-speculatrix/networking-a-dot-matrix-printer-eeda870f5728) : Nothing adds more value to resources like printers than being able to share them. 12/06/2026.
- [**SmartMatrix: A Raspberry Pi Pico parallel printer interface**](https://medium.com/machina-speculatrix/smartmatrix-a-raspberry-pi-pico-parallel-printer-interface-a771d79b1975) : This simple board makes an ancient dot matrix printer available to modern devices across the whole network. 13/08/2026.
