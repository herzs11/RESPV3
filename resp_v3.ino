#include <Arduino.h>
#include <AccelStepper.h>
#include <LiquidCrystal_I2C.h>
#include <Timer.h>
#include <Event.h>
#include <Wire.h>
#include <ResponsiveAnalogRead.h>
#include <HX711.h>

//HX711 pressure; later
enum pins {pulsePin = 6, dir = 5, buttonA = 2, buttonB = 3, pressureSensor = 8, Clock = 9, diff = A5, buzzer = 11} pins;
AccelStepper stepper(1, pulsePin, dir);
LiquidCrystal_I2C lcd(0x27, 20, 4);
Timer t;
enum potPins {POT1 = A0, POT2 = A1, POT3 = A2, POT4 = A3};
ResponsiveAnalogRead TIDALPEAK(POT1, true);
ResponsiveAnalogRead RESPRATE(POT2, true);
ResponsiveAnalogRead IERATIO(POT3, true);
ResponsiveAnalogRead TRIGGER(POT4, true);
enum constants {MAXPRESSURE = 3919, FULLOPEN = 300, FLOW_MEASURE_INTERVAL = 30, CALIBRATION_FACTOR = 15000, EXHAUST_POS = 120};
bool potChanged = false;
int flowTimer;
int count;
const float PA_TO_FR[28][2] = {{0.0,69.958},{349.7884,28.977},{494.6755,22.235},{605.8513,18.745},{699.5768,16.515},{782.1506,14.93},{856.8031,13.73},{925.4531,12.78},{989.351,12.003},{1049.3652,11.353},{1106.128,10.798},{1160.1168,10.317},{1211.7025,9.895},{1261.18,9.522},{1308.7883,9.187},{1354.7246,8.886},{1399.1535,8.612},{1442.2145,8.362},{1484.0264,8.133},{1524.6922,7.922},{1564.3012,7.726},{1602.9318,7.544},{1640.653,7.375},{1677.5262,7.216},{1713.6061,7.067},{1748.9419,6.927},{1783.5778,6.795},{1817.5538,6.795}};
long flowTime = 0;
typedef struct {
    float period;
    float startTime;
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
    stepper.setMaxSpeed(5000);
    stepper.setAcceleration(5000);
    homeMotor();
    /*
    pressure.begin(pressureSensor, Clock);
    pressure.set_scale();
    pressure.tare();
    pressure.set_offset(CALIBRATION_FACTOR); // TODO: ???
    */
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
        lcd.setCursor(13, 0);
        TIDALPEAK.update();
        settings->tidalPeak = map(TIDALPEAK.getValue(), 0, 1023, 200, 800); //cc
        lcd.print(settings->tidalPeak);
        lcd.setCursor(13, 1);
        RESPRATE.update();
        settings->respRate = map(RESPRATE.getValue(), 0, 1023, 6, 40); //bpm
        lcd.print(settings->respRate);
        lcd.setCursor(14, 2);
        IERATIO.update();
        settings->IEratio = map(IERATIO.getValue(), 0, 1023, 1, 5);
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
    
    flowTimer = t.every(FLOW_MEASURE_INTERVAL, getFlow, -1);
    vars->startTime = millis();
}

void loop() {
    t.update();
    stepper.run();
    if (count%10==0) {
        if (not potChanged){
            stepper.run();
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print(readings->flowRate);
            lcd.setCursor(0,1);
            lcd.print(readings->totalVol);
        }
        else {
            if(count%20==0)lcd.clear();
            stepper.run();
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
            if(digitalRead(buttonA)==LOW){
              potChanged = false;
              recalcVars();
            }
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
    //stepper.moveTo(pos+3200);
    stepper.move(3200);
    while (stepper.distanceToGo()!=0) {
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

    Serial.print("HIGHEST: ");
    Serial.println(highest);
    delay(1000);
    readings->diff = getDiff();
    //Serial.println(2);
    while (readings->diff < highest-10){
        stepper.runToNewPosition(stepper.currentPosition()+5);
        readings->diff = getDiff();
        delay(1);
    }
    Serial.println(readings->diff);
    Serial.print("LOWEST: ");
    Serial.println(lowest);
    while (readings->diff > lowest*3) {
        stepper.runToNewPosition(stepper.currentPosition()+5);
        readings->diff = getDiff();
        delay(1);
    }
    stepper.setCurrentPosition(0);
    Serial.println(readings->diff);
    Serial.println("HOME");
}


float getDiff() {
    return (190*(analogRead(diff)*.0049))/5-38;
}

void getFlow() {
    lcd.println(millis()-flowTime);
    flowTime = millis();
    //Serial.println("FLOW");
    /*readings->pressure = getPressure();
    if (readings->pressure >= MAXPRESSURE) {
        alert();
    }*/
    readings->diff = getDiff();
    int index = (int)readings->diff/5;
    readings->flowRate = (PA_TO_FR[index][0]+(readings->diff-(float)index*5)*PA_TO_FR[index][1])/1000;
    Serial.println(readings->flowRate);
    Serial.println(vars->flowDesired);
    Serial.println();
    if (readings->flowRate > vars->flowDesired) {
        //stepper.moveTo(stepper.currentPosition()+5); // TODO: Empirical equation?
        stepper.move(5);
    }
    else if (readings->flowRate < vars->flowDesired) {
        //stepper.moveTo(stepper.currentPosition()-5);
        stepper.move(-5);
    }
    readings->totalVol += (readings->flowRate*30);
    if (abs(readings->totalVol-settings->tidalPeak)<10 || readings->totalVol-settings->tidalPeak>30){ // TODO:  || millis()-vars->startTime > vars->Tin*1.1
        exhale();
    }
}

/*float getPressure(){
    return pressure.get_units()/14.27; // TODO: FIX
}*/

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
    Serial.println("EXHALE");
    count = 0;
    //readings->PEEP = getPressure();
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
    //int startExhale = millis();
    t.stop(flowTimer);
    stepper.runToNewPosition(EXHAUST_POS);
    delay(vars->Tex);
    readings->totalVol = 0;
    flowTimer = t.every(FLOW_MEASURE_INTERVAL, getFlow, -1);
    lcd.clear();
    stepper.runToNewPosition(-60);
    vars->startTime = millis();

}
