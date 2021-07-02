/*
  Send Temperature, Humidity, Pressure and Altitude using gy-bme/p280, from Arduino to Zabbix.
  Code for gy-bme/p280 copied and adapted from these posts:
  https://forum.arduino.cc/t/bmp-280-didnt-work/433903/12
  https://forum.arduino.cc/t/bme280-cant-be-found-but-i2c-scanner-recognises-it/602474

  In order to run, install libraries "Adafruit BME280 Library" and "Adafruit Unified Sensor"
  I use this sensor: https://forum.arduino.cc/t/bme280-problem/451941
  Connections:
  gy-bme/p280       Aruino UNO
  ----------------------------
  VCC           >>  3.3v        (My hardware doesn't have 5v to 3.3v integrated regulator)
  GND           >>  GND
  SCL           >>  A5
  SDA           >>  A4
  CSB (Not connected)
  SDD (Not connected)

  Pablo Hawkes
  2021-07-02: original code
*/

#include <Ethernet.h>
#include "Adafruit_BME280.h"

Adafruit_BME280 bme;

//Local network config:
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 240); //Arduino Fixed IP

//Zabbix Config:
const char* server = "192.168.1.90";      //ZABBIX Server IP
const String clientHostName = "Arduino";  //Hostname from this client on Zabbix
const int port = 10051;                   //Zabbix server port is 10051

//Zabbix items:
const String Gybmep280temperatureItemName = "Gybmep280.Temperature";  //Temperature item for gy-bme/p280
const String Gybmep280HumidityItemName  = "Gybmep280.Humidity";       //Humidity item for gy-bme/p280
const String Gybmep280PressureItemName = "Gybmep280.Pressure";        //Pressure item for gy-bme/p280
const String Gybmep280AltitudeItemName = "Gybmep280.Altitude";        //Altitude item for gy-bme/p280

//Interval Config:
const unsigned long interval = 60000;         //Loop time in milliseconds
const unsigned long ResponseTimeout = 10000;  //Zabbix response timeout in milliseconds

const double PressureAtSeaLevel = 1015; //Change this value to your city current barometric pressure (https://www.wunderground.com)

//Global variables:
EthernetClient client;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
unsigned long diff = 0;
unsigned long timeout = 0;
int firstLengthByte = 0;
int secondLengthByte = 0;

double Gybmep280Temperature = -99;
double Gybmep280Humidity = -99;
double Gybmep280Pressure = -99;
double Gybmep280Altitude = -99;

//Functions:
void zabbix_sender(void);

void setup()
{
  Serial.begin(9600);
  Serial.println(F("Connecting to Ethernet. Wait..."));
  delay (2000);

  Ethernet.begin(mac, ip); //Fixed IP
  //Ethernet.begin(mac); //DHCP

  //delay (5000);
  Serial.print(F("IP Address: "));
  Serial.println(Ethernet.localIP());

  if (!bme.begin(BME280_ADDRESS_ALTERNATE)) //Change or clear this parameter in order to use original address
  {
    Serial.println(F("Could not find a valid BME280 sensor, check wiring, address, sensor ID!"));
    Serial.print(F("SensorID was: 0x"));
    Serial.println(bme.sensorID(), 16);
    Serial.print(F("ID 0xFF probably means a bad address, a BMP 180 or BMP 085\n"));
    Serial.print(F("ID 0x56-0x58 is BMP 280,\n"));
    Serial.print(F("ID 0x60 is BME 280.\n"));
    Serial.print(F("ID 0x61 is BME 680.\n"));

    while (1) delay(10);
  }

  Serial.println(F("Setup OK"));
}

void loop()
{
  currentMillis = millis();

  if ( (currentMillis - previousMillis) > interval)
  {
    Ethernet.maintain();  //execute periodically to maintain DHCP lease

    Serial.println("Current: " + String(currentMillis) + " - Previous: " + String(previousMillis));
    previousMillis = currentMillis;

    //Send to zabbix:
    zabbix_sender();
  }
}

void zabbix_sender()
{
  //1. Get data from sensors (in int or double):
  Gybmep280Temperature = bme.readTemperature();
  Gybmep280Humidity = bme.readHumidity();
  Gybmep280Pressure = bme.readPressure() / 100;
  Gybmep280Altitude = bme.readAltitude (PressureAtSeaLevel);

  Serial.print(F("Gybmep280Temperature (°C): "));
  Serial.println(String(Gybmep280Temperature));
  Serial.print(F("Gybmep280Humidity (%): "));
  Serial.println(String(Gybmep280Humidity));
  Serial.print(F("Gybmep280Pressure (mbar): "));
  Serial.println(String(Gybmep280Pressure));
  Serial.print(F("Gybmep280Altitude (m): "));
  Serial.println(String(Gybmep280Altitude));

  //2. creating message:
  String zabbixMessagePayloadHeader =  "{ \"request\":\"sender data\", \"data\":[ ";
  String zabbixMessagePayloadPart1 = "{\"host\":\"" + String(clientHostName) + "\",\"key\":\"" + Gybmep280HumidityItemName + "\",\"value\":\"" + String(Gybmep280Humidity) + "\"} ";
  String zabbixMessagePayloadPart2 = ", {\"host\":\"" + String(clientHostName) + "\",\"key\":\"" + Gybmep280PressureItemName + "\",\"value\":\"" + String(Gybmep280Pressure) + "\"} ";
  String zabbixMessagePayloadPart3 = ", {\"host\":\"" + String(clientHostName) + "\",\"key\":\"" + Gybmep280AltitudeItemName + "\",\"value\":\"" + String(Gybmep280Altitude) + "\"} ";
  String zabbixMessagePayloadPart4 = ", {\"host\":\"" + String(clientHostName) + "\",\"key\":\"" + Gybmep280temperatureItemName + "\",\"value\":\"" + String(Gybmep280Temperature) + "\"} ";
  String zabbixMessagePayloadFooter = "] }";

  //3.Sending message to zabbix:
  Serial.print(F("Message sent to zabbix: "));
  Serial.println(zabbixMessagePayloadHeader);
  Serial.println(zabbixMessagePayloadPart1);
  Serial.println(zabbixMessagePayloadPart2);
  Serial.println(zabbixMessagePayloadPart3);
  Serial.println(zabbixMessagePayloadPart4);
  Serial.println(zabbixMessagePayloadFooter);

  int payloadLenght = zabbixMessagePayloadHeader.length() +
                      zabbixMessagePayloadPart1.length() +
                      zabbixMessagePayloadPart2.length() +
                      zabbixMessagePayloadPart3.length() +
                      zabbixMessagePayloadPart4.length() +
                      zabbixMessagePayloadFooter.length();

  Serial.print(F("Message Lenght: "));
  Serial.println(String(payloadLenght));

  Serial.println(F("trying connect to zabbix..."));
  if (client.connect(server, port) == 1)
  {
    Serial.println(F("Connected to zabbix"));

    //3.1: Clear garbage:
    firstLengthByte = 0;
    secondLengthByte = 0;

    //3.2: Send Header:
    char txtHeader[5] = {'Z', 'B', 'X', 'D', 1};     //ZBXD<SOH>
    client.print(txtHeader);

    if (payloadLenght >= 256)
    {
      if (payloadLenght >= 65536) // too much for arduino memory
      {
        Serial.println(F("ERROR: Message too long. Not sent."));
        return;
      }
      else
      {
        secondLengthByte = payloadLenght / 256;
        firstLengthByte = payloadLenght % 256 ;
      }
    }
    else
    {
      secondLengthByte = 0;
      firstLengthByte = payloadLenght;
    }

    //3.3: Send Length:
    client.write((byte)firstLengthByte);
    client.write((byte)secondLengthByte);
    client.write((byte)0);
    client.write((byte)0);
    client.write((byte)0);
    client.write((byte)0);
    client.write((byte)0);
    client.write((byte)0);

    //3.4: Send Payload (json):
    //DO NOT USE PRINTLN, because adds <cr><lf> at the end and changes lenght
    client.print(zabbixMessagePayloadHeader);
    client.print(zabbixMessagePayloadPart1);
    client.print(zabbixMessagePayloadPart2);
    client.print(zabbixMessagePayloadPart3);
    client.print(zabbixMessagePayloadPart4);
    client.print(zabbixMessagePayloadFooter);

    Serial.print(F("Message sent to zabbix: "));
    Serial.println(zabbixMessagePayloadHeader);
    Serial.println(zabbixMessagePayloadPart1);
    Serial.println(zabbixMessagePayloadPart2);
    Serial.println(zabbixMessagePayloadPart3);
    Serial.println(zabbixMessagePayloadPart4);
    Serial.println(zabbixMessagePayloadFooter);

    Serial.println(F("Message sent. Waiting for response..."));

    //3.5: Message timeout try:
    timeout = millis();

    while (client.available() == 0)
    {
      if (millis() - timeout > ResponseTimeout)
      {
        Serial.println(F(">>>>> Response Timeout"));
        client.stop();
        return;
      }
    }

    //3.6: Get response:
    while (client.available())
    {
      //Serial.print(F("Waiting for response..."));
      String response = client.readStringUntil('\r');

      Serial.print(F("Zabbix response OK: "));
      Serial.println(response);
    }
  }
  else
  {
    Serial.println(F("NO Connection to zabbix"));
  }
  client.stop();
}
