// Sound Test — cycle through candidate sounds over Serial
// Board : Arduino Mega2560
// Wiring: same as main project — PAM8403 RINP -> Pin 44 via 200 Ω resistor
//
// Open Serial Monitor at 9600 baud.
// Send 'n' to advance to the next sound, 'r' to repeat the current one.
// The sketch prints which sound is playing so you can note your favourites.

#define AUDIO_PIN 44
#define VOLUME     1   // 1–100 — lower = quieter (try 3–6 for soft)

// ── Tone engine (same as main sketch) ────────────────────────────────────────

void softTone(int freq, int durationMs) {
  unsigned long periodUs = 1000000UL / freq;
  unsigned long highUs   = periodUs * VOLUME / 100;
  unsigned long lowUs    = periodUs - highUs;
  unsigned long endMs    = millis() + (unsigned long)durationMs;
  while (millis() < endMs) {
    digitalWrite(AUDIO_PIN, HIGH);
    delayMicroseconds(highUs);
    digitalWrite(AUDIO_PIN, LOW);
    delayMicroseconds(lowUs);
  }
}

void rest(int ms) { delay(ms); }

// ── Candidate sounds ──────────────────────────────────────────────────────────

// 1. Soft single chime
void sound1() {
  softTone(1047, 300);
  rest(50);
  softTone(1047, 150);
}

// 2. Gentle two-note rise
void sound2() {
  softTone(784, 200);
  rest(40);
  softTone(1047, 350);
}

// 3. Soft three-step ascending (C-E-G)
void sound3() {
  softTone(523, 150);
  rest(30);
  softTone(659, 150);
  rest(30);
  softTone(784, 300);
}

// 4. Mellow descending (G-E-C)
void sound4() {
  softTone(784, 150);
  rest(30);
  softTone(659, 150);
  rest(30);
  softTone(523, 300);
}

// 5. Soft double-ping
void sound5() {
  softTone(880, 120);
  rest(80);
  softTone(880, 120);
}

// 6. Calm four-note (C-D-E-G)
void sound6() {
  softTone(523, 120);
  rest(30);
  softTone(587, 120);
  rest(30);
  softTone(659, 120);
  rest(30);
  softTone(784, 300);
}

// 7. Soft falling two-note
void sound7() {
  softTone(1047, 200);
  rest(40);
  softTone(784,  350);
}

// 8. Gentle three-note descend (E-D-C)
void sound8() {
  softTone(659, 150);
  rest(30);
  softTone(587, 150);
  rest(30);
  softTone(523, 300);
}

// ── Table ─────────────────────────────────────────────────────────────────────

typedef void (*SoundFn)();

SoundFn sounds[] = { sound1, sound2, sound3, sound4, sound5, sound6, sound7, sound8 };
const char* names[] = {
  "1. Soft single chime",
  "2. Gentle two-note rise",
  "3. Soft ascending C-E-G",
  "4. Mellow descending G-E-C",
  "5. Soft double-ping",
  "6. Calm four-note C-D-E-G",
  "7. Soft falling two-note",
  "8. Gentle descend E-D-C"
};
const int NUM_SOUNDS = sizeof(sounds) / sizeof(sounds[0]);

int current = 0;

void playAndPrint() {
  Serial.print("Playing: ");
  Serial.println(names[current]);
  sounds[current]();
  Serial.println("  -> Send 'n' for next, 'r' to repeat.");
}

// ── Setup / loop ──────────────────────────────────────────────────────────────

void setup() {
  pinMode(AUDIO_PIN, OUTPUT);
  Serial.begin(9600);
  Serial.println("=== Sound Test ===");
  Serial.print("VOLUME = "); Serial.println(VOLUME);
  Serial.println("Change #define VOLUME at the top to adjust loudness.");
  Serial.println();
  playAndPrint();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'n') {
      current = (current + 1) % NUM_SOUNDS;
      playAndPrint();
    } else if (c == 'r') {
      playAndPrint();
    }
  }
}
