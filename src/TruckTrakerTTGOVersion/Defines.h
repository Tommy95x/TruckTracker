//---------------Common Includes--------------------
#include <FreeRTOSConfig.h>

//-----------------GPS Stuff-----------------------
#define GPS_RX_PIN 12  // filo bianco (TX GPS)
#define GPS_TX_PIN 13  // filo giallo (RX GPS)
#define RXD2 GPS_RX_PIN
#define TXD2 GPS_TX_PIN
#define GPS_BAUD 38400
const TickType_t tReadAndWriteGPSDataDelay = 500 / portTICK_PERIOD_MS;
const TickType_t tConnectAndSendDataDelay = 500 / portTICK_PERIOD_MS;

//-----------------WireGuard Stuff-----------------------

String endpoint_address = "http://casagrilli.ddns.net";    // [Peer] Endpoint
int endpoint_port = 5055;     

struct GPSData
{
    bool bDataValidity;
    int iSatellites;
    float fHdop;
    int iAltitude;
    double dLatitude;
    double dLongitude;
    double dSpeed;
    int iCourse;
    int iDay;
    int iMonth;
    int iYear;
    int iHour;
    int iMinute;
    int iSecond;

    GPSData() {}

    GPSData(const GPSData& t) : bDataValidity(t.bDataValidity),
                                iSatellites{t.iSatellites},
                                fHdop{t.fHdop},
                                iAltitude{t.iAltitude},
                                dLatitude{t.dLatitude},
                                dLongitude{t.dLongitude},
                                dSpeed{t.dSpeed},
                                iCourse{t.iCourse},
                                iDay{t.iDay},
                                iMonth{t.iMonth},
                                iYear{t.iYear},
                                iHour{t.iHour},
                                iMinute{t.iMinute},
                                iSecond{t.iSecond} {}
};

//-----------------EEPROM Stuff-----------------------
#define EEPROM_SIZE 512

//-----------------Console Stuff----------------------
#define CONSOLE_BAUD 115200

unsigned long ullast_serial_time = 0;

//-----------------SD Card Stuff----------------------
#define SD_CS 5 //SD Pin Module
#define UPDATE_LOG_LOOP 50

//-----------------Traccar Stuff----------------------
const String strTraccatDeviceNum = "traccar1";
const String strTraccarUrl =  endpoint_address+ ":" +String(endpoint_port);

//------------------Wifi Stuff------------------------
// Replace with your network credentials
const char* ssid     = "TIM-98428495";
const char* password = "grilligiuseppe2018";

//-----------------GPRS/SIM Stuff---------------------
// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22

// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb
#define GSM_BOUND 250000

// Your GPRS credentials (leave empty, if not needed)
const char apn[]      = ""; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
const char gprsUser[] = ""; // GPRS User
const char gprsPass[] = ""; // GPRS Password

// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = ""; 