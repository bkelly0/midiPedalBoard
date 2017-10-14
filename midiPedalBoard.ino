#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MIDI.h>

MIDI_CREATE_DEFAULT_INSTANCE();

#define DEBUG_NONE 0
#define DEBUG_SENSORS 1 //sensor values to serial plotter
#define DEBUG_VELOCITY 2 //velocity values to serial plotter
#define DEBUG_PEAKS 3 //peak difference values to serial plotter
#define DEBUG_NOTE 4 //midi note on/off
#define DEBUG_DIFFERENCE 5 //debug changes in sensor reading

//sensor/note states
#define SENSOR_BASELINE 0
#define SENSOR_RISING 1
#define SENSOR_HOLDING 2
#define SENSOR_FALLING 3

#define LED 11

byte MODE_SINGLE = 0;
byte MODE_OCTAVE = 1;
byte MODE_5THS = 2;

//note: debugging decreases performace
int debugMode = DEBUG_SENSORS;
boolean configMode = false;

//TODO: modes for this stuff
byte major = 0;
byte minor = 1;
byte diminished = 2;
byte majorPattern[] = {major, minor, minor, major, major, minor, diminished};

long enableTime;
int currentNote;

class Note {
  public:
    int mpNoteChannel, mpLedChannel;
    byte sensorState = SENSOR_BASELINE;
    int stateChangeCount = 0;
    long peakReading = 0;
    long baseline = 0;
    long threashold = 0;
    float averagePeak = 600.0; //default - debug for each sensor
    float velocity = 0;
    int midiBaseNote = 0;
    int midiNote = 0;
    long reenableTime = 0;

    Note() {}
    Note(int _mpNoteChannel, int _mpLedChannel, int _midiBaseNote) {
      mpNoteChannel = _mpNoteChannel;
      mpLedChannel = _mpLedChannel;
      midiBaseNote = _midiBaseNote;
      midiNote = _midiBaseNote;
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
      velocity = getVelocityPercentage() * 127.0;
      return velocity;
    }
    float getAftertouch() {
      return getVelocityPercentage() * 127.0;
    }

    float getVelocityPercentage() {
      float p = (peakReading - baseline) / averagePeak;
    
      if (p < 0) {
        p = 0;
      } else if (p > 1) {
        p = 1;
      }
      return p;
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
  int mpAddresses[14][4] = {
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
    {1,0,1,1}
    //not used:
    //{0,1,1,1},
    //{1,1,1,1}
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
  String title;
  int id;
  Mode() {}
  Mode(int _id, String _title) {
    id = _id;
    title = _title;
  }
};


class Config {
  public:
  int midiChannel = 1;
  int octave = 3;
  byte mode;
  boolean afterTouch = false;
  String description1;
  String description2;
  Config() {}
  Config(int _midiChannel, int _octave, byte _mode, boolean _afterTouch, String _desc1, String _desc2) {
    midiChannel = _midiChannel;
    octave = _octave;
    mode = _mode;
    afterTouch = _afterTouch;
    description1 = _desc1;
    description2 =_desc2;
  }

};


Note notes[14];
Multiplexer sensorMp = Multiplexer(A0,5,4,3,2);
//http://henrysbench.capnfatz.com/henrys-bench/arduino-projects-tips-and-more/arduino-quick-tip-find-your-i2c-address/
LiquidCrystal_I2C  lcd(0x3F,16, 2);
int configIndex = 0;
Config configs[3] = {
    Config(2, 3, MODE_SINGLE, false, "Single - Oct: 3", "XXXXXXX"),
    Config(2, 5, MODE_OCTAVE, false, "Octave - Oct: 5", "YYYYYYYY"),
    Config(2, 5, MODE_5THS, false, "Fifths - Oct: 5", "ZZZZZZZ")
  };
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
  analogWrite(LED, 0);

  initLCD();
  updateLCD();

  //String noteNames[13] = {"C", "C#","D","D#","E","F","F#","G","G#","A","A#","B","C2"};
  for (int i=0; i<13; i++) {
    notes[i] = Note(i,i,i);
  }
  notes[13] = Note(13,13,13);
  sensorMp.init();

  setupPeakValues();
  updateMidiNoteValues();

  enableTime = millis() + 2000;
}

void selectNextConfig() {
  configIndex++;
  if (configIndex > 2) {
    configIndex = 0;
  }
  updateMidiNoteValues();
  updateLCD();
}

void updateLCD() {
    writeLCD(configs[configIndex].description1, configs[configIndex].description2);
}

void updateMidiNoteValues() {
  int interval = configs[configIndex].octave * 12;
  for (int i=0; i<13; i++) {
    notes[i].midiNote = notes[i].midiBaseNote + interval;  
  }
}

void setupPeakValues() {
  //these values are determined by debugging the average peak for each sensor
  //for fine tuning velocity
  //notes[0].averagePeak = 600.0;
  //notes[1].averagePeak = 500.0;
}

void loop() {
  readSensors();
}

void initLCD() {
  lcd.init();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Starting...");
}

void writeLCD(String line1, String line2) {
  lcd.clear();
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

  ms = millis();
  
  for (i=0; i<14; i++) {
    long value = sensorMp.analogReadChannel(notes[i].mpNoteChannel);
    long difference = value - notes[i].baseline;

    if (debugMode == DEBUG_SENSORS) {
      Serial.print(value);
      Serial.print(" ");
    } else if (debugMode == DEBUG_DIFFERENCE) {
      Serial.print(difference);
      Serial.print(" ");
    }

    if (enableTime > 0 && ms < enableTime) {
      //let a baseline be established
      continue;
    }
    enableTime = 0;




    if (notes[i].sensorState == SENSOR_BASELINE && notes[i].baseline > 0 && difference > 7) {
      if (notes[i].reenableTime > 0) {
        if (ms >= notes[i].reenableTime) {
          notes[i].reenableTime = 0;
        } else {
          continue;
        }
      }
        
      if (difference >= 100) {
        //skip rising state 
        notes[i].peakReading = value;
        noteOn(i);
      } else {
        notes[i].sensorState = SENSOR_RISING;
      } 
    } else if (notes[i].sensorState == SENSOR_RISING) {
      notes[i].stateChangeCount++;
      
      if (value > notes[i].peakReading && notes[i].stateChangeCount < 3) {
        notes[i].peakReading = value; //still rising
      } else if (value <= notes[i].peakReading || notes[i].stateChangeCount == 3) {
        noteOn(i);
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
         if (configs[configIndex].afterTouch && debugMode == DEBUG_NONE) {
            MIDI.sendPolyPressure(notes[i].midiNote, notes[i].getAftertouch(), configs[configIndex].midiChannel);
         }
       }
       
    } else if (notes[i].sensorState == SENSOR_FALLING) {
       if (value <= notes[i].baseline) {
          notes[i].updateBaseline(value);
          notes[i].sensorState = SENSOR_BASELINE;
          notes[i].velocity = 0;
          notes[i].reenableTime = ms + 200; //let baseline get reestablished
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
 
    if (maxVelocity > 0) {
      analogWrite(LED, (maxVelocity / 127.0) * 255);
    } else {
      analogWrite(LED, 0);
    }
  
  if (debugMode != DEBUG_NONE && debugMode != DEBUG_NOTE) {
      Serial.println();
  }
}

void noteOn(int index) {
  notes[index].stateChangeCount = 0;
  notes[index].sensorState = SENSOR_HOLDING;
  notes[index].captureVelocity();
  digitalWrite(13, HIGH);

  if (index == 13) {
    return;
  }
  
  if (debugMode == DEBUG_NONE) {
        MIDI.sendNoteOn(notes[index].midiNote, 127, configs[configIndex].midiChannel);

    //MIDI.sendNoteOn(notes[index].midiNote, notes[index].velocity, configs[configIndex].midiChannel);
    if (currentNote) {
      stopNote(currentNote);
    }
    /*
    if (configs[configIndex].mode.id == 1) {
      //octave
      MIDI.sendNoteOn(notes[index].midiNote+12, notes[index].velocity, configs[configIndex].midiChannel);
    }
    */
  } else if (debugMode == DEBUG_NOTE) {
    Serial.print("ON ");
    Serial.println(notes[index].midiNote);   
  }
  currentNote = index;

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
  digitalWrite(13, LOW);
  if (index == 13) {
    selectNextConfig();
  }
  if (debugMode == DEBUG_NONE) {
    MIDI.sendNoteOff(notes[index].midiNote, 0, configs[configIndex].midiChannel);
  } else if (debugMode == DEBUG_NOTE) {
    Serial.print("OFF ");
    Serial.println(notes[index].midiNote);          
  }
  notes[index].reset();
  if (currentNote == index) {
    currentNote = NULL;
  }
  //digitalWrite(13, LOW);
}

