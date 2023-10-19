#include <LoRa.h>
#include "boards.h"

#include <WiFi.h>
#include <sys/time.h>                                                                                    
#include <time.h>

#include "driver/adc.h"
#include <esp_wifi.h>
#include <esp_bt.h>
#include <Ticker.h>

//Libraries for BME280
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define RADIO_DIO0_PIN   26

//BME280 definition
#define SDA 21
#define SCL 22

float temperature = 0;
float humidity = 0; 
float pressure = 0;

int flag = 0;

WiFiClient client; 

#define TZ "EST+5EDT,M3.2.0/2,M11.1.0/2"

int DOW, MONTH, DATE, YEAR, HOUR, MINUTE, SECOND;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

int lc = 0;
time_t tnow;

char strftime_buf[64];

String dtStamp(strftime_buf);

Ticker myTicker;

// How many seconds the ESP should sleep
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 60 * 2 * uS_TO_S_FACTOR   /* Time ESP32 will go to sleep (in seconds)      TIME_TO_SLEEP = seconds * minutes  */

TwoWire I2Cone = TwoWire(1);
Adafruit_BME280 bme;

void startBME(){
  I2Cone.begin(SDA, SCL, 100000); 
  bool status1 = bme.begin(0x76, &I2Cone);  
  if (!status1) {
    Serial.println("Could not find a valid BME280_1 sensor, check wiring!");
    while (1);
  }
}

//define the pins used by the LoRa transceiver module
#define SCK 5
#define MISO 19
#define MOSI 27
#define CS 18
#define RST 23
#define DIO0 26

//433E6 for Asia
//866E6 for Europe
//915E6 for North America
#define BAND 915E6

//packet counter
int readingID = 0;

int counter = 0;
String LoRaMessage = "";

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
  strftime(strftime_buf, sizeof(strftime_buf), "%a %m %d %Y  %H:%M:%S", localtime(&tnow));   
  dtStamp = strftime_buf;
  dtStamp.replace(" ", "-");
  return (dtStamp);  

}

void getReadings(){
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
}

void sendReadings() {
  LoRaMessage = String(readingID) + "/" + String(temperature * 1.8 + 32) + "&" + String(humidity) + "#" + String(pressure);
  //Send LoRa packet to receiver
  LoRa.beginPacket();
  LoRa.print(LoRaMessage);
  LoRa.endPacket();
  readingID++;
}


void tick() {
  flag = 1;
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n\n", wakeup_reason); break;
  }

}


void setup()
{
  initBoard();
  // When the power is turned on, a delay is required.
  delay(1500);

  Serial.println("sleep test");

  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
  if (!LoRa.begin(LoRa_frequency)) {
      Serial.println("Starting LoRa failed!");
      while (1);
  }
  
  delay(2000);

  print_wakeup_reason();

  u8g2->sleepOn();
  LoRa.sleep();

  SPI.end();
  SDSPI.end();

  pinMode(RADIO_CS_PIN, INPUT);
  pinMode(RADIO_RST_PIN, INPUT);
  pinMode(RADIO_DIO0_PIN, INPUT);
  pinMode(RADIO_CS_PIN, INPUT);
  pinMode(I2C_SDA, INPUT);
  pinMode(I2C_SDA, INPUT);
  pinMode(I2C_SCL, INPUT);
  pinMode(OLED_RST, INPUT);
  pinMode(RADIO_SCLK_PIN, INPUT);
  pinMode(RADIO_MISO_PIN, INPUT);
  pinMode(RADIO_MOSI_PIN, INPUT);
  pinMode(SDCARD_MOSI, INPUT);
  pinMode(SDCARD_MISO, INPUT);
  pinMode(SDCARD_SCLK, INPUT);
  pinMode(SDCARD_CS, INPUT);
  pinMode(BOARD_LED, INPUT);
  pinMode(ADC_PIN, INPUT);

  delay(2000);

  esp_sleep_enable_timer_wakeup(60 * 1000 *1000);

  startBME(); 

  getReadings();
 
  startLoRA();
  delay(2000);
  sendReadings();
  Serial.println("Data packet sent");
  BME280_Sleep();
  flag = 0;
  goToDeepSleep();

}

void loop()
{
}

void BME280_Sleep() 
{

  Wire.beginTransmission(0x76);    // or 0x77
  Wire.write((uint8_t)0xF4);       // Select Control Measurement Register
  Wire.write((uint8_t)0b00000000); // Send '00' for Sleep mode
  Wire.endTransmission();

}

void goToDeepSleep()
{

  Serial.println("Going to sleep...");
  
  /*
  LoRa.sleep();
  SPI.end();
  //SDSPI.end();
  delay(2000);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  //btStop();
  adc_power_off();
  esp_wifi_stop();
  //esp_bt_controller_disable();
  */
  
  esp_deep_sleep_start();

}

//Initialize LoRa module
void startLoRA(){
  //SPI LoRa pins
  SPI.begin(SCK, MISO, MOSI, CS);

  while (!LoRa.begin(BAND) && counter < 10) {
    Serial.print(".");
    counter++;
    delay(500);
  }
  if (counter == 10) {
    // Increment readingID on every new reading
    readingID++;
    Serial.println("Starting LoRa failed!"); 
  }
  Serial.println("LoRa Initialization OK!");
  delay(2000);
}


void start_wifi()
{
	
  // Replace with your network details
  const char* ssid = "R2D2";
  const char* password = "sissy4357";
  
  //setting the addresses
  IPAddress ip(10, 0, 0, 30);
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
