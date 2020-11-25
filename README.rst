===========================================
RAK811 based open/close sensors LoRa node
===========================================

LoRa node can be used as a 4 button remote control, or a mix of different kinds: float switches, reed switches, vibration, inclination, etc.

Features
---------

* up to 4 digital i/o 
* led indicator for Join Ok and Tx ok
* periodic transmission time configurable via downlink
* enable/disable sensors and led via downlink
* sleep between transmissions

Hardware setup
---------------
* Tie a 470k pull-up resistor for each of used RAK811 input pins: 3 (PB14), 4 (PB15), 14 (PA15), 16 (PB5) 
* Tie each of the used sensors between their respective input (3,4,14,16) and Gnd
* Tie a led between pin 8 (PA12) and Gnd with a 470 Ohm resistor
* Make a voltage divider ( 1.6 M | 1 M ) to monitor battery voltage to pin 20 (PA2)

Behavior
--------
Every time a sensor changes its state open-to-close or close-to-open the module leaves sleep, transmit a packet, and goes to sleep again.
The minimum transmission time between successive sensor status change is 5 seconds.
The module also transmits sensor status in a periodic way

Data frames
------------
Data frames were based on cayenne LPP V2.0

Uplink fport 1

======  ===============  ====================  ======  ======  ==============================
1 byte  1 byte           1 byte                1 byte  1 byte  2 byte
======  ===============  ====================  ======  ======  ==============================
0x01    0x00             Digital Input status  0x02    0x02    Battery voltage (0.01v) units
======  ===============  ====================  ======  ======  ==============================

Digital Input status bits 0-3: pin logic level value

Digital Input status bits 7-4: wich of the input pins caused the interupt


Downlink fport 11

======  =============================================
1 byte  4 byte
======  =============================================
0x02    periodic transmission time (1 second) units
======  =============================================


Downlink fport 14

==============================  ===
1 byte  
==============================  ===
I/O pins enable/disable mask   
==============================  ===

I/O pins enable/disable mask bits 0-3: Digital input pins

I/O pins enable/disable mask bit 7: Led output

bit value 0 = input/output disabled
bit value 1 = input/output enabled

Disable an input is only recommendable when a sensor has a mechanical failure, giving false readings, and isn't possible to physically unplug it.


After sending a downlink config frame, the next transmission of the node will be a config response frame.

Provisioning
------------------
As the code was derived from a RAK Wireless existing product, it uses de same AT commands for provisioning using UART. Here is the  `manual </docs/RAK811_AT_Command_Manual_V1.0.pdf>`_ with the complete AT commands.

Power consumption
------------------
RAK811 module current in sleeping mode is around 10uA. Every closed sensor adds approx 5uA in sleep mode. So the worst case scenario with all 4 switches closed in sleep mode will be around 30uA.

Tested environment
------------------
* RAK RUI version 1.0.0 beta
* FIRMWARE  V 3.0.0.14.H
* Bootloader 3.0.4 
* RAK Device Firmware Upgrade Tool v1.4 for Ubuntu Linux

License
-------

This is an Open Source project and is licensed under a MIT License