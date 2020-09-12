# RAK811_RUI_SWITCH_SENSOR



4 digital switch based sensors: pushbuttons, float switches, reed switches, vibration, inclination
led for Join Ok and tx ok
periodic transmission time configurable via downlink
enable/disable sensors and led via downlink

RAK811 module
led tied between output zz and ground
pullup for sensors (470k) betwen inputs a,b,c,d and vcc
sensor wired between input and ground

every time a sensor changes state open to close or close to open a transmision is done
every x time a periodic transmission of the status of the inputs and voltage
sleep between transmissions
minimun transmision time between consecutive activation tx

based on cayenne LPP v2.0 
uplink frame, periodic or interrupt

config downlink frames
periodic time change
sensor, led enabe/disable

uplink frames in response to config commands

aprox power consumption


RAK RUI version 1.0.0 beta
FW 3.0.0.14h beta
bootloader 3.0.4 
RAK Device Firmware Upgrade Tool v1.4 for Ubuntu Linux
