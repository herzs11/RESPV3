#include <Arduino.h>
#include <AccelStepper.h>
#include <LiquidCrystal_I2C.h>
#include <Timer.h>
#include <Event.h>
#include <Wire.h>
#include <ResponsiveAnalogRead.h>
#include <HX711.h>

HX711 pressure;
enum pins {pulsePin = 6, dir = 5, buttonA = 2, buttonB = 3, pressureSensor = 8, Clock = 9, diff = A5, buzzer = 11} pins;
AccelStepper stepper(1, pulsePin, dir);
LiquidCrystal_I2C lcd(0x27, 20, 4);
Timer t;
enum potPins {POT1 = A0, POT2 = A1, POT3 = A2, POT4 = A3};
ResponsiveAnalogRead TIDALPEAK(POT1, true);
ResponsiveAnalogRead RESPRATE(POT2, true);
ResponsiveAnalogRead IERATIO(POT3, true);
ResponsiveAnalogRead TRIGGER(POT4, true);
enum constants {MAXPRESSURE = 3919, FULLOPEN = 300, FLOW_MEASURE_INTERVAL = 30, CALIBRATION_FACTOR = 15000};
bool potChanged = false;
int flowTimer;
int count;

typedef struct {
    float period;
    float Tin;
    float Tex;
    float Thold;
    float flowDesired;
    bool inhale;
} VARS;
VARS *vars, v;

typedef struct {
    float pressure;
    float diff;
    float flowRate;
    float totalVol;
    float PEEP;
} READINGS;

READINGS *readings, r;

typedef struct {
    float tidalPeak;
    float respRate;
    float IEratio;
    float triggerSensitivity;
} SETTINGS;

SETTINGS *settings, s;

void homeMotor();
float getPressure();
void alert();
void recalcVars();
float getDiff();
void getFlow();

void setup() {
    vars = &v;
    settings = &s;
    readings = &r;
    
    pinMode(buttonA, INPUT_PULLUP);
    pinMode(buttonB, INPUT_PULLUP);
    pinMode(diff, INPUT);
    pinMode(POT1, INPUT);
    pinMode(POT2, INPUT);
    pinMode(POT3, INPUT);
    pinMode(POT4, INPUT);
    pinMode(buzzer, OUTPUT);
    pinMode(pulsePin, OUTPUT);
    pinMode(dir, OUTPUT);
    Serial.begin(9600);
    Serial.print("HERE1");

    pressure.begin(pressureSensor, Clock);
    pressure.set_scale();
    pressure.tare();
    pressure.set_offset(CALIBRATION_FACTOR); // TODO: ???
    Serial.print("here");
    lcd.begin();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Tidal Peak: ");
    lcd.setCursor(0,1);
    lcd.print("Resp. Rate: ");
    lcd.setCursor(0,2);
    lcd.print("I/E Ratio: 1/");
    lcd.setCursor(0,3);
    lcd.print("Trigger: ");
    while (digitalRead(buttonA)==HIGH) {
        Serial.print("HERE3");
        lcd.setCursor(13, 0);
        TIDALPEAK.update();
        settings->tidalPeak = map(TIDALPEAK.getValue(), 0, 1023, 200, 800); //cc
        lcd.print(settings->tidalPeak);
        lcd.setCursor(13, 1);
        RESPRATE.update();
        settings->respRate = map(RESPRATE.getValue(), 0, 1023, 8, 40); //bpm
        lcd.print(settings->respRate);
        lcd.setCursor(14, 2);
        IERATIO.update();
        settings->IEratio = map(IERATIO.getValue(), 0, 1023, 1, 4);
        lcd.print(settings->IEratio);
        lcd.setCursor(11, 3);
        TRIGGER.update();
        settings->triggerSensitivity = map(TRIGGER.getValue(), 0, 1023, -1, -5); //?? cc
        lcd.print(settings->triggerSensitivity);
        delay(5);
    }
    lcd.clear();
    delay(500);
    recalcVars();
    stepper.setMaxSpeed(5000);
    stepper.setAcceleration(5000);
    Serial.print("\nHERE\n");
    //homeMotor();
    flowTimer = t.every(FLOW_MEASURE_INTERVAL, getFlow, -1);

}

void loop() {
    Serial.print("ASDFASDF");
    t.update();
    stepper.run();
    if (count%5==0) {
        if (not potChanged){
            stepper.run();
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print(readings->flowRate);
            lcd.setCursor(0,1);
            lcd.print(readings->totalVol);
        }
        else {
            stepper.run();
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Tidal Peak: ");
            lcd.setCursor(0,1);
            lcd.print("Resp. Rate: ");
            lcd.setCursor(0,2);
            lcd.print("I/E Ratio: 1/");
            lcd.setCursor(0,3);
            lcd.print("Trigger: ");
            lcd.clear();
            lcd.setCursor(13, 0);
            TIDALPEAK.update();
            settings->tidalPeak = map(TIDALPEAK.getValue(), 0, 1023, 200, 800); //cc
            lcd.print(settings->tidalPeak);
            lcd.setCursor(13, 1);
            RESPRATE.update();
            settings->respRate = map(RESPRATE.getValue(), 0, 1023, 8, 40); //bpm
            lcd.print(settings->respRate);
            lcd.setCursor(14, 2);
            IERATIO.update();
            settings->IEratio = map(IERATIO.getValue(), 0, 1023, 1, 4);
            lcd.print(settings->IEratio);
            lcd.setCursor(11, 3);
            TRIGGER.update();
            settings->triggerSensitivity = map(TRIGGER.getValue(), 0, 1023, -1, -5); //?? cc
            lcd.print(settings->triggerSensitivity);
        }
    }
    stepper.run();
    if (count%10==0 && !potChanged){
        checkPot();
    }
    count++;
}


void homeMotor() {
    Serial.println("Homing");
    stepper.setCurrentPosition(0);
    float lowest = getDiff();
    float highest = getDiff();
    int pos = stepper.currentPosition();
    stepper.moveTo(pos+1600);
    while (stepper.distanceToGo()!=0) {
        Serial.println(1);
        stepper.run();
        readings->diff = getDiff();
        if (readings->diff<lowest){
            lowest = readings->diff;
        }
        if (readings->diff>highest){
            highest = readings->diff;
        }
        delay(1);
    }
    Serial.print(highest);
    Serial.print(lowest);
    delay(1000);
    stepper.moveTo(pos+3200);
    readings->diff = getDiff();
    while (readings->diff < highest-.01){
        Serial.println(2);
        stepper.run();
        readings->diff = getDiff();
        delay(1);
    }
    while (readings->diff > lowest*3) {
        Serial.println(3);
        stepper.run();
        readings->diff = getDiff();
        delay(1);
    }
    stepper.setCurrentPosition(0);
}


float getDiff() {
    return ((analogRead(diff)*.0049)-1)*.4-.59;
}

void getFlow() {
    readings->pressure = getPressure();
    if (readings->pressure >= MAXPRESSURE) {
        alert();
    }
    readings->flowRate = getDiff();
    if (readings->flowRate > vars->flowDesired) {
        stepper.moveTo(stepper.currentPosition()-50); // TODO: Empirical equation?
    }
    else if (readings->flowRate < vars->flowDesired) {
        stepper.moveTo(stepper.currentPosition()+50);
    }
    readings->totalVol += (vars->flowDesired*30);
    if (abs(readings->totalVol-settings->tidalPeak)<10){
        exhale();
    }
}

float getPressure(){
    return pressure.get_units()/14.27; // TODO: FIX
}

void alert(){
    while (digitalRead(buttonA) == HIGH){
        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        delay(200);
    }
}

void recalcVars(){
    vars->period = 60000/settings->respRate;
    vars->Tin = vars->period/(settings->IEratio+1)*.8;
    vars->Thold = vars->period/(settings->IEratio+1)*.2;
    vars->Tex = vars->period-vars->Tin-vars->Thold;
    vars->flowDesired = settings->tidalPeak/vars->Tin;
}

void checkPot(){
    typedef enum {tidalpeak,resprate,ieratio,trigger} pot;
    static pot p = tidalpeak;
    switch(p) {
        case tidalpeak:
            TIDALPEAK.update();
            if (abs(settings->tidalPeak - map(TIDALPEAK.getValue(), 0, 1023, 200, 800)) > 20){
                potChanged=true;
            }
            p = resprate;
            break;
        case resprate:
            RESPRATE.update();
            if (abs(settings->respRate - map(RESPRATE.getValue(), 0, 1023, 8, 40)) > 1){
                potChanged=true;
            }
            p = ieratio;
            break;
        case ieratio:
            IERATIO.update();
            if (abs(settings->IEratio - map(IERATIO.getValue(), 0, 1023, 1, 4)) > 0) {
                potChanged=true;
            }
            p = trigger;
            break;
        case trigger:
            TRIGGER.update();
            if (abs(settings->triggerSensitivity - map(TRIGGER.getValue(), 0, 1023, -1, -5)) > 0) {
                potChanged=true;
            }
            p = tidalpeak;
            break;
    }
}

void exhale() {
    count = 0;
    readings->PEEP = getPressure();
    //int startHold = millis();
    /*while (millis()-startHold < vars->Thold){
        if (getPressure() > readings->PEEP){
            stepper.moveTo(stepper.currentPosition()-10);
        }
        else {
            stepper.moveTo(stepper.currentPosition()+10);
        }
        stepper.run();
    }*/
    delay(vars->Thold); //TODO: CHOOSE THIS OR ABOVE^??
    int startExhale = millis();
    t.stop(flowTimer);
    stepper.runToNewPosition(0);
    while (millis()-startExhale<vars->Tex){
        // HOLD
    }
    readings->totalVol = 0;
    flowTimer = t.every(FLOW_MEASURE_INTERVAL, getFlow, -1);
    lcd.clear();

}
