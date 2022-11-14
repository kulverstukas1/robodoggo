#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

#define LIGHT_SENSOR A1 // photoresistor
#define GROUND_SENSOR 3 // wheel that detects when it is on the ground
#define SERIAL_TX_PIN 5
#define SERIAL_RX_PIN 2 // must be an interrupt pin!
#define AUDIO_BUSY_PIN 9 // reads LOW if playing, HIGH if not
#define LEFT_PIN 10
#define FORWARD_PIN 11
#define RIGHT_PIN 12
#define DET_PIN 13 // don't know what this actually does but the dog starts to move sporadically

#define INIT_MP3_NUM 1
#define ANSWER_MP3_NUM 2
#define IN_AIR_MP3_NUM 3
#define PAUSE_TIME 2000 // wait this many millis before repeating sound
#define IDLE_TIME 8000
#define LUX_REDUCTION_PERC 0.75

int BARK_MP3_NUMS[] = {4, 5, 6};
int GASP_MP3_NUMS[] = {7};
int WHINE_MP3_NUMS[] = {8};
int LUX_LIMIT; // photoresistor value below which we trigger an action

int lightVal;
int movementDuration;
int idleDuration;
int currFilePlaying;
bool doMovement;
unsigned long lastPlayMillis;
unsigned long lastMovementMillis;
unsigned long lastIdleMillis;
long movementDirection;
long tempDirection;
DFRobotDFPlayerMini player;
SoftwareSerial softSerial(SERIAL_RX_PIN, SERIAL_TX_PIN);

//------------------------------------------------------------------------------
bool isPlaying() {
  return digitalRead(AUDIO_BUSY_PIN) == LOW;
}
//------------------------------------------------------------------------------
void play(int trackNum) {
  player.play(trackNum);
  currFilePlaying = trackNum;
  delay(40);
}
//------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  
  delay(1000); // wait for DFPlayer to initialize

  randomSeed(analogRead(A0));

  pinMode(LIGHT_SENSOR, INPUT);
  pinMode(GROUND_SENSOR, INPUT_PULLUP);
  pinMode(AUDIO_BUSY_PIN, INPUT_PULLUP);
  pinMode(FORWARD_PIN, OUTPUT);
  pinMode(LEFT_PIN, OUTPUT);
  pinMode(RIGHT_PIN, OUTPUT);
  pinMode(DET_PIN, OUTPUT);

  digitalWrite(DET_PIN, HIGH);
  digitalWrite(FORWARD_PIN, HIGH);
  digitalWrite(LEFT_PIN, HIGH);
  digitalWrite(RIGHT_PIN, HIGH);

  softSerial.begin(9600);
  if (player.begin(softSerial)) {
    Serial.println("Player started");
  } else {
    Serial.println("Connecting to player failed!");
    while (true) {}
  }

  player.volume(30);

  // adjust lux limit dynamically depending on the environment
  int luxs;
  int iters = 5;
  for (int c = 0; c <= iters; c++) {
    luxs += analogRead(LIGHT_SENSOR);
    delay(100);
  }
  int avgLuxs = luxs / iters;
  Serial.print("LUX AVG: ");
  Serial.println(avgLuxs);
  LUX_LIMIT = avgLuxs - (avgLuxs * LUX_REDUCTION_PERC);
  Serial.print("LUX LIMIT: ");
  Serial.println(LUX_LIMIT);

  play(INIT_MP3_NUM);
  while (isPlaying()) {}
}

void loop() {
  if (!doMovement && digitalRead(GROUND_SENSOR) == HIGH) {
    // Serial.println("Dog is on the ground");
    if (currFilePlaying == IN_AIR_MP3_NUM) player.stop();
    if (digitalRead(FORWARD_PIN) == LOW) digitalWrite(FORWARD_PIN, HIGH);
    lightVal = analogRead(LIGHT_SENSOR);
    if (lightVal <= LUX_LIMIT) {
      Serial.print("Dog was pet (lux: ");
      Serial.print(lightVal);
      Serial.println(")");
      if (isPlaying()) player.stop();
      digitalWrite(DET_PIN, HIGH);
      if (!isPlaying()) play(ANSWER_MP3_NUM);
      doMovement = true;
    } else {
      if (lastIdleMillis+IDLE_TIME <= millis()) {
        if (digitalRead(DET_PIN) == HIGH) {
          digitalWrite(DET_PIN, LOW);
          Serial.println("Activating DET_PIN");
        } else {
          digitalWrite(DET_PIN, HIGH);
          Serial.println("De-Activating DET_PIN");
        }
        lastIdleMillis = millis();
        Serial.print("LUX: ");
        Serial.println(lightVal);

        if (!isPlaying()) {
          int soundNum;
          switch (random(3)) {
            case 0:
              soundNum = BARK_MP3_NUMS[random(3)];
              break;
            case 1:
              soundNum = GASP_MP3_NUMS[0];
              break;
            case 2:
              soundNum = WHINE_MP3_NUMS[0];
              break;
          }
          play(soundNum);
        }
      }
    }
  } else if (!doMovement) {
    // Serial.println("Dog is in the air");
    doMovement = false;
    digitalWrite(DET_PIN, HIGH);
    if (digitalRead(FORWARD_PIN) == HIGH) digitalWrite(FORWARD_PIN, LOW);
    if (isPlaying()) {
      lastPlayMillis = 0;
    } else {
      if (lastPlayMillis == 0) {
        lastPlayMillis = millis();
      } else {
        if (lastPlayMillis+PAUSE_TIME <= millis()) {
          Serial.println("Playing IN_AIR_MP3_NUM");
          play(IN_AIR_MP3_NUM);
        }
      }
    }
  }

  if (doMovement) {
    if (isPlaying()) {
      if (lastMovementMillis+movementDuration <= millis()) {
        digitalWrite(FORWARD_PIN, HIGH);
        digitalWrite(LEFT_PIN, HIGH);
        digitalWrite(RIGHT_PIN, HIGH);
        delay(50);
        lastMovementMillis = millis();
        movementDuration = (random(4)+2)*1000;
        do {
          tempDirection = random(3);
        } while (tempDirection == movementDirection);
        movementDirection = tempDirection;
        switch (movementDirection) {
          case 0:
            digitalWrite(FORWARD_PIN, LOW);
            Serial.print("Going forward for ");
            break;
          case 1:
            digitalWrite(LEFT_PIN, LOW);
            Serial.print("Going left for ");
            break;
          case 2:
            digitalWrite(RIGHT_PIN, LOW);
            Serial.print("Going right for ");
            break;
        }
        Serial.print(movementDuration);
        Serial.println(" millis");
      }
      if (digitalRead(GROUND_SENSOR) == LOW) player.stop();
    } else {
      if (lastMovementMillis != 0) {
        digitalWrite(FORWARD_PIN, HIGH);
        digitalWrite(LEFT_PIN, HIGH);
        digitalWrite(RIGHT_PIN, HIGH);
      }
      lastMovementMillis = 0;
      doMovement = false;
    }
  }
}