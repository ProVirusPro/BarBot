/*
 * ══════════════════════════════════════════════════════════════
 *  BARBOT — Cocktail Mixer System
 *  Arduino Mega 2560 · ILI9341 2.8" 240×320 TFT (portrait)
 *  6× Kamoer CKP-DC-S08 peristaltic pumps (12 V, ~90 ml/min)
 *  2× moduli relè 6-ch  (pompe + LED)
 * ══════════════════════════════════════════════════════════════
 *
 *  Librerie necessarie (Arduino Library Manager):
 *    - Adafruit GFX Library
 *    - Adafruit ILI9341
 *    - Adafruit BusIO
 *    - XPT2046_Touchscreen  (Paul Stoffregen)
 *
 *  Se il display NON ha il touch XPT2046 (modulo solo 7 pin),
 *  imposta USE_TOUCH a 0: verranno usati 3 pulsanti fisici.
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <EEPROM.h>

// ─── Scegli modalità input ───────────────────────────────────
#define USE_TOUCH 1            // 1 = XPT2046 touch, 0 = pulsanti fisici

#if USE_TOUCH
  #include <XPT2046_Touchscreen.h>
#endif

// ═════════════════════════════════════════════════════════════
//  PIN DEFINITIONS
// ═════════════════════════════════════════════════════════════

// Display ILI9341  (HW-SPI: MOSI=51, SCK=52)
#define TFT_CS    53
#define TFT_DC     9
#define TFT_RST    8
#define TFT_BL     7          // backlight (HIGH = on)

// Touch XPT2046
#define TOUCH_CS   6
#define TOUCH_IRQ  5

// Pulsanti fisici (se USE_TOUCH == 0)
#define BTN_UP     2
#define BTN_DOWN   3
#define BTN_OK     4

// Relè Pompe (active LOW)
const uint8_t PIN_PUMP[6] = {22, 23, 24, 25, 26, 27};

// Relè LED (active LOW)
const uint8_t PIN_LED[6]  = {30, 31, 32, 33, 34, 35};

// ═════════════════════════════════════════════════════════════
//  OGGETTI DISPLAY / TOUCH
// ═════════════════════════════════════════════════════════════

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

#if USE_TOUCH
  XPT2046_Touchscreen ts(TOUCH_CS);  // senza IRQ, usa polling
  // Calibrazione touch (valori dal pannello reale)
  #define TX_MIN 550
  #define TX_MAX 3700
  #define TY_MIN 530
  #define TY_MAX 3750
#endif

// ═════════════════════════════════════════════════════════════
//  PALETTE COLORI (RGB-565)
// ═════════════════════════════════════════════════════════════

#define C_BG      0x0841   // sfondo scuro
#define C_HDR     0xFD20   // arancio (header)
#define C_BTN     0x2104   // bottone scuro
#define C_BTN_HL  0x3186   // bottone evidenziato
#define C_TXT     0xFFFF   // testo bianco
#define C_DIM     0x8410   // testo grigio
#define C_ACC     0x2D7F   // accento teal
#define C_GRN     0x07E0
#define C_RED     0xF800
#define C_BAR     0xFD20   // barra progresso

// ─── PORTRAIT 240×320 ───────────────────────────────────────
#define SW 240
#define SH 320

// ═════════════════════════════════════════════════════════════
//  EEPROM
// ═════════════════════════════════════════════════════════════

#define EE_MAGIC      0
#define EE_BOTTLES    1          // 6 byte
#define EE_CALIB     10          // 6×4 byte (float)
#define EE_MAGIC_VAL  0xBC

// ═════════════════════════════════════════════════════════════
//  COSTANTI
// ═════════════════════════════════════════════════════════════

#define NUM_POS 6
#define DEFAULT_ML_S 1.5f       // 90 ml/min @12 V

#define TUBE_ID_MM       2.5f   // diametro interno tubo silicone (mm)
#define TUBE_BOTTLE_CM  50      // lunghezza bottiglia → pompa (cm)
#define TUBE_OUTPUT_CM  40      // lunghezza pompa → uscita (cm)

// ═════════════════════════════════════════════════════════════
//  DATABASE BOTTIGLIE (alfabetico, PROGMEM)
// ═════════════════════════════════════════════════════════════

const char bAQ[] PROGMEM = "Acqua";
const char b00[] PROGMEM = "Amaretto";
const char b01[] PROGMEM = "Aperol";
const char b02[] PROGMEM = "Bourbon";
const char b03[] PROGMEM = "Cachaca";
const char b04[] PROGMEM = "Campari";
const char b05[] PROGMEM = "Cointreau";
const char b06[] PROGMEM = "Gin";
const char b07[] PROGMEM = "Grand Marnier";
const char b08[] PROGMEM = "Kahlua";
const char b09[] PROGMEM = "Limoncello";
const char b10[] PROGMEM = "Maraschino";
const char b11[] PROGMEM = "Mezcal";
const char b12[] PROGMEM = "Prosecco";
const char b13[] PROGMEM = "Rum Bianco";
const char b14[] PROGMEM = "Rum Scuro";
const char b15[] PROGMEM = "Sambuca";
const char b16[] PROGMEM = "Scir. Zucchero";
const char b17[] PROGMEM = "Scotch Whisky";
const char b18[] PROGMEM = "Succo Lime";
const char b19[] PROGMEM = "Succo Limone";
const char b20[] PROGMEM = "Tequila";
const char b21[] PROGMEM = "Triple Sec";
const char b22[] PROGMEM = "Vermouth Dry";
const char b23[] PROGMEM = "Vermouth Rosso";
const char b24[] PROGMEM = "Vodka";
const char b25[] PROGMEM = "Whiskey";

#define NUM_BOTTLES 27

const char* const BNAME[NUM_BOTTLES] PROGMEM = {
  bAQ,b00,b01,b02,b03,b04,b05,b06,b07,b08,b09,
  b10,b11,b12,b13,b14,b15,b16,b17,b18,b19,
  b20,b21,b22,b23,b24,b25
};

void bname(uint8_t i, char* buf, uint8_t mx = 18) {
  if (i >= NUM_BOTTLES) { strncpy(buf, "Vuoto", mx); buf[mx-1]=0; return; }
  strncpy_P(buf, (PGM_P)pgm_read_ptr(&BNAME[i]), mx);
  buf[mx-1] = 0;
}

// ═════════════════════════════════════════════════════════════
//  DATABASE COCKTAIL
// ═════════════════════════════════════════════════════════════

// Indici bottiglia:
//  0 Acqua        1 Amaretto     2 Aperol       3 Bourbon
//  4 Cachaca      5 Campari      6 Cointreau    7 Gin
//  8 Grand Marnier 9 Kahlua    10 Limoncello  11 Maraschino
// 12 Mezcal      13 Prosecco    14 Rum Bianco  15 Rum Scuro
// 16 Sambuca     17 Scir.Zucch. 18 Scotch      19 Succo Lime
// 20 Succo Limone 21 Tequila   22 Triple Sec  23 Vermouth D
// 24 Vermouth R  25 Vodka       26 Whiskey

struct Ingr { uint8_t bot; uint8_t ml; };

struct Drink {
  const char* nm;       // PROGMEM
  uint8_t n;
  Ingr ing[6];
};

const char d00[] PROGMEM = "Americano";
const char d01[] PROGMEM = "Boulevardier";
const char d02[] PROGMEM = "Caipirinha";
const char d03[] PROGMEM = "Cosmopolitan";
const char d04[] PROGMEM = "Daiquiri";
const char d05[] PROGMEM = "Espresso Martini";
const char d06[] PROGMEM = "Gimlet";
const char d07[] PROGMEM = "Long Island";
const char d08[] PROGMEM = "Manhattan";
const char d09[] PROGMEM = "Margarita";
const char d10[] PROGMEM = "Martini Dry";
const char d11[] PROGMEM = "Mezcal Sour";
const char d12[] PROGMEM = "Mojito";
const char d13[] PROGMEM = "Negroni";
const char d14[] PROGMEM = "Old Fashioned";
const char d15[] PROGMEM = "Sidecar";
const char d16[] PROGMEM = "Spritz";
const char d17[] PROGMEM = "Whiskey Sour";

#define NUM_DRINKS 18

const Drink DRINKS[NUM_DRINKS] = {
  {d00,2, {{5,30},{24,30}}},                          // Americano
  {d01,3, {{3,30},{5,30},{24,30}}},                   // Boulevardier
  {d02,3, {{4,60},{19,30},{17,15}}},                  // Caipirinha
  {d03,3, {{25,40},{6,15},{19,15}}},                  // Cosmopolitan
  {d04,3, {{14,60},{19,30},{17,15}}},                 // Daiquiri
  {d05,3, {{25,40},{9,30},{17,10}}},                  // Espresso Martini
  {d06,3, {{7,60},{19,30},{17,15}}},                  // Gimlet
  {d07,5, {{25,15},{7,15},{14,15},{21,15},{22,15}}},  // Long Island
  {d08,2, {{3,50},{24,20}}},                          // Manhattan
  {d09,3, {{21,50},{6,20},{19,15}}},                  // Margarita
  {d10,2, {{7,60},{23,10}}},                          // Martini Dry
  {d11,3, {{12,60},{19,30},{17,15}}},                 // Mezcal Sour
  {d12,3, {{14,45},{19,20},{17,15}}},                 // Mojito
  {d13,3, {{7,30},{5,30},{24,30}}},                   // Negroni
  {d14,2, {{3,60},{17,10}}},                          // Old Fashioned
  {d15,3, {{3,40},{6,20},{20,20}}},                   // Sidecar
  {d16,2, {{2,60},{13,90}}},                          // Spritz
  {d17,3, {{3,50},{20,25},{17,15}}},                  // Whiskey Sour
};

void dname(uint8_t i, char* buf, uint8_t mx = 18) {
  if (i >= NUM_DRINKS) { buf[0]=0; return; }
  strncpy_P(buf, DRINKS[i].nm, mx);
  buf[mx-1] = 0;
}

// ═════════════════════════════════════════════════════════════
//  STATO RUNTIME
// ═════════════════════════════════════════════════════════════

uint8_t  bottles[NUM_POS];
float    cal[NUM_POS];
uint8_t  avail[NUM_DRINKS];
uint8_t  nAvail = 0;

int  scroll    = 0;
int  selPos    = -1;
int  selDrink  = -1;
int  calPump   = -1;

#if !USE_TOUCH
  int  cursor    = 0;
  int  maxCursor = 2;
#endif

enum Scr {
  S_MAIN, S_SETUP, S_SELECT,
  S_DRINKS, S_DETAIL, S_DISPENSING,
  S_CALIB, S_CALIB_P
};
Scr scr = S_MAIN;

unsigned long lastInput = 0;
#define DEBOUNCE 300

// ═════════════════════════════════════════════════════════════
//  EEPROM LOAD / SAVE
// ═════════════════════════════════════════════════════════════

void loadEE() {
  if (EEPROM.read(EE_MAGIC) != EE_MAGIC_VAL) {
    for (uint8_t i = 0; i < NUM_POS; i++) {
      bottles[i] = 0xFF;
      cal[i] = DEFAULT_ML_S;
    }
    saveEE();
    return;
  }
  for (uint8_t i = 0; i < NUM_POS; i++) {
    bottles[i] = EEPROM.read(EE_BOTTLES + i);
    EEPROM.get(EE_CALIB + i * 4, cal[i]);
    if (cal[i] <= 0 || cal[i] > 10.0f) cal[i] = DEFAULT_ML_S;
  }
}

void saveEE() {
  EEPROM.update(EE_MAGIC, EE_MAGIC_VAL);
  for (uint8_t i = 0; i < NUM_POS; i++) {
    EEPROM.update(EE_BOTTLES + i, bottles[i]);
    EEPROM.put(EE_CALIB + i * 4, cal[i]);
  }
}

// ═════════════════════════════════════════════════════════════
//  LED
// ═════════════════════════════════════════════════════════════

void ledOn(uint8_t i)  { digitalWrite(PIN_LED[i], LOW);  }
void ledOff(uint8_t i) { digitalWrite(PIN_LED[i], HIGH); }

void ledsBase() {
  for (uint8_t i = 0; i < NUM_POS; i++)
    (bottles[i] != 0xFF) ? ledOn(i) : ledOff(i);
}

void ledsAllOff() {
  for (uint8_t i = 0; i < NUM_POS; i++) ledOff(i);
}

void ledChase(uint8_t cycles) {
  for (uint8_t c = 0; c < cycles; c++) {
    for (int8_t i = 0; i < (int8_t)NUM_POS; i++)
      { ledsAllOff(); ledOn(i); delay(80); }
    for (int8_t i = NUM_POS - 2; i >= 0; i--)
      { ledsAllOff(); ledOn(i); delay(80); }
  }
  ledsBase();
}

// ═════════════════════════════════════════════════════════════
//  POMPE
// ═════════════════════════════════════════════════════════════

void pumpOn(uint8_t i)  { digitalWrite(PIN_PUMP[i], LOW);  }
void pumpOff(uint8_t i) { digitalWrite(PIN_PUMP[i], HIGH); }
void pumpsOff()          { for (uint8_t i = 0; i < NUM_POS; i++) pumpOff(i); }

int8_t pumpFor(uint8_t bottleIdx) {
  for (uint8_t i = 0; i < NUM_POS; i++)
    if (bottles[i] == bottleIdx) return (int8_t)i;
  return -1;
}

// ═════════════════════════════════════════════════════════════
//  FILTRO DRINK DISPONIBILI
// ═════════════════════════════════════════════════════════════

void filterDrinks() {
  nAvail = 0;
  for (uint8_t d = 0; d < NUM_DRINKS; d++) {
    bool ok = true;
    for (uint8_t j = 0; j < DRINKS[d].n; j++) {
      if (pumpFor(DRINKS[d].ing[j].bot) < 0) { ok = false; break; }
    }
    if (ok) avail[nAvail++] = d;
  }
}

// ═════════════════════════════════════════════════════════════
//  INPUT
// ═════════════════════════════════════════════════════════════

bool getTouch(int &x, int &y) {
#if USE_TOUCH
  if (!ts.touched()) return false;
  if (millis() - lastInput < DEBOUNCE) return false;
  lastInput = millis();
  TS_Point p = ts.getPoint();
  x = map(p.x, TX_MAX, TX_MIN, 0, SW);   // X invertito
  y = map(p.y, TY_MAX, TY_MIN, 0, SH);   // Y invertito
  x = constrain(x, 0, SW - 1);
  y = constrain(y, 0, SH - 1);
  return true;
#else
  return false;
#endif
}

bool hit(int tx, int ty, int rx, int ry, int rw, int rh) {
  return tx >= rx && tx < rx + rw && ty >= ry && ty < ry + rh;
}

#if !USE_TOUCH
enum BtnEvt { BT_NONE, BT_UP, BT_DOWN, BT_OK };

BtnEvt readBtn() {
  if (millis() - lastInput < DEBOUNCE) return BT_NONE;
  if (digitalRead(BTN_UP) == LOW)   { lastInput = millis(); return BT_UP;   }
  if (digitalRead(BTN_DOWN) == LOW) { lastInput = millis(); return BT_DOWN; }
  if (digitalRead(BTN_OK) == LOW)   { lastInput = millis(); return BT_OK;   }
  return BT_NONE;
}
#endif

// ═════════════════════════════════════════════════════════════
//  DRAW HELPERS  (portrait 240×320)
// ═════════════════════════════════════════════════════════════

void drawBtn(int x, int y, int w, int h,
             const char* lbl, uint16_t bg = C_BTN, bool sel = false)
{
  uint16_t border = sel ? C_GRN : C_ACC;
  tft.fillRoundRect(x, y, w, h, 6, bg);
  tft.drawRoundRect(x, y, w, h, 6, border);
  if (sel) tft.drawRoundRect(x+1, y+1, w-2, h-2, 5, border);
  int16_t bx, by; uint16_t tw, th;
  tft.setTextSize(2);
  tft.getTextBounds(lbl, 0, 0, &bx, &by, &tw, &th);
  tft.setCursor(x + (w - tw) / 2, y + (h - th) / 2);
  tft.setTextColor(C_TXT);
  tft.print(lbl);
}

void drawSm(int x, int y, int w, int h,
            const char* lbl, uint16_t bg = C_BTN, bool sel = false)
{
  uint16_t border = sel ? C_GRN : C_ACC;
  tft.fillRoundRect(x, y, w, h, 4, bg);
  tft.drawRoundRect(x, y, w, h, 4, border);
  if (sel) tft.drawRoundRect(x+1,y+1,w-2,h-2,3,border);
  tft.setTextSize(1);
  int16_t bx, by; uint16_t tw, th;
  tft.getTextBounds(lbl, 0, 0, &bx, &by, &tw, &th);
  tft.setCursor(x + (w - tw) / 2, y + (h - th) / 2);
  tft.setTextColor(C_TXT);
  tft.print(lbl);
}

void hdr(const char* t) {
  tft.fillRect(0, 0, SW, 32, C_HDR);
  tft.setTextSize(2); tft.setTextColor(C_BG);
  tft.setCursor(8, 8); tft.print(t);
}

void backBtn() { drawSm(196, 4, 40, 24, "<-", C_BG); }

void toast(const char* msg) {
  tft.setTextSize(1); tft.setTextColor(C_GRN);
  int16_t bx, by; uint16_t tw, th;
  tft.getTextBounds(msg, 0, 0, &bx, &by, &tw, &th);
  int x = (SW - tw) / 2;
  tft.fillRect(x - 4, SH / 2 - 4, tw + 8, th + 8, C_BG);
  tft.setCursor(x, SH / 2); tft.print(msg);
  delay(1200);
}

// ═════════════════════════════════════════════════════════════
//  SCHERMATE  (portrait 240×320)
// ═════════════════════════════════════════════════════════════

// ─── Menu Principale ─────────────────────────────────────────
void showMain() {
  scr = S_MAIN;
  tft.fillScreen(C_BG);

  tft.setTextSize(3); tft.setTextColor(C_HDR);
  tft.setCursor(42, 30); tft.print("BARBOT");

  tft.setTextSize(1); tft.setTextColor(C_DIM);
  tft.setCursor(50, 62); tft.print("Cocktail Mixer v1.0");

  #if USE_TOUCH
    drawBtn(20,  95, 200, 50, "Setup Bottiglie");
    drawBtn(20, 160, 200, 50, "Drink");
    drawBtn(20, 225, 200, 50, "Setup Pompe");
  #else
    drawBtn(20,  95, 200, 50, "Setup Bottiglie", C_BTN, cursor==0);
    drawBtn(20, 160, 200, 50, "Drink",           C_BTN, cursor==1);
    drawBtn(20, 225, 200, 50, "Setup Pompe",     C_BTN, cursor==2);
  #endif
}

// ─── Setup Bottiglie ─────────────────────────────────────────
void showSetup() {
  scr = S_SETUP;
  tft.fillScreen(C_BG);
  hdr("SETUP BOTTIGLIE");
  backBtn();

  char buf[18];
  for (uint8_t i = 0; i < NUM_POS; i++) {
    int y = 42 + i * 36;

    tft.setTextSize(1); tft.setTextColor(C_ACC);
    tft.setCursor(5, y + 7);
    tft.print(i + 1); tft.print(':');

    bname(bottles[i], buf);
    uint16_t bg = (bottles[i] != 0xFF) ? C_BTN_HL : C_BTN;
    #if !USE_TOUCH
      bool s = (cursor == i);
    #else
      bool s = false;
    #endif
    drawSm(22, y, 170, 26, buf, bg, s);

    uint16_t lc = (bottles[i] != 0xFF) ? C_GRN : 0x4208;
    tft.fillCircle(216, y + 13, 8, lc);
    tft.drawCircle(216, y + 13, 8, C_ACC);
  }

  #if !USE_TOUCH
    drawSm(60, 268, 120, 28, "SALVA", C_GRN, cursor == NUM_POS);
  #else
    drawSm(60, 268, 120, 28, "SALVA", C_GRN);
  #endif
}

// ─── Selezione Bottiglia ─────────────────────────────────────
#define SEL_VIS 10

void showSelect() {
  scr = S_SELECT;
  tft.fillScreen(C_BG);
  hdr("SELEZIONA");
  backBtn();

  if (scroll == 0) {
    #if !USE_TOUCH
      drawSm(6, 38, 228, 22, "-- Vuoto (rimuovi) --", C_RED, cursor == 0);
    #else
      drawSm(6, 38, 228, 22, "-- Vuoto (rimuovi) --", C_RED);
    #endif
  }

  int y0    = (scroll == 0) ? 64 : 38;
  int slots = (scroll == 0) ? SEL_VIS - 1 : SEL_VIS;

  char buf[18];
  for (int i = 0; i < slots; i++) {
    int idx = scroll + i;
    if (idx >= NUM_BOTTLES) break;
    int y = y0 + i * 26;
    bname(idx, buf);

    #if !USE_TOUCH
      int ci = (scroll == 0) ? i + 1 : i;
      bool s = (cursor == ci);
    #else
      bool s = false;
    #endif
    tft.fillRoundRect(6, y, 228, 22, 3, s ? C_BTN_HL : C_BTN);
    tft.drawRoundRect(6, y, 228, 22, 3, s ? C_GRN : C_ACC);
    tft.setTextSize(1);
    tft.setTextColor(C_TXT);
    tft.setCursor(14, y + 6); tft.print(buf);
  }

  if (scroll > 0)
    tft.fillTriangle(115, 34, 125, 34, 120, 30, C_ACC);
  if (scroll + SEL_VIS < NUM_BOTTLES)
    tft.fillTriangle(115, SH-6, 125, SH-6, 120, SH-2, C_ACC);
}

// ─── Lista Drink ─────────────────────────────────────────────
#define DRK_VIS 9

void showDrinks() {
  scr = S_DRINKS;
  tft.fillScreen(C_BG);
  hdr("DRINK MENU");
  backBtn();

  filterDrinks();

  if (nAvail == 0) {
    tft.setTextSize(1); tft.setTextColor(C_DIM);
    tft.setCursor(15, 100); tft.print("Nessun drink disponibile.");
    tft.setCursor(15, 120); tft.print("Configura prima le bottiglie!");
    return;
  }

  char buf[18];
  for (int i = 0; i < DRK_VIS && scroll + i < (int)nAvail; i++) {
    int y = 40 + i * 30;
    dname(avail[scroll + i], buf);
    #if !USE_TOUCH
      bool s = (cursor == i);
    #else
      bool s = false;
    #endif
    tft.fillRoundRect(6, y, 228, 26, 5, s ? C_BTN_HL : C_BTN);
    tft.drawRoundRect(6, y, 228, 26, 5, s ? C_GRN : C_ACC);
    tft.setTextSize(2); tft.setTextColor(C_TXT);
    tft.setCursor(14, y + 5); tft.print(buf);
  }

  if (scroll > 0)
    tft.fillTriangle(115, 34, 125, 34, 120, 30, C_ACC);
  if (scroll + DRK_VIS < (int)nAvail)
    tft.fillTriangle(115, SH-6, 125, SH-6, 120, SH-2, C_ACC);
}

// ─── Dettaglio Drink ─────────────────────────────────────────
void showDetail(uint8_t d) {
  scr = S_DETAIL;
  tft.fillScreen(C_BG);
  char buf[18]; dname(d, buf);
  hdr(buf);
  backBtn();

  uint16_t tot = 0;
  for (uint8_t i = 0; i < DRINKS[d].n; i++) {
    int y = 50 + i * 28;
    bname(DRINKS[d].ing[i].bot, buf);
    tft.setTextSize(1); tft.setTextColor(C_ACC);
    tft.setCursor(14, y); tft.print(buf);
    tft.setTextColor(C_TXT); tft.setCursor(170, y);
    tft.print(DRINKS[d].ing[i].ml); tft.print(" ml");
    tot += DRINKS[d].ing[i].ml;
  }

  int ly = 56 + DRINKS[d].n * 28;
  tft.drawFastHLine(14, ly, 212, C_DIM);
  tft.setTextColor(C_TXT); tft.setTextSize(1);
  tft.setCursor(14, ly + 10);
  tft.print("Totale: "); tft.print(tot); tft.print(" ml");

  float etime = 0;
  for (uint8_t i = 0; i < DRINKS[d].n; i++) {
    int8_t p = pumpFor(DRINKS[d].ing[i].bot);
    if (p >= 0) {
      float t = DRINKS[d].ing[i].ml / cal[p];
      if (t > etime) etime = t;   // parallelo: tempo = ingrediente più lungo
    }
  }
  tft.setCursor(14, ly + 26); tft.setTextColor(C_DIM);
  tft.print("Tempo stimato: ~");
  tft.print((int)(etime + 0.5f)); tft.print(" sec");

  drawBtn(20, 272, 200, 38, "PREPARA", C_GRN);
}

// ─── Schermata Erogazione ────────────────────────────────────
void showDispensing(uint8_t d) {
  tft.fillScreen(C_BG);
  char buf[18]; dname(d, buf);
  hdr("PREPARAZIONE");
  tft.setTextSize(2); tft.setTextColor(C_HDR);
  tft.setCursor(14, 52); tft.print(buf);
  tft.drawRoundRect(14, 140, 212, 26, 4, C_ACC);
}

void updateBar(uint8_t pct, const char* ingr) {
  int fw = map(pct, 0, 100, 0, 208);
  tft.fillRoundRect(16, 142, fw, 22, 3, C_BAR);

  tft.fillRect(80, 178, 80, 20, C_BG);
  tft.setTextSize(2); tft.setTextColor(C_TXT);
  tft.setCursor(pct < 10 ? 104 : (pct < 100 ? 96 : 84), 178);
  tft.print(pct); tft.print('%');

  tft.fillRect(14, 210, 220, 14, C_BG);
  tft.setTextSize(1); tft.setTextColor(C_DIM);
  tft.setCursor(14, 212); tft.print("Erogando: "); tft.print(ingr);
}

// ─── Calibrazione ────────────────────────────────────────────
void showCalib() {
  scr = S_CALIB;
  tft.fillScreen(C_BG);
  hdr("SETUP POMPE");
  backBtn();

  char buf[18];
  for (uint8_t i = 0; i < NUM_POS; i++) {
    int y = 42 + i * 36;
    bname(bottles[i], buf);
    uint16_t bg = (bottles[i] != 0xFF) ? C_BTN : 0x18C3;
    #if !USE_TOUCH
      bool s = (cursor == i);
    #else
      bool s = false;
    #endif
    tft.fillRoundRect(6, y, 228, 28, 5, s ? C_BTN_HL : bg);
    tft.drawRoundRect(6, y, 228, 28, 5, s ? C_GRN : C_ACC);
    tft.setTextSize(1); tft.setTextColor(C_ACC);
    tft.setCursor(12, y + 5); tft.print('P'); tft.print(i + 1);
    tft.setTextColor(C_TXT); tft.setCursor(36, y + 5); tft.print(buf);
    tft.setTextColor(C_DIM); tft.setCursor(140, y + 16);
    tft.print(cal[i], 2); tft.print(" ml/s");
  }

  #if !USE_TOUCH
    drawSm(6, 260, 112, 28, "Init Pompe", C_ACC, cursor == NUM_POS);
    drawSm(122, 260, 112, 28, "Svuota Tubi", C_ACC, cursor == NUM_POS + 1);
  #else
    drawSm(6, 260, 112, 28, "Init Pompe", C_ACC);
    drawSm(122, 260, 112, 28, "Svuota Tubi", C_ACC);
  #endif
}

void showCalibP(uint8_t p) {
  scr = S_CALIB_P;
  tft.fillScreen(C_BG);
  char t[24]; sprintf(t, "CALIBRA P%d", p + 1);
  hdr(t);
  backBtn();

  tft.setTextSize(1); tft.setTextColor(C_DIM);
  tft.setCursor(8, 44);  tft.print("1. Posiziona un contenitore");
  tft.setCursor(8, 58);  tft.print("2. Premi EROGA per 10 sec");
  tft.setCursor(8, 72);  tft.print("3. Misura i ml erogati");
  tft.setCursor(8, 86);  tft.print("4. Regola il valore con +/-");

  tft.setTextSize(2); tft.setTextColor(C_TXT);
  tft.setCursor(14, 118);
  tft.print("ml/s: "); tft.print(cal[p], 2);

  drawBtn(10,  160, 220, 36, "EROGA 10s", C_ACC);
  drawBtn(10,  210, 105, 36, "+ 0.05");
  drawBtn(125, 210, 105, 36, "- 0.05");
  drawBtn(30,  264, 180, 34, "SALVA", C_GRN);
}

// ═════════════════════════════════════════════════════════════
//  INIT / SVUOTA TUBI
// ═════════════════════════════════════════════════════════════

float tubeVolumeMl(float lengthCm) {
  float r = TUBE_ID_MM / 2.0f;
  return 3.14159f * r * r * lengthCm * 10.0f / 1000.0f;
}

void runTubesProgress(const char* title, bool onlyAssigned, float lengthCm) {
  tft.fillScreen(C_BG);
  hdr(title);

  float vol = tubeVolumeMl(lengthCm);

  unsigned long pMs[NUM_POS];
  bool pAct[NUM_POS];
  unsigned long maxMs = 0;
  uint8_t nPumps = 0;

  for (uint8_t i = 0; i < NUM_POS; i++) {
    if (onlyAssigned && bottles[i] == 0xFF) {
      pAct[i] = false;
      continue;
    }
    pMs[i] = (unsigned long)(vol / cal[i] * 1000.0f);
    pAct[i] = true;
    if (pMs[i] > maxMs) maxMs = pMs[i];
    pumpOn(i);
    ledOn(i);
    nPumps++;
  }

  if (nPumps == 0) {
    tft.setTextSize(1); tft.setTextColor(C_DIM);
    tft.setCursor(15, 100); tft.print("Nessuna pompa da attivare!");
    delay(2000);
    showCalib();
    return;
  }

  tft.setTextSize(1); tft.setTextColor(C_DIM);
  tft.setCursor(14, 44);
  tft.print("Volume tubo: "); tft.print(vol, 1); tft.print(" ml");
  tft.setCursor(14, 58);
  tft.print("Pompe attive: "); tft.print(nPumps);
  tft.setCursor(14, 72);
  tft.print("Tempo: ~"); tft.print((int)(maxMs / 1000 + 1)); tft.print(" sec");

  tft.drawRoundRect(14, 140, 212, 26, 4, C_ACC);

  unsigned long t0 = millis();

  while (millis() - t0 < maxMs) {
    unsigned long elapsed = millis() - t0;

    for (uint8_t i = 0; i < NUM_POS; i++) {
      if (pAct[i] && elapsed >= pMs[i]) {
        pumpOff(i);
        pAct[i] = false;
      }
    }

    uint8_t pct = (uint8_t)((uint32_t)elapsed * 100 / maxMs);
    if (pct > 100) pct = 100;

    int fw = map(pct, 0, 100, 0, 208);
    tft.fillRoundRect(16, 142, fw, 22, 3, C_BAR);

    tft.fillRect(80, 178, 80, 20, C_BG);
    tft.setTextSize(2); tft.setTextColor(C_TXT);
    tft.setCursor(pct < 10 ? 104 : (pct < 100 ? 96 : 84), 178);
    tft.print(pct); tft.print('%');

    delay(150);
  }

  pumpsOff();
  ledsBase();

  tft.fillRect(80, 178, 80, 20, C_BG);
  tft.setTextSize(2); tft.setTextColor(C_GRN);
  tft.setCursor(56, 178); tft.print("FATTO!");
  delay(2000);
  showCalib();
}

void primeTubes() { runTubesProgress("INIT POMPE", true, TUBE_BOTTLE_CM); }
void flushTubes() { runTubesProgress("SVUOTA TUBI", false, TUBE_BOTTLE_CM + TUBE_OUTPUT_CM); }

// ═════════════════════════════════════════════════════════════
//  LOGICA EROGAZIONE
// ═════════════════════════════════════════════════════════════

void dispense(uint8_t d) {
  scr = S_DISPENSING;
  showDispensing(d);

  uint16_t tot = 0;
  for (uint8_t i = 0; i < DRINKS[d].n; i++) tot += DRINKS[d].ing[i].ml;

  // ── Erogazione PARALLELA ──────────────────────────────────
  int8_t        pIdx[6];       // indice pompa fisica per ogni ingrediente
  unsigned long pMs[6];        // durata erogazione per ogni pompa (ms)
  bool          pAct[6];       // pompa ancora attiva?
  unsigned long maxMs = 0;

  ledsAllOff();

  for (uint8_t i = 0; i < DRINKS[d].n; i++) {
    pIdx[i] = pumpFor(DRINKS[d].ing[i].bot);
    if (pIdx[i] < 0) { pAct[i] = false; continue; }

    pMs[i]  = (unsigned long)(DRINKS[d].ing[i].ml / cal[pIdx[i]] * 1000.0f);
    pAct[i] = true;
    if (pMs[i] > maxMs) maxMs = pMs[i];

    pumpOn(pIdx[i]);
    ledOn(pIdx[i]);
  }

  unsigned long t0 = millis();

  while (millis() - t0 < maxMs) {
    unsigned long elapsed = millis() - t0;
    uint16_t dispensed = 0;
    uint8_t  nActive   = 0;
    int8_t   lastAct   = -1;

    for (uint8_t i = 0; i < DRINKS[d].n; i++) {
      if (pIdx[i] < 0) continue;

      // spegni pompa quando ha finito la sua quota
      if (pAct[i] && elapsed >= pMs[i]) {
        pumpOff(pIdx[i]);
        ledOff(pIdx[i]);
        pAct[i] = false;
      }

      if (pAct[i]) {
        float elap = elapsed / 1000.0f;
        uint16_t ml = (uint16_t)(elap * cal[pIdx[i]]);
        if (ml > DRINKS[d].ing[i].ml) ml = DRINKS[d].ing[i].ml;
        dispensed += ml;
        nActive++;
        lastAct = i;
      } else {
        dispensed += DRINKS[d].ing[i].ml;
      }
    }

    if (dispensed > tot) dispensed = tot;
    uint8_t pct = (uint8_t)((uint32_t)dispensed * 100 / tot);

    char sts[22];
    if (nActive > 1) {
      sprintf(sts, "%d pompe attive", nActive);
    } else if (nActive == 1) {
      bname(DRINKS[d].ing[lastAct].bot, sts);
    } else {
      strcpy(sts, "Completato!");
    }

    updateBar(pct, sts);
    delay(150);
  }

  pumpsOff();
  updateBar(100, "Completato!");
  ledChase(3);

  tft.setTextSize(3); tft.setTextColor(C_GRN);
  tft.setCursor(40, 52); tft.print("PRONTO!");

  delay(4000);
  scroll = 0;
  showDrinks();
}

// ═════════════════════════════════════════════════════════════
//  GESTIONE INPUT TOUCH  (portrait)
// ═════════════════════════════════════════════════════════════

#if USE_TOUCH
void handleTouch() {
  int tx, ty;
  if (!getTouch(tx, ty)) return;

  switch (scr) {

  case S_MAIN:
    if (hit(tx,ty,20, 95,200,50)) { scroll=0; showSetup();  return; }
    if (hit(tx,ty,20,160,200,50)) { scroll=0; showDrinks(); return; }
    if (hit(tx,ty,20,225,200,50)) { showCalib();             return; }
    break;

  case S_SETUP:
    if (hit(tx,ty,196,4,40,24)) { showMain(); return; }
    for (uint8_t i = 0; i < NUM_POS; i++) {
      if (hit(tx,ty,22, 42 + i*36, 170, 26)) {
        selPos = i; scroll = 0; showSelect(); return;
      }
    }
    if (hit(tx,ty,60,268,120,28)) {
      saveEE(); ledsBase(); toast("Salvato!"); showSetup();
    }
    break;

  case S_SELECT: {
    if (hit(tx,ty,196,4,40,24)) { showSetup(); return; }

    if (scroll == 0 && hit(tx,ty,6,38,228,22)) {
      bottles[selPos] = 0xFF; ledsBase(); showSetup(); return;
    }

    int y0    = (scroll == 0) ? 64 : 38;
    int slots = (scroll == 0) ? SEL_VIS - 1 : SEL_VIS;

    for (int i = 0; i < slots; i++) {
      int idx = scroll + i;
      if (idx >= NUM_BOTTLES) break;
      if (hit(tx,ty, 6, y0 + i*26, 228, 22)) {
        bottles[selPos] = (uint8_t)idx;
        ledsBase(); showSetup(); return;
      }
    }

    if (ty < 34 && scroll > 0) {
      scroll = max(0, scroll - SEL_VIS); showSelect();
    }
    if (ty > SH - 10 && scroll + SEL_VIS < NUM_BOTTLES) {
      scroll = min((int)NUM_BOTTLES - SEL_VIS, scroll + SEL_VIS);
      showSelect();
    }
    break;
  }

  case S_DRINKS: {
    if (hit(tx,ty,196,4,40,24)) { showMain(); return; }

    for (int i = 0; i < DRK_VIS && scroll + i < (int)nAvail; i++) {
      if (hit(tx,ty,6, 40 + i*30, 228, 26)) {
        selDrink = avail[scroll + i];
        showDetail(selDrink); return;
      }
    }

    if (ty < 34 && scroll > 0) {
      scroll = max(0, scroll - DRK_VIS); showDrinks();
    }
    if (ty > SH - 10 && scroll + DRK_VIS < (int)nAvail) {
      scroll = min((int)nAvail - DRK_VIS, scroll + DRK_VIS); showDrinks();
    }
    break;
  }

  case S_DETAIL:
    if (hit(tx,ty,196,4,40,24)) { scroll=0; showDrinks(); return; }
    if (hit(tx,ty,20,272,200,38)) { dispense(selDrink); }
    break;

  case S_CALIB:
    if (hit(tx,ty,196,4,40,24)) { showMain(); return; }
    for (uint8_t i = 0; i < NUM_POS; i++) {
      if (hit(tx,ty,6, 42 + i*36, 228, 28)) {
        calPump = i; showCalibP(i); return;
      }
    }
    if (hit(tx,ty,6,260,112,28))   { primeTubes(); return; }
    if (hit(tx,ty,122,260,112,28)) { flushTubes(); return; }
    break;

  case S_CALIB_P:
    if (hit(tx,ty,196,4,40,24)) { showCalib(); return; }

    if (hit(tx,ty,10,160,220,36)) {
      pumpOn(calPump);
      for (int s = 10; s > 0; s--) {
        tft.fillRect(14, 118, 220, 22, C_BG);
        tft.setTextSize(2); tft.setTextColor(C_RED);
        tft.setCursor(14, 118);
        tft.print("Erogando.. "); tft.print(s); tft.print('s');
        delay(1000);
      }
      pumpOff(calPump);
      showCalibP(calPump);
    }

    if (hit(tx,ty,10,210,105,36))  { cal[calPump] += 0.05f; showCalibP(calPump); }
    if (hit(tx,ty,125,210,105,36)) { if(cal[calPump]>0.1f) cal[calPump]-=0.05f; showCalibP(calPump); }
    if (hit(tx,ty,30,264,180,34))  { saveEE(); toast("Salvato!"); showCalibP(calPump); }
    break;

  default: break;
  }
}
#endif

// ═════════════════════════════════════════════════════════════
//  GESTIONE INPUT PULSANTI FISICI  (portrait)
// ═════════════════════════════════════════════════════════════

#if !USE_TOUCH
void handleButtons() {
  BtnEvt e = readBtn();
  if (e == BT_NONE) return;

  switch (scr) {

  case S_MAIN:
    if (e == BT_UP)   { cursor = (cursor + maxCursor) % (maxCursor+1); showMain(); }
    if (e == BT_DOWN) { cursor = (cursor + 1) % (maxCursor+1);         showMain(); }
    if (e == BT_OK) {
      if (cursor==0) { scroll=0; cursor=0; maxCursor=NUM_POS; showSetup(); }
      if (cursor==1) { scroll=0; cursor=0; showDrinks(); }
      if (cursor==2) { cursor=0; maxCursor=NUM_POS-1; showCalib(); }
    }
    break;

  case S_SETUP:
    if (e == BT_UP)   { cursor = (cursor + maxCursor) % (maxCursor+1); showSetup(); }
    if (e == BT_DOWN) { cursor = (cursor + 1) % (maxCursor+1);         showSetup(); }
    if (e == BT_OK) {
      if (cursor < NUM_POS) {
        selPos = cursor; scroll=0; cursor=0;
        maxCursor = (scroll==0) ? min((int)SEL_VIS-1, NUM_BOTTLES) : min((int)SEL_VIS, NUM_BOTTLES-scroll);
        showSelect();
      } else {
        saveEE(); ledsBase(); toast("Salvato!"); showSetup();
      }
    }
    break;

  case S_SELECT: {
    int total = (scroll==0) ? 1 + min((int)SEL_VIS-1, NUM_BOTTLES) : min((int)SEL_VIS, NUM_BOTTLES-scroll);
    if (e == BT_UP) {
      if (cursor > 0) { cursor--; showSelect(); }
      else if (scroll > 0) { scroll -= SEL_VIS; if(scroll<0) scroll=0; cursor=0; showSelect(); }
    }
    if (e == BT_DOWN) {
      if (cursor < total-1) { cursor++; showSelect(); }
      else if (scroll + SEL_VIS < NUM_BOTTLES) { scroll += SEL_VIS; cursor=0; showSelect(); }
    }
    if (e == BT_OK) {
      if (scroll==0 && cursor==0) {
        bottles[selPos]=0xFF; ledsBase(); cursor=selPos; maxCursor=NUM_POS; showSetup();
      } else {
        int idx = scroll + ((scroll==0) ? cursor-1 : cursor);
        if (idx < NUM_BOTTLES) {
          bottles[selPos]=(uint8_t)idx;
          ledsBase(); cursor=selPos; maxCursor=NUM_POS; showSetup();
        }
      }
    }
    break;
  }

  case S_DRINKS: {
    int total = min((int)DRK_VIS, (int)nAvail - scroll);
    if (total <= 0) { if(e==BT_OK){cursor=0;maxCursor=2;showMain();} break; }
    if (e == BT_UP) {
      if (cursor>0) { cursor--; showDrinks(); }
      else if (scroll>0) { scroll-=DRK_VIS; if(scroll<0) scroll=0; cursor=0; showDrinks(); }
    }
    if (e == BT_DOWN) {
      if (cursor<total-1) { cursor++; showDrinks(); }
      else if (scroll+DRK_VIS<(int)nAvail) { scroll+=DRK_VIS; cursor=0; showDrinks(); }
    }
    if (e == BT_OK) { selDrink=avail[scroll+cursor]; showDetail(selDrink); }
    break;
  }

  case S_DETAIL:
    if (e == BT_OK) dispense(selDrink);
    if (e == BT_UP || e == BT_DOWN) { scroll=0; cursor=0; showDrinks(); }
    break;

  case S_CALIB: {
    uint8_t total = NUM_POS + 2;
    if (e == BT_UP)   { cursor=(cursor+total-1)%total; showCalib(); }
    if (e == BT_DOWN) { cursor=(cursor+1)%total;       showCalib(); }
    if (e == BT_OK) {
      if (cursor < NUM_POS)      { calPump=cursor; showCalibP(cursor); }
      else if (cursor == NUM_POS) primeTubes();
      else                        flushTubes();
    }
    break;
  }

  case S_CALIB_P:
    if (e == BT_UP)   { cal[calPump]+=0.05f; showCalibP(calPump); }
    if (e == BT_DOWN) { if(cal[calPump]>0.1f) cal[calPump]-=0.05f; showCalibP(calPump); }
    if (e == BT_OK)   { saveEE(); toast("Salvato!"); showCalibP(calPump); }
    break;

  default: break;
  }
}
#endif

// ═════════════════════════════════════════════════════════════
//  SETUP  (portrait)
// ═════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < NUM_POS; i++) {
    pinMode(PIN_PUMP[i], OUTPUT); digitalWrite(PIN_PUMP[i], HIGH);
    pinMode(PIN_LED[i],  OUTPUT); digitalWrite(PIN_LED[i],  HIGH);
  }

  #if !USE_TOUCH
    pinMode(BTN_UP,   INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_OK,   INPUT_PULLUP);
  #endif

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.begin(8000000);  // SPI a 8 MHz (per level shifter HW-221)
  tft.setRotation(0);          // PORTRAIT 240×320
  tft.fillScreen(C_BG);

  #if USE_TOUCH
    ts.begin();
    ts.setRotation(0);
  #endif

  loadEE();
  ledsBase();

  // splash screen
  tft.setTextSize(3); tft.setTextColor(C_HDR);
  tft.setCursor(42, 100); tft.print("BARBOT");
  tft.setTextSize(1); tft.setTextColor(C_DIM);
  tft.setCursor(45, 140); tft.print("Cocktail Mixer System");
  tft.setCursor(70, 170); tft.print("Avvio...");
  ledChase(2);
  delay(1000);

  #if !USE_TOUCH
    cursor = 0; maxCursor = 2;
  #endif
  showMain();
}

// ═════════════════════════════════════════════════════════════
//  LOOP
// ═════════════════════════════════════════════════════════════

void loop() {
  #if USE_TOUCH
    handleTouch();
  #else
    handleButtons();
  #endif
}
