
#include <LoRa.h>
#include "boards.h"

// Import Wi-Fi library
#include <WiFi.h>
#include <AsyncTCP.h> 
#include "ESPAsyncWebServer.h"
#include <FTPServer.h> 

//Library to send Alerts
#include "EMailSender.h"   //https://github.com/xreef/EMailSender

#include <LittleFS.h>

// Libraries to get time from NTP Server
#include <WiFiUdp.h>
#include <sys/time.h>                                                                                    
#include <time.h>

//Library for Graphing
#include <ThingSpeak.h>

// Replace with your network credentials
const char* ssid     = "R2D2";
const char* password = "sissy4357";

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

#define FORMAT_LITTLEFS_IF_FAILED false

/*
  Found this reference on setting TZ: http://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
  Here are some example TZ values, including the appropriate Daylight Saving Time and its dates of applicability. 
  In North American Eastern Standard Time (EST) and Eastern Daylight Time (EDT), the normal offset from UTC is 5 hours; 
  since this is west of the prime meridian, the sign is positive. Summer time begins on March's second Sunday at 
  2:00am, and ends on November's first Sunday at 2:00am.
  
*/

#define TZ "EST+5EDT,M3.2.0/2,M11.1.0/2"

WiFiClient client;

FTPServer ftpSrv(LittleFS);

const char * ftpUser = "tech500";
const char * ftpPassword = "treyburn";

///Are we currently connected?
boolean connected = false;

WiFiUDP udp;
// local port to listen for UDP packets
//Settings pertain to NTP time servers
const int udpPort = 123;
//NTP Time Servers
const char * udpAddress1 = "us.pool.ntp.org";
const char * udpAddress2 = "time.nist.gov";
char incomingPacket[255];
char replyPacket[] = "Hi there! Got the message :-)";

// Variables to save date and time
String formattedDate;
String day;
String hour;
String timestamp;

// Initialize variables to get and save LoRa data
int rssi;
String loRaMessage;
String temperature;
String humidity;
String pressure;
String readingID;


int DOW, MONTH, DATE, YEAR, HOUR, MINUTE, SECOND;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

int lc = 0;
time_t tnow;

char strftime_buf[64];

String dtStamp(strftime_buf);

EMailSender emailSend("lucidw.esp8266@gmail.com", "adhsmbhxkthrhvut");  //gmail account, password --not primary gmail account!

//Graphing requires "FREE" "ThingSpeak.com" account..  
//Enter "ThingSpeak.com" data here....
unsigned long myChannelNumber =  2246200;
const char * myWriteAPIKey = "UR6ULZK3YZ3C52D4";

/*
  This is the ThingSpeak channel number for the MathwWorks weather station
  https://thingspeak.com/channels/YourChannelNumber.  It senses a number of things and puts them in the eight
  field of the channel:

  Field 1 - Temperature (Degrees F )
   
*/

int alert = 1;
int alertFlag;

// Create AsyncWebServer object on port 80
AsyncWebServer serverAsync(8025);

void onRequest(AsyncWebServerRequest *request)
{
  //Handle Unknown Request
  request->send(404);
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  //Handle body
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  //Handle upload
}

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  //Handle WebSocket event
}

// Read LoRa packet and get the sensor readings
void getLoRaData() 
{
  Serial.print("Lora packet received: ");
  // Read packet
  while (LoRa.available()) {
    String LoRaData = LoRa.readString();
    // LoRaData format: readingID/temperature&soilMoisture#batterylevel
    // String example: 1/27.43&654#95.34
    Serial.print(LoRaData); 
    
    // Get readingID, temperature and soil moisture
    int pos1 = LoRaData.indexOf('/');
    int pos2 = LoRaData.indexOf('&');
    int pos3 = LoRaData.indexOf('#');
    readingID = LoRaData.substring(0, pos1);
    temperature = LoRaData.substring(pos1 +1, pos2);
    humidity = LoRaData.substring(pos2+1, pos3);
    pressure = LoRaData.substring(pos3+1, LoRaData.length());    
  }
  
  // Get RSSI
  rssi = LoRa.packetRssi();
  Serial.print(" with RSSI ");    
  Serial.println(rssi);
}

void setup()
{
  initBoard();
  // When the power is turned on, a delay is required.
  delay(1500);

  Serial.println("LoRa Receiver");

  start_wifi();

  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DI0_PIN);
  if (!LoRa.begin(LoRa_frequency)) {
      Serial.println("Starting LoRa failed!");
      while (1);
  }

  //set up TimeZone in local environment
  configTime(0, 0, udpAddress1, udpAddress2);
  setenv("TZ", "EST+5EDT,M3.2.0/2,M11.1.0/2", 3);   // this sets TZ to Indianapolis, Indiana
  tzset();

  bool fsok = LittleFS.begin(true);
  Serial.printf_P(PSTR("FS init: %s\n"), fsok ? PSTR("ok") : PSTR("fail!"));

  ftpSrv.begin(F(ftpUser), F(ftpPassword)); //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)

  Serial.print("wait for first valid timestamp ");

  while (time(nullptr) < 100000ul)
  {
    Serial.print(".");
    delay(5000);
  }

  Serial.println(" time synced");
  
  getDateTime();

  Serial.println(dtStamp);

  ThingSpeak.begin(client);
  
  // Route for root / web page
  serverAsync.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/data/index.html", String(), false, processor);
  });
  
  serverAsync.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", temperature.c_str());
  });
   
  serverAsync.on("/dtStamp", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", dtStamp.c_str());
  });
 
  serverAsync.on("/rssi", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(rssi).c_str());
  });
  serverAsync.on("/winter", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/data/winter.jpg", "image/jpg");
  });

  serverAsync.on("/turnOffAlert", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "ok");
    alert = 0;
    alertFlag = 1;
  });

  serverAsync.onNotFound(onRequest);
  serverAsync.onFileUpload(onUpload);
  serverAsync.onRequestBody(onBody);
  serverAsync.onNotFound(notFound);

  // Start server
  serverAsync.begin();

}

void loop()
{
    
  if(alertFlag == 1)
  {
    alertFlag = 0;
    exit;
  }

  for (int x = 1; x < 5000; x++)
  {
    ftpSrv.handleFTP();    
  }
  
  //udp only send data when connected
	if (connected)
	{
    //Send a packet
    udp.beginPacket(udpAddress1, udpPort);
    udp.printf("Seconds since boot: %u", millis() / 1000);
    udp.endPacket();
	}

  getDateTime();
  
  if((MINUTE % 2 == 0) && (SECOND == 10))
  {
    //Serial.println(dtStamp);
    //sendAlerts();
  }
  
	//Set alert back to enabled if disabled
	if ((MINUTE % 30 == 0) && (SECOND == 0))
	{

	  alert = 1;
      
	  //Serial.println(dtStamp);
 
 	  

	}

  // Check if there are LoRa packets available
	int packetSize = LoRa.parsePacket();
	if (packetSize) {
		getLoRaData();
		getDateTime();
    thingSpeak();
	} 

/*
#ifdef HAS_DISPLAY
        if (u8g2) {
            u8g2->clearBuffer();
            char buf[256];
            u8g2->drawStr(0, 12, "Received OK!");
            u8g2->drawStr(0, 26, recv.c_str());
            snprintf(buf, sizeof(buf), "RSSI:%.2f", LoRa.packetRssi());
            u8g2->drawStr(0, 40, buf);
            u8g2->sendBuffer();
        }
#endif

    } 
    */  
}

String getDateTime()
{
  struct tm *ti;

  tnow = time(nullptr) + 1;
  ti = localtime(&tnow);
  DOW = ti->tm_wday;
  YEAR = ti->tm_year + 1900;
  MONTH = ti->tm_mon + 1;
  DATE = ti->tm_mday;
  HOUR  = ti->tm_hour;
  MINUTE  = ti->tm_min;
  SECOND = ti->tm_sec;

  strftime(strftime_buf, sizeof(strftime_buf), "%a , %m/%d/%Y , %H:%M:%S %Z", localtime(&tnow));
  dtStamp = strftime_buf;
  return (dtStamp);

}

void sendAlerts()
{
      
  if((temperature > "32") && (alert == 1))
  {    
      EMailSender::EMailMessage message;
      message.subject = "Warning --Temperature Rise!!!";
      message.message = "Urgent --Check Refridgerator!!";
    
      EMailSender::Response resp = emailSend.send("3173405675@vtext.com", message);
      emailSend.send("lucidw.esp8266@gmail.com", message);
      
      Serial.println("Sending status: ");
    
      Serial.println(resp.status);
      Serial.println(resp.code);
      Serial.println(resp.desc);
  }
   
  alert = 0;
  
}

String processor(const String& var){

  getDateTime();

  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return temperature;
  }
  else if(var == "TIMESTAMP"){

    return dtStamp;
  }
  else if (var == "RRSI"){
    return String(rssi);
  }
  return String();
}

void thingSpeak()
{

  // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
  // pieces of information in a channel.  Here, we write to field 1.
  ThingSpeak.writeField(myChannelNumber, 1, temperature, myWriteAPIKey); 

  Serial.println("Sent data to Thingspeak.com  " + dtStamp + "\n");

  delay(2000);

}

void start_wifi()
{
	
  // Replace with your network details
  const char* ssid = "R2D2";
  const char* password = "sissy4357";
  
  //setting the addresses
  IPAddress ip(10, 0, 0, 27);
  IPAddress gateway(10, 0, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress primaryDNS(10,0,0,1);
  IPAddress secondaryDNS(10,0,0,1);

  WiFi.persistent( false ); // for time saving

  // Connecting to local WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.begin(ssid, password);
  delay(10);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Server IP:  ");
  Serial.println(WiFi.localIP());
  Serial.println("");

}