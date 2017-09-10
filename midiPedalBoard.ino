#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MIDI.h>

MIDI_CREATE_DEFAULT_INSTANCE();

#define DEBUG_NONE 0
#define DEBUG_SENSORS 1
#define DEBUG_VELOCITY 2
#define DEBUG_PEAKS 3

//TODO: test and adjust
#define SENSOR_THREASHOLD 200
#define MODE_SINGLE_NOTE  0
#define TRACKING_STEPS 10
#define MIN_TOTAL 2000.0
#define MAX_TOTAL 8000.0

#define SENSOR_BASELINE 0
#define SENSOR_RISING 1
#define SENSOR_HOLDING 2
#define SENSOR_FALLING 3

#define LED 11

boolean calibrationMode = false;
boolean configMode = false;
long calibrationTime = 0;
int debugMode = DEBUG_VELOCITY;

byte major = 0;
byte minor = 1;
byte diminished = 2;
byte majorPattern[] = {major, minor, minor, major, major, minor, diminished};
long tempCount=0;
class Note {
  public:
    int mpNoteChannel, mpLedChannel;
    String name;
    byte sensorState = SENSOR_BASELINE;
    int stateChangeCount = 0;
    long peakReading = 0;
    long baseline = 0;
    long threashold = 0;
    float averagePeak = 600.0; //default - debug for each sensor
    float velocity = 0;

    Note() {}
    Note(int _mpNoteChannel, int _mpLedChannel, String _name) {
      mpNoteChannel = _mpNoteChannel;
      mpLedChannel = _mpLedChannel;
      name = _name;
    }
    void updateBaseline(long value) {
        baseline = value;
        threashold = value + 10;
    }
    void reset() {
      peakReading = 0;
      velocity = 0;
    }
    float captureVelocity() {
      velocity = (peakReading - baseline) / averagePeak;
     
      if (velocity < 0) {
        velocity = 0;
      } else if (velocity > 1) {
        velocity = 1;
      }
      velocity = velocity * 127.0;
      return velocity;
    }

    float getPeakDebug() {
      if (velocity > 0) {
        return peakReading - baseline;
      } 
      return 0;
    }
};

class Multiplexer {
    //cd74hc4067 multiplexer
  int mpAddresses[16][4] = {
    {0,0,0,0},
    {1,0,0,0},
    {0,1,0,0},
    {1,1,0,0},
    {0,0,1,0},
    {1,0,1,0},
    {0,1,1,0},
    {1,1,1,0},
    {0,0,0,1},
    {1,0,0,1},
    {0,1,0,1},
    {1,1,0,1},
    {0,0,1,1},
    {1,0,1,1},
    {0,1,1,1},
    {1,1,1,1}
  };
  int sig, s0, s1, s2, s3;
  public:
  Multiplexer() {}
  Multiplexer(int _sig, int _s0, int _s1, int _s2, int _s3) {
    s0 = _s0;
    s1 = _s1;
    s2 = _s2;
    s3 = _s3;
    sig = _sig;
  }
  void init() {
    pinMode(s0, OUTPUT);
    pinMode(s1, OUTPUT);
    pinMode(s2, OUTPUT);
    pinMode(s3, OUTPUT);
    pinMode(sig, INPUT);
  }
  long analogReadChannel(int channel) {
    selectChannel(channel);
    return analogRead(sig);
  }
  void selectChannel(int channel) {
    digitalWrite(s0,mpAddresses[channel][0]);
    digitalWrite(s1,mpAddresses[channel][1]);
    digitalWrite(s2,mpAddresses[channel][2]);
    digitalWrite(s3,mpAddresses[channel][3]);
  }
};

class Mode {
  public:
  String name;
  int id;
  Mode() {}
  Mode(int _id, String _name) {
    id = _id;
    name = _name;
  }
};
Mode modes[2] = {Mode(0, "Single Note"), Mode(1, "5ths")};

class Settings {
  public:
  int midiChannel = 1;
  int octave = 3;
  Mode mode = modes[0];
};
Settings settings = Settings();

Note notes[13];
Multiplexer sensorMp = Multiplexer(A0,5,4,3,2);
//http://henrysbench.capnfatz.com/henrys-bench/arduino-projects-tips-and-more/arduino-quick-tip-find-your-i2c-address/
LiquidCrystal_I2C  lcd(0x3F,16, 2);

long ms;//current time

void setup() {
  
  if (debugMode != DEBUG_NONE) {
    Serial.begin(9600);
    Serial.println("Starting...");
  } else {
      MIDI.begin(MIDI_CHANNEL_OMNI);
  }

  pinMode(LED, OUTPUT);
  analogWrite(LED, 255);
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  String noteNames[13] = {"C", "C#","D","D#","E","F","F#","G","G#","A","A#","B","C2"};
  for (int i=0; i<13; i++) {
    notes[i] = Note(i,i,noteNames[i]);
  }
  sensorMp.init();

  setupPeakValues();
 
  initLCD();
  writeLCD("Mode: " + String(settings.mode.name), "Ch: " + String(settings.midiChannel) + " Oct:" + String(settings.octave));
}

void setupPeakValues() {
  //these values are determined by debugging the average peak for each sensor
  //for fine tuning velocity
  //notes[0].averagePeak = 600.0;
  //notes[1].averagePeak = 500.0;

}

void loop() {
  /*
  ms = millis();
  if (calibrationMode) {
    if (calibrationTime == 0) {
      calibrationTime = ms + 10000;
    }
    collectSensorCalibrationData();
    if (ms >= calibrationTime) {
      calibrationTime = 0;
      for (int i=0; i<2; i++) {
        Serial.print(notes[i].peakReading);
        Serial.print(" ");
        Serial.print(notes[i].baseline);
        Serial.println();
      }
    }
   
  } 

  else { */
    if (!configMode) {
      readSensors();
      //MIDI.sendNoteOn(42, 127, 1);
      //delay(1000);
      //MIDI.sendNoteOff(42, 0, 1);
      //delay(500);
    }  
 // }
  
}

void initLCD() {
  lcd.init();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Starting...");
}

void writeLCD(String line1, String line2) {
  lcd.setCursor(0,0);
  lcd.print(line1);
  lcd.setCursor(0,1);
  lcd.print(line2);
}

void debugDifference(long difference) {
    Serial.print(difference);
    Serial.print(" ");
    Serial.println();
}

void collectConfigData() {
  
}

void readSensors() {
  int i;
  float maxVelocity;
  
  for (i=0; i<8; i++) {
    long value = sensorMp.analogReadChannel(notes[i].mpNoteChannel);
    long difference = value - notes[i].baseline;

    if (debugMode == DEBUG_SENSORS) {
      Serial.print(value);
      Serial.print(" ");
    }
    
    if (notes[i].sensorState == SENSOR_BASELINE && notes[i].baseline > 0 && difference > 15) {
        notes[i].sensorState = SENSOR_RISING;
    
    } else if (notes[i].sensorState == SENSOR_RISING) {
      notes[i].stateChangeCount++;
      
      if (value > notes[i].peakReading && notes[i].stateChangeCount < 3) {
        notes[i].peakReading = value; //still rising
      } else if (value <= notes[i].peakReading || notes[i].stateChangeCount == 3) {
          //digitalWrite(13, HIGH); 
          notes[i].stateChangeCount = 0;
          notes[i].sensorState = SENSOR_HOLDING;
          notes[i].captureVelocity();
      }

    
    } else if (notes[i].sensorState == SENSOR_HOLDING) {
       if(value < notes[i].peakReading - 10) {
          notes[i].stateChangeCount++;
          if (notes[i].stateChangeCount == 2) {
            notes[i].sensorState = SENSOR_FALLING;
            notes[i].peakReading = 0;
            notes[i].stateChangeCount = 0;
            stopNote(i);
          }
       } else {
         notes[i].stateChangeCount = 0;
       }
       
    } else if (notes[i].sensorState == SENSOR_FALLING) {
       if (value <= notes[i].baseline) {
          notes[i].updateBaseline(value);
          notes[i].sensorState = SENSOR_BASELINE;
          notes[i].velocity = 0;
       }
  
    } else {
      notes[i].updateBaseline(value);
    }

    //get value for led
    float velocity = notes[i].velocity;
    if (velocity > maxVelocity) {
      maxVelocity = velocity;
    }
    if (debugMode == DEBUG_VELOCITY) {
      Serial.print(velocity);
      Serial.print(" ");
    } else if (debugMode == DEBUG_PEAKS) {
      Serial.print(notes[i].getPeakDebug());
      Serial.print(" ");
    }
   
  }

  analogWrite(LED, (maxVelocity / 127.0) * 255);
  
  if (debugMode != DEBUG_NONE) {
      Serial.println();
  }
}

void collectSensorCalibrationData() {
    for (int i=0; i<12; i++) {
      long value = sensorMp.analogReadChannel(notes[i].mpNoteChannel);
      if (notes[i].baseline == 0) {
        notes[i].baseline = value;
      }
      if (value > notes[i].peakReading) {
        notes[i].peakReading = value;
      } else if (value < notes[i].baseline) {
        notes[i].baseline = value;
      }
      
    }
}

void stopNote(int index) {
  //TODO: stop midi note
  notes[index].reset();
  //digitalWrite(13, LOW);
}

