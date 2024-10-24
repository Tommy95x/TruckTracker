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

String endpoint_address = "https://casagrilli.ddns.net";    // [Peer] Endpoint
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


//-------------------Server Certificate---------------

const char* server_cert = R"(
-----BEGIN CERTIFICATE-----
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
-----END CERTIFICATE-----
)";

// Bearer token statico
String bearerToken = "Bearer 08a80a21-d459-45c5-b0b3-527681f14a9e-99b7fbc98ea3d7f466c9ab015ee23abf";

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