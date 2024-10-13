//-------------------------------------------------
//------------------Includes-----------------------

#include "FS.h"
#include "SD.h"
#include "Defines.h"
#include "TinyGPS++.h"
#include "SoftwareSerial.h"
#include "BluetoothSerial.h"

#include <WiFi.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <TinyGsmClient.h>

//-------------------------------------------------
//-------------Functions Declaration---------------
void setupSD();
void setupGPS();
void setupSIM();
void setupWifi();
void setupEEPROM();
void setupConsole();
void decodeGPSData();
void writeFile(fs::FS &fs, const char * path, const char * message);
void appendFile(fs::FS &fs, const char * path, const char * message);

void sendData();
void readGPSData();
void memorizeData();
void openOrCreateLogFile();
void simConnectionManagement();
void wifiConnectionManagement();

void readAndWriteGPSDataTask(void* parameters);
void connectAndSendDataTask(void* parameters);

//-------------------------------------------------
//-------------Variables Declaration---------------
TinyGPSPlus gps;
GPSData gpsData;

String strLogFile;

TinyGsm modem(SerialAT);

TinyGsmClient client(modem);

SoftwareSerial GPSSerial (GPS_RX_PIN, GPS_TX_PIN);

bool bWifiConnectedFlag = false;
bool bSIMConnectedFlag = false;
bool bFileFlag = false;
bool bSDFlag = false;

TaskHandle_t thReadAndWriteHandlerTask;
TaskHandle_t thconnectAndSendHandleTask;

SemaphoreHandle_t shGPSDataSemaphore = NULL;

//-------------------------------------------------
//-------------Main Functions----------------------

//-------------------------------------------------
void setup() 
{
  shGPSDataSemaphore = xSemaphoreCreateMutex();

  setupConsole();
  setupEEPROM();  
  setupSD();
  setupWifi();
  //setupSIM();  
  setupGPS();

  Serial.println("Create Task readAndWriteGPSDataTask");
  xTaskCreatePinnedToCore(readAndWriteGPSDataTask,          // Function that should be called
                          "ReadGPS&WriteData",              // Name of the task (for debugging)
                          10000,                            // Stack size (bytes)
                          NULL,                             // Parameter to pass
                          0,                                // Task priority
                          &thReadAndWriteHandlerTask,       // Task handle
                          0                                 // Core you want to run the task on (0 or 1)
  );

  Serial.println("Create Task connectAndSendDataTask");
  xTaskCreatePinnedToCore(connectAndSendDataTask,           // Function that should be called
                          "ManageConnectionAndSendData",    // Name of the task (for debugging)
                          10000,                            // Stack size (bytes)
                          NULL,                             // Parameter to pass
                          1,                                // Task priority
                          &thconnectAndSendHandleTask,      // Task handle
                          1                                 // Core you want to run the task on (0 or 1)
  );
}

//-------------------------------------------------
void loop() {}

//-------------------------------------------------
//------------Functions Implementation-------------

//-------------------------------------------------
void setupSD()
{
  // Initialize SD card

  uint8_t cardType;

  SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) 
  {
    Serial.println("Card Mount Failed");
    return;
  } 

  cardType = SD.cardType();
  if(cardType == CARD_NONE) 
  {
    Serial.println("No SD card attached");
    return;
  }
  
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) 
  {
    Serial.println("ERROR - SD card initialization failed!");
    return;    // init failed
  }

  bSDFlag = true;
}

//-------------------------------------------------
void setupGPS()
{
  ullast_serial_time = 0;
  shGPSDataSemaphore = xSemaphoreCreateMutex();
  
  Serial.println("Setup GPS");
  pinMode(GPS_RX_PIN, INPUT);
  pinMode(GPS_TX_PIN, OUTPUT);
  // GPS Serial
  //GPSSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  GPSSerial.begin(GPS_BAUD);
}

void setupSIM()
{
  Serial.println("SIM Setup");
  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(GSM_BOUND, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  modem.restart();
  // use modem.init() if you don't need the complete restart

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) 
    modem.simUnlock(simPIN);
}

//-------------------------------------------------
void setupWifi()
{
  Serial.println("Wifi Setup");
  WiFi.begin(ssid, password);
}

//-------------------------------------------------
void setupEEPROM()
{
  //EPROM
  Serial.println("EEPROM Setup");
  EEPROM.begin(EEPROM_SIZE); 
}

//-------------------------------------------------
void setupConsole()
{
  //Debug Software Serail
  Serial.begin(CONSOLE_BAUD);
  Serial.println("Serial Start");
}

//-------------------------------------------------
void readGPSData()
{
  decodeGPSData();
  if(GPS_DEBUG && millis() - ullast_serial_time > SERIAL_DEBUG_LOOP)
  {
    char sz[32];
    
    Serial.print("--------GPS Encoded Data:--------\n");
    Serial.print(gps.satellites.value());
    Serial.print(",");
    Serial.print(gps.altitude.meters());
    Serial.print(" m,");

    if (gps.date.isValid()) 
    {
      sprintf(sz, "%02d/%02d/%02d,", gps.date.day(), gps.date.month(), gps.date.year());
      Serial.print(sz);

      if(strLogFile.length() <= 0)
      {
        sprintf(sz, "%02d_%02d_%02d,", gps.date.day(), gps.date.month(), gps.date.year());
        String strFileName = sz;
        strLogFile = "/"+strFileName+".txt";
      }

      sprintf(sz, "%02d:%02d:%02d,", gps.time.hour(), gps.time.minute(), gps.time.second());
      Serial.print(sz);
    } 
    else 
    {
      Serial.print(F("********,"));
      Serial.print(F("********,"));
    }

    Serial.printf("%011.8f°,", gps.location.lat());
    Serial.printf("%011.8f°,", gps.location.lng());
    Serial.printf("%03d,", (int)gps.course.deg());
    Serial.printf("%03.1f", gps.speed.knots());
    Serial.print("\n--------END GPS Encoded Data--------");
    Serial.println();

    ullast_serial_time = millis();
  }
}

//-------------------------------------------------
void memorizeData()
{
  while (xSemaphoreTake(shGPSDataSemaphore, portMAX_DELAY) != pdPASS);
  
  char sz[32];
  
  sprintf(sz, "%02d/%02d/%02d,", gps.date.day(), gps.date.month(), gps.date.year());
  String strDate = sz;

  sprintf(sz, "%02d:%02d:%02d,", gps.time.hour(), gps.time.minute(), gps.time.second());
  String strTime = sz;
  
  String dataMessage = strTraccatDeviceNum + "," + String(strDate) + "," + String(strTime) + "," + String(gpsData.dLatitude) + "," + String(gpsData.dLongitude) + String(gpsData.iAltitude) + String(gpsData.dSpeed) + "\r\n";
  xSemaphoreGive(shGPSDataSemaphore);
  
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  
  appendFile(SD, strLogFile.c_str(), dataMessage.c_str());
}

//-------------------------------------------------
void decodeGPSData()
{
  // GPS Read
  char tmp_c;
  while (GPSSerial.available()) 
  {
    tmp_c = GPSSerial.read();
    if (RAW_GPS_DEBUG)
    {
      Serial.print("--------GPS Raw Data:--------\n");
      Serial.print(tmp_c);
      Serial.print("\n--------END GPS Raw Data--------");
      Serial.println();
    }

    gps.encode(tmp_c);
  }

  while (xSemaphoreTake(shGPSDataSemaphore, portMAX_DELAY) != pdPASS);

  gpsData.bDataValidity  = gps.location.isValid();
  gpsData.iSatellites    = gps.satellites.isValid()  ? gps.satellites.value()      : 0;
  gpsData.dLatitude      = gps.location.isValid()    ? gps.location.lat()          : 0.0;
  gpsData.dLongitude     = gps.location.isValid()    ? gps.location.lng()          : 0.0;
  gpsData.fHdop          = gps.hdop.isValid()        ? gps.hdop.hdop()             : 0.0;
  gpsData.iAltitude      = gps.altitude.isValid()    ? (int)gps.altitude.meters()  : 0;
  gpsData.dSpeed         = gps.speed.isValid()       ? gps.speed.knots()           : 0.0;
  gpsData.iCourse        = gps.course.isValid()      ? (int)gps.course.deg()       : 0;
  gpsData.iDay           = gps.date.isValid()        ? (int)gps.date.day()         : 0;
  gpsData.iMonth         = gps.date.isValid()        ? (int)gps.date.month()       : 0;
  gpsData.iYear          = gps.date.isValid()        ? (int)gps.date.year()        : 0;
  gpsData.iHour          = gps.date.isValid()        ? (int)gps.time.hour()        : 0;
  gpsData.iMinute        = gps.date.isValid()        ? (int)gps.time.minute()      : 0;
  gpsData.iSecond        = gps.date.isValid()        ? (int)gps.time.second()      : 0;

  xSemaphoreGive(shGPSDataSemaphore);
}

//-------------------------------------------------
void sendData()
{
  if(bSIMConnectedFlag || bWifiConnectedFlag)
  {
    Serial.println("Try to send data");

    while (xSemaphoreTake(shGPSDataSemaphore, portMAX_DELAY) != pdPASS);
    Serial.println("Get GPS data to send");
    if(gpsData.bDataValidity)
    {
      HTTPClient http;
      String strRequest = strTraccarUrl+"/?id="+strTraccatDeviceNum+"&lat="+gpsData.dLatitude+"&lon="+gpsData.dLongitude+"&speed="+gpsData.dSpeed+"";

      Serial.println("Sending Data to " + strTraccarUrl + ": " + strRequest);

      http.begin(strRequest); 
      xSemaphoreGive(shGPSDataSemaphore);
    
      int httpCode = http.GET();
                                           
      if (httpCode == 200) 
      { //Check for the returning code
        Serial.println("DATA SENT TO THE SERVER");
        delay(3000);
      }
      else 
      {         
        Serial.println("Error on HTTP request");
      }
  
      http.end(); //Free the resources}
    }
    else
    {
      xSemaphoreGive(shGPSDataSemaphore);
    }
  }
}

//-------------------------------------------------
void openOrCreateLogFile()
{
  //"/"+strLogFileName+".txt";
  File fLogFile = SD.open(strLogFile.c_str());
  if(!fLogFile) 
  {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, strLogFile.c_str(), "Truck ID, Date, Time of Day, Latitude, Longitude, Altitude, Speed \r\n");
  }
  else 
  {
    Serial.println("File " +strLogFile+" already exists");  
  }

  fLogFile.close();

  bFileFlag = true;
}

//-------------------------------------------------
void simConnectionManagement()
{
  
  Serial.print("Connecting to APN: ");
  Serial.print(apn);

  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) 
  {
    Serial.println(" fail");
  }
  else 
  {
    Serial.println(" OK");
    
    bSIMConnectedFlag = true;
  }    
}

//-------------------------------------------------
void wifiConnectionManagement()
{
  if(WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Not connected to Wifi");
    bWifiConnectedFlag = false;
  } 
  else
  {
    Serial.println("Connect to Wifi");
    bWifiConnectedFlag = true;
  } 
}

//-------------------------------------------------
void writeFile(fs::FS &fs, const char * path, const char * message) 
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

//-------------------------------------------------
void appendFile(fs::FS &fs, const char * path, const char * message) 
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

//-------------------------------------------------
void readAndWriteGPSDataTask(void* parameters)
{
  while(true)
  {
    readGPSData();
    
    if(bSDFlag && !bFileFlag && strLogFile.length() > 0)
      openOrCreateLogFile();
    
    if(bSDFlag && !bFileFlag && !gpsData.bDataValidity)
      memorizeData(); 

    if(tReadAndWriteGPSDataDelay > 0)
      vTaskDelay( tReadAndWriteGPSDataDelay );
  }
}

//-------------------------------------------------
void connectAndSendDataTask(void* parameters)
{
  while(true)
  {
    wifiConnectionManagement();
    
    //simConnectionManagement();

    if(bSIMConnectedFlag || bWifiConnectedFlag)
      sendData();

    if(bSIMConnectedFlag)
    {
      modem.gprsDisconnect();
      Serial.println(F("GPRS disconnected"));      
      bSIMConnectedFlag = false;      
    }
      
    if(tConnectAndSendDataDelay > 0)
      vTaskDelay( tConnectAndSendDataDelay );
  }
}