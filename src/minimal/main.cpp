#include <stdint.h>
#include "Arduino.h"
#include "avr/io.h"

// Minimal Version of the controller script for OpenBuzz V1:
//    - Winner detection
//    - Tone generation (with piezo buzzer on pin 9)
//    - Master reset

// MCU pins
#define SER 21
#define SRCLK 6
#define RCLK 5
#define MR 20       // Active LOW 
#define BUZZER 9
#define RST_BTN 10  // Active LOW

// Teams
#define TEAM_A 0
#define TEAM_B 1
#define NO_TEAM 0xFF

// Tones
#define TEAM_A_TONE 3500
#define TEAM_B_TONE 3300
#define TONE_DURATION_MS 300

// Player input pins
#define A1 PB0
#define A2 PD1
#define A3 PD7
#define A4 PD0

#define B1 PC0
#define B2 PC1
#define B3 PC2
#define B4 PC3

uint8_t pod_led_lookup_team_a[] = {
  0b00000000,
  0b00100000,
  0b01000000,
  0b10000000,
  0b00010000,
};

// Status variables
volatile uint8_t team_buzzed = NO_TEAM;
volatile bool buzzed = false;
bool handled = false;

// Input buffer
volatile uint8_t snapshotB;
volatile uint8_t snapshotC;
volatile uint8_t snapshotD;

void initMCU() {
  // Set all pins as inputs unless overriden
  DDRB = 0x00;
  DDRC = 0x00;
  DDRD = 0x00;

  pinMode(SER, OUTPUT);
  pinMode(SRCLK, OUTPUT);
  pinMode(RCLK, OUTPUT);
  pinMode(MR, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(MR, true);
}

void initInterrputs() {
  pinMode(2, INPUT); // INT0 / TEAM B
  pinMode(3, INPUT); // INT1 / TEAM A

  EIMSK |= _BV(INT0) | _BV(INT1); // Enable INT0 and INT1
  EICRA = 0b00001111; // Trigger both on rising edge (ISC = 0b11)

  sei();
}

void shift(uint16_t output) {
  digitalWrite(RCLK, false);
  shiftOut(SER, SRCLK, MSBFIRST, output >> 8);
  shiftOut(SER, SRCLK, MSBFIRST, output & 0xFF);
  digitalWrite(RCLK, true);
}

void selectLED(uint8_t team, uint8_t player) {
  uint8_t base_LEDs = (1 << (player - 1 + team * 4));
  uint8_t pod_LEDs = 0x00;
  
  if (team == TEAM_B)
    pod_LEDs = (1 << (4 - player + !team * 4));
  else
    pod_LEDs = pod_led_lookup_team_a[player];

  uint16_t output = (pod_LEDs << 8) + base_LEDs;

  shift(output);
}

void clearLED() {
  shift(0x0000);
}

void resetBuzzers() {
  digitalWrite(MR, false);
  delay(1);
  digitalWrite(MR, true);

  clearLED();
  buzzed = false;
  handled = false;
  team_buzzed = NO_TEAM;
}

void handleBuzz() {
  uint8_t player = 0;

  if (team_buzzed == TEAM_A) {
    if (snapshotB & _BV(A1))
      player = 1;

    if (snapshotD & _BV(A2))
      player = 2;

    if (snapshotD & _BV(A3))
      player = 3;

    if (snapshotD & _BV(A4))
      player = 4;
    
    tone(BUZZER, TEAM_A_TONE, TONE_DURATION_MS);
  }

  if (team_buzzed == TEAM_B) {
    if (snapshotC & _BV(B1))
      player = 1;

    if (snapshotC & _BV(B2))
      player = 2;

    if (snapshotC & _BV(B3))
      player = 3;

    if (snapshotC & _BV(B4))
      player = 4;
  
    tone(BUZZER, TEAM_B_TONE, TONE_DURATION_MS);
  }

  selectLED(team_buzzed, player);
}

int main() {
  init();
  initMCU();
  initInterrputs();
  clearLED();

  while (true) {
    if (buzzed && !handled) {
      handleBuzz();
      handled = true;
    }

    if (!digitalRead(RST_BTN)) {
      resetBuzzers();
    }
  }
}

// Team B buzzed
ISR(INT0_vect) {
  if (!buzzed) {
    snapshotB = PINB;
    snapshotC = PINC;
    snapshotD = PIND;

    buzzed = true;
    team_buzzed = TEAM_B;
  }
}

// Team A buzzed
ISR(INT1_vect) {
  if (!buzzed) {
    snapshotB = PINB;
    snapshotC = PINC;
    snapshotD = PIND;

    buzzed = true;
    team_buzzed = TEAM_A;
  }
}