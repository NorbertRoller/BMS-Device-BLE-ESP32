// Manufacturer Homepage: https://www.mobilektro.eu/

/* The MOBILEKTRO® MLB-12200-NT LiFePO4 200Ah 12V 2560Wh Lithium Batterie with BMS, Bluetooth, -30 °C - EQ 320Ah - 400Ah AGM, GEL
uses a BMS system that offers BLE. Mobilektro offers an app that reads the most important values (Voltage, remaining Amps, remaining State of Charge (RSOC)).
The App can monitor Balancing between cells and can enable heating for charging in cold temperatures (<5°C).
However there are more then 50 other technical parameters that can be viewed or altered.

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

The below code extracts the remaining Amps and few others. It only connects for a short time, 
so that the app can be used as well. As with most BLE devices, only one device can connect at a time.

This is a fully functioning test program that will be imported into a Camper-Monitoring-Display project.

Important:
The BMS reacts very slow to connects and commands. So sometimes delays and re-sends had to be included to make a stable connection.

As the 2.4GHz Frequenz is used by a lot of devices, it may happen that a connection / reconnection fails. Therefore it is advisable to try a few times. 
This has been added to re-connect and connect and provides a stable (on first connect) communication.

The device requests connection parameters that I have set to the same value during initialisation [updateConnParams(12,24,0,400)].
However, it continues to ask for it while connecting. So no chance to save some time.

The NimBLE-Arduino BLE stack (2.3.2) is used for the connection. https://github.com/h2zero/NimBLE-Arduino
The ESP-32 Dev Kit 4 is used as a hardware platform.
Arduino-IDE 2.3.6 compiled it nicely..

*/

#include <Arduino.h>
#include <NimBLEDevice.h>
NimBLEScan *pBLEScan;
NimBLEAddress MAC_LiPo4("a4:c1:37:01:22:5f", 0); // 0=Public, 1= Random --- BLE_ADDR_PUBLIC (0) BLE_ADDR_RANDOM (1)
uint lastAdv = millis();
static bool doConnect_LiPo4 = false;
#define nullptr NULL
unsigned long delay_Millis = 0;

unsigned int LiPo4_remAh=0;          // Neu gemessene Remaining A/h - LITH_Ah
unsigned int LiPo4_RSOC=0;           // Neu gemessene Remaining SOC - LITH
unsigned int LiPo4_Volt=0;           // Neu gemessene Spannung - LITH_V

char BMS_CMD[7]= {0xDD,0xA5,0x03,0x00,0xFF,0xFD,0x77};
static const NimBLEAdvertisedDevice* advDevice_LiPo4;

NimBLEClient* pClient_LiPo4 = nullptr;
NimBLERemoteService* pSvc_LiPo4 = nullptr;
NimBLERemoteCharacteristic* pChrW_LiPo4 = nullptr;    // Write = Send Data to LiPo4
NimBLERemoteCharacteristic* pChrR_LiPo4 = nullptr;


/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ClientCallbacks : public NimBLEClientCallbacks
  {

  //void onConnect(NimBLEClient* pClient) {
  //  Serial.println("Connected and change Connection Parameter LiPo4");
  /** After connection we should change the parameters if we don't need fast response times.
   *  These settings are 150ms interval, 0 latency, 450ms timout.
   *  Timeout should be a multiple of the interval, minimum is 100ms.
   *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
   *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
  */
  void onDisconnect(NimBLEClient* pClient, int reason)
    {
    Serial.printf("%s Client Disconnected, reason = %d \n",
    pClient->getPeerAddress().toString().c_str(), reason);
    };

  /** Called when the peripheral requests a change to the connection parameters.
   *  Return true to accept and apply them or false to reject and keep
   *  the currently used parameters. Default will return true.
   * Developer answered: Accept all requested changes !!!
   */
  bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params)
    {
    Serial.println("ClientCallback: onConnParamsUpdateRequest");
    Serial.printf("Device MAC address: %s\n", pClient->getPeerAddress().toString().c_str());
    
    Serial.print("onConnParamsUpdateRequest itvl_min, New Value: ");    //requested Minimum value for connection interval 1.25ms units
    Serial.println(params->itvl_min,DEC);

    Serial.print("onConnParamsUpdateRequest itvl_max, New Value: ");    //requested Minimum value for connection interval 1.25ms units
    Serial.println(params->itvl_max,DEC);

    Serial.print("onConnParamsUpdateRequest latency, New Value: ");
    Serial.println(params->latency,DEC);

    Serial.print("onConnParamsUpdateRequest supervision_timeout, New Value:");
    Serial.println(params->supervision_timeout,DEC);
      
    Serial.printf("NimBLE: onConnParamsUpdateRequest accepted. MAC address: %s\n", pClient->getPeerAddress().toString().c_str());
    return true;
    };
  };

class scanCallbacks : public NimBLEScanCallbacks // BLEAdvertisedDeviceCallbacks
{
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override 
      {
      Serial.printf("NimBLE_scan: Discovered Device: %s Type: %d \n", advertisedDevice->toString().c_str(),advertisedDevice->getAddressType());

      // Checke for LiFePo4 MAC discovered
      if (advertisedDevice->getAddress().equals(MAC_LiPo4)) // Discovered MAC == Target MAC ?
      //if(MAC_LiPo4 == advertisedDevice->getAddress())
        {Serial.println("NimBLE_scan: MLB 200AH Lith an MAC-Adresse gefunden.");
        if(advertisedDevice->isAdvertisingService(NimBLEUUID("ff00"))) //MLB 200AH Lith Service 6e400001
          {
          Serial.println("NimBLE_scan: Found MLB 200AH Lith - BLE Service");
          advDevice_LiPo4 = advertisedDevice;
          doConnect_LiPo4 = true;
          Serial.println("NimBLE_scan: doConnect_LiPo4 = true");
          }
        }
      }

    void onScanEnd(const NimBLEScanResults& results, int reason) override
      {
      Serial.print("Scan Ended; reason = "); Serial.println(reason);
      }
} scanCallbacks;

/** Notification / Indication receiving handler callback */
void notifyCB_LiPo4(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
  if(pData[0]==0xDD && pData[2]==0)   // Erster Teil und Status == 0 == OK
    {
    LiPo4_Volt = (pData[4] * 0xFF)+pData[5];   // Total Voltage
    LiPo4_remAh = (pData[8] * 0xFF)+pData[9];  // Remaining Ah
    Serial.printf("***LiPo4_Volt in Hex %x in Dez %d\n",LiPo4_Volt,LiPo4_Volt);
    Serial.printf("***LiPo4_remAh in Hex %x in Dez %d\n",LiPo4_remAh, LiPo4_remAh);
    }

  if(pData[15]==0x77)               // Zweiter Teil mit Ende Kennzeichen
    {LiPo4_RSOC=pData[3];           // RSOC in %
    Serial.printf("***LiPo4_RSOC in Hex %x in Dez %d\n",LiPo4_RSOC,LiPo4_RSOC);
    }         
 
  // Kontrollausgabe der Werte  
    std::string str = (isNotify == true) ? "Notification" : "Indication";
    str += " from ";
    /** NimBLEAddress and NimBLEUUID have std::string operators */
    str += std::string(pRemoteCharacteristic->getClient()->getPeerAddress());
    str += ": Service = " + std::string(pRemoteCharacteristic->getRemoteService()->getUUID());
    str += ", Characteristic = " + std::string(pRemoteCharacteristic->getUUID());
    str += ", Value = " + std::string((char*)pData, length);
    
    Serial.println(str.c_str());
    Serial.printf("Länge in Hex %x Dez %d\n",length,length);
    for (int d=0;d<length;d++)
      {Serial.printf("pdata%d in Hex %x Dez %d\n",d,pData[d], pData[d]);}
    Serial.println(" ");

    if(pClient_LiPo4)                       // Only 1 reading required, Disconnect if active
      {pClient_LiPo4->disconnect();
      Serial.println("LiPo4-BLE disconnect");}
}

/** Create a single global instance of the callback class to be used by all clients */
static ClientCallbacks ClientCB_LiPo4;

bool connectToServer_LiPo4()
    /** Check if we have a client we should reuse first **/
    {
    if(NimBLEDevice::getCreatedClientCount()) 
      {
      // Special case when we already know this device, we send false as the
      //  second argument in connect() to prevent refreshing the service database.
      //  This saves considerable time and power.
      //
      pClient_LiPo4 = NimBLEDevice::getClientByPeerAddress(advDevice_LiPo4->getAddress());
      if(pClient_LiPo4)                                                   // Yes, this MAC is known
        {
        if(!pClient_LiPo4->connect(advDevice_LiPo4, false))
          {
          Serial.println("1. LiPo4-Reconnect failed");            // Try again
          if(!pClient_LiPo4->connect(advDevice_LiPo4, false))
            {
            Serial.println("2. LiPo4-Reconnect failed");          // Try again
            if(!pClient_LiPo4->connect(advDevice_LiPo4, false))
              {
              Serial.println("3. LiPo4-Reconnect failed");
              pClient_LiPo4->disconnect();
              return false;
              }
            }   
          }
        Serial.println("LiPo4-Reconnected client");
        }

      /** We don't already have a client that knows this device,
       *  we will check for a client that is disconnected that we can use.
       */
    else 
      {
      pClient_LiPo4 = NimBLEDevice::getDisconnectedClient();
      }
    }

   /** No client to reuse? Create a new one. */
    if(!pClient_LiPo4)
      {
      if(NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) 
        {
        Serial.println("LiPO4: Max clients reached - no more connections available");
        return false;
        }

      pClient_LiPo4 = NimBLEDevice::createClient();
      Serial.println("LiPo4 - New client created");

      pClient_LiPo4->setClientCallbacks(&ClientCB_LiPo4, false);
        /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
         */
      pClient_LiPo4->setConnectionParams(12,24,0,400); //12,12,0,51         // Lipo4 versucht genau diese zu setzen

        /** Set how long we are willing to wait for the connection to complete (milliseconds), default is 30000. */
      pClient_LiPo4->setConnectTimeout(4 * 1000);


      if (!pClient_LiPo4->connect(advDevice_LiPo4, false)) 
        {// Created a client but failed to connect, don't need to keep it as it has no data
        Serial.println("1. LiPo4-connect failed");
        if (!pClient_LiPo4->connect(advDevice_LiPo4, false))   // try again
          {
          NimBLEDevice::deleteClient(pClient_LiPo4);
          Serial.println("2. LiPo4-connect failed, deleted client");
          return false;
          }
        }
      }

    if(!pClient_LiPo4->isConnected())
      {if (!pClient_LiPo4->connect(advDevice_LiPo4)) 
        {
        Serial.println("1. LiPo4-connect failed");
        if (!pClient_LiPo4->connect(advDevice_LiPo4))     // Try again
          {
          Serial.println("2. LiPo4-connect failed");
          return false;
          }
        }
      }

      Serial.print("Connected to LiPo4: ");
      Serial.println(pClient_LiPo4->getPeerAddress().toString().c_str());
      Serial.print("RSSI LiPo4: ");
      Serial.println(pClient_LiPo4->getRssi());

/** Now we can read/write/subscribe the charateristics of the services we are interested in */
    pSvc_LiPo4 = pClient_LiPo4->getService("6e400001-b5a3-f393-e0a9-e50e24dcca9e");   // Lithium BAT
    if(pSvc_LiPo4) 
      {     // make sure it's not null
      pChrR_LiPo4 = pSvc_LiPo4->getCharacteristic("6e400003-b5a3-f393-e0a9-e50e24dcca9e"); // Read Command
      if(pChrR_LiPo4) // make sure it's not null
        {      
        if(pChrR_LiPo4->canRead())              // Not used as Notify only
          {
          Serial.print(pChrR_LiPo4->getUUID().toString().c_str());
          Serial.print(" Value: ");
          Serial.println(pChrR_LiPo4->readValue().c_str());  //println ASCII

          std::string rxValue = pChrR_LiPo4->readValue().c_str();    //c_Str = null terminated
	        if (rxValue.length() > 0)
            {
		        //Serial.print(rxValue.length(),DEC);
            //Serial.println(" *********");
		        Serial.print("Received Value: ");
		        for (int i = 0; i < rxValue.length(); i++)
		          {Serial.print(rxValue[i],HEX);}
            Serial.println("Not the real value!");
            }
          }

            /** registerForNotify() has been removed and replaced with subscribe() / unsubscribe().
            *  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr.
            *  Unsubscribe takes no parameters.
            */
        if(pChrR_LiPo4->canNotify())
          {
          Serial.println("Notify für LiPo4 einrichten");
          if(!pChrR_LiPo4->subscribe(true, notifyCB_LiPo4, true)) // Disconnect if subscribe failed
            { 
            Serial.println("1. LiPo4 Notify registering failed");
            if(!pChrR_LiPo4->subscribe(true, notifyCB_LiPo4, true))
              {
              Serial.println("2. LiPo4 Notify registering failed");
              if(!pChrR_LiPo4->subscribe(true, notifyCB_LiPo4, true))
                {
                Serial.println("3. LiPo4 Notify registering failed");
                pClient_LiPo4->disconnect();
                return false;
                }
              }
            }
          else 
            {Serial.println("LiPo4 Notification / Subscribe successfull");}
          }
        else 
          {if(pChrR_LiPo4->canIndicate())
            // Send false as first argument to subscribe to indications instead of notifications 
            {if(!pChrR_LiPo4->subscribe(false, notifyCB_LiPo4))     //if(!pChr->registerForNotify(notifyCB_LiPo4, false)) {
              // Disconnect if subscribe failed 
              {pClient_LiPo4->disconnect();
              return false;
              }
            }
          }
        }

      pChrW_LiPo4 = pSvc_LiPo4->getCharacteristic("6e400002-b5a3-f393-e0a9-e50e24dcca9e"); // Write Command after Notify !!!
      if(pChrW_LiPo4)                  // make sure it's not null 
        {if(pChrW_LiPo4->canWrite())   //canWriteNoResponse()
          {
          delay(100);              // Kurze Pause damit die Lipo4 das Notify eingerichtet hat und das 1.Write nicht veloren geht
          pChrW_LiPo4->writeValue(BMS_CMD,7);      //char BMS_CMD[7]= {0xDD,0xA5,0x03,0x00,0xFF,0xFD,0x77};
          delay(100);              // Kurze Pause damit die Lipo4 das Notify eingerichtet hat und das 1.Write nicht veloren geht
          pChrW_LiPo4->writeValue(BMS_CMD,7);      //char BMS_CMD[7]= {0xDD,0xA5,0x03,0x00,0xFF,0xFD,0x77};
          Serial.printf("Wrote new value to: %s\n", pChrW_LiPo4->getUUID().toString().c_str());
          }
        }
      }
    else 
      {
      Serial.println("LiPo4-Service not found.");
      pClient_LiPo4->disconnect();                // Test try again
      return false;
      }

    Serial.println("Done with this LiPo4-device!");
    return true;
}

void setup()
  {
    Serial.begin(115200);
    NimBLEDevice::init("");
    NimBLEDevice::setMTU(36);                // Max Anzahl Zeichen pro Datensatz / Notify (Default 23)
    pBLEScan = NimBLEDevice::getScan();   // Create the scan object.
    pBLEScan->setScanCallbacks(&scanCallbacks, false); // Set the callback for when devices are discovered, no duplicates.
    pBLEScan->setActiveScan(true);       // Set active scanning, this will get more data from the advertiser.
    pBLEScan->setFilterPolicy(0);         // 1=BLE_HCI_SCAN_FILT_USE_WL funktioniert nicht. Warum ????
    //pBLEScan->setMaxResults(0);         // 0=do not store the scan results => no connectio, use callback only.
  }

void loop()
{
  Serial.println("Scanning for BLE Devices ...");
  pBLEScan->start(10000, nullptr, false);       // 0 == scan until infinity 

  while(pBLEScan->isScanning() == true)     // Scanned x Sec = Blocking
    {if(doConnect_LiPo4 == true)          // Device found-> stop scan
	    {pBLEScan->stop();
      Serial.println("LiPo4 found");}
    }
  Serial.println("Scan finished");
  pBLEScan->stop();
  delay(100);              // wait 100ms, so scan can finish and clean up

  if(doConnect_LiPo4) // MAC+Service found
    {doConnect_LiPo4 = false;                  // Reset for next loop
    Serial.println("Connect to Lipo4 and enable notifications");
    if(connectToServer_LiPo4()) 
      {
      Serial.println("Success! we should now be getting LiPo4 notifications!");
      }
    }

  Serial.println("Wait for disconnect to finish");
  while(pClient_LiPo4->isConnected())     // wait until disconnected
    {delay(10);}

  Serial.println("Wait 20 sec befor connecting, again"); 
  delay(20000);
}
