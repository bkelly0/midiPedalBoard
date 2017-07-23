#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//TODO: test and adjust
#define SENSOR_THREASHOLD 100
#define MODE_SINGLE_NOTE  0
#define TRACKING_STEPS 4

boolean configMode = false;

byte major = 0;
byte minor = 1;
byte diminished = 2;
byte majorPattern[] = {major, minor, minor, major, major, minor, diminished};
long tempCount=0;
class Note {
  public:
    int mpNoteChannel, mpLedChannel;
    String name;
    boolean isOn = false;
    long trackingStartTime = 0;
    long trackingCount = 0;
    long trackingTotal = 0;
    Note() {}
    Note(int _mpNoteChannel, int _mpLedChannel, String _name) {
      mpNoteChannel = _mpNoteChannel;
      mpLedChannel = _mpLedChannel;
      name = _name;
    }
    void reset() {
      isOn = false;
      trackingStartTime = 0;
      trackingCount = 0;
      trackingTotal = 0;
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
  Multiplexer(int sig, int _s0, int _s1, int _s2, int _s3) {
    s0 = _s0;
    s1 = _s1;
    s2 = _s2;
    s3 = _s3;
    pinMode(s0, OUTPUT);
    pinMode(s1, OUTPUT);
    pinMode(s2, OUTPUT);
    pinMode(s3, OUTPUT);
    pinMode(sig, INPUT);
  }
  long analogRead(int channel) {
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
Multiplexer sensorMp = Multiplexer(A0,1,2,3,4);
LiquidCrystal_I2C  lcd(0x27,16,2);

long ms;//current time

void setup() {
  Serial.begin(9600);
  String noteNames[13] = {"C", "C#","D","D#","E","F","F#","G","G#","A","A#","B","C2"};
  for (int i=0; i<13; i++) {
    notes[i] = Note(i,i,noteNames[i]);
  }

  initLCD();
  writeLCD("Mode: " + String(settings.mode.name), "Ch: " + String(settings.midiChannel));
}

void loop() {
  ms = millis();
  if (!configMode) {
    readSensors();
  }
  tempCount ++;
  Serial.println((ms/1000) + " " + tempCount);
}

void initLCD() {
  lcd.begin();
  lcd.backlight();
  lcd.clear();
}

void writeLCD(String line1, String line2) {
  lcd.setCursor(0,0);
  lcd.print(line1);
  lcd.setCursor(0,1);
  lcd.print(line2);
  delay(1000);
}

void readSensors() {
  for (int i=0; i<13; i++) {
    Note note = notes[i];
    long value = sensorMp.analogRead(note.mpNoteChannel);
    if (!note.isOn && value >= SENSOR_THREASHOLD) {
      if (note.trackingStartTime == 0) {
        note.trackingStartTime = ms;
        note.trackingTotal = value;
        note.trackingCount++;
      } else {
        note.trackingTotal += value;
        note.trackingCount++;
        if (note.trackingCount == TRACKING_STEPS) {
          //TODO: calculate velocity and send midi on
        }
      }
    } else if (note.isOn && value < SENSOR_THREASHOLD) {
      stopNote(i);
    }
  }
}

void stopNote(int index) {
  //TODO: stop midi note
  notes[index].reset();
}

