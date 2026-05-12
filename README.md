# Productivity Buddy 

A Pomodoro-style focus timer built on an Arduino Mega 2560 with a 4" TFT display, force-sensitive resistor (FSR), rotary encoder, and speaker. Place your phone on the holder to start a session; lift it and the timer pauses automatically.

---

## Hardware

| Component | Part |
|-----------|------|
| Microcontroller | Arduino Mega 2560 |
| Display | Hosyond 4.0" 480×320 TFT (ST7796S driver) |
| Input | KY-040 rotary encoder |
| Force sensor | FSR (Force Sensitive Resistor) |
| Audio | PAM8403 amplifier + speaker |

### Wiring

**Display (SPI)**

| Display pin | Arduino pin |
|-------------|-------------|
| CS | A5 |
| DC | A3 |
| RST | A4 |
| LED | A0 |
| MOSI | 51 |
| MISO | 50 |
| SCK | 52 |

**Rotary encoder (KY-040)**

| Encoder pin | Arduino pin |
|-------------|-------------|
| CLK (A) | 2 (INT0) |
| DT (B) | 3 |
| SW | 4 |
| GND | GND |

**Force sensor (FSR)**

- One leg → 5V
- Other leg → A1 and a 10 kΩ resistor to GND (voltage divider)
- Reads ~0 with no weight, rises with pressure

**PAM8403 audio amplifier**

| PAM8403 pin | Connection |
|-------------|------------|
| VDD | 5V |
| GND | GND |
| SDNN | 5V (keeps amp enabled) |
| RINP | Arduino Pin 44 via 10 kΩ resistor |
| RINN | GND |
| ROUTP / ROUTN | Speaker + / Speaker - |

---

## Libraries required

Install both via Arduino IDE Library Manager or manually:

- [LCDWIKI_GUI](http://www.lcdwiki.com)
- [LCDWIKI_SPI](http://www.lcdwiki.com)

---

## How to use

### Navigation
- **Rotate encoder UP/DN** — navigate menus, adjust values
- **Press encoder button (SEL)** — confirm selection

### Starting a session
1. From the home screen, select **Start Focus Session** and press SEL.
2. Place your phone on the FSR holder — the timer starts automatically.

### During focus
- The countdown timer and a progress bar are shown.
- **Lift your phone** — the timer pauses and dims. The screen time clock starts.
- **Replace your phone** — the timer resumes from where it left off.

### Breaks
- After each focus session a break timer starts automatically.
- The break timer runs regardless of whether the phone is on the holder.
- Screen time is still counted if the phone is off the holder during a break.

### End of all sessions
After the final session a congratulations screen shows:
- Total **screen time** (time phone was off the holder, across focus and break)
- Total **phone pickups** (number of times the phone was lifted, across focus and break)

### Set Times
From the home screen, select **Set Times** to adjust:

| Setting | Default | Range |
|---------|---------|-------|
| Focus duration | 1 min | 5–120 min (5 min steps) |
| Break duration | 5 min | 1–30 min (1 min steps) |
| Number of sessions | 4 | 1–10 |

---

## Configurable constants

All are near the top of `focus_menu_fsr.ino`:

| Constant | Default | Description |
|----------|---------|-------------|
| `FSR_ON` | 20 | Raw ADC threshold — above this, phone is detected on holder |
| `FSR_OFF` | 8 | Raw ADC threshold — below this, phone is detected as lifted |
| `VOLUME` | 1 | Audio duty cycle (1–100). Lower = quieter |

### FSR threshold tuning
If the timer starts on its own with no phone, increase `FSR_ON`. To read raw sensor values for calibration, add `Serial.println(analogRead(FSR_PIN))` inside the `PLACE_PHONE` case temporarily.

---

## Audio
Sounds are played via software PWM on Pin 44 through the PAM8403 amplifier.

| Event | Sound |
|-------|-------|
| Focus session complete | Soft ascending C–E–G |
| Break complete | Soft double-ping |
| All sessions complete | Calm four-note C–D–E–G |

To test or change sounds, open the companion sketch at `../sound_test/sound_test/sound_test.ino`, upload it, and use the Serial Monitor (9600 baud) to cycle through options with `n` (next) and `r` (repeat).

---
