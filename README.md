# BMS-Device-BLE-ESP32
BLE BMS-Device connected to ESP32 to read basic values

The MOBILEKTRO® MLB-12200-NT LiFePO4 200Ah 12V 2560Wh Lithium Batterie with BMS, Bluetooth, -30 °C - EQ 320Ah - 400Ah AGM, GEL
uses a BMS system that offers BLE. Mobilektro offers an app that reads the most important values (Voltage, remaining Amps, remaining State of Charge (RSOC)).
The App can monitor Balancing between cells and can enable heating for charging in cold temperatures (<5°C).
However there are more then 50 other technical parameters that can be viewed or altered.
Manufacturer Homepage: https://www.mobilektro.eu/

For my monitoring project I need the remaining Amps value, only.

LiFePo4 Batterie capacity, availability and size are changing quickly. So the same model might not be available for long. 
But the BMS-Modul will be mostlikely be the same accross this vendor and other vendors like (Offgridtec, Jarocells, Skanbatt, Wattstunde, Powerextreme,...).

The manufacturer of the BMS seems to be xiaoxiang (https://xiaoxiangbms.com/). 

It took me quite some time to find specifications as well as an example.

Thanks to "simat" who documented the parameters: https://github.com/simat/BatteryMonitor/blob/master/BMSdecoded.pdf

Thanks to "bre55" for even more documented parameters: https://github.com/bres55/Smart-BMS-arduino-Reader

Thanks to "kolins-cz" for an example to work with: https://github.com/kolins-cz/Smart-BMS-Bluetooth-ESP32/blob/master/BLE.ino

The BLE Service is: ("6e400001-b5a3-f393-e0a9-e50e24dcca9e")

The BLE Characteristic to write to the BMS is: ("6e400002-b5a3-f393-e0a9-e50e24dcca9e")

The BLE Characteristic to read from the BMS is: ("6e400003-b5a3-f393-e0a9-e50e24dcca9e").

After signing up for notification, a command needs to be send to start receiving data {0xDD,0xA5,0x03,0x00,0xFF,0xFD,0x77}.
The data I was interested in is in register 3 and the dataset is 26 bytes. Byte 8 + 9 (high byte first) holds the remaining Amps.

The below code extracts the remaining Amps and a few others. It only connects for a short time, 
so that the app can be used as well. As with most BLE devices, only one device can connect at a time.

This is a fully functioning test program that will be imported into a Camper-Monitoring-Display project.

# Important:

The BMS reacts very slow to connects and commands. So sometimes delays and re-sends had to be included to make a stable connection.

As the 2.4GHz Frequenz is used by a lot of devices, it may happen that a connection / reconnection fails. Therefore it is advisable to try a few times. 
This has been added to re-connect and connect and provides a stable (on first connect) communication.

The device requests connection parameters that I have set to the same value during initialisation [updateConnParams(12,24,0,400)].
However, it continues to ask for it while connecting. So no chance to save some time.

The NimBLE-Arduino BLE stack (2.3.2) is used for the connection. https://github.com/h2zero/NimBLE-Arduino

The ESP-32 Dev Kit 4 is used as a hardware platform.

Arduino-IDE 2.3.6 compiled it nicely..
