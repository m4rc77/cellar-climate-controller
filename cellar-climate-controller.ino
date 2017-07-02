//*****************************************************************************
// cellar-climate-controller
//
// by m4rc77
//*****************************************************************************

// include the library code:
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <math.h>

// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// These #defines make it easy to set the backlight color
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

const int DHT_PIN_INSIDE = 2;
const int DHT_PIN_OUTSIDE = 4;

const int VIN_MEASURE_PIN = A0;

const int FAN_PIN_ON =  8;
const int FAN_PIN_RPM = 3;

const int PAGE_DISPLAY_DURATION = 5000;
const int CONTROL_INTERVAL = 10000;

const int MODE_AUTO = 10;
const int MODE_ON = 11;
const int MODE_OFF = 12;

const long MAX_FORCED_MODE_DURATION = 12L * 60L * 60L * 1000L; // 12h

const float HUMIDTITY_LIMIT = 53.5;
const float HUMIDTITY_LIMIT_TOLERANCE = 1.0;
const float ABS_HUMIDITY_LIMIT_TOLERANCE = 0.25;

const float TEMP_IN_LIMIT = 14.0;
const float TEMP_OUT_LIMIT = 28.0;

// See guide for details on sensor wiring and usage:
//   https://learn.adafruit.com/dht/overview
DHT_Unified dhtIn (DHT_PIN_INSIDE,  DHT22);
DHT_Unified dhtOut(DHT_PIN_OUTSIDE, DHT22);

volatile int fanRpmCounter;
int fanRpm;
boolean fanRunning = false;
long fanLastCheck;
long fanCheckDelay = 2000;

long tempLastCheck;
long tempCheckDelay;
boolean tempLastInside = false;

float tempInside;
float tempOutside;
float humidityInside;
float humidityOutside;
float absHumidityInside;
float absHumidityOutside;

boolean showFirstPage = true;
long lastPageSwitch;
byte pageCount = 0;

int mode;
long forceModeOnTime=0;

String why = "Starting ...";
long lastControl;

float volt;

void readInputVoltage() {
    int sensorValue = analogRead(VIN_MEASURE_PIN);
    volt = sensorValue * (5.0 / 1023.0);
    // correction by voltmeter measurements ...
    float correction = 1.00926706;
    volt = volt / 20.0 * (20.0 + 50.0) * correction;
    Serial.print( volt, DEC ); Serial.println(" V");
}

//This is the function that the interupt calls
void rpm () {
    fanRpmCounter++;
}

void calculateFanRpm() {
    //Times fanRpmCounter (which is apprioxiamately the fequency the fan
    //is spinning at) by 60 seconds before dividing by the fan's divider
    if (fanRpmCounter == 0) {
        fanRpm = 0;
    } else {
        fanRpm = (fanRpmCounter * (60000 / (millis() - fanLastCheck))) / 2;
    }

    // reset the counter ...
    fanRpmCounter = 0;

    //Prints the number calculated above
    Serial.print(fanRpm, DEC); Serial.println(" rpm");
}

void showPageOne(boolean absHumidity) {
    lcd.setCursor(0,0);
    lcd.print("I:");
    printTemp(tempInside);
    if(absHumidity) {
        printAbsHumidity(absHumidityInside);
    } else {
        printHumidity(humidityInside);
    }

    lcd.setCursor(0,1);
    lcd.print("O:");
    printTemp(tempOutside);
    if (absHumidity) {
        printAbsHumidity(absHumidityOutside);
    } else {
        printHumidity(humidityOutside);
    }
}

void printTemp(float temperature) {
    if (isnan(temperature)) {
        lcd.print("Temp?? ");
    } else {
        if (10 <= temperature) {
            lcd.print("  ");
        } else if (0 <= temperature && temperature < 10) {
            lcd.print("   ");
        } else if (-10 < temperature && temperature < 0) {
            lcd.print("  ");
        } else if (temperature <= -10) {
            lcd.print(" ");
        }
        lcd.print(doRound(temperature), 1);lcd.print((char)223);
    }
}

void printHumidity(float humidity) {
    if (isnan(humidity)) {
        lcd.print("Hmid?? ");
    } else {
        if (10 <= humidity) {
            lcd.print(" ");
        } else if (0 <= humidity && humidity < 10) {
            lcd.print("  ");
        }
        lcd.print(doRound(humidity), 1); lcd.print("% ");
    }
}

void printAbsHumidity(float humidity) {
    if (10 <= humidity) {
        lcd.print(" ");
    } else if (0 <= humidity && humidity < 10) {
        lcd.print("  ");
    }
    lcd.print(doRound(humidity), 1); lcd.print("g");
}

void showPageTwo() {
    lcd.clear();

    lcd.setCursor(0,0);
    lcd.print(why);

    lcd.setCursor(0,1);
    lcd.print("Fan: ");
    if (fanRunning) {
        lcd.print(fanRpm, DEC); lcd.print(" rpm     ");
    } else {
        lcd.print("OFF        ");
    }
}

void showPageThree() {
    lcd.clear();

    lcd.setCursor(0,0);
    lcd.print("Up:   ");lcd.print((millis()/1000/60/60));lcd.print("h ");lcd.print(((millis()/1000/60)%60));lcd.print("min");

    lcd.setCursor(0,1);
    lcd.print("V-In: ");lcd.print(doRound2(volt), 2); lcd.print("V");
}

void setup() {
    pinMode(FAN_PIN_ON, OUTPUT);
    pinMode(FAN_PIN_RPM,  INPUT);

    attachInterrupt(digitalPinToInterrupt(FAN_PIN_RPM), rpm, RISING);

    // Debugging output
    Serial.begin(9600);

    // set up the LCD's number of columns and rows:
    lcd.begin(16, 2);

    // Print a message to the LCD...
    int time = millis();
    lcd.setCursor(0, 0);
    lcd.print("***FanControl***");
    lcd.setCursor(0, 1);
    lcd.print("***   1.33   ***");
    time = millis() - time;
    Serial.print("Took "); Serial.print(time); Serial.println(" ms");
    lcd.setBacklight(WHITE);

    // switch FAN off...
    digitalWrite(FAN_PIN_ON, LOW);

    // Initialize device.
    dhtIn.begin();
    dhtOut.begin();

    // Print temperature sensor details.
    sensor_t sensor;
    dhtIn.temperature().getSensor(&sensor);
    Serial.println("INSIDE: ------------------");
    Serial.println("Temperature");
    Serial.print  ("Sensor:       "); Serial.println(sensor.name);
    Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
    Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
    Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" *C");
    Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" *C");
    Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" *C");
    Serial.println("------------------------------------");
    // Print humidity sensor details.
    dhtIn.humidity().getSensor(&sensor);
    Serial.println("------------------------------------");
    Serial.println("Humidity");
    Serial.print  ("Sensor:       "); Serial.println(sensor.name);
    Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
    Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
    Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println("%");
    Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println("%");
    Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println("%");
    Serial.println("------------------------------------");
    dhtOut.temperature().getSensor(&sensor);
    Serial.println("OUTSIDE: ------------------");
    Serial.println("Temperature");
    Serial.print  ("Sensor:       "); Serial.println(sensor.name);
    Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
    Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
    Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" *C");
    Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" *C");
    Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" *C");
    Serial.println("------------------------------------");
    // Print humidity sensor details.
    dhtOut.humidity().getSensor(&sensor);
    Serial.println("------------------------------------");
    Serial.println("Humidity");
    Serial.print  ("Sensor:       "); Serial.println(sensor.name);
    Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
    Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
    Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println("%");
    Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println("%");
    Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println("%");
    // Set delay between sensor readings based on sensor details.

    tempCheckDelay = sensor.min_delay / 1000;

    mode = MODE_AUTO;
    pageCount = 1;
    showFirstPage = true;
    lastControl= millis();
}

void loop() {

    if (fanLastCheck + fanCheckDelay <= millis()) {
        calculateFanRpm();
        fanLastCheck = millis();

        readInputVoltage();
        Serial.print("MODE:");Serial.println(mode);
    }

    if(tempLastCheck + tempCheckDelay <= millis()) {
        if (tempLastInside) {
            tempCheckOutside();
            absHumidityOutside = calcAbsHumidity(tempOutside, humidityOutside);
            Serial.print("Abs Humidity Outside ");Serial.print(absHumidityOutside);Serial.println("g/m³");
        } else {
            tempCheckInside();
            absHumidityInside = calcAbsHumidity(tempInside, humidityInside);
            Serial.println("Abs Humidity inside ");Serial.print(absHumidityInside);Serial.println("g/m³");
        }
        tempLastInside = !tempLastInside;
        tempLastCheck = millis();
    }

    if(lastPageSwitch + PAGE_DISPLAY_DURATION <= millis()) {
        if (showFirstPage) {
            showPageOne((pageCount+1) % 3 == 0);
            if ((pageCount+1) % 3 == 0) {
                showFirstPage = false;
            }
        } else {
            if (pageCount % 6 == 0) {
                showPageThree();
            } else {
                showPageTwo();
            }
            showFirstPage = true;
        }
        pageCount++;
        lastPageSwitch  = millis();
    }

    uint8_t buttons = lcd.readButtons();

    if (buttons) {
        if (buttons & BUTTON_UP) {
            if (mode == MODE_AUTO) {
                mode = MODE_ON;
                forceModeOnTime = millis();
                Serial.println("MODE=ON");
            } else {
                mode = MODE_AUTO;
                Serial.println("MODE=AUTO");
            }
            controlFan();
            showPageTwo();
            lastPageSwitch  = millis();
            showFirstPage = true;
            delay(500);
        }
        if (buttons & BUTTON_DOWN) {
            if (mode == MODE_AUTO) {
                mode = MODE_OFF;
                forceModeOnTime = millis();
                Serial.println("MODE=OFF");
            } else {
                mode = MODE_AUTO;
                Serial.println("MODE=AUTO");
            }
            controlFan();
            showPageTwo();
            lastPageSwitch  = millis();
            showFirstPage = true;
            delay(500);
        }
        if (buttons & BUTTON_LEFT) {
            //lcd.print("LEFT   ");
        }
        if (buttons & BUTTON_RIGHT) {
            //lcd.print("RIGHT  ");
        }
        if (buttons & BUTTON_SELECT) {
            //lcd.print("SELECT ");
        }
    }

    if(lastControl + CONTROL_INTERVAL <= millis()) {
        controlFan();
        lastControl = millis();
    }
}

void controlFan() {
    if (mode == MODE_AUTO) {
        if (tempInside < TEMP_IN_LIMIT) {
            fanOff();
            why = "Temp In < ";
            why = why + ((int)TEMP_IN_LIMIT);
            why = why + (char)223;
        } else if (tempOutside > TEMP_OUT_LIMIT) {
            fanOff();
            why = "Temp Out > ";
            why = why + ((int)TEMP_OUT_LIMIT);
            why = why + (char)223;
        } else if ((humidityInside < (HUMIDTITY_LIMIT - HUMIDTITY_LIMIT_TOLERANCE)) && fanRunning) {
            fanOff();
            why = "Humi: In < ";
            why = why + ((int)HUMIDTITY_LIMIT);
            why = why + "%";
        } else if ((humidityInside < HUMIDTITY_LIMIT) && !fanRunning) {
            fanOff();
            why = "Humi: In < ";
            why = why + ((int)HUMIDTITY_LIMIT);
            why = why + "%";
        } else if ((absHumidityOutside >= absHumidityInside) && fanRunning) {
            fanOff();
            why = "A-Humi: Out > In";
        } else if (((absHumidityOutside + ABS_HUMIDITY_LIMIT_TOLERANCE) >= absHumidityInside) && !fanRunning) {
            fanOff();
            why = "A-Humi: Out > In";    
        } else if ((absHumidityOutside < absHumidityInside) && fanRunning)  {
            fanOn();
            why = "Drying ...";   
        } else if (((absHumidityOutside + ABS_HUMIDITY_LIMIT_TOLERANCE) < absHumidityInside) && !fanRunning)  {
            fanOn();
            why = "Drying ...";     
        } else {
            fanOff();
            why = "why?";
        }
    } else if (mode == MODE_ON) {
        if (((millis() - forceModeOnTime) > MAX_FORCED_MODE_DURATION) || (absHumidityOutside >= absHumidityInside)) {
          fanOff();
          mode = MODE_AUTO;
        } else {
          fanOn();
          why = "Forced ON-Mode";
        }
    } else if (mode == MODE_OFF) {
        if ((millis() - forceModeOnTime) > MAX_FORCED_MODE_DURATION) {
          fanOff();
          mode = MODE_AUTO;
        } else {      
          fanOff();
          why = "Forced OFF-Mode";
        }
    } else {
      why = "Unkown Mode " + mode;
    }
    Serial.println(why);
}

void fanOn() {
    digitalWrite(FAN_PIN_ON, HIGH);
    fanRunning = true;
    Serial.println("Switching fan ON");
}

void fanOff() {
    digitalWrite(FAN_PIN_ON, LOW);
    fanRunning = false;
    Serial.println("Switching fan OFF");
}

void tempCheckInside() {
    sensors_event_t event;
    dhtIn.temperature().getEvent(&event);
    tempInside = event.temperature;
    dhtIn.humidity().getEvent(&event);
    humidityInside = event.relative_humidity;

    debugTempAndHumidity(tempInside, humidityInside, "Inside");
}

void tempCheckOutside() {
    sensors_event_t event;
    dhtOut.temperature().getEvent(&event);
    tempOutside = event.temperature;
    dhtOut.humidity().getEvent(&event);
    humidityOutside = event.relative_humidity;

    debugTempAndHumidity(tempOutside, humidityOutside, "Outside");
}

float calcAbsHumidity(float temperature, float humidity) {
    if (isnan(temperature)) {
        return 0;
    }
    if (isnan(humidity)) {
        return 0;
    }
    // see https://carnotcycle.wordpress.com/2012/08/04/how-to-convert-relative-humidity-to-absolute-humidity/
    // gramms/m³ = ( 6.112 x e^[(17.67 x temp) / (temp + 243.5)] x humidity x 2.1674 ) / (273.15 + temp)
    float expo = (17.67 * temperature) / (temperature + 243.5);
    return ((6.112 *  exp(expo) * humidity * 2.1674 ) / (273.15 + temperature));
}

void debugTempAndHumidity(float temp, float humidity, String where) {
    if (isnan(temp)) {
        Serial.println("Error reading " + where + "temperature!");
    } else {
        Serial.print("Temperature (" + where + "): "); Serial.print(temp); Serial.println(" *C");
    }

    if (isnan(humidity)) {
        Serial.println("Error reading " + where + " humidity!");
    } else {
        Serial.print("Humidity (" + where + "): "); Serial.print(humidity); Serial.println("%");
    }
}

float doRound(float val) {
    return round(val * 10) / 10.0;
}

float doRound2(float val) {
    return round(val * 100) / 100.0;
}
