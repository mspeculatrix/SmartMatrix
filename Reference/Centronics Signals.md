# Centronics Parallel Interface Signals

Standard signals on a 25-pin D-sub connector.

| **Pin** | **Function** | **Host** | **Def** | **Cntl by** | **Note** |
| :---: | :---: | :---: | :---: | :---: | --- |
| 1 | /STROBE | Out —> | HIGH | Host | Pulsed low by host for 0.5-500 µsecs |
| 2-9 | Data D0-D7 | Out —> | – | Host | Data bus |
| 10 | /ACK | <— In | HIGH | Printer | Pulsed low by printer to acknowledge receipt of data |
| 11 | BUSY | <— In | LOW | Printer | Taken high by printer when busy |
| 12 | PE | <— In | LOW | Printer | Taken high by printer if paper out |
| 13 | SELECT | <— In | HIGH | Printer | Taken low when printer goes offline |
| 14 | /AUTOFEED | Out —> | LOW | Host | Host pulls high to force automatic linefeed |
| 15 | /ERROR | <— In | HIGH | Printer | Taken low to indicate error |
| 16 | /INIT | Out —> | HIGH | Host | Reset. Taken low by host to initialise/reset printer |
| 17 | /SEL-IN | Out —> | LOW | Host | We don't use this |
| 18-25 | GND | – | – | – | – |

On the MX-80, DIP switch 2-3 can be used to 'fix' the `/AUTOFEED` setting. Factory default for the switch is OFF (which is how my printer has it). In effect, this allows the host to control this function.

When `/AUTOFEED` is LOW, the printer will automatically issue a linefeed when it receives a carriage return. The DIP switch setting on the Epson is ORed with the signal on line 14. So if the DIP switch is set to OFF, the line is pulled high and then the host can either allow this to remain high (Autofeed disabled) or take it low (Autofeed enabled). If the DIP switch is set to ON, Autofeed is always disabled.
