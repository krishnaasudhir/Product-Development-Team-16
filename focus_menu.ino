// Productivity Buddy
// Display : Hosyond 4.0" 480x320 TFT ST7796S  (landscape)
// Board   : Arduino Mega2560
//
// ── Display wiring ───────────────────────────────────────────
//   CS -> A5  |  DC -> A3  |  RST -> A4
//   LED-> A0  |  MOSI->51  |  MISO->50  |  SCK->52
//
// ── Rotary encoder (KY-040) wiring ───────────────────────────
//   CLK -> Pin 2   (interrupt pin INT0)
//   DT  -> Pin 3
//   SW  -> Pin 4   (push-button, INPUT_PULLUP)
//   VCC -> 5V
//   GND -> GND
//
// ── PAM8403 audio amplifier wiring ───────────────────────────
//   PAM8403 VDD  -> 5V      PAM8403 GND  -> GND
//   PAM8403 SDNN -> 5V      (keeps amp enabled)
//   PAM8403 LINP -> Arduino Pin 8 (AUDIO_PIN) via 10 kΩ resistor
//   PAM8403 LINN -> GND
//   PAM8403 LOUTP / LOUTN -> Speaker + / Speaker -
//   Right channel (RINP/RINN/ROUTP/ROUTN) can be left unconnected.
//   Add a 1 µF capacitor between PAM8403 BYPASS pin and GND.

#include <LCDWIKI_GUI.h>
#include <LCDWIKI_SPI.h>
#include <math.h>

#define MODEL  ST7796S
#define CS     A5
#define CD     A3
#define RST    A4
#define LED    A0
LCDWIKI_SPI mylcd(MODEL, CS, CD, RST, LED);

// ── Input ────────────────────────────────────────────────────────────────────
#define ENC_CLK   2   // rotary encoder CLK (interrupt pin)
#define ENC_DT    3   // rotary encoder DT
#define BTN_SEL   4   // push-button SW (INPUT_PULLUP, active LOW)
#define AUDIO_PIN 8   // tone output to PAM8403 via 10 kΩ resistor

// ── Palette (RGB565) ──────────────────────────────────────────────────────────
#define BLACK    0x0000
#define WHITE    0xFFFF
#define DARK_BG  0x1082
#define ACCENT   0x051D
#define IDLE_C   0x2104
#define CYAN     0x07FF
#define GREEN    0x07E0
#define YELLOW   0xFFE0
#define ORANGE   0xFD20
#define PINK     0xFC18
#define LGRAY    0xAD55

// ── User-adjustable settings ──────────────────────────────────────────────────
int focusMin  = 25;
int breakMin  = 5;
int sessTotal = 4;
int sessDone  = 0;

// ── Screen state ─────────────────────────────────────────────────────────────
enum Scr { HOME, SET_FOCUS, SET_BREAK, SET_SESS, PLACE_PHONE, RUN_FOCUS, RUN_BREAK };
Scr scr     = HOME;
int homeSel = 0;

// ── Input state ───────────────────────────────────────────────────────────────
volatile int encDelta = 0;   // incremented/decremented by ISR
char         _schar   = 0;   // buffered serial char

void encoderISR() {
  if (digitalRead(ENC_DT) == HIGH) encDelta++;
  else                              encDelta--;
}

// ── Non-blocking timer ────────────────────────────────────────────────────────
unsigned long tStart    = 0;
unsigned long tDurMs    = 0;
int           prevSec   = -1;
int           prevFillW = 0;
char          prevTimeStr[8] = "";
int           prevAdjVal = -1;

// ═════════════════════════════════════════════════════════════════════════════
//  Low-level drawing helpers
// ═════════════════════════════════════════════════════════════════════════════

void setCol(uint16_t c) {
  mylcd.Set_Draw_color(c >> 11, (c >> 5) & 0x3F, c & 0x1F);
}

void fillRR(int x, int y, int w, int h, int r, uint16_t c) {
  setCol(c);
  mylcd.Fill_Rectangle(x+r, y,     x+w-r, y+h);
  mylcd.Fill_Rectangle(x,   y+r,   x+r,   y+h-r);
  mylcd.Fill_Rectangle(x+w-r, y+r, x+w,   y+h-r);
  mylcd.Fill_Circle(x+r,   y+r,   r);
  mylcd.Fill_Circle(x+w-r, y+r,   r);
  mylcd.Fill_Circle(x+r,   y+h-r, r);
  mylcd.Fill_Circle(x+w-r, y+h-r, r);
}

void ringRR(int x, int y, int w, int h, int r, uint16_t c) {
  setCol(c);
  mylcd.Draw_Round_Rectangle(x, y, x+w, y+h, r);
}

void txtAt(const char* s, int x, int y, int sz, uint16_t fc, uint16_t bc) {
  mylcd.Set_Text_Mode(0);
  mylcd.Set_Text_Back_colour(bc);
  mylcd.Set_Text_colour(fc);
  mylcd.Set_Text_Size(sz);
  mylcd.Print_String((char*)s, x, y);
}

void txtC(const char* s, int y, int sz, uint16_t fc, uint16_t bc) {
  int x = (480 - (int)strlen(s) * 6 * sz) / 2;
  if (x < 0) x = 0;
  txtAt(s, x, y, sz, fc, bc);
}

void txtCT(const char* s, int y, int sz, uint16_t fc) {
  int x = (480 - (int)strlen(s) * 6 * sz) / 2;
  if (x < 0) x = 0;
  mylcd.Set_Text_Mode(1);
  mylcd.Set_Text_colour(fc);
  mylcd.Set_Text_Size(sz);
  mylcd.Print_String((char*)s, x, y);
  mylcd.Set_Text_Mode(0);
}

void hline(int y, uint16_t c) {
  setCol(c);
  mylcd.Draw_Fast_HLine(40, y, 400);
}

void screenWipe(uint16_t c) {
  setCol(c);
  for (int y = 0; y < 320; y += 32)
    mylcd.Fill_Rectangle(0, y, 480, y + 32);
}

void arrowUp(int cx, int tipY, int sz, uint16_t c) {
  setCol(c);
  for (int i = 0; i <= sz; i++)
    mylcd.Draw_Fast_HLine(cx - i, tipY + i, 2*i + 1);
}

void arrowDn(int cx, int tipY, int sz, uint16_t c) {
  setCol(c);
  for (int i = 0; i <= sz; i++)
    mylcd.Draw_Fast_HLine(cx - i, tipY - i, 2*i + 1);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Input  (potentiometer + push-button + Serial Monitor fallback: u/d/s)
// ═════════════════════════════════════════════════════════════════════════════

void drainSerial() {
  while (Serial.available()) {
    char c = (char)Serial.peek();
    if (c == 'u' || c == 'd' || c == 's') break;
    Serial.read();
  }
  if (Serial.available() && _schar == 0)
    _schar = (char)Serial.read();
}

bool isUp() {
  drainSerial();
  if (_schar == 'u') { _schar = 0; return true; }
  noInterrupts();
  bool v = (encDelta > 0);
  if (v) encDelta = 0;
  interrupts();
  return v;
}

bool isDn() {
  drainSerial();
  if (_schar == 'd') { _schar = 0; return true; }
  noInterrupts();
  bool v = (encDelta < 0);
  if (v) encDelta = 0;
  interrupts();
  return v;
}

bool isSel() {
  drainSerial();
  if (_schar == 's') { _schar = 0; return true; }
  if (digitalRead(BTN_SEL) == LOW) {
    delay(40);
    if (digitalRead(BTN_SEL) == LOW) {
      while (digitalRead(BTN_SEL) == LOW);
      delay(20);
      return true;
    }
  }
  return false;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Audio signals  (PAM8403 via AUDIO_PIN)
// ═════════════════════════════════════════════════════════════════════════════

// 1 = very quiet, 50 = full volume
#define VOLUME 10

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

void beepFocusDone() {
  softTone(880,  200); delay(50);
  softTone(1047, 200); delay(50);
  softTone(1319, 400);
}

void beepBreakDone() {
  softTone(660, 150); delay(50);
  softTone(660, 150);
}

void beepAllDone() {
  softTone(523,  150); delay(50);
  softTone(659,  150); delay(50);
  softTone(784,  150); delay(50);
  softTone(1047, 500);
}

// ═════════════════════════════════════════════════════════════════════════════
//  HOME screen
// ═════════════════════════════════════════════════════════════════════════════

void drawHomeItem(int y, const char* label, bool sel) {
  fillRR(70, y, 340, 68, 8, IDLE_C);
  if (sel) {
    setCol(CYAN);
    mylcd.Fill_Rectangle(70, y + 9, 77, y + 59);
  }
  txtCT(label, y + 26, 2, sel ? WHITE : LGRAY);
}

void drawHome() {
  screenWipe(DARK_BG);
  txtCT("Productivity Buddy", 16, 3, WHITE);
  txtCT("focus  *  breathe  *  achieve", 50, 1, LGRAY);
  hline(66, CYAN);
  drawHomeItem(88,  "Start Focus Session", homeSel == 0);
  drawHomeItem(182, "Set Times",           homeSel == 1);
  txtCT("UP/DN: navigate    SEL: choose", 300, 1, LGRAY);
}

// ═════════════════════════════════════════════════════════════════════════════
//  "Place your phone on holder" screen
// ═════════════════════════════════════════════════════════════════════════════

void drawPlacePhone() {
  screenWipe(DARK_BG);
  txtCT("Productivity Buddy", 16, 3, WHITE);
  hline(46, CYAN);

  setCol(CYAN);
  mylcd.Draw_Rectangle(202, 68, 278, 168);
  mylcd.Draw_Rectangle(203, 69, 277, 167);
  mylcd.Fill_Circle(240, 158, 4);

  txtCT("Place your phone", 182, 2, WHITE);
  txtCT("on the holder", 206, 2, WHITE);
  txtCT("Press SEL to start timer", 270, 1, LGRAY);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Adjuster screens
// ═════════════════════════════════════════════════════════════════════════════

void drawAdjScreen(const char* title, int val, const char* unit, uint16_t ac) {
  screenWipe(BLACK);
  txtC(title, 15, 3, ac, BLACK);
  hline(57, ac);
  arrowUp(240, 78, 22, ac);

  char buf[8];
  sprintf(buf, "%d", val);
  int tw = strlen(buf) * 36;
  txtAt(buf, (480 - tw) / 2, 110, 6, WHITE, BLACK);

  txtC(unit, 164, 2, LGRAY, BLACK);
  prevAdjVal = val;

  arrowDn(240, 208, 22, ac);

  fillRR(140, 252, 200, 50, 8, ACCENT);
  ringRR(140, 252, 200, 50, 8, ac);
  txtCT("Set", 265, 3, WHITE);
  txtCT("UP/DN: adjust    SEL: set", 307, 1, LGRAY);
}

void redrawAdjVal(int val, const char* unit) {
  char buf[8];
  if (prevAdjVal >= 0) {
    sprintf(buf, "%d", prevAdjVal);
    int otw = strlen(buf) * 36;
    txtAt(buf, (480 - otw) / 2, 110, 6, BLACK, BLACK);
  }
  prevAdjVal = val;
  sprintf(buf, "%d", val);
  int tw = strlen(buf) * 36;
  txtAt(buf, (480 - tw) / 2, 110, 6, WHITE, BLACK);
}

void runSetFocus() {
  drawAdjScreen("Focus Time", focusMin, "minutes", CYAN);
  for (;;) {
    if (isUp()) { if (focusMin < 120) { focusMin += 5; redrawAdjVal(focusMin, "minutes"); } }
    if (isDn()) { if (focusMin > 5)   { focusMin -= 5; redrawAdjVal(focusMin, "minutes"); } }
    if (isSel()) return;
  }
}

void runSetBreak() {
  drawAdjScreen("Break Time", breakMin, "minutes", ORANGE);
  for (;;) {
    if (isUp()) { if (breakMin < 30) { breakMin += 1; redrawAdjVal(breakMin, "minutes"); } }
    if (isDn()) { if (breakMin > 1)  { breakMin -= 1; redrawAdjVal(breakMin, "minutes"); } }
    if (isSel()) return;
  }
}

void runSetSess() {
  drawAdjScreen("Sessions", sessTotal, "sessions", GREEN);
  for (;;) {
    if (isUp()) { if (sessTotal < 10) { sessTotal += 1; redrawAdjVal(sessTotal, "sessions"); } }
    if (isDn()) { if (sessTotal > 1)  { sessTotal -= 1; redrawAdjVal(sessTotal, "sessions"); } }
    if (isSel()) return;
  }
}

void runSetTimes() {
  runSetFocus();
  runSetBreak();
  runSetSess();
  scr = HOME;
  drawHome();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Timer helpers
// ═════════════════════════════════════════════════════════════════════════════

void startTimer(int minutes) {
  tDurMs    = (unsigned long)minutes * 60000UL;
  prevSec   = -1;
  prevFillW = 0;
  prevTimeStr[0] = '\0';
}

void launchTimer() { tStart = millis(); }

void updateDigits(int remSec, uint16_t c) {
  char buf[8];
  sprintf(buf, "%02d:%02d", remSec / 60, remSec % 60);
  const int x0 = 150, y0 = 108, cw = 36;
  for (int i = 0; i < 5; i++) {
    if (buf[i] != prevTimeStr[i]) {
      char ch[2] = { buf[i], '\0' };
      txtAt(ch, x0 + i * cw, y0, 6, c, BLACK);
    }
  }
  memcpy(prevTimeStr, buf, 6);
}

void updateBar(unsigned long elapsed) {
  const int bx = 50, by = 176, bw = 380, bh = 18;
  if (elapsed == 0) {
    fillRR(bx, by, bw, bh, 6, IDLE_C);
    prevFillW = 0;
    txtAt("  0%", 228, 202, 1, LGRAY, BLACK);
    return;
  }
  int newFillW = (elapsed >= tDurMs) ? bw : (int)((long)bw * elapsed / tDurMs);
  if (newFillW > prevFillW) {
    setCol(GREEN);
    mylcd.Fill_Rectangle(bx + prevFillW, by, bx + newFillW, by + bh);
    prevFillW = newFillW;
  }
  int pct = (elapsed >= tDurMs) ? 100 : (int)(100L * newFillW / bw);
  char buf[8];
  sprintf(buf, "%3d%%", pct);
  txtAt(buf, 228, 202, 1, LGRAY, BLACK);
}

void drawFocusScreen() {
  screenWipe(BLACK);
  char hdr[32];
  sprintf(hdr, "Session %d / %d", sessDone + 1, sessTotal);
  txtC(hdr, 12, 2, CYAN, BLACK);
  hline(36, CYAN);
  txtCT("Stay focused - you got this!", 282, 1, LGRAY);
  updateDigits(focusMin * 60, WHITE);
  updateBar(0);
}

void drawBreakScreen() {
  screenWipe(BLACK);
  char hdr[32];
  sprintf(hdr, "Break  %d / %d", sessDone, sessTotal);
  txtC(hdr, 12, 2, ORANGE, BLACK);
  hline(36, ORANGE);
  txtCT("Relax and recharge :)", 282, 1, LGRAY);
  updateDigits(breakMin * 60, ORANGE);
  updateBar(0);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Animations
// ═════════════════════════════════════════════════════════════════════════════

void animSessionDone() {
  mylcd.Fill_Screen(BLACK);
  for (int r = 8; r <= 55; r += 7) {
    setCol(YELLOW);
    mylcd.Fill_Circle(240, 110, r);
    delay(45);
  }
  setCol(BLACK);
  mylcd.Fill_Circle(220, 94, 7);
  mylcd.Fill_Circle(260, 94, 7);
  for (int dx = -26; dx <= 26; dx += 2) {
    int dy = (dx * dx) / 28;
    mylcd.Fill_Circle(240 + dx, 135 - dy, 2);
  }
  txtCT("Well done!", 190, 4, GREEN);
  char buf[32];
  sprintf(buf, "Session %d complete!", sessDone);
  txtCT(buf, 230, 2, LGRAY);
  delay(3000);
}

void animAllDone() {
  uint16_t cols[] = { YELLOW, ORANGE, CYAN, GREEN, PINK };
  for (int i = 0; i < 8; i++) {
    mylcd.Fill_Screen(BLACK);
    txtCT("Congratulations!", 85, 3, cols[i % 5]);
    delay(220);
  }
  mylcd.Fill_Screen(DARK_BG);
  const uint16_t dc[] = { YELLOW, ORANGE, CYAN, GREEN, PINK };
  const int dx[] = {  55, 115, 175, 240, 305, 365, 425,  85, 200, 330 };
  const int dy[] = {  35,  75,  45,  30,  50,  70,  35, 110,  95, 100 };
  const int dr[] = {   7,   5,   9,   6,   8,   5,   7,   6,   9,   5 };
  for (int i = 0; i < 10; i++) {
    setCol(dc[i % 5]);
    mylcd.Fill_Circle(dx[i], dy[i], dr[i]);
    delay(40);
  }
  txtCT("Congratulations!", 148, 3, YELLOW);
  char buf[40];
  sprintf(buf, "All %d sessions complete!", sessTotal);
  txtCT(buf, 198, 2, WHITE);
  txtCT("You are amazing - great work!", 226, 2, LGRAY);
  delay(5000);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Timer ticks
// ═════════════════════════════════════════════════════════════════════════════

void tickFocus() {
  unsigned long elapsed = millis() - tStart;
  if (elapsed >= tDurMs) {
    updateDigits(0, GREEN);
    updateBar(tDurMs);
    delay(400);
    beepFocusDone();
    sessDone++;
    animSessionDone();
    if (sessDone >= sessTotal) {
      beepAllDone();
      animAllDone();
      scr      = HOME;
      sessDone = 0;
      drawHome();
    } else {
      scr = RUN_BREAK;
      startTimer(breakMin);
      drawBreakScreen();
      launchTimer();
    }
    return;
  }
  int remSec = (int)((tDurMs - elapsed) / 1000UL);
  if (remSec != prevSec) {
    prevSec = remSec;
    updateDigits(remSec, WHITE);
    updateBar(elapsed);
  }
}

void tickBreak() {
  unsigned long elapsed = millis() - tStart;
  if (elapsed >= tDurMs) {
    updateDigits(0, GREEN);
    updateBar(tDurMs);
    delay(400);
    beepBreakDone();
    screenWipe(BLACK);
    txtCT("Break over!", 105, 3, ORANGE);
    txtCT("Get ready to focus!", 158, 2, LGRAY);
    delay(2200);
    scr = RUN_FOCUS;
    startTimer(focusMin);
    drawFocusScreen();
    launchTimer();
    return;
  }
  int remSec = (int)((tDurMs - elapsed) / 1000UL);
  if (remSec != prevSec) {
    prevSec = remSec;
    updateDigits(remSec, ORANGE);
    updateBar(elapsed);
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  setup / loop
// ═════════════════════════════════════════════════════════════════════════════

void setup() {
  pinMode(ENC_CLK,   INPUT_PULLUP);
  pinMode(ENC_DT,    INPUT_PULLUP);
  pinMode(BTN_SEL,   INPUT_PULLUP);
  pinMode(AUDIO_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), encoderISR, FALLING);

  Serial.begin(9600);
  mylcd.Init_LCD();
  mylcd.Set_Rotation(1);
  drawHome();
}

void loop() {
  bool up  = isUp();
  bool dn  = isDn();
  bool sel = isSel();

  switch (scr) {

    case HOME:
      if (up || dn) {
        homeSel ^= 1;
        for (int i = 0; i < 2; i++) {
          int y  = (i == 0) ? 88 : 182;
          bool s = (homeSel == i);
          setCol(s ? CYAN : IDLE_C);
          mylcd.Fill_Rectangle(70, y + 9, 77, y + 59);
          const char* lbl = (i == 0) ? "Start Focus Session" : "Set Times";
          setCol(IDLE_C);
          mylcd.Fill_Rectangle(80, y + 24, 420, y + 44);
          txtCT(lbl, y + 26, 2, s ? WHITE : LGRAY);
        }
      }
      if (sel) {
        if (homeSel == 0) {
          sessDone = 0;
          scr = PLACE_PHONE;
          drawPlacePhone();
        } else {
          runSetTimes();
        }
      }
      break;

    case PLACE_PHONE:
      if (sel) {
        scr = RUN_FOCUS;
        startTimer(focusMin);
        drawFocusScreen();
        launchTimer();
      }
      break;

    case RUN_FOCUS:
      tickFocus();
      break;

    case RUN_BREAK:
      tickBreak();
      break;

    default:
      break;
  }
}
