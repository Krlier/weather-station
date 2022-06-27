#include <Ethernet_W5500.h>
#include "Secrets.h"
#include "DHT.h"
#include <string.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include "ThingSpeak.h"

// Initializes variables
#define DHTPIN A1
#define DHTTYPE DHT11
byte macAddress[] = SECRET_MAC;
unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;

// Rainmeter
const int reedSwitch = 9; //The reed switch outputs to digital pin 9
int reedSwitchVal = 0;        //Current value of reed switch
int old_reedSwitchVal = 0;    //Old value of reed switch
int reedSwitchCount = 0;  //This is the variable that holds the count of switching
int rainmeterMeasurementPeriod = 20000; // Durantion of measuremente (miliseconds)

// Wind Direction Indicator
int windDirectionPin = 3;
float windDirectionVolts = 0;
int windDirectionAngle = 0;

// Wind Speed
# define Hall sensor 2  //  Digital pin 2
const float pi = 3.14159265;
int windSpeedMeasurementPeriod = 5000;  // Duration of measurement (miliseconds)
int radius = 147;   // Anemometer radius(mm)
unsigned int magnetCounter = 0; // Magnet counter for sensor
unsigned int RPM = 0;   // Revolutions per minute
float speedWind = 0;    // Wind speed (km/h)

DHT dht(DHTPIN, DHTTYPE);
EthernetClient client;
Adafruit_BMP280 bmp;

// Wind Direction methods
void WindDirection() {
    windDirectionVolts = analogRead(windDirectionPin)* (5.0 / 1023.0);
    if (windDirectionVolts <= 0.28) { // Pointing E
        windDirectionAngle = 0;
    }
    else if (windDirectionVolts <= 0.31) { // Pointing NE
        windDirectionAngle = 45;
    }
    else if (windDirectionVolts <= 0.35) { // Pointing N
        windDirectionAngle = 90;
    }
    else if (windDirectionVolts <= 0.40) { // Pointing NO
        windDirectionAngle = 135;
    }
    else if (windDirectionVolts <= 0.45) { // Pointing O
        windDirectionAngle = 180;
    }
    else if (windDirectionVolts <= 0.52) { // Pointing SO
        windDirectionAngle = 225;
    }
    else if (windDirectionVolts <= 0.68) { // Pointing S
        windDirectionAngle = 270;
    }
    else {
        windDirectionAngle = 315; // Pointing SE
    }
}

// RainmeterReadings updates the rainmeter indicator
void RainmeterReadings() {

    reedSwitchCount = 0;
    unsigned long millis();
    long startTime = millis();

    while(millis() < startTime + rainmeterMeasurementPeriod) {
        reedSwitchVal = digitalRead(reedSwitch);
        if ((reedSwitchVal == LOW) && (old_reedSwitchVal == HIGH)){    //Check to see if the status has changed
            delay(50);  // Delay put in to deal with any "bouncing" in the switch.

            reedSwitchCount = reedSwitchCount + 1;  //Add 1 to the count of bucket tips
            old_reedSwitchVal = reedSwitchVal;  //Make the old value equal to the current value
        } else {
            //If the status hasn't changed, then do nothing
            old_reedSwitchVal = reedSwitchVal;
        }
    }
    reedSwitchCount = reedSwitchCount/2;
}

// Wind speed methods
void RPMcalc(){
    RPM=((magnetCounter)*60)/(windSpeedMeasurementPeriod/1000);  // Calculate revolutions per minute (RPM)
}
void SpeedWind(){
    RPMcalc();
    speedWind = (((4 * pi * radius * RPM)/60) / 1000)*3.6;  // Calculate wind speed on km/h
}
void addcount(){
    magnetCounter++;
}
void WindVelocity() {
    speedWind = 0;

    magnetCounter = 0;
    attachInterrupt(0, addcount, RISING);
    unsigned long millis();
    long startTime = millis();
    while(millis() < startTime + windSpeedMeasurementPeriod) {
    }

    SpeedWind();
}

void setup() {
    Serial.begin(9600);

    // Activates Rainmeter pull up resistor
    pinMode (reedSwitch, INPUT_PULLUP);

    // Set the pins for anemometer
    pinMode(2, INPUT);
    digitalWrite(2, HIGH);     //internall pull-up active

    // Start the Ethernet connection:
    Serial.println("Initialize Ethernet with DHCP:");
    if (Ethernet.begin(macAddress) == 0) {
        delay(1000);
        Serial.println("Failed to configure Ethernet using DHCP");
        exit(1);
    } else {
        Serial.print("DHCP assigned IP ");
        Serial.println(Ethernet.localIP());
    }
    // Give the Ethernet shield a second to initialize:
    delay(1000);

    ThingSpeak.begin(client);  // Initialize ThingSpeak
    dht.begin(); // Initialize DHT11 sensor
    if(!bmp.begin(0x76)){ // Initialize BMP sensor
        Serial.println("BMP280 sensor not found.");
        delay(1000);
        exit(1);
    }
}

void loop() {

    // DHT11 Readings
    float dhtHumidity = dht.readHumidity();
    float dhtTemperature = dht.readTemperature();

    // BMP280 Readings
    float bmpTemperature = bmp.readTemperature();
    float bmpReadPressure = bmp.readPressure();

    // MQ135 Readings
    float airPPM = analogRead(A0);

    // Rainmeter Readings
    RainmeterReadings();

    // Wind Direction Readings
    WindDirection();

    // Wind speed Readings
    WindVelocity();

    // Set ThingSpeak fields with values
    ThingSpeak.setField(1, (dhtTemperature + bmpTemperature) / 2); // Temperature
    ThingSpeak.setField(2, dhtHumidity); // Humidity
    ThingSpeak.setField(3, bmpReadPressure); // Pressure
    ThingSpeak.setField(4, airPPM); // Air Quality
    ThingSpeak.setField(5, speedWind); // Wind Speed
    ThingSpeak.setField(6, windDirectionAngle); // Wind Direction
    ThingSpeak.setField(7, float(reedSwitchCount*0.25)); // Rainmeter

    // Write to ThingSpeak
    int responseStatusCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if(responseStatusCode == 200){
        Serial.println("Channel update successful.\n");
    }
    else{
        Serial.println("Problem updating channel. HTTP error code " + String(responseStatusCode) + "\n");
    }

    delay(30000);
}
