# SmartMatrix PCB

This folder contains the files you'd need to get a copy of the SmartMatrix device fabbed by the PCB manufacturer of your choice (I use [PCBway](https://www.pcbway.com)).

The files are:

- SmartMatrix_B1_Gerbers.zip - the Gerber and drill files.
- SmartMatrix_B1_BOM.xlsx - the bill of materials for the surface-mount parts.
- SmartMatrix_B1_placement.pos - the position/Centroid file.

Plus:

- SmartMatrix_B1_schematic.pdf - the schematic.
- SmartMatrix_B1_render.png - a render from KiCad.
- Centronics-dsub25.jpeg - in case you were wondering what all those pins do on a parallel port.

![SmartMatrix_B1_render.png](SmartMatrix_B1_render.png)

## LEDs

The LEDs on board the SmartMatrix are:

| LABEL | SCHEMATIC | COLOUR | DESCRIPTION |
| :---: | :-------: | :----: | ----------- |
| A | D1 | YELLOW | User LED A |
| B | D2 | BLUE | User LED B |
| BUSY | D3 | YELLOW | Busy |
| ERR | D4 | RED | Error |
| PE | D5 | RED | Paper out |
| ONLN | D6 | GREEN | Online |
| PWR | D7 | GREEN | Power |
