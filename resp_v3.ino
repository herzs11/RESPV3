#include <Arduino.h>
#include <AccelStepper/AccelStepper.h>
#include <LiquidCrystalI2C/LiquidCrystal_I2C.h>
#include <Timer2/Timer.h>
#include <Timer2/Event.h>
#include <Wire/Wire.h>
#include <ResponsiveAnalogRead/src/ResponsiveAnalogRead.h>
#include <HX711/HX711.h>

HX711 pressure;
enum pins = {pulse = 6, dir = 5, buttonA = 2, buttonB = 3, pressure = 8, clock = 9, diff = A5, buzzer = 11};
AccelStepper stepper(1, pins.pulse, pins.dir);
LiquidCrystal_I2C lcd(0x27, 20, 4);
Timer t;
enum potPins = {POT1 = A0, POT2 = A1, POT3 = A2, POT4 = A3};
enum POTS = {ResponsiveAnalogRead TIDALPEAK(potPins.POT1, true), ResponsiveAnalogRead RESPRATE(potPins.POT2, true),
        ResponsiveAnalogRead IERATIO(potPins.POT3, true), ResponsiveAnalogRead TRIGGER(potPins.POT4, true)};
enum constants = {MAXPRESSURE = 3919, FULLOPEN = 300, FLOW_MEASURE_INTERVAL = 30, CALIBRATION_FACTOR = 15000};

bool potChanged = false;
int flowTimer;
int count;

typedef struct {
    float period,
    float Tin,
    float Tex,
    float Thold,
    float flowDesired,
    bool inhale
} vars;
vars VARS;

typedef struct {
    float pressure,
    float diff,
    float flowRate,
    float totalVol,
    float PEEP
} readings;

readings READINGS;

typedef struct {
    float tidalPeak,
    float respRate,
    float IEratio,
    float triggerSensitivity,
} settings;

settings SETTINGS;
void homeMotor();
float getPressure();
void alert();
void recalcVars();
float getDiff();
void getFlow();

void setup() {
    pinMode(pins.buttonA, INPUT_PULLUP);
    pinMode(pins.buttonB, INPUT_PULLUP);
    pinMode(pins.diff, INPUT);
    pinMode(potPins.POT1, INPUT);
    pinMode(potPins.POT2, INPUT);
    pinMode(potPins.POT3, INPUT);
    pinMode(potPins.POT4, INPUT);
    pinMode(pins.buzzer, OUTPUT);
    pinMode(pins.pulse, OUTPUT);
    pinMode(pins.dir, OUTPUT);
    pressure.begin(pins.pressure, pins.clock);
    pressure.set_scale();
    pressure.tare();
    pressure.set_scale(constants.CALIBRATION_FACTOR)
    lcd.init();
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
    while (digitalRead(pins.buttonA)==HIGH) {
        Serial.print("HERE");
        lcd.setCursor(13, 0);
        POTS.TIDALPEAK.update();
        SETTINGS.tidalPeak = map(POTS.TIDALPEAK.getValue(), 0, 1023, 200, 800); //cc
        lcd.print(SETTINGS.tidalPeak);
        lcd.setCursor(13, 1);
        POTS.RESPRATE.update();
        SETTINGS.respRate = map(POTS.RESPRATE.getValue(), 0, 1023, 8, 40); //bpm
        lcd.print(SETTINGS.respRate);
        lcd.setCursor(14, 2);
        POTS.IERATIO.update();
        SETTINGS.IEratio = map(POTS.IERATIO.getValue(), 0, 1023, 1, 4);
        lcd.print(SETTINGS.IEratio);
        lcd.setCursor(11, 3);
        POTS.TRIGGER.update();
        SETTINGS.triggerSensitivity = map(POTS.TRIGGER.getValue(), 0, 1023, -1, -5); //?? cc
        lcd.print(SETTINGS.triggerSensitivity);
        delay(5);
    }
    lcd.clear();
    delay(500);
    recalcVars();
    stepper.setMaxSpeed(5000);
    stepper.setAcceleration(5000);
    homeMotor();
    flowTimer = t.every(constants.FLOW_MEASURE_INTERVAL, getFlow, -1);

}

void loop() {
    t.update();
    stepper.run();
    if (count%5==0) {
        if (not potChanged){
            stepper.run();
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print(READINGS.flowRate);
            lcd.setCursor(0,1);
            lcd.print(READINGS.totalVol);
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
            POTS.TIDALPEAK.update();
            SETTINGS.tidalPeak = map(POTS.TIDALPEAK.getValue(), 0, 1023, 200, 800); //cc
            lcd.print(SETTINGS.tidalPeak);
            lcd.setCursor(13, 1);
            POTS.RESPRATE.update();
            SETTINGS.respRate = map(POTS.RESPRATE.getValue(), 0, 1023, 8, 40); //bpm
            lcd.print(SETTINGS.respRate);
            lcd.setCursor(14, 2);
            POTS.IERATIO.update();
            SETTINGS.IEratio = map(POTS.IERATIO.getValue(), 0, 1023, 1, 4);
            lcd.print(SETTINGS.IEratio);
            lcd.setCursor(11, 3);
            POTS.TRIGGER.update();
            SETTINGS.triggerSensitivity = map(POTS.TRIGGER.getValue(), 0, 1023, -1, -5); //?? cc
            lcd.print(SETTINGS.triggerSensitivity);
        }
    }
    stepper.run();
    if (count%10==0 && !potChanged){
        checkPot()
    }
    count++;
}

void homeMotor(){ // TODO: FIND REAL HOME FUNCTION
    while (.1>getDiff()>-.1) {
        digitalWrite(pins.pulse, HIGH);
        delay(10);
        digitalWrite(pins.pulse, LOW);
        delay(10);
    }
    stepper.setCurrentPosition(0);

}

float getDiff() {
    return ((analogRead(pins.diff)*.0049)-1)*.4-.59;
}

void getFlow() {
    READINGS.pressure = getPressure();
    if (READINGS.pressure >= constants.MAXPRESSURE) {
        alert();
    }
    READINGS.flowRate = getDiff();
    if (READINGS.flowRate > VARS.flowDesired) {
        stepper.moveTo(stepper.currentPosition()-50); // TODO: Empirical equation?
    }
    else if (READINGS.flowRate < VARS.flowDesired) {
        stepper.moveTo(stepper.currentPosition()+50);
    }
    READINGS.totalVol += (VARS.flowDesired*30);
    if (abs(READINGS.totalVol-SETTINGS.tidalPeak)<10){
        inhale();
    }
}

float getPressure(){
    return pressure.get_units()/14.27; // TODO: FIX
}

void alert(){
    while (digitalRead(pins.buttonA) == HIGH){
        digitalWrite(pins.buzzer, HIGH);
        delay(200);
        digitalWrite(pins.buzzer, LOW);
        delay(200);
    }
}

void recalcVars(){
    VARS.period = 60000/SETTINGS.respRate;
    VARS.Tin = VARS.period/(SETTINGS.IEratio+1)*.8;
    VARS.Thold = VARS.period/(SETTINGS.IEratio+1)*.2;
    VARS.Tex = VARS.period-VARS.Tin-VARS.Thold;
    VARS.flowDesired = SETTINGS.tidalPeak/VARS.Tin;
}

void checkPot(){
    static POTS pot = TIDALPEAK;
    switch(pot) {
        case TIDALPEAK:
            POTS.TIDALPEAK.update();
            if (abs(SETTINGS.tidalPeak - map(POTS.TIDALPEAK.getValue(), 0, 1023, 200, 800)) > 20){
                potChanged=true;
            }
            pot = RESPRATE;
            break;
        case RESPRATE:
            POTS.RESPRATE.update();
            if (abs(SETTINGS.respRate - map(POTS.RESPRATE.getValue(), 0, 1023, 8, 40)) > 1){
                potChanged=true;
            }
            pot = IERATIO;
            break;
        case IERATIO:
            POTS.IERATIO.update();
            if (abs(SETTINGS.IEratio - map(POTS.IERATIO.getValue(), 0, 1023, 1, 4)) > 0) {
                potChanged=true;
            }
            pot = TRIGGER;
            break;
        case TRIGGER:
            POTS.TRIGGER.update();
            if (abs(SETTINGS.triggerSensitivity - map(POTS.TRIGGER.getValue(), 0, 1023, -1, -5)) > > 0) {
                potChanged=true;
            }
            pot = TIDALPEAK;
            break;
    }
}

void inhale() {
    count = 0;
    READINGS.PEEP = getPressure();
    int startHold = millis();
    /*while (millis()-startHold < VARS.Thold){
        if (getPressure() > READINGS.PEEP){
            stepper.moveTo(stepper.currentPosition()-10);
        }
        else {
            stepper.moveTo(stepper.currentPosition()+10);
        }
        stepper.run();
    }*/
    delay(VARS.Thold); //TODO: CHOOSE THIS OR ABOVE^??
    int startExhale = millis();
    t.stop(flowTimer);
    stepper.runToNewPosition(0);
    while (millis()-startExhale<VARS.Tex){
        // HOLD
    }
    READINGS.totalVol = 0;
    flowTimer = t.every(constants.FLOW_MEASURE_INTERVAL, getFlow, -1);
    lcd.clear();

}