/* 
 My version of HexBrightFLEX based on hexbright_bjh
 v1.3.2 lightdrop (change the 'Powered Up!' text below to match)
 ++see line 340 to set your password++
 __________________________________________________
 
 See sund/HexBright/HexBright_Mine/SJSHexBrightFLEX/
 README.MD
 __________________________________________________
 TO DO:
 Short term
 .Flash red LED when within 10 or so of OVERTEMP and
 flashes 2x within 5
 
 Long Term
 .Calc temp from wire
 .change dazzle mode to pressing button and shaking light.
 and enter SOS after holding for 5 sec
 .beacon mode and 'stabbing motion' down
 .momentary press - enter with ?
 .static mode - press while pointing down and set on surface and stay on for 5 more so minutes
 __________________________________________________
 See the sund/HexBright/README.md for info on sources.
 
 */

#include <math.h>
#include <Wire.h>

// Settings
#define OVERTEMP                300 

// Accelerometer defines
#define ACC_ADDRESS             0x4C
#define ACC_REG_XOUT            0
#define ACC_REG_YOUT            1
#define ACC_REG_ZOUT            2
#define ACC_REG_TILT            3
#define ACC_REG_INTS            6
#define ACC_REG_MODE            7

// Pin assignments
#define DPIN_RLED_SW            2 //PD2, INT0, MLF PIN 28 - Red LED
#define DPIN_ACC_INT            3 //PD3, INT1, MLF PIN 1 - Accel Int
#define DPIN_GLED               5 //PD5, OC0B, MLF PIN 7 - Green LED
#define DPIN_PGOOD              7 //PD7, MLF PIN 9 - Power Good
#define DPIN_PWR                8 //PB0, MLF PIN 10 - POWER
#define DPIN_DRV_MODE           9 //PB1, OC1A, MLF PIN 11 - Drv Mode
#define DPIN_DRV_EN             10 //PB2, OC1B, MLF PIN 12 - Drv Int?
#define APIN_TEMP               0 //PC0, ADC0, MLF PIN 19 - Temp Diode
#define APIN_CHARGE             3 //PC3, ADC3, MLF PIN 22 - Charge State

// Interrupts
#define INT_SW                  0
#define INT_ACC                 1

// Modes
#define MODE_OFF                0
#define MODE_LOW                1
#define MODE_MED                2
#define MODE_HIGH               3
#define MODE_DAZZLE             4
#define MODE_DAZZLE_PREVIEW     5
#define MODE_SOS                6
#define MODE_SOS_PREVIEW        7

// Variables
const unsigned int SerialPrintIntrvl = 5000;

// State
byte mode = 0;
unsigned long btnTime = 0;
boolean btnDown = false;
boolean dazzle_on = true;
long dazzle_period = 100;

void setup()
{
  // We just powered on!  That means either we got plugged 
  // into USB, or the user is pressing the power button.
  pinMode(DPIN_PWR,      INPUT);
  digitalWrite(DPIN_PWR, LOW);

  // Initialize GPIO
  pinMode(DPIN_RLED_SW,  INPUT);
  pinMode(DPIN_GLED,     OUTPUT);
  pinMode(DPIN_DRV_MODE, OUTPUT);
  pinMode(DPIN_DRV_EN,   OUTPUT);
  pinMode(DPIN_ACC_INT,  INPUT);
  pinMode(DPIN_PGOOD,    INPUT);
  digitalWrite(DPIN_DRV_MODE, LOW);
  digitalWrite(DPIN_DRV_EN,   LOW);
  digitalWrite(DPIN_ACC_INT,  HIGH);

  // Initialize serial busses
  Serial.begin(9600);
  Wire.begin();

  // Configure accelerometer
  byte config[] = {
    ACC_REG_INTS,  // First register (see next line)
    0xE4,  // Interrupts: shakes, taps
    0x00,  // Mode: not enabled yet
    0x00,  // Sample rate: 120 Hz active
    0x0F,  // Tap threshold
    0x10   // Tap debounce samples
  };
  Wire.beginTransmission(ACC_ADDRESS);
  Wire.write(config, sizeof(config));
  Wire.endTransmission();

  btnTime = millis();
  btnDown = digitalRead(DPIN_RLED_SW);
  mode = MODE_OFF;

  Serial.println("Powered up! v1.3.2");
  randomSeed(analogRead(1));
}

void loop()
{
  static unsigned long lastDazzleTime, lastTempTime, lastChrgTime, lastModeTime, lastAccTime, lastModeSOSTime;
  static unsigned long ditdah;
  static int ledState = LOW;

  unsigned long time = millis(); // time on in millsecs

    // Check the state of the charge controller
  int chargeState = analogRead(APIN_CHARGE);

  // Print the charge every so often
  if (time-lastChrgTime > SerialPrintIntrvl)
  {
    lastChrgTime = time;
    Serial.print("chargeState: ");
    Serial.println(chargeState);
  }

  if (chargeState < 128)  // Low - charging
  {
    digitalWrite(DPIN_GLED, (time&0x0200)?LOW:HIGH);
  }
  else if (chargeState > 768) // High - charged
  {
    digitalWrite(DPIN_GLED, HIGH);
  }
  else // Hi-Z - shutdown
  {
    digitalWrite(DPIN_GLED, LOW);    
  }

  // Check the temperature sensor
  if (time-lastTempTime > SerialPrintIntrvl)
  {
    lastTempTime = time;
    int temperature = analogRead(APIN_TEMP);
    //Serial.print("Temp: ");
    //Serial.println(temperature);
    if (temperature > OVERTEMP && mode != MODE_OFF)
    {
      Serial.println("** Overheating!");

      for (int i = 0; i < 6; i++)
      {
        digitalWrite(DPIN_DRV_MODE, LOW);
        delay(100);
        digitalWrite(DPIN_DRV_MODE, HIGH);
        delay(100);
      }
      digitalWrite(DPIN_DRV_MODE, LOW);

      mode = MODE_LOW;
    }
  }

  // Check if the accelerometer wants to interrupt
  byte tapped = 0, shaked = 0;
  if (!digitalRead(DPIN_ACC_INT))
  {
    Wire.beginTransmission(ACC_ADDRESS);
    Wire.write(ACC_REG_TILT);
    Wire.endTransmission(false);       // End, but do not stop!
    Wire.requestFrom(ACC_ADDRESS, 1);  // This one stops.
    byte tilt = Wire.read();

    if (time-lastAccTime > 500)
    {
      lastAccTime = time;

      tapped = !!(tilt & 0x20);
      shaked = !!(tilt & 0x80);

      //if (tapped) Serial.println("** Tap!");
      //if (shaked) Serial.println("** Shake!");
    }
  }

  // If we are in the special modes (DAZZLE or SOS)
  // then flip the switch on and off as needed.
  switch (mode)
  {
  case MODE_DAZZLE:
  case MODE_DAZZLE_PREVIEW:
    if (time - lastDazzleTime > dazzle_period)
    {
      digitalWrite(DPIN_DRV_EN, dazzle_on);
      dazzle_on = !dazzle_on;
      lastDazzleTime = time;
      dazzle_period = random(25,100);
    }    
    break;
  case MODE_SOS:
  case MODE_SOS_PREVIEW:
    digitalWrite(DPIN_DRV_EN, morseCodeSOS(time - lastModeSOSTime));
    break;
  }

  // Periodically pull down the button's pin, since
  // in certain hardware revisions it can float.
  // Do we have to do this on the shipping version of hexbright?
  pinMode(DPIN_RLED_SW, OUTPUT);
  pinMode(DPIN_RLED_SW, INPUT);

  // Check for mode changes
  byte newMode = mode;
  byte newBtnDown = digitalRead(DPIN_RLED_SW);
  switch (mode)
  {
  case MODE_OFF:
    if (btnDown && !newBtnDown && (time-btnTime)>20)
      newMode = MODE_LOW;
    if (btnDown && newBtnDown && (time-btnTime)>500)
      newMode = MODE_DAZZLE_PREVIEW;
    //
    //
    //
    if (btnDown && newBtnDown && (time-btnTime)>500)
      newMode = MODE_DAZZLE_PREVIEW;
    break;
  case MODE_LOW:
    if (btnDown && !newBtnDown && (time-btnTime)>50)
      if (time-lastModeTime > 1000)
        newMode = MODE_OFF;
      else newMode = MODE_MED;
    break;
  case MODE_MED:
    if (btnDown && !newBtnDown && (time-btnTime)>50)
      if (time-lastModeTime > 1000)
        newMode = MODE_OFF;
      else newMode = MODE_HIGH;
    break;
  case MODE_HIGH:
    if (btnDown && !newBtnDown && (time-btnTime)>50)
      newMode = MODE_OFF;
    break;
  case MODE_DAZZLE_PREVIEW:
    // This mode exists just to ignore this button release.
    if (btnDown && !newBtnDown)
      newMode = MODE_DAZZLE;
    break;
  case MODE_DAZZLE:
    if (btnDown && !newBtnDown && (time-btnTime)>50)
      newMode = MODE_OFF;
    if (btnDown && newBtnDown && (time-btnTime)>2000) //full 2 second press
      newMode = MODE_SOS_PREVIEW;
    break;
  case MODE_SOS_PREVIEW:
    // This mode exists just to ignore this button release.
    if (btnDown && !newBtnDown)
      newMode = MODE_SOS;
    break;
  case MODE_SOS:    
    if (btnDown && !newBtnDown && (time-btnTime)>50)
      newMode = MODE_OFF;
    break;
  }

  //activity power down EXCLUDES SOS MODE!
  if ((time - max(lastAccTime,lastModeTime) > 600000UL ) && newMode != MODE_SOS && mode != MODE_OFF ) { // 10 minutes
    newMode = MODE_OFF;
    Serial.println("** Inactivity shutdown!");
  }

  // Do the mode transitions
  if (newMode != mode)
  {
    lastModeTime = millis();

    // Enable or Disable accelerometer
    byte disable[] = {
      ACC_REG_MODE, 0x00                                        };  // Mode: standby!
    byte enable[] = {
      ACC_REG_MODE, 0x01                                        };  // Mode: active!
    Wire.beginTransmission(ACC_ADDRESS);
    if (newMode == MODE_OFF) {
      Wire.write(disable, sizeof(disable));
    } 
    else Wire.write(enable, sizeof(enable));
    Wire.endTransmission();

    switch (newMode)
    {
    case MODE_OFF:
      Serial.println("Mode = off");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, LOW);
      digitalWrite(DPIN_DRV_MODE, LOW);
      digitalWrite(DPIN_DRV_EN, LOW);
      break;
    case MODE_LOW:
      Serial.println("Mode = low");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, LOW);
      analogWrite(DPIN_DRV_EN, 32);
      break;
    case MODE_MED:
      Serial.println("Mode = medium");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, LOW);
      analogWrite(DPIN_DRV_EN, 255);
      break;
    case MODE_HIGH:
      Serial.println("Mode = high");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, HIGH);
      analogWrite(DPIN_DRV_EN, 255);
      break;
    case MODE_DAZZLE:
    case MODE_DAZZLE_PREVIEW:
      Serial.println("Mode = dazzle");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, HIGH);
      // check for serial input print the code out
      // type a char in the input hit send at the same time
      // you go into dazzle mode
      if (Serial.available()>0) 
      {
        //Serial.println("reading");
        char inSerial[7];    // the length of you password +1
        int i=0; 

        while (Serial.available() > 0) {

          inSerial[i]=Serial.read(); //read data  
          i++;      
        }
        inSerial[i]='\0';
        char* shibboleet = inSerial;
        //Serial.println(shibboleet);
        if (!strcmp(shibboleet,"Revere")) // here's the password
        {
          Serial.println("The British are coming!!");
          Serial.println("http://sund.org/y/lightdrop");
        }
      }
      //
      break;
    case MODE_SOS:
    case MODE_SOS_PREVIEW:
      Serial.println("Mode = SOS");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, HIGH);
      lastModeSOSTime = time;
      break;
    }

    mode = newMode;
  }

  // Remember button state so we can detect transitions
  if (newBtnDown != btnDown)
  {
    btnTime = time;
    btnDown = newBtnDown;
    delay(50);
  }
}

bool morseCodeSOS(unsigned long time){
  const unsigned long dit = 180; 
  // 180 ms is the frame for each dit "on", the larger this number the slower the SOS

  // Morse Code:  (thanks jaebird!)
  // S = ...  O = ---
  // SOS word = ...---...

  // word space = 7 dits duration
  // S = 5 dits duration
  // char space = 3 dits duration
  // O = 11 dits duration
  // char space = 3 dits duration
  // S = 5 dits duration
  // total duration = 34

  byte step = (time / dit) % 34; //dit number modulo the length of the sequence;
  // Start with word space
  if (step < 7) return false;
  step -= 7;
  // First S
  if (step < 5) return (step % 2) == 0; // every second dit is off
  step -= 5;
  // Char space
  if (step < 3) return false;
  step -= 3;
  // O
  if (step < 11) return (step % 4) != 3; // every fourth dit is off
  step -= 11;
  // Char space
  if (step < 3) return false;
  step -= 3;
  // Last S
  if (step < 5) return (step % 2) == 0; // every second dit is off
  // Should never get here
  Serial.println("** Morse SOS overrun error");  
  return false;
}









