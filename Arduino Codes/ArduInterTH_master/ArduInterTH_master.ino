//Name: ArduInterTH_master
//Description: This program measures the DHT11 Temperature and Humidity.
//             It interacts with other another Arduino device using XBee communication and posts
//             the current values of the measurements to Thingworx IoT platform every 2 secs using a WiFi
//             Shield for Arduino. When the temperature goes above 30°C, it sends a 'W' character through the
//             XBee in order to activate the slave system. When the temperature goes below 27°C, it sends a 'O' character
//             through the XBee informing the slave system that everything is in order, which disables it.
//Authors: David Velasquez (mail: dvelas25@eafit.edu.co)
//         Raul Mazo       (mail: raul.mazo@univ-paris1.fr)
//Version: 2.0
//Date: 26/10/2017

//Required Libraries
#include <SPI.h>
#include <WiFi.h>
#include "DHT.h"

//Finite elements machine (FEM) states
#define EOK 0 //Ok State
#define EWAR 1  //Warning State
#define EWARTX 2  //Warning XBee transmit State

//FEM of LED tilting
#define ELOFF 0 //RED LED OFF State
#define ELON 1  //RED LED ON State

//Pin I/O Labeling
#define DHTPIN 2  //DHT sensor connected to Arduino digital pin 2
#define LR 6  //Red LED connected to Arduino pin 5
#define LG 5  //Green LED connected to Arduino pin 6

//Constants
const unsigned long postingInterval = 2 * 1000; //Delay between TWX POST updates, 2000 milliseconds
const unsigned int sensorCount = 2; //Number of sensor vars sent to TWX, 2 vars
const float Tmax = 30;  //Maximum Temperature for going into Warning State (30°C)
const float Tnor = 27;  //Temperature in order to return the system to Normal State (27°C)
const unsigned long tDHTmeas = 200; //Time to measure DHT11 (200 ms)
const unsigned long tTX = 1000; //Time to send the command through XBee (1 sec)
const unsigned int CTX = 3; //Constant for # of times to send the same command through the XBee in order to alert (3)
const unsigned int tilt = 500;  //Constant for tilting initialized in 500 msec

//Variables
unsigned int state = EOK;  //Variable for storing the current state of the FSM MAIN, initialized in EOK state.
unsigned int statetilt = ELOFF;  //Variable for storing the current state of the FSM of tilting
//->WiFi Shield Vars
char ssid[] = "IoT-B19";  //Network SSID (name)
char pass[] = "meca2017*";  //Network password
int status = WL_IDLE_STATUS;  //Network status
WiFiClient client;
//->TWX Vars
char* host = "iot.dis.eafit.edu.co";  //TWX server (do not include http://)
char* appKey = "84247955-b951-446f-a864-9aa795d02837";  //API Key from TWX
char* thingName = "termo_master_thing";  //Name of your Thing in TWX
char* serviceName = "termo_master_service";  //Name of your Service in TWX
char* nameArray[] = {"ArdValue1", "ArdValue2"}; //Vector Var names from TWX service inputs
float sensorValues[sensorCount];  //Vector for Var values
//->DHT11 Vars
#define DHTTYPE DHT11 //Use DHT11 sensor variant
float T = 0;  //Var Environment Temperature (t) as float
float H = 0;  //Var Relative Humidity (h) as float
DHT dht(DHTPIN, DHTTYPE); //DHT object var
//->XBee Communication Vars
//char readCommXB = '\0'; //Char command read through XBee communication
//char sendCommXB = '\0'; //Char command sent through XBee communication
//->Computer Communication Vars
//char readCommPC = '\0'; //Char command read through PC communication
//char sendCommPC = '\0'; //Char command sent through PC communication
//->Counters
unsigned int C = 0; //Counter for # of times to send the same command through the XBee
//->Timing Vars
unsigned long lastConnectionTime = 0; //Last time you connected to the TWX, in milliseconds
unsigned long tini = 0; //Timing for FSM transitions
unsigned long tiniDHT = 0;  //Timing for DHT11 measurements
unsigned long tinitilt = 0;  //Timing for FSM tilting

//Subroutines & Functions
//FSM of tilting
void FSMtilt() {
  switch (statetilt) {
    case ELOFF:
      //Physical outputs states
      digitalWrite(LR, LOW);

      //Variables states

      //Transitions questions
      if (millis() - tinitilt >= tilt) {
        statetilt = ELON; //Change to ELON state
        tinitilt = millis();  //Reset timing for new timing
      }
      break;

    case ELON:
      //Physical outputs states
      digitalWrite(LR, HIGH);

      //Variables states

      //Transitions questions
      if (millis() - tinitilt >= tilt) {
        statetilt = ELOFF; //Change to ELON state
        tinitilt = millis();  //Reset timing for new timing
      }
      break;
  }
}

void readDHT() {
  if ((millis() - tiniDHT) > tDHTmeas) {
    T = dht.readTemperature();
    Serial.println("temperature in the master is: ");
    Serial.println(T);
    H = dht.readHumidity();
    Serial.println("humidity in the master is: ");
    Serial.println(H);
    tiniDHT = millis(); //Reset the timing for measuring DHT11
  }
}

void POST() {
  sensorValues[0] = T;
  sensorValues[1] = H;
  if ((millis() - lastConnectionTime) > postingInterval)
  {
    POST_send(sensorCount, nameArray, sensorValues);  //This is the library function that constructs the REST call and sends it to your ThingWorx service
    lastConnectionTime = millis();  //Update the last connection time
  }
}

void POST_send(int sensorCount, char* sensorNames[], float values[]) {
  //build the String with the data that you will send
  //through REST calls to your TWX server
  // if you get a connection, report back via serial:
  String body = "";
  for (int idx = 0; idx < sensorCount; idx++) {
    if (idx != 0) body += "&";
    body += sensorNames[idx];
    body += "=";
    body += values[idx];
  }
  if (client.connect(host, 80)) {
    //Serial.println("connected");
    // send the HTTP POST request:
    client.print("POST /Thingworx/Things/");
    client.print(thingName);
    client.print("/Services/");
    client.print(serviceName);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(host);
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Content-Length: " + String(body.length()));
    client.println("Connection: close");
    client.println("appKey: " + String(appKey) + "\r\n");
    client.println(body + "\r\n");

    for (int idx = 0; idx < sensorCount; idx++)
    {
      client.print("&");
      client.print(sensorNames[idx]);
      client.print("=");
      client.print(values[idx]);
    }
    client.println(); //Double ENTER for sending all POST REQUEST
    //    int timeout = 0;
    //    while (!client.available() && timeout < 5000) { //Available is for checking if there are bytes available to be read in the WiFi Client
    //      delay(1);
    //      timeout++;
    //    }
    //    while (client.available()) {
    //      char c = client.read();
    //      Serial.write(c);  //Uncomment if you want to see the Response from server
    //    }
    //    client.stop();
    //
    //    // print the request out  //Uncomment if you want to see how the POST request was made
    //    Serial.print("POST /Thingworx/Things/");
    //    Serial.print(thingName);
    //    Serial.print("/Services/");
    //    Serial.print(serviceName);
    //    Serial.println(" HTTP/1.1");
    //    Serial.print("Host: ");
    //    Serial.println(host);
    //    Serial.println("Content-Type: application/x-www-form-urlencoded");
    //    Serial.println("Content-Length: " + String(body.length()));
    //    Serial.println("Connection: close");
    //    Serial.println("appKey: " + String(appKey) + "\r\n");
    //    Serial.println(body + "\r\n");
    client.stop();
  }
  else {
    // kf you didn't get a connection to the server:
    Serial.println("the connection could not be established");
    client.stop();
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void WiFiInit() {
  // give the WiFi Shield module time to boot up:
  delay(1000);

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv != "1.1.0" )
    Serial.println("Please upgrade the firmware");

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  // you're connected now, so print out the status:
  printWifiStatus();
}

//void readPC() {
//  if (Serial.available() > 0) { //Check if there are bytes sent from PC
//    readCommPC = Serial.read(); //Store in readCommPC the incoming char
//    Serial.flush(); //Clean Serial port buffer
//  }
//}

//void readXBee() {
//  if (Serial1.available() > 0) { //Check if there are bytes sent from XBee
//    readCommXBee = Serial1.read(); //Store in readCommXBee the incoming char
//    Serial1.flush(); //Clean Serial1 port buffer
//  }
//}

void setup() {
  //Pin type definition
  //pinMode(PIN#, OUTPUT or INPUT);
  pinMode(LR, OUTPUT);  //Red LED (LR) as an OUTPUT
  pinMode(LG, OUTPUT);  //Green LED (LG) as an OUTPUT

  //Physical output cleaning
  //digitalWrite(PIN#, LOW);
  digitalWrite(LR, LOW);
  digitalWrite(LG, LOW);

  //Communications initialization
  Serial.begin(9600); //Initialize Serial communications through TX0 and RX0 (PC)
  Serial1.begin(9600);  //Initialize Serial communications through TX1 and RX1 (XBee)
  dht.begin();  //Initialize communications with DHT11 sensor
  WiFiInit(); //Initialize communications through WiFi Shield

  //(Debug only)
  Serial.println("State: EOK");
  //(Optional) Reset timers if timing is required at the start of the loop cycle
  lastConnectionTime = millis();  //Reset the TWX POST timing
  tiniDHT = millis(); //Reset the DHT measurment timing
}

void loop() {
  //readPC(); //Check and store the incoming command from PC
  //readXBee(); //Check and store the incoming command from XBee
  switch (state) {
    case EOK: //OK State
      //Physical outputs state
      digitalWrite(LG, HIGH); //Turn green led ON because everything is OK
      digitalWrite(LR, LOW);  //Turn off red led at the OK state

      //Variables state
      readDHT();  //Measure DHT11 vars (Temperature and Humidity)
      POST(); //POST Variables to TWX

      //Transitions questions
      if (T >= Tmax) {  //If temperature is greater or equal to Tmax (30°C) go to Warning State
        state = EWAR; //Change to EWAR State
        Serial.println("State: EWAR");
        C = 0;  //Reset counter
        tini = millis();  //Reset timer
        tinitilt = millis();  //Reset tilting timer
      }
      break;

    case EWAR:  //Warning State
      //Physical outputs state
      digitalWrite(LG, LOW); //Turn green led OFF because there is an overtemperature warning
      //Variables state
      FSMtilt();  //Execute tilt Finite State Machine
      readDHT();  //Measure DHT11 vars (Temperature and Humidity)
      POST(); //POST Variables to TWX

      //Transitions questions
      if ((millis() - tini) >= tTX && C < CTX) {  //If the current time is greater or equal to transmit time (tTX - 1 sec) and the counter is below CTX (3 cycles), go to Transmit State
        state = EWARTX; //Change to EWARTX State
        Serial.println("State: EWARTX");
      }
      else if (T < Tnor) {  //If the temperature is below the normal temperature (27°C), go to OK State
        state = EOK;  //Change to EOK State
        Serial.println("State: EOK");
        Serial1.println('O'); //Send 'O' character through the XBee to inform the slave that everything is OK
      }
      break;

    case EWARTX:  //Warning Transmit State
      //Physical outputs state
      digitalWrite(LG, LOW); //Turn green led OFF because there is an overtemperature warning

      //Variables state
      FSMtilt();  //Execute tilt Finite State Machine
      readDHT();  //Measure DHT11 vars (Temperature and Humidity)
      POST(); //POST Variables to TWX
      Serial1.println('W'); //Send 'W' character through the XBee to inform the slave there is a warning in order to activate it

      //Transitions questions
      if (C < CTX) {  //If the counter is below CTX (3 cycles), go to Warning State
        state = EWAR; //Change to EWAR State
        Serial.println("State: EWAR");
        C = C + 1;  //Increase the counter for # of transmited times (C++)
        tini = millis();  //Reset the timer for transmit time
      }
      else if (T < Tnor) {  //If the temperature is below the normal temperature (27°C), go to OK State
        state = EOK;  //Change to EOK State
        Serial.println("State: EOK");
        Serial1.println('O'); //Send 'O' character through the XBee to inform the slave that everything is OK
      }
      break;
  }
}
