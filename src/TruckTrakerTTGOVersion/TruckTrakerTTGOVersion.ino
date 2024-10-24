//-------------------------------------------------
//------------------Includes-----------------------

#include "FS.h"
#include "SD.h"
#include "Defines.h"
#include "TinyGPS++.h"
#include "SoftwareSerial.h"
#include "Common/FilesManager.h"
#include "Common/CommonDefines.h"

#include <WiFi.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <TinyGsmClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

//-------------------------------------------------
//-------------Functions Declaration---------------
void setupSD();
void setupGPS();
void setupSIM();
void setupWifi();
void setupEEPROM();
void setupConsole();
void decodeGPSData();

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
  uint64_t cardSize;

  if(SD_DEBUG)
    Serial.println("Initializing SD card...");
 
  if(!SD.begin(SD_CS)) 
  {
    if(SD_DEBUG)
      Serial.println("Card Mount Failed");
    return;
  } 

  cardType = SD.cardType();
  if(cardType == CARD_NONE) 
  {
    if(SD_DEBUG)
      Serial.println("No SD card attached");
    return;
  }

  cardSize = SD.cardSize() / (1024 * 1024);
  if(SD_DEBUG)
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

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

  if(millis() - ullast_serial_time > UPDATE_LOG_LOOP)
  {
    char sz[32];
    
    if(GPS_DEBUG)
    {    
      Serial.print("--------GPS Encoded Data:--------\n");
      Serial.print(gps.satellites.value());
      Serial.print(",");
      Serial.print(gps.altitude.meters());
      Serial.print(" m,");
    }

    if (gps.date.isValid()) 
    {
      sprintf(sz, "%02d/%02d/%02d,", gps.date.day(), gps.date.month(), gps.date.year());

      String strFileName = sz;
      if(GPS_DEBUG)
        Serial.println("Date Info: " + strFileName);
      
      strFileName += ".txt";

      if(strLogFile.length() <= 0 || strFileName != strLogFile)
      {
        strLogFile = "/"+strFileName+".txt";
        bFileFlag = false;
      }

      sprintf(sz, "%02d:%02d:%02d,", gps.time.hour(), gps.time.minute(), gps.time.second());
      if(GPS_DEBUG)
      {
        Serial.print("Time Info: ");
        Serial.print(sz);
        Serial.print("\n");
      }
    } 
    else 
    {
      if(GPS_DEBUG)
      {
        Serial.print(F("********,"));
        Serial.print(F("********,"));
      }
    }

    if(GPS_DEBUG)
    {
      Serial.printf("%011.8f°,", gps.location.lat());
      Serial.printf("%011.8f°,", gps.location.lng());
      Serial.printf("%03d,", (int)gps.course.deg());
      Serial.printf("%03.1f", gps.speed.kmph());
      Serial.print("\n--------END GPS Encoded Data--------");
      Serial.println();
    }

    ullast_serial_time = millis();
  }
}

//-------------------------------------------------
void memorizeData()
{
  while (xSemaphoreTake(shGPSDataSemaphore, portMAX_DELAY) != pdPASS);
  
  char sz[32];
  
  sprintf(sz, "%02d/%02d/%02d,", gpsData.iDay, gpsData.iMonth, gpsData.iYear);
  String strDate = sz;

  sprintf(sz, "%02d:%02d:%02d,", gpsData.iHour, gpsData.iMinute, gpsData.iSecond);
  String strTime = sz;

  sprintf(sz, "%011.8f,", gpsData.dLatitude);
  String strLatitude = sz;

  sprintf(sz, "%011.8f,", gpsData.dLongitude);
  String strLongitude = sz;

  sprintf(sz, "%03d,", gpsData.iAltitude);
  String strAltitude = sz;

  sprintf(sz, "%03.1f", gpsData.dSpeed);
  String strSpeed = sz;
  
  String strDataMessage = strTraccatDeviceNum + "," + strDate + "," + strTime + "," + strLatitude + "," + strLongitude + "," + strAltitude + "," + strSpeed + "\r\n";
  xSemaphoreGive(shGPSDataSemaphore);
  
  if(SD_DEBUG)
  {
    Serial.print("Save data: ");
    Serial.println(strDataMessage);
  }

  appendFile(SD, strLogFile.c_str(), strDataMessage.c_str());
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
  gpsData.dSpeed         = gps.speed.isValid()       ? gps.speed.kmph()            : 0.0;
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
    if(TRACCAR_DEBUG)
      Serial.println("Try to send data");

    while (xSemaphoreTake(shGPSDataSemaphore, portMAX_DELAY) != pdPASS);

    if(gpsData.bDataValidity)
    {
      char sz[32];

      if(TRACCAR_DEBUG)
        Serial.println("Get GPS data to send");

      sprintf(sz, "%011.8f,", gpsData.dLatitude);
      String strLatitude = sz;

      sprintf(sz, "%011.8f,", gpsData.dLongitude);
      String strLongitude = sz;

      sprintf(sz, "%03.1f", gpsData.dSpeed);
      String strSpeed = sz;

      String timeSinceEpoch = calculateTimeSinceToEpoch();

      HTTPClient http;

      int httpCode;

      if(bWifiConnectedFlag){
        httpCode = sendDataWifi(strLatitude, strLongitude, strSpeed, timeSinceEpoch, &http);
      } else {
        //Chiamata sendDataGsm(strLatitude, strLongitude, strSpeed, &http)
      }
                                           
      if (httpCode == 200) 
      { //Check for the returning code
        if(TRACCAR_DEBUG)
          Serial.println("DATA SENT TO THE SERVER");
        //delay(3000);
      }
      else 
      {
        if(TRACCAR_DEBUG)
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

//------------------------------------------------

int sendDataWifi(String strLatitude, String strLongitude, String strSpeed, String timestamp, HTTPClient *http)
{

  WiFiClientSecure *client = new WiFiClientSecure;
  client->setCACert(server_cert);  // Usa il certificato self-signed


  String postData = "id=" + strTraccatDeviceNum + "&lat=" + strLatitude + "&lon=" + strLongitude + "&speed=" + strSpeed + "&timestamp=" + timestamp;

  if(TRACCAR_DEBUG)
    Serial.println("Sending Data to " + strTraccarUrl + ":/?" + postData);

  http->begin(*client, strTraccarUrl); 
  http->addHeader("Authorization", bearerToken);
  http->addHeader("Content-Type", "application/x-www-form-urlencoded");

  xSemaphoreGive(shGPSDataSemaphore);

  int httpCode = http->POST(postData);

  delete client;

  return httpCode;
}

String calculateTimeSinceToEpoch(){
  int year = gpsData.iYear;
  int month = gpsData.iMonth;  // Ottobre
  int day = gpsData.iDay;
  int hour = gpsData.iHour;
  int minute = gpsData.iMinute;
  int second = gpsData.iSecond;

  struct tm t = {0};  // Inizializza tutti i campi a 0
  t.tm_year = year - 1900;  // Anno dal 1900
  t.tm_mon = month - 1;     // Mese da 0 (Gennaio è 0)
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;

  time_t timeSinceEpoch = mktime(&t);

  return String(timeSinceEpoch);
}

//-------------------------------------------------
void openOrCreateLogFile()
{
  //"/"+strLogFileName+".txt";
  File fLogFile = SD.open(strLogFile.c_str());
  if(!fLogFile) 
  {
    if(SD_DEBUG)
    {
      Serial.println("File doens't exist");
      Serial.println("Creating file...");
    }

    writeFile(SD, strLogFile.c_str(), "Truck ID, Date, Time of Day, Latitude, Longitude, Altitude, Speed \r\n");
  }
  else 
  {
    if(SD_DEBUG)
      Serial.println("File " +strLogFile+" already exists");  
  }

  fLogFile.close();

  bFileFlag = true;
}

//-------------------------------------------------
void simConnectionManagement()
{
  
  if(GPRS_DEBUG)
  {
    Serial.print("Connecting to APN: ");
    Serial.print(apn);
  }

  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) 
  {
    if(GPRS_DEBUG)
      Serial.println(" fail");
  }
  else 
  {
    if(GPRS_DEBUG)
      Serial.println(" OK");
    
    bSIMConnectedFlag = true;
  }    
}

//-------------------------------------------------
void wifiConnectionManagement()
{
  if(WiFi.status() != WL_CONNECTED)
  {
    if(WIFI_DEBUG)
      Serial.println("Not connected to Wifi");
    bWifiConnectedFlag = false;
  } 
  else
  {
    if(WIFI_DEBUG)
      Serial.println("Connect to Wifi");
    bWifiConnectedFlag = true;
  } 
}

//-------------------------------------------------
void readAndWriteGPSDataTask(void* parameters)
{
  while(true)
  {
    readGPSData();

    if(FILE_DEBUG)
    {
      String strDataMessage = "Test File\n";
      if(strLogFile.length() <= 0)
      {
        String strFileName = "Test";
        strLogFile = "/"+strFileName+".txt";
      }

      if(!bFileFlag && strLogFile.length() > 0)
        openOrCreateLogFile();

      Serial.print("Save data: ");
      Serial.println(strDataMessage);

      appendFile(SD, strLogFile.c_str(), strDataMessage.c_str());
    }
    
    if(bSDFlag && !bFileFlag && strLogFile.length() > 0)
      openOrCreateLogFile();
    
    if(bSDFlag && bFileFlag && !gpsData.bDataValidity)
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
    
    simConnectionManagement();


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