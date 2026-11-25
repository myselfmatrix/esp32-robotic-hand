/* ============================================================
   PHASE 4 : BASIC HUMANOID PALM  (5 FINGERS)
   ------------------------------------------------------------
   Yeh program paanch ungliyon ko PCA9685 ke CH0..CH4 se
   chalata hai, Serial Monitor se commands leta hai, aur
   ungliyon ki CALIBRATION flash memory mein save kar sakta hai
   (reboot ke baad bhi yaad rehti hai -- code edit nahi karna).

   App baad mein banayenge -- abhi keyboard se hi test karenge.

   ------------------------------------------------------------
   WIRING (Phase 2-3 wali, same):
     ESP32 3V3    -> PCA9685 VCC   (logic power)
     ESP32 GND    -> PCA9685 GND   (common ground -- ZAROORI)
     ESP32 GPIO22 -> PCA9685 SDA
     ESP32 GPIO23 -> PCA9685 SCL
     SMPS +5V     -> PCA9685 V+    (servo power, 5.01V verified)
     SMPS -V      -> PCA9685 GND

   SERVO CHANNELS:
     CH0 = THUMB   (angutha)
     CH1 = INDEX   (tarjani)
     CH2 = MIDDLE  (bich wali)
     CH3 = RING    (anamika)
     CH4 = PINKY   (chhoti)

   ------------------------------------------------------------
   !! CURRENT KA DHYAN !!
   Paanch SG90 ek saath chalein to peak current 2.5-3A tak ja
   sakta hai. Isliye yeh code jaan-bujh ke DHEERE chalta hai
   (ek tick mein thoda-thoda), taaki SMPS par jhatka na pade
   aur ESP32 brown-out se reboot na ho.
   Board reboot ho ya servo jhatke maare -> 'speed' kam karo
   ya bada SMPS lagao.

   ------------------------------------------------------------
   PEHLI BAAR: FINGERS_CONNECTED ko 1 rakho (sirf CH0 lagi hai).
   Jaise-jaise ungliyan jodo, isko 2, 3, 4, 5 karte jao.
   ============================================================ */

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_PWMServoDriver.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "esp_wifi.h"
#include "page.h"

// ---------------- WiFi ----------------
// Apne router se judna hai to niche SSID/PASS bhar do.
// Khali chhoda to ESP32 KHUD ka hotspot banayega (192.168.4.1).
// Apna router ka naam-password include/secrets.h mein daalo.
// Wo file .gitignore mein hai, isliye repo mein kabhi nahi jati --
// password public nahi hota. Sample: include/secrets.example.h
//
// secrets.h na ho to bhi sab chalta hai: ESP32 router dhoondhega
// hi nahi, seedha apna hotspot bana lega -> http://192.168.4.1
#if __has_include("secrets.h")
  #include "secrets.h"
#endif
#ifndef WIFI_SSID
  #define WIFI_SSID  ""
  #define WIFI_PASS  ""
#endif
#define AP_SSID    "RoboticHand"
#define AP_PASS    "hand12345"

// mDNS naam -- browser mein http://hand.local
// IP badalta rehta hai, naam nahi badalta.
#define MDNS_NAME  "hand"

// ---------------- Hardware settings ----------------
#define SDA_PIN      22
#define SCL_PIN      23
#define PCA_ADDRESS  0x40

// Abhi kitni ungliyan asli mein judi hain? (1 se 5)
// Jin channels par servo nahi laga, unko yeh code chhuyega bhi nahi.
// (Calibration ke dauran chuni hui ungli ko exception milta hai.)
#define FINGERS_CONNECTED  5

#define NUM_FINGERS  5

// ---------------- SAFETY LIMITS ----------------
// DEFAULT limits = wahi conservative range jo Phase 3 mein verified hui.
// 'limits' command se inhe badla ja sakta hai, par ABSOLUTE deewar
// ke bahar nahi -- SG90 usse aage mechanical stop se takrata hai.
// Global deewar ab chaudi hai -- asli suraksha PER-FINGER limits
// (niche struct mein) karti hain, kyunki ch4 (arm) ko poora range
// chahiye par ungliyon ko nahi.
#define DEF_MIN_US    500
#define DEF_MAX_US   2400
#define ABS_MIN_US    500      // isse neeche kabhi nahi
#define ABS_MAX_US   2400      // isse upar kabhi nahi

// ---------------- DEGREE MAPPING ----------------
// SG90 datasheet range: 500us = 0 deg , 2400us = 180 deg.
// Agar aapka servo alag nikle (arm 170 par poora na ghoome, ya
// pehle hi stop se takra jaye), to yeh do number badlo -- baaki
// code apne aap adjust ho jayega.
#define DEG_MIN_US   500       // 0 degree
#define DEG_MAX_US  2400       // 180 degree

uint16_t limMinUs = DEF_MIN_US;
uint16_t limMaxUs = DEF_MAX_US;

// ---------------- Finger definition ----------------
struct Finger {
  const char* name;
  uint8_t     channel;
  uint16_t    usOpen;    // ungli SEEDHI (khuli) is pulse par
  uint16_t    usClosed;  // ungli MUDI (band) is pulse par
  uint16_t    minUs;     // IS ungli ki apni deewar -- isse neeche nahi
  uint16_t    maxUs;     // IS ungli ki apni deewar -- isse upar nahi
};

// usOpen > usClosed ya usClosed > usOpen -- dono chalega.
// Agar koi ungli ulti taraf mude, to us ungli ke do number
// aapas mein badal do (swap). Baaki kuch nahi badalna.
// Yeh sirf DEFAULTS hain -- 'save' karne ke baad flash wale
// numbers inpar haavi ho jate hain.
// ############################################################
// #  MOTOR ARRANGEMENT  --  SIRF YEH TABLE BADALNI HAI
// #  ----------------------------------------------------------
// #  Ungli ka naam fix hai, uska CHANNEL badla ja sakta hai.
// #  Saare signs/gestures ungli ke NAAM se likhe hain, channel
// #  se nahi -- isliye taar idhar-udhar ho jayein to bas niche
// #  wala 'ch' number badal do, aur kuch nahi chhuna.
// #
// #  NAYA HAATH (22 Nov 2026) -- channel map SEEDHA ho gaya:
// #  CH0=THUMB, CH1=INDEX, CH2=MIDDLE, CH3=RING, CH4=PINKY.
// #  Purane haath mein ULTA tha (CH4=THUMB). Agar kabhi gesture
// #  ulta chale, to sabse pehle yahi column check karna.
// ############################################################
// MOUNTING REFERENCE -- 15 deg (658us) = HORN UPAR, UNGLI KHULI.
// 0 deg jaan-bujh ke nahi liya: wahan servo apne internal stop se
// ragadta hai (stall current, garmi). 15 se 165 = 150 deg ka pull.
//
// CLOSED = 90 deg (1450us) SIRF EK SAFE SHURUAT hai, asli value
// nahi. Naye haath ka thread aur horn angle alag hai; purane haath
// ke 178 deg wale numbers yahan servo ko stop se ragad dete ya
// thread tod dete. Har ungli ka asli CLOSED web UI se dhoond ke
// SAVE karna hai.
Finger fingers[NUM_FINGERS] = {
  //  name        ch   open   closed    min   max
  { "THUMB",   0,    658,  1661,     500, 2400 },   // 15 -> 110 deg  (naapa)
  { "INDEX",   1,    658,  1978,     500, 2400 },   // 15 -> 140 deg  (naapa)
  { "MIDDLE",  2,    658,  2347,     500, 2400 },   // 15 -> 175 deg  (naapa)
  { "RING",    3,    658,  2347,     500, 2400 },   // 15 -> 175 deg  (naapa, stop se 5 deg -- thread kaso)
  { "PINKY",   4,    658,  2294,     500, 2400 }   // 15 -> 170 deg  (naapa; 158 adhoora tha)
};

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA_ADDRESS, Wire);
Preferences prefs;

bool  pcaFound = false;
bool  relaxed  = false;              // true = PWM band, servo dheela

float currentUs[NUM_FINGERS];        // servo abhi kahan hai
float targetUs [NUM_FINGERS];        // servo ko kahan jaana hai

int   calFinger = -1;                // calibration ke liye chuni hui ungli

// Speed: har tick mein kitne microseconds khiskenge.
// Chhota = dheema aur safe. Bada = tez par current spike zyada.
#define TICK_MS        20
// 4.0 bahut dheema tha: thumb ke 1721us travel mein 8.6 second
// lagte the. 10.0 = 500us/sec, yaani thumb ~3.4 sec mein poora.
float  speedUsPerTick = 10.0;

unsigned long lastTick = 0;

// ---------------- AUTO-RELAX (servo ko garam hone se bachao) ----------------
// Servo apni position PAKDE rehne ke liye current kheenchta rehta hai.
// Agar wo apne mechanical stop se takra raha ho, ya tendon string tani
// ho, to yeh stall current ban jata hai -- ~700mA lagatar. Motor garam,
// plastic gear ghisne lagte hain, aur SMPS par bhi bojh padta hai.
//
// Isliye: hilna band hone ke 'idleMs' millisecond baad PWM apne aap
// band. Koi bhi naya command aate hi wapas chalu.
//
//   idle 0     -> auto-relax band (grip pakde rehna ho to)
//   idle 8     -> 8 second baad band (default)
uint32_t      idleMs     = 8000;
unsigned long lastMoveMs = 0;
bool          autoRelaxed = false;

// ============================================================
//  HELPERS
// ============================================================

// ------------------------------------------------------------
//  Servo ko pulse bhejo -- BINA chip se kuch padhe
//  ------------------------------------------------------------
//  Adafruit ka writeMicroseconds() har call par chip se PRESCALE
//  register PADHTA hai. Is setup mein woh I2C read fail ho raha
//  tha (Wire error -1), jiski wajah se count ka hisaab bigad
//  jata tha aur itna bada number banta tha ki uska bit PCA9685
//  ke "full OFF" flag par ja girta -- yaani channel poora band.
//  Isliye hum count khud nikaalte hain aur sirf LIKHTE hain.
//
//  50Hz => 20000us ka period, 4096 counts mein bata hua.
// ------------------------------------------------------------
void writeUs(uint8_t channel, uint16_t us) {
  uint16_t counts = (uint16_t)(((uint32_t)us * 4096UL) / 20000UL);
  if (counts > 4095) counts = 4095;
  pwm.setPWM(channel, 0, counts);
}

// Kisi bhi value ko safe range ke andar rakho
uint16_t clampUs(int us) {
  if (us < limMinUs) return limMinUs;
  if (us > limMaxUs) return limMaxUs;
  return (uint16_t)us;
}

// Degree -> microseconds (0..180)
uint16_t degToUs(float deg) {
  if (deg < 0)   deg = 0;
  if (deg > 180) deg = 180;
  return (uint16_t)(DEG_MIN_US + (deg * (float)(DEG_MAX_US - DEG_MIN_US)) / 180.0f + 0.5f);
}

// microseconds -> degree (sirf dikhane ke liye)
float usToDeg(uint16_t us) {
  return ((float)us - DEG_MIN_US) * 180.0f / (float)(DEG_MAX_US - DEG_MIN_US);
}

// Per-finger deewar. Global limit ke andar rehte hue, har ungli ki
// apni min/max. Web UI aur serial dono isi se guzarte hain.
uint16_t clampF(uint8_t idx, int us) {
  if (idx >= NUM_FINGERS) return clampUs(us);
  int lo = fingers[idx].minUs;  if (lo < limMinUs) lo = limMinUs;
  int hi = fingers[idx].maxUs;  if (hi > limMaxUs) hi = limMaxUs;
  if (lo > hi) { lo = limMinUs; hi = limMaxUs; }
  if (us < lo) return (uint16_t)lo;
  if (us > hi) return (uint16_t)hi;
  return (uint16_t)us;
}

// Is channel ko PWM bhejna theek hai ya nahi?
// (jo ungli judi nahi uspar bekaar pulse nahi bhejte)
bool channelLive(uint8_t i) {
  return (i < FINGERS_CONNECTED) || ((int)i == calFinger);
}

// percent: 0 = poori khuli, 100 = poori mudi
uint16_t percentToUs(uint8_t idx, int percent) {
  if (percent < 0)   percent = 0;
  if (percent > 100) percent = 100;
  int lo = fingers[idx].usOpen;
  int hi = fingers[idx].usClosed;
  int us = lo + ((hi - lo) * percent) / 100;   // ulte range par bhi sahi
  return clampF(idx, us);
}

// Ek ungli ka target set karo (percent mein)
void setFinger(uint8_t idx, int percent) {
  if (idx >= NUM_FINGERS) return;
  targetUs[idx] = percentToUs(idx, percent);
  if (relaxed) relaxed = false;      // hilne ka order aaya = wapas active
}

// Paanchon ka target ek saath set karo
void setHand(const int percent[NUM_FINGERS]) {
  for (uint8_t i = 0; i < NUM_FINGERS; i++) setFinger(i, percent[i]);
}

// PWM band -> servo dheela ho jata hai (garmi aur current bachta hai)
void relaxHand() {
  for (uint8_t i = 0; i < NUM_FINGERS; i++) {
    if (channelLive(i)) pwm.setPWM(fingers[i].channel, 0, 0);
  }
  relaxed = true;
  Serial.println("  Hand RELAXED (PWM off). Koi bhi command dene par wapas active.");
}

// ============================================================
//  CALIBRATION : flash (NVS) mein save / load
// ============================================================
void calSave() {
  prefs.begin("palm", false);
  for (uint8_t i = 0; i < NUM_FINGERS; i++) {
    char k[8];
    snprintf(k, sizeof(k), "o%u", i);  prefs.putUShort(k, fingers[i].usOpen);
    snprintf(k, sizeof(k), "c%u", i);  prefs.putUShort(k, fingers[i].usClosed);
    snprintf(k, sizeof(k), "n%u", i);  prefs.putUShort(k, fingers[i].minUs);
    snprintf(k, sizeof(k), "x%u", i);  prefs.putUShort(k, fingers[i].maxUs);
    snprintf(k, sizeof(k), "h%u", i);  prefs.putUChar (k, fingers[i].channel);
  }
  prefs.putUShort("lmin", limMinUs);
  prefs.putUShort("lmax", limMaxUs);
  prefs.putBool("ok", true);
  prefs.end();
  Serial.println("  SAVED. Reboot ke baad bhi yeh calibration yaad rahegi.");
}

// return: true = flash mein calibration mili thi
bool calLoad(bool quiet = false) {
  prefs.begin("palm", true);               // read-only
  bool ok = prefs.getBool("ok", false);
  if (ok) {
    for (uint8_t i = 0; i < NUM_FINGERS; i++) {
      char k[8];
      snprintf(k, sizeof(k), "o%u", i);
      fingers[i].usOpen   = prefs.getUShort(k, fingers[i].usOpen);
      snprintf(k, sizeof(k), "c%u", i);
      fingers[i].usClosed = prefs.getUShort(k, fingers[i].usClosed);
      snprintf(k, sizeof(k), "n%u", i);
      fingers[i].minUs    = prefs.getUShort(k, fingers[i].minUs);
      snprintf(k, sizeof(k), "x%u", i);
      fingers[i].maxUs    = prefs.getUShort(k, fingers[i].maxUs);
      snprintf(k, sizeof(k), "h%u", i);
      fingers[i].channel  = prefs.getUChar (k, fingers[i].channel);
    }
    limMinUs = prefs.getUShort("lmin", DEF_MIN_US);
    limMaxUs = prefs.getUShort("lmax", DEF_MAX_US);
  }
  prefs.end();
  if (!quiet) {
    Serial.println(ok ? "  Flash se calibration load ho gayi."
                      : "  Flash mein calibration nahi hai -- code ke defaults chal rahe hain.");
  }
  return ok;
}

void calDefaults() {
  const uint16_t dOpen[NUM_FINGERS]  = { 658, 658, 658, 658, 658 };
  // 1450us = 90 deg -- safe shuruat, asli CLOSED nahi. Dekho
  // fingers[] ke upar wala comment.
  const uint16_t dClose[NUM_FINGERS] = { 1661, 1978, 2347, 2347, 2294 };
  // Channel bhi flash mein save hota hai (key "h<i>"), isliye yahan
  // wapas lana ZAROORI hai -- warna naya code chadhane par bhi
  // purana channel map NVS se load hoke haavi ho jata hai.
  const uint8_t  dCh[NUM_FINGERS]    = { 0, 1, 2, 3, 4 };
  for (uint8_t i = 0; i < NUM_FINGERS; i++) {
    fingers[i].usOpen   = dOpen[i];
    fingers[i].usClosed = dClose[i];
    fingers[i].channel  = dCh[i];
    fingers[i].minUs    = ABS_MIN_US;
    fingers[i].maxUs    = ABS_MAX_US;
  }
  limMinUs = DEF_MIN_US;
  limMaxUs = DEF_MAX_US;
  Serial.println("  Defaults wapas aa gaye (flash mein abhi nahi gaye -- 'save' karo to jayenge).");
}

// Calibration table -- seedha code mein paste karne layak
void printTable() {
  Serial.println();
  Serial.println("  ---- ABHI KI CALIBRATION ----");
  Serial.printf ("  limits: %d .. %d us\n", limMinUs, limMaxUs);
  Serial.println("  Finger fingers[NUM_FINGERS] = {");
  for (uint8_t i = 0; i < NUM_FINGERS; i++) {
    Serial.printf("    { \"%-6s\", %d, %4d, %4d }%s\n",
                  fingers[i].name, fingers[i].channel,
                  fingers[i].usOpen, fingers[i].usClosed,
                  (i == NUM_FINGERS - 1) ? "" : ",");
  }
  Serial.println("  };");
  Serial.println("  -----------------------------");
}

void calStart(int ch) {
  if (ch < 0 || ch >= NUM_FINGERS) { Serial.println("  ch 0 se 4 tak"); return; }
  calFinger = ch;
  relaxed = false;
  // Ungli ko uski abhi ki OPEN position par le aao -- wahan se shuru karna safe hai
  targetUs[ch] = clampF(ch, fingers[ch].usOpen);
  Serial.println();
  Serial.printf ("  CALIBRATION MODE -- %s (ch%d)\n", fingers[ch].name, ch);
  Serial.printf ("  Ungli ko %dus par la rahe hain (abhi ka 'open').\n", (int)targetUs[ch]);
  Serial.println("  Ab aise karo:");
  Serial.println("    +  /  -        -> 10us aage / peeche khiskao");
  Serial.println("    + 50 / - 50    -> 50us khiskao");
  Serial.println("    setopen        -> yeh position 'khuli ungli' maan lo");
  Serial.println("    setclose       -> yeh position 'mudi ungli' maan lo");
  Serial.println("    save           -> flash mein pakka kar do");
  Serial.println("    table          -> numbers dekho");
  Serial.println("    cal <ch>       -> agli ungli par jao");
  Serial.println("    exit           -> calibration band karo");
  Serial.println("  DHYAN: ungli ko zor lagta hua dekho to wahin ruk jao --");
  Serial.println("         servo stall hoga, garam hoga, current kheenchega.");
}

void calNudge(int delta) {
  if (calFinger < 0) {
    Serial.println("  Pehle ungli chuno: cal 0");
    return;
  }
  int want = (int)targetUs[calFinger] + delta;
  uint16_t safe = clampF(calFinger, want);
  if (safe != want) {
    Serial.printf("  (%dus limit se bahar -> %dus. Aage jaana hai to: limits %d %d)\n",
                  want, safe, limMinUs - 50, limMaxUs + 50);
  }
  targetUs[calFinger] = safe;
  relaxed = false;
  Serial.printf("  %s -> %dus\n", fingers[calFinger].name, safe);
}

// ============================================================
//  DIAGNOSTIC : PCA9685 ke registers wapas padho
//  ------------------------------------------------------------
//  Yeh batata hai ki chip ne hamari PWM value apne andar rakhi
//  ya nahi. Agar values sahi dikhein par servo na hile, to galti
//  chip ke BAAD hai (power / taar / servo) -- code mein nahi.
// ============================================================
uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(PCA_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return 0xFF;
  if (Wire.requestFrom((uint8_t)PCA_ADDRESS, (uint8_t)1) != 1) return 0xFF;
  return Wire.read();
}

void dumpRegs() {
  uint8_t mode1 = readReg(0x00);
  uint8_t mode2 = readReg(0x01);
  uint8_t pre   = readReg(0xFE);

  Serial.println();
  Serial.println("  ---- PCA9685 REGISTERS ----");
  Serial.printf ("  MODE1    = 0x%02X   SLEEP=%s  RESTART=%s  AI=%s\n",
                 mode1,
                 (mode1 & 0x10) ? "HAAN (!! PWM BAND !!)" : "nahi (theek)",
                 (mode1 & 0x80) ? "1" : "0",
                 (mode1 & 0x20) ? "1" : "0");
  Serial.printf ("  MODE2    = 0x%02X   OUTDRV=%s  INVRT=%s\n",
                 mode2,
                 (mode2 & 0x04) ? "totem-pole (theek)" : "open-drain (!! servo nahi chalega !!)",
                 (mode2 & 0x10) ? "1 (ULTA!)" : "0 (theek)");

  // Ek PWM count kitne microseconds ka hai
  float usPerCount = ((float)(pre + 1) * 1000000.0) / 27000000.0;
  float freqHz     = 27000000.0 / (4096.0 * (float)(pre + 1));
  Serial.printf ("  PRESCALE = %d (0x%02X)  -> %.1f Hz, 1 count = %.2f us\n",
                 pre, pre, freqHz, usPerCount);
  if (pre < 100 || pre > 170) {
    Serial.println("  !! PRESCALE ajeeb hai -- 50Hz ke liye ~131 hona chahiye !!");
  }

  Serial.println("  Ch   ON     OFF    -> asli pulse");
  for (uint8_t i = 0; i < NUM_FINGERS; i++) {
    uint8_t base = 0x06 + 4 * fingers[i].channel;
    uint16_t on  = readReg(base)     | ((uint16_t)readReg(base + 1) << 8);
    uint16_t off = readReg(base + 2) | ((uint16_t)readReg(base + 3) << 8);
    Serial.printf("  %2d   %5u  %5u  -> %.0f us%s\n",
                  fingers[i].channel, on, off,
                  (off - on) * usPerCount,
                  (off == 0) ? "   <<< KUCH NAHI NIKAL RAHA" : "");
  }
  Serial.println("  ---------------------------");
  Serial.println("  Values theek dikhein par servo na hile => galti chip ke");
  Serial.println("  BAAD hai: V+ power, servo ka taar, ya servo khud.");
}

// ============================================================
//  GESTURES        { THUMB, INDEX, MIDDLE, RING, PINKY }
//  0 = khuli ungli , 100 = mudi ungli
// ============================================================
const int G_OPEN  [NUM_FINGERS] = {   0,   0,   0,   0,   0 };
const int G_FIST  [NUM_FINGERS] = { 100, 100, 100, 100, 100 };
const int G_POINT [NUM_FINGERS] = { 100,   0, 100, 100, 100 };
const int G_PEACE [NUM_FINGERS] = { 100,   0,   0, 100, 100 };
const int G_THUMBS[NUM_FINGERS] = {   0, 100, 100, 100, 100 };
const int G_ROCK  [NUM_FINGERS] = { 100,   0, 100, 100,   0 };
const int G_PINCH [NUM_FINGERS] = {  70,  70,   0,   0,   0 };

// Ginti: 0 se 5
const int G_COUNT[6][NUM_FINGERS] = {
  { 100, 100, 100, 100, 100 },   // 0 = muthhi
  { 100,   0, 100, 100, 100 },   // 1
  { 100,   0,   0, 100, 100 },   // 2
  { 100,   0,   0,   0, 100 },   // 3
  { 100,   0,   0,   0,   0 },   // 4
  {   0,   0,   0,   0,   0 }    // 5 = poora haath khula
};


// ============================================================
//  SIGNS  --  ek jagah ruk jane wale poses
//  ------------------------------------------------------------
//  Numbers ungli ke NAAM ke kram mein hain, channel ke nahi:
//      THUMB , INDEX , MIDDLE , RING , PINKY
//  0   = ungli SEEDHI (khuli)
//  100 = ungli MUDI (band)
// ============================================================
struct Sign {
  const char* id;        // URL mein jata hai
  const char* label;     // button par dikhta hai
  int         pct[NUM_FINGERS];
};

const Sign SIGNS[] = {
  //  id            label          TH   IN   MI   RI   PI
  { "open",       "OPEN",       {   0,   0,   0,   0,   0 } },
  { "fist",       "FIST",       { 100, 100, 100, 100, 100 } },
  { "peace",      "PEACE",      { 100,   0,   0, 100, 100 } },
  { "middle",     "MIDDLE",     { 100, 100,   0, 100, 100 } },
  { "thumbsup",   "THUMBS UP",  {   0, 100, 100, 100, 100 } },
  { "rock",       "ROCK",       { 100,   0, 100, 100,   0 } },
  { "ok",         "OK",         {  70,  70,   0,   0,   0 } },
  { "point",      "POINT",      { 100,   0, 100, 100, 100 } },
  { "callme",     "CALL ME",    {   0, 100, 100, 100,   0 } },
  { "gun",        "GUN",        {   0,   0, 100, 100, 100 } },
  { "three",      "THREE",      { 100,   0,   0,   0, 100 } },
  { "four",       "FOUR",       { 100,   0,   0,   0,   0 } }
};
const uint8_t SIGN_N = sizeof(SIGNS) / sizeof(SIGNS[0]);

// ============================================================
//  GESTURES  --  chalne wale (animated) sequences
//  ------------------------------------------------------------
//  Har step ek pose hai + holdMs (kitni der us pose par ruke).
//  Player non-blocking hai: loop() har baar thoda sa aage
//  badhata hai, isliye commands beech mein bhi lagte rehte hain.
// ============================================================
struct Step { int pct[NUM_FINGERS]; uint16_t holdMs; };
// wait = true  -> har pose POORA hone ka intezaar (ginti, grab jaise)
// wait = false -> sirf ghadi ke hisaab se aage (wave, drum jaise) --
//                 isse ungliyan EK SAATH chalti hain, bas thodi peeche,
//                 aur lehar banti hai. Intezaar karne par wo thap-thap
//                 ban jata hai.
// reps = kitni baar poora sequence chale (loop=true ho to bemaani)
struct Seq  { const char* id; const char* label; const Step* steps;
              uint8_t n; bool loop; bool wait; uint8_t reps; };

// WAVE -- ungliyan ek-ek karke mudti hain, phir ek-ek karke khulti
// ------------------------------------------------------------
//  WAVE ka hisaab
//  ------------------------------------------------------------
//  Ek ungli ko poora fold hone mein ~1.15 sec lagte hain
//  (1721us travel / 1500us per sec).
//
//  Step har 420ms par aata hai. Matlab ek ungli ko poora mudne
//  ka time mil jata hai, PAR agli ungli uske khatam hone se
//  pehle hi shuru ho jati hai -- lagbhag TEEN ungliyan hamesha
//  ek saath chal rahi hoti hain, har ek alag phase par.
//
//  Pehle 130ms tha: koi ungli poori mudti hi nahi thi.
//  Usse pehle "pose poora hone ka intezaar" tha: ek-ek karke
//  mudti thi, lehar nahi banti thi. Yeh beech ka raasta hai.
//
//  Ek chakkar = 10 steps x 420ms = ~4.2 sec, aur 3 reps = ~13 sec.
// ------------------------------------------------------------
const Step SEQ_WAVE[] = {
  { {   0,   0,   0,   0, 100 }, 420 },   // pinky mudna shuru
  { {   0,   0,   0, 100, 100 }, 420 },   // ring juda
  { {   0,   0, 100, 100, 100 }, 420 },   // middle juda
  { {   0, 100, 100, 100, 100 }, 420 },   // index juda
  { { 100, 100, 100, 100, 100 }, 560 },   // thumb juda -- sab mudi
  { { 100, 100, 100, 100,   0 }, 420 },   // pinky khulna shuru
  { { 100, 100, 100,   0,   0 }, 420 },   // ring khula
  { { 100, 100,   0,   0,   0 }, 420 },   // middle khula
  { { 100,   0,   0,   0,   0 }, 420 },   // index khula
  { {   0,   0,   0,   0,   0 }, 560 }    // thumb khula -- sab khuli
};

// HELLO -- haath khol-band, jaise hilaa rahe ho
const Step SEQ_HELLO[] = {
  { {   0,   0,   0,   0,   0 }, 350 },
  { {  60,  80,  80,  80,  80 }, 350 },
  { {   0,   0,   0,   0,   0 }, 350 },
  { {  60,  80,  80,  80,  80 }, 350 },
  { {   0,   0,   0,   0,   0 }, 450 }
};

// COUNT -- 0 se 5 tak ginti
const Step SEQ_COUNT[] = {
  { { 100, 100, 100, 100, 100 }, 600 },
  { { 100,   0, 100, 100, 100 }, 600 },
  { { 100,   0,   0, 100, 100 }, 600 },
  { { 100,   0,   0,   0, 100 }, 600 },
  { { 100,   0,   0,   0,   0 }, 600 },
  { {   0,   0,   0,   0,   0 }, 800 }
};

// GRAB -- kuch pakadna aur chhodna
const Step SEQ_GRAB[] = {
  { {   0,   0,   0,   0,   0 }, 450 },
  { {  80,  90,  90,  90,  90 }, 900 },
  { {   0,   0,   0,   0,   0 }, 550 }
};

// DRUM -- ungliyan bari-bari thap thap
const Step SEQ_DRUM[] = {
  { {   0, 100,   0,   0,   0 }, 120 },
  { {   0,   0,   0,   0,   0 },  90 },
  { {   0,   0, 100,   0,   0 }, 120 },
  { {   0,   0,   0,   0,   0 },  90 },
  { {   0,   0,   0, 100,   0 }, 120 },
  { {   0,   0,   0,   0,   0 },  90 },
  { {   0,   0,   0,   0, 100 }, 120 },
  { {   0,   0,   0,   0,   0 }, 160 }
};

#define SQ(a) (a), (sizeof(a) / sizeof(a[0]))
const Seq SEQS[] = {
  //  id       label    steps          loop   wait   reps
  { "wave",  "WAVE",  SQ(SEQ_WAVE),  false, false,  3 },
  { "hello", "HELLO", SQ(SEQ_HELLO), false, true,   2 },
  { "count", "COUNT", SQ(SEQ_COUNT), false, true,   1 },
  { "grab",  "GRAB",  SQ(SEQ_GRAB),  false, true,   1 },
  { "drum",  "DRUM",  SQ(SEQ_DRUM),  true,  false,  0 }
};
const uint8_t SEQ_N = sizeof(SEQS) / sizeof(SEQS[0]);

// Motion engine neeche define hua hai, par seqUpdate ko yahan
// chahiye -- isliye prototype.
bool handSettled();

// ---- player state ----
const Seq*    seqNow   = nullptr;
uint8_t       seqIdx   = 0;
uint8_t       seqRep   = 0;      // kitne chakkar ho chuke
unsigned long seqAt    = 0;
unsigned long seqArrived = 0;     // pose poora hone ka waqt
float         seqSaveSpeed = 0;

// Agar koi ungli atak jaye (thread tight, servo stall), to sequence
// hamesha ke liye wahin na ruke -- itne ms baad zabardasti aage.
#define SEQ_STUCK_MS  7000

// Sequence ke dauran thodi tez raftaar, warna pose poora hone se
// pehle hi agla step aa jata hai aur movement adhoori dikhti hai.
// 30 = 1500us/sec. Isse zyada karne par paanchon servo ek saath
// tezi se chalte hain aur current spike se board reboot ho sakta
// hai (brown-out). Reboot ho to ise kam karo.
#define SEQ_SPEED  30.0f

void seqStop() {
  if (seqNow && seqSaveSpeed > 0) speedUsPerTick = seqSaveSpeed;
  seqNow = nullptr;
  seqSaveSpeed = 0;
}

void seqStart(const Seq* q) {
  if (!seqNow) seqSaveSpeed = speedUsPerTick;
  speedUsPerTick = SEQ_SPEED;
  seqNow = q;
  seqIdx = 0;
  seqRep = 0;
  seqAt  = 0;              // 0 = pehla step abhi lagana hai
}

// holdMs ab "pose POORA hone ke baad kitni der rukna hai" hai,
// "step shuru hone ke baad" nahi. Isliye gesture apne aap us
// hardware ke hisaab se chalta hai -- lamba travel wali ungli ko
// pahunchne ka pura time milta hai, chahe calibration kuch bhi ho.
void seqUpdate() {
  if (!seqNow) return;

  if (seqAt == 0) {                       // pehla step
    setHand(seqNow->steps[0].pct);
    seqAt = millis();
    seqArrived = 0;
    return;
  }

  if (seqNow->wait) {
    // POSE POORA HONE KA INTEZAAR (ginti, grab -- jahan har pose
    // saaf dikhna chahiye). Koi ungli atak jaye to 7 sec baad aage.
    bool stuck = (millis() - seqAt > SEQ_STUCK_MS);
    if (!handSettled() && !stuck) { seqArrived = 0; return; }
    if (seqArrived == 0) { seqArrived = millis(); return; }
    if (millis() - seqArrived < seqNow->steps[seqIdx].holdMs) return;
  } else {
    // SIRF GHADI (wave, drum) -- ungli poori mudne se pehle hi agla
    // step. Isse movement overlap hoti hai aur lehar banti hai.
    if (millis() - seqAt < seqNow->steps[seqIdx].holdMs) return;
  }

  seqIdx++;
  if (seqIdx >= seqNow->n) {
    seqIdx = 0;
    seqRep++;
    // loop = hamesha chalta rahe. warna reps poore hote hi ruk jao.
    if (!seqNow->loop && seqRep >= seqNow->reps) { seqStop(); return; }
  }
  setHand(seqNow->steps[seqIdx].pct);
  seqAt = millis();
  seqArrived = 0;
}

const Sign* findSign(const String& id) {
  for (uint8_t i = 0; i < SIGN_N; i++) if (id == SIGNS[i].id) return &SIGNS[i];
  return nullptr;
}
const Seq* findSeq(const String& id) {
  for (uint8_t i = 0; i < SEQ_N; i++) if (id == SEQS[i].id) return &SEQS[i];
  return nullptr;
}

// ============================================================
//  MOTION ENGINE  (non-blocking -- isliye commands turant lagte hain)
// ============================================================
// Saari ungliyan apne target par pahunch gayi?
bool handSettled() {
  for (uint8_t i = 0; i < NUM_FINGERS; i++) {
    if (!channelLive(i)) continue;
    if (fabs(targetUs[i] - currentUs[i]) >= 0.5) return false;
  }
  return true;
}

// ------------------------------------------------------------
//  SYNCHRONIZED MOTION
//  ------------------------------------------------------------
//  Pehle har ungli EK HI raftaar (us/tick) se chalti thi. Problem:
//  har ungli ki calibration alag hai, to doori bhi alag hai.
//  THUMB ko 1721us chalna hai par INDEX ko sirf 475us -- ek hi
//  raftaar par thumb ko 3.6 guna zyada time lagta tha. Gesture
//  mein baaki ungliyan pahunch jati thi aur thumb beech raste
//  mein hi reh jata tha, phir agla step aa jata tha.
//
//  Ab: sabse LAMBI doori wali ungli poori raftaar se chalti hai,
//  baaki apni doori ke anupaat mein DHEERE. Isse sab EK SAATH
//  pahunchti hain, chahe calibration kuch bhi ho.
// ------------------------------------------------------------
void updateMotion() {
  float maxDiff = 0;
  for (uint8_t i = 0; i < NUM_FINGERS; i++) {
    if (!channelLive(i)) continue;
    float d = fabs(targetUs[i] - currentUs[i]);
    if (d > maxDiff) maxDiff = d;
  }
  if (maxDiff < 0.5) return;                        // sab pahunch gaye

  bool moved = false;
  for (uint8_t i = 0; i < NUM_FINGERS; i++) {
    if (!channelLive(i)) continue;

    float diff = targetUs[i] - currentUs[i];
    if (fabs(diff) < 0.5) continue;

    // apni doori ke hisaab se raftaar
    float step = speedUsPerTick * (fabs(diff) / maxDiff);
    if (step < 0.05f) step = 0.05f;

    if (fabs(diff) <= step) currentUs[i] = targetUs[i];
    else currentUs[i] += (diff > 0 ? step : -step);

    writeUs(fingers[i].channel, clampF(i, (int)currentUs[i]));
    moved = true;
  }
  if (moved) { lastMoveMs = millis(); autoRelaxed = false; }
}

// ============================================================
//  SERIAL COMMANDS
// ============================================================
void printHelp() {
  Serial.println();
  Serial.println("  ---------- GESTURES ----------");
  Serial.println("   o        -> OPEN  (haath khola)");
  Serial.println("   f        -> FIST  (muthhi)");
  Serial.println("   p        -> POINT (ek ungli)");
  Serial.println("   v        -> PEACE");
  Serial.println("   t        -> THUMBS UP");
  Serial.println("   k        -> ROCK  (sing)");
  Serial.println("   n        -> PINCH (pakadna)");
  Serial.println("   0..5     -> utni ungli dikhao (ginti)");
  Serial.println("   sign <naam>    -> peace, middle, rock, ok, callme, gun...");
  Serial.println("   seq <naam>     -> wave, hello, count, grab, drum");
  Serial.println("   signs          -> saare naam dekho");
  Serial.println("   stop           -> chalta gesture band karo");
  Serial.println("  ---------- MANUAL ------------");
  Serial.println("   a <0-100>      -> saari ungliyan itne percent mod do");
  Serial.println("   s <ch> <0-100> -> ek ungli (ch = 0..4)");
  Serial.println("   u <ch> <us>    -> ek ungli seedha microseconds par");
  Serial.println("   deg <ch> <0-180> -> seedha DEGREE mein ghumao (deg 4 170)");
  Serial.println("   speed <1-15>   -> raftaar (default 4, chhota = safe)");
  Serial.println("   r        -> RELAX (PWM off, servo dheela)");
  Serial.println("   idle <sec>     -> itne sec baad PWM apne aap off (0 = band)");
  Serial.println("   w        -> WHERE (abhi kahan hai)");
  Serial.println("  ---------- CALIBRATION -------");
  Serial.println("   cal <ch>       -> us ungli ki calibration shuru karo");
  Serial.println("   + / -          -> 10us khiskao   (+ 50 = 50us)");
  Serial.println("   setopen        -> yeh position = khuli ungli");
  Serial.println("   setclose       -> yeh position = mudi ungli");
  Serial.println("   save / load    -> flash mein pakka / wapas padho");
  Serial.println("   defaults       -> code ke shuruati numbers");
  Serial.println("   limits <a> <b> -> safe range badlo (1000-2000 ke andar)");
  Serial.println("   table          -> abhi ke numbers dekho");
  Serial.println("   exit           -> calibration mode band");
  Serial.println("   ?        -> yeh list");
  Serial.println("  ------------------------------");
}

void printWhere() {
  Serial.println();
  Serial.printf("  limits %d..%d us   speed %.1f   %s\n",
                limMinUs, limMaxUs, speedUsPerTick,
                relaxed ? "RELAXED" : "active");
  Serial.println("  Ungli    ch   abhi(us)  target(us)  open  closed");
  for (uint8_t i = 0; i < NUM_FINGERS; i++) {
    if (!channelLive(i)) continue;
    Serial.printf("  %-7s  %2d   %6d     %6d    %4d   %4d%s\n",
                  fingers[i].name, fingers[i].channel,
                  (int)currentUs[i], (int)targetUs[i],
                  fingers[i].usOpen, fingers[i].usClosed,
                  ((int)i == calFinger) ? "   <<< CAL" : "");
  }
  if (FINGERS_CONNECTED < NUM_FINGERS) {
    Serial.printf("  (CH%d..CH%d connected nahi -- chhue nahi ja rahe)\n",
                  FINGERS_CONNECTED, NUM_FINGERS - 1);
  }
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;

  // ================= poore-shabd wale commands pehle =================
  // (inhe single-letter switch se PEHLE dekhna zaroori hai, warna
  //  "save" ko 's' aur "table" ko 't' samajh liya jayega)

  if (cmd == "save")     { calSave();      return; }
  if (cmd == "load")     { calLoad();  printTable(); return; }
  if (cmd == "defaults") { calDefaults(); printTable(); return; }
  if (cmd == "table")    { printTable();   return; }
  if (cmd == "reg")      { dumpRegs();     return; }
  if (cmd == "help")     { printHelp();    return; }
  if (cmd == "relax")    { relaxHand();    return; }

  if (cmd == "exit") {
    calFinger = -1;
    Serial.println("  Calibration mode band. ('save' kiya tha? warna numbers reboot par chale jayenge)");
    return;
  }

  if (cmd == "setopen" || cmd == "setclose") {
    if (calFinger < 0) { Serial.println("  Pehle ungli chuno: cal 0"); return; }
    uint16_t here = clampF(calFinger, (int)targetUs[calFinger]);
    if (cmd == "setopen") {
      fingers[calFinger].usOpen = here;
      Serial.printf("  %s ka OPEN = %dus set ho gaya.\n", fingers[calFinger].name, here);
    } else {
      fingers[calFinger].usClosed = here;
      Serial.printf("  %s ka CLOSED = %dus set ho gaya.\n", fingers[calFinger].name, here);
    }
    Serial.println("  (flash mein daalne ke liye: save)");
    return;
  }

  // ---- deg <ch> <angle> : seedha degree mein ghumao ----
  if (cmd.startsWith("deg")) {
    String rest = cmd.substring(3);
    rest.trim();
    int sp = rest.indexOf(' ');
    if (sp < 0) { Serial.println("  Format: deg <ch> <0-180>   jaise: deg 4 170"); return; }
    int   ch  = rest.substring(0, sp).toInt();
    float ang = rest.substring(sp + 1).toFloat();
    if (ch < 0 || ch >= NUM_FINGERS) { Serial.println("  ch 0 se 4 tak"); return; }

    uint16_t want = degToUs(ang);
    uint16_t safe = clampF(ch, want);
    if (safe != want) {
      Serial.printf("  (%.0f deg = %dus, par limit %d..%d hai -> %dus = %.0f deg)\n",
                    ang, want, limMinUs, limMaxUs, safe, usToDeg(safe));
      Serial.printf("  Poora jaana hai to: limits %d %d\n", ABS_MIN_US, ABS_MAX_US);
    }
    targetUs[ch] = safe;
    relaxed = false;
    Serial.printf("  %s (ch%d) -> %.0f deg = %dus\n",
                  fingers[ch].name, ch, usToDeg(safe), safe);
    return;
  }

  if (cmd.startsWith("cal")) {
    String rest = cmd.substring(3);
    rest.trim();
    if (rest.length() == 0) { Serial.println("  Format: cal <ch>   jaise: cal 0"); return; }
    calStart(rest.toInt());
    return;
  }

  if (cmd.startsWith("limits")) {
    String rest = cmd.substring(6);
    rest.trim();
    int sp = rest.indexOf(' ');
    if (sp < 0) { Serial.println("  Format: limits <min> <max>   jaise: limits 1100 1900"); return; }
    int lo = rest.substring(0, sp).toInt();
    int hi = rest.substring(sp + 1).toInt();
    if (lo < ABS_MIN_US || hi > ABS_MAX_US || lo >= hi) {
      Serial.printf("  Nahi. %d..%d ke andar rehna hai aur min < max.\n", ABS_MIN_US, ABS_MAX_US);
      return;
    }
    limMinUs = (uint16_t)lo;
    limMaxUs = (uint16_t)hi;
    Serial.printf("  Limits ab %d..%d us.\n", limMinUs, limMaxUs);
    Serial.println("  DHEERE badhao -- servo mechanical stop se takra sakta hai.");
    return;
  }

  // ---- naye SIGNS aur GESTURES, serial se bhi ----
  // Web ke bina bhi sab kuch chale, isliye yahan bhi daal rahe hain.
  if (cmd == "signs" || cmd == "list") {
    Serial.println();
    Serial.print("  SIGNS:  ");
    for (uint8_t i = 0; i < SIGN_N; i++) { Serial.print(SIGNS[i].id); Serial.print("  "); }
    Serial.println();
    Serial.print("  SEQS :  ");
    for (uint8_t i = 0; i < SEQ_N; i++) { Serial.print(SEQS[i].id); Serial.print("  "); }
    Serial.println();
    Serial.println("  use karo:  sign peace     ya    seq wave");
    return;
  }

  if (cmd == "net") {
    Serial.println();
    Serial.print("  WiFi status : ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "JUDA HUA" : "TOOTA HUA");
    Serial.print("  SSID        : "); Serial.println(WiFi.SSID());
    Serial.print("  IP          : http://"); Serial.println(WiFi.localIP().toString());
    Serial.print("  signal      : "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
    Serial.print("  naam se     : http://"); Serial.print(MDNS_NAME); Serial.println(".local");
    Serial.print("  HOTSPOT     : http://"); Serial.print(WiFi.softAPIP().toString());
    Serial.print("   ("); Serial.print(AP_SSID);
    Serial.print(" / "); Serial.print(AP_PASS); Serial.println(")");
    Serial.print("  hotspot par judey devices: ");
    Serial.println(WiFi.softAPgetStationNum());
    Serial.print("  free heap   : "); Serial.println(ESP.getFreeHeap());
    Serial.print("  uptime      : "); Serial.print(millis() / 1000); Serial.println(" sec");
    return;
  }

  if (cmd == "scan") {
    Serial.println();
    Serial.println("  Aas-paas ke WiFi (ESP32 ki nazar se):");
    int n = WiFi.scanNetworks();
    if (n <= 0) Serial.println("    kuch nahi mila");
    for (int i = 0; i < n && i < 15; i++) {
      Serial.print("    ");
      Serial.print(WiFi.SSID(i));
      Serial.print("   ");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm  ch");
      Serial.println(WiFi.channel(i));
    }
    WiFi.scanDelete();
    return;
  }

  if (cmd == "stop") { seqStop(); Serial.println("  gesture band."); return; }

  if (cmd.startsWith("sign")) {
    String id = cmd.substring(4); id.trim();
    const Sign* g = findSign(id);
    if (!g) { Serial.println("  aisa sign nahi hai. 'signs' likho list ke liye."); return; }
    seqStop();
    setHand(g->pct);
    Serial.print("  "); Serial.println(g->label);
    return;
  }

  if (cmd.startsWith("seq")) {
    String id = cmd.substring(3); id.trim();
    const Seq* q = findSeq(id);
    if (!q) { Serial.println("  aisa gesture nahi hai. 'signs' likho list ke liye."); return; }
    seqStart(q);
    Serial.print("  "); Serial.print(q->label); Serial.println(" chalu...");
    return;
  }

  if (cmd.startsWith("idle")) {
    String rest = cmd.substring(4);
    rest.trim();
    if (rest.length() == 0) {
      if (idleMs == 0) Serial.println("  auto-relax: BAND hai");
      else Serial.printf("  auto-relax: %lu second baad\n", idleMs / 1000UL);
      Serial.println("  Format: idle <second>   (idle 0 = band)");
      return;
    }
    long v = rest.toInt();
    if (v < 0 || v > 600) { Serial.println("  0 se 600 second ke bich do."); return; }
    idleMs = (uint32_t)v * 1000UL;
    lastMoveMs = millis();
    if (idleMs == 0) Serial.println("  auto-relax BAND. Servo position pakde rahega (garam ho sakta hai).");
    else Serial.printf("  auto-relax: hilna band hone ke %ld second baad PWM off.\n", v);
    return;
  }

  if (cmd.startsWith("speed")) {
    float v = cmd.substring(5).toFloat();
    if (v >= 1 && v <= 15) {
      speedUsPerTick = v;
      Serial.printf("  Speed = %.1f us/tick\n", speedUsPerTick);
    } else {
      Serial.println("  Speed 1 se 15 ke bich do. Jaise: speed 3");
    }
    return;
  }

  // ---- calibration nudge:  +   -   + 50   -50 ----
  if (cmd.charAt(0) == '+' || cmd.charAt(0) == '-') {
    int sign = (cmd.charAt(0) == '+') ? 1 : -1;
    String rest = cmd.substring(1);
    rest.trim();
    int step = (rest.length() > 0) ? rest.toInt() : 10;
    if (step <= 0) step = 10;
    calNudge(sign * step);
    return;
  }

  // ================= single-letter commands =================
  char c = cmd.charAt(0);

  if (c == 'a' && cmd.length() > 1) {
    int pct = cmd.substring(1).toInt();
    for (uint8_t i = 0; i < NUM_FINGERS; i++) setFinger(i, pct);
    Serial.printf("  Saari ungliyan -> %d%%\n", pct);
    return;
  }

  if (c == 's') {
    int sp  = cmd.indexOf(' ');
    int sp2 = cmd.indexOf(' ', sp + 1);
    if (sp < 0 || sp2 < 0) { Serial.println("  Format: s <ch> <0-100>"); return; }
    int ch  = cmd.substring(sp + 1, sp2).toInt();
    int pct = cmd.substring(sp2 + 1).toInt();
    if (ch < 0 || ch >= NUM_FINGERS) { Serial.println("  ch 0 se 4 tak"); return; }
    setFinger(ch, pct);
    Serial.printf("  %s (ch%d) -> %d%%  = %dus\n",
                  fingers[ch].name, ch, pct, percentToUs(ch, pct));
    return;
  }

  if (c == 'u') {
    int sp  = cmd.indexOf(' ');
    int sp2 = cmd.indexOf(' ', sp + 1);
    if (sp < 0 || sp2 < 0) { Serial.println("  Format: u <ch> <us>"); return; }
    int ch = cmd.substring(sp + 1, sp2).toInt();
    int us = cmd.substring(sp2 + 1).toInt();
    if (ch < 0 || ch >= NUM_FINGERS) { Serial.println("  ch 0 se 4 tak"); return; }
    uint16_t safe = clampF(ch, us);
    if (safe != us) {
      Serial.printf("  (%dus limit se bahar tha -> %dus kiya)\n", us, safe);
    }
    targetUs[ch] = safe;
    relaxed = false;
    Serial.printf("  %s (ch%d) -> %dus\n", fingers[ch].name, ch, safe);
    return;
  }

  switch (c) {
    case 'o': setHand(G_OPEN);   Serial.println("  OPEN");      break;
    case 'f': setHand(G_FIST);   Serial.println("  FIST");      break;
    case 'p': setHand(G_POINT);  Serial.println("  POINT");     break;
    case 'v': setHand(G_PEACE);  Serial.println("  PEACE");     break;
    case 't': setHand(G_THUMBS); Serial.println("  THUMBS UP"); break;
    case 'k': setHand(G_ROCK);   Serial.println("  ROCK");      break;
    case 'n': setHand(G_PINCH);  Serial.println("  PINCH");     break;
    case 'r': relaxHand();                                      break;
    case 'w': printWhere();                                     break;
    case '?': printHelp();                                      break;
    case '0': case '1': case '2': case '3': case '4': case '5':
      setHand(G_COUNT[c - '0']);
      Serial.printf("  COUNT %c\n", c);
      break;
    default:
      Serial.printf("  '%s' samajh nahi aaya. '?' dabao list ke liye.\n", cmd.c_str());
  }
}

// ============================================================
//  WEB UI  --  WiFi + HTTP server
//  ------------------------------------------------------------
//  Browser se har ungli ko up/down kiya ja sakta hai.
//  JSON ke tukde R"(...)" raw literals se bane hain taaki
//  backslash-quote ka jhamela na ho.
// ============================================================
WebServer server(80);
String    wifiIp = "-";          // router wala IP -- badalta rehta hai
String    apIp   = "-";          // ESP32 ke khud ke hotspot ka IP -- fix

String jsonState() {
  String s;
  // Pehle se jagah maang lo. Bina iske har += par String dobara
  // allocate hoti hai (~100 baar per request), aur page har 700ms
  // request bhejta hai -- heap tukdon mein bat jata hai aur kuch
  // der baad allocation fail hone lagti hai (server girna shuru).
  s.reserve(900);
  s = "{";
  s += R"("heap":)";  s += (uint32_t)ESP.getFreeHeap();
  s += ",";
  s += R"("relaxed":)";  s += relaxed ? "true" : "false";
  s += R"(,"speed":)";   s += String(speedUsPerTick, 1);
  s += R"(,"idle":)";    s += (int)(idleMs / 1000UL);
  s += R"(,"seq":")";    s += (seqNow ? seqNow->id : "");
  s += R"(")";
  s += R"(,"ip":")";     s += wifiIp;
  s += R"(","f":[)";
  for (uint8_t i = 0; i < NUM_FINGERS; i++) {
    if (i) s += ",";
    s += R"({"n":")";  s += fingers[i].name;
    s += R"(","ch":)"; s += fingers[i].channel;
    s += R"(,"us":)";  s += (int)currentUs[i];
    s += R"(,"deg":)"; s += (int)usToDeg((uint16_t)currentUs[i]);
    s += R"(,"lo":)";  s += fingers[i].minUs;
    s += R"(,"hi":)";  s += fingers[i].maxUs;
    s += R"(,"lod":)"; s += (int)(usToDeg(fingers[i].minUs) + 0.5f);
    s += R"(,"hid":)"; s += (int)(usToDeg(fingers[i].maxUs) + 0.5f);
    s += R"(,"op":)";  s += fingers[i].usOpen;
    s += R"(,"cl":)";  s += fingers[i].usClosed;
    s += R"(,"opd":)"; s += (int)(usToDeg(fingers[i].usOpen)   + 0.5f);
    s += R"(,"cld":)"; s += (int)(usToDeg(fingers[i].usClosed) + 0.5f);
    s += "}";
  }
  s += "]}";
  return s;
}

void hState() { server.send(200, "application/json", jsonState()); }
void hRoot()  { server.send_P(200, "text/html", PAGE_HTML); }

// step ab DEGREE mein aata hai, microseconds mein nahi.
// Current degree nikaal ke usme jodte hain, phir wapas us banate
// hain -- isse baar-baar dabane par drift nahi hota.
void hNudge() {
  seqStop();                 // haath se hilaya => animation band
  int   ch = server.arg("ch").toInt();
  int   d  = server.arg("d").toInt();
  float st = server.arg("step").toFloat();
  if (st <= 0) st = 5;
  if (ch >= 0 && ch < NUM_FINGERS) {
    float now = usToDeg((uint16_t)targetUs[ch]);
    targetUs[ch] = clampF(ch, degToUs(now + d * st));
    relaxed = false;
  }
  hState();
}

// Dono chalte hain:  /api/set?ch=0&deg=170   ya   /api/set?ch=0&us=2294
void hSet() {
  seqStop();                 // haath se hilaya => animation band
  int ch = server.arg("ch").toInt();
  if (ch >= 0 && ch < NUM_FINGERS) {
    int us;
    if (server.hasArg("deg")) us = degToUs(server.arg("deg").toFloat());
    else                      us = server.arg("us").toInt();
    targetUs[ch] = clampF(ch, us);
    relaxed = false;
  }
  hState();
}

// Saare signs aur gestures ki list -- page inhi se buttons banata
// hai, isliye naya sign add karne par UI apne aap update ho jata hai.
void hList() {
  String s = R"({"signs":[)";
  for (uint8_t i = 0; i < SIGN_N; i++) {
    if (i) s += ",";
    s += R"({"id":")"; s += SIGNS[i].id;
    s += R"(","l":")"; s += SIGNS[i].label; s += R"("})";
  }
  s += R"(],"seqs":[)";
  for (uint8_t i = 0; i < SEQ_N; i++) {
    if (i) s += ",";
    s += R"({"id":")"; s += SEQS[i].id;
    s += R"(","l":")"; s += SEQS[i].label; s += R"("})";
  }
  s += "]}";
  server.send(200, "application/json", s);
}

// ---- CALIBRATION over HTTP ----
// Ungli ko slider se jahan chahiye wahan le jao, phir "YAHI OPEN"
// ya "YAHI CLOSED" daba do. Direction ulti ho to koi baat nahi --
// usOpen > usClosed bhi chalta hai, percentToUs dono taraf sahi
// kaam karta hai. Aakhir mein SAVE -> flash (reboot par bhi rahega).
void hSetOpen() {
  int ch = server.arg("ch").toInt();
  if (ch >= 0 && ch < NUM_FINGERS) fingers[ch].usOpen = (uint16_t)targetUs[ch];
  hState();
}

void hSetClose() {
  int ch = server.arg("ch").toInt();
  if (ch >= 0 && ch < NUM_FINGERS) fingers[ch].usClosed = (uint16_t)targetUs[ch];
  hState();
}

void hSave()     { calSave();                 hState(); }
void hLoad()     { calLoad(true);             hState(); }
void hDefaults() { calDefaults();             hState(); }

void hSign() {
  const Sign* g = findSign(server.arg("n"));
  if (g) { seqStop(); setHand(g->pct); }
  hState();
}

void hSeq() {
  const Seq* q = findSeq(server.arg("n"));
  if (q) seqStart(q);
  hState();
}

void hSeqStop() { seqStop(); hState(); }

void hIdle() {
  long v = server.arg("s").toInt();
  if (v >= 0 && v <= 600) { idleMs = (uint32_t)v * 1000UL; lastMoveMs = millis(); }
  hState();
}

void hG() {
  String n = server.arg("n");
  if      (n == "open")  setHand(G_OPEN);
  else if (n == "fist")  setHand(G_FIST);
  else if (n == "relax") relaxHand();
  hState();
}

// ------------------------------------------------------------
//  WiFi : AP aur STA, DONO ek saath
//  ------------------------------------------------------------
//  Laptop ke bina chalane ke liye yeh zaroori hai.
//
//  STA = ghar ke router se judta hai. IP router deta hai (DHCP),
//        aur power cycle ke baad wo BADAL SAKTA hai. Tab purana
//        link nahi khulega aur bina laptop ke naya IP dekhne ka
//        koi raasta nahi hota.
//
//  AP  = ESP32 ka apna hotspot. Iska IP HAMESHA 192.168.4.1
//        rehta hai, kabhi nahi badalta. Router band ho, naya
//        ghar ho, kuch bhi ho -- yeh hamesha kaam karega.
//
//  mDNS = http://hand.local (iPhone/Mac par seedha chalta hai,
//         Android par kabhi-kabhi nahi).
//
//  Teeno ek saath chalte hain. Jo mile, usse khol lo.
// ------------------------------------------------------------
// Region INDIA = channel 1-13. ESP32 default US/FCC par hota hai
// (sirf 1-11) aur Android hotspot aksar ch12-13 par chala jata hai:
// network DIKHTA hai par JUDTA nahi. Radio reset ke baad yeh wapas
// lagana padta hai, isliye alag function.
void setRegionIN() {
  wifi_country_t ctry;
  memset(&ctry, 0, sizeof(ctry));
  strcpy(ctry.cc, "IN");
  ctry.schan  = 1;
  ctry.nchan  = 13;
  ctry.policy = WIFI_COUNTRY_POLICY_MANUAL;
  esp_wifi_set_country(&ctry);
}

// ---- ESP32 ka apna hotspot -- HAMESHA chalu ----
// Router ka IP bharosemand nahi: Android hotspot band-chalu hone par
// poora subnet naya bana deta hai (10.120.3.x -> 172.22.138.x ->
// 10.231.202.x -- teen baar badla). ESP32 ka apna hotspot hamesha
// 192.168.4.1 deta hai, kyunki DHCP wahi chalata hai. Isliye AP ko
// hamesha chalu rakhte hain, chahe router mila ho ya nahi.
//
// ZAROORI: AP ko STA ke JUDNE KE BAAD uthana hai. Pehle uthaya to
// ek hi radio dono ke liye channel chunta hai aur STA jud nahi pata
// -- yeh galti pehle ho chuki hai, din barbaad hua tha.
void startAp() {
  WiFi.mode(WiFi.status() == WL_CONNECTED ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(300);
  apIp = WiFi.softAPIP().toString();
}

// http://hand.local -- IP yaad rakhne ki zaroorat khatam.
// Windows aur iPhone mein built-in hai. Android Chrome mein kabhi
// chalta hai kabhi nahi -- isliye AP wala raasta bhi rakha hai.
void startMdns() {
  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
  }
}

void startWifi() {
  Serial.println();
  Serial.println("[WIFI] shuru kar rahe hain...");

  // SIDHA-SADA STA. Pehle yahi chal raha tha.
  // AP, dual-mode, setSleep, txPower -- sab hata diya; unhi se
  // WiFi driver gadbad ho raha tha (status 255 aa raha tha).
  // Hotspot sirf TAB uthta hai jab router bilkul na mile.
  if (strlen(WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);

    setRegionIN();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("  Router se jud rahe hain: ");
    Serial.println(WIFI_SSID);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
      delay(400);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      // Connection toote to ESP32 khud wapas jude. Bina iske board
      // chalta rehta hai par network se bahar ho jata hai -- serial
      // par jawab deta hai, par link nahi khulta. Reset karna padta
      // tha. Yeh built-in hai, boot wale connection ko nahi chhedta.
      WiFi.setAutoReconnect(true);
      wifiIp = WiFi.localIP().toString();
      Serial.print("  JUD GAYE: ");
      Serial.print(wifiIp);
      Serial.print("   signal ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("  Router se nahi juda -- apna hotspot bana rahe hain.");
    }
  }

  // Router mila ya nahi -- apna hotspot dono haalat mein chalu
  startAp();
  startMdns();

  Serial.println();
  Serial.println("  >>> BROWSER MEIN KHOLO -- teen raaste:");
  if (wifiIp != "-") {
    Serial.print("     1. http://"); Serial.print(wifiIp);
    Serial.println("        (router -- IP badal sakta hai)");
    Serial.print("     2. http://"); Serial.print(MDNS_NAME);
    Serial.println(".local            (naam -- IP badle to bhi chalega)");
  } else {
    Serial.println("     (router se nahi juda -- sirf hotspot chalega)");
  }
  Serial.print  ("     3. http://"); Serial.print(apIp);
  Serial.println("       <<< YE KABHI NAHI BADALTA");
  Serial.print  ("        pehle is WiFi se judo:  ");
  Serial.print(AP_SSID); Serial.print("  /  "); Serial.println(AP_PASS);
  Serial.println();

  server.on("/",          hRoot);
  server.on("/api/state", hState);
  server.on("/api/nudge", hNudge);
  server.on("/api/set",   hSet);
  server.on("/api/g",     hG);
  server.on("/api/idle",  hIdle);
  server.on("/api/list",  hList);
  server.on("/api/sign",  hSign);
  server.on("/api/seq",   hSeq);
  server.on("/api/stop",  hSeqStop);
  server.on("/api/setopen",  hSetOpen);
  server.on("/api/setclose", hSetClose);
  server.on("/api/save",     hSave);
  server.on("/api/load",     hLoad);
  server.on("/api/defaults", hDefaults);
  server.begin();

  Serial.println();
  Serial.print  ("  >>> BROWSER MEIN KHOLO:   http://");
  Serial.println(wifiIp);
  Serial.println();
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("==========================================");
  Serial.println("  HUMANOID PALM  --  5 FINGER CONTROL");
  Serial.printf ("  Connected fingers: %d / %d\n", FINGERS_CONNECTED, NUM_FINGERS);
  Serial.println("==========================================");

  // Flash mein calibration padi hai to wahi use karo
  calLoad();
  Serial.printf("  Safe range: %d - %d us\n", limMinUs, limMaxUs);

  startWifi();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  // ---- STEP A : PCA9685 hai ya nahi? ----
  Serial.println();
  Serial.println("[STEP A] I2C scan...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("   Device mila at 0x%02X", addr);
      if (addr == PCA_ADDRESS) { Serial.print("   <<< PCA9685"); pcaFound = true; }
      if (addr == 0x70)        Serial.print("   <<< all-call (normal)");
      Serial.println();
    }
  }

  if (!pcaFound) {
    Serial.println();
    Serial.println("  **** FAIL: PCA9685 0x40 par nahi mila ****");
    Serial.println("  Servo ko touch nahi kar rahe. Check karo:");
    Serial.println("    1. VCC <- 3V3 , GND <- GND (common ground!)");
    Serial.println("    2. SDA <- GPIO22 , SCL <- GPIO23");
    Serial.println("    3. Jumper wire dheela hai?");
    return;
  }

  Serial.println("  **** PASS: PCA9685 mil gaya ****");

  // ---- STEP B : PWM chalu karo ----
  Serial.println();
  Serial.println("[STEP B] PCA9685 -> 50Hz");
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);
  delay(100);

  // ---- STEP C : har ungli ko DHEERE se OPEN par lao ----
  // Ek saath paanchon ko jhatka dene se current spike aata hai,
  // isliye ek-ek ungli ko bari-bari se start kar rahe hain.
  Serial.println();
  Serial.println("[STEP C] Ungliyan OPEN position par la rahe hain (ek-ek)...");
  for (uint8_t i = 0; i < NUM_FINGERS; i++) {
    currentUs[i] = clampF(i, fingers[i].usOpen);
    targetUs[i]  = currentUs[i];
    if (i < FINGERS_CONNECTED) {
      writeUs(fingers[i].channel, (uint16_t)currentUs[i]);
      Serial.printf("   %-7s ch%d -> %dus\n",
                    fingers[i].name, fingers[i].channel, (int)currentUs[i]);
      delay(400);                     // current spike se bachne ke liye
    }
  }

  lastMoveMs = millis();

  Serial.println();
  Serial.println("  Taiyar. Serial Monitor mein command type karo.");
  printHelp();
  Serial.print("> ");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  // Web server hamesha chale -- PCA na mile tab bhi page khulna chahiye
  server.handleClient();

  if (!pcaFound) { delay(20); return; }

  // Serial se command padho
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    Serial.println(line);
    handleCommand(line);
    Serial.print("> ");
  }

  // ---- RELAX se wapas jaagna ----
  // Auto-relax ne PWM band kiya tha. Naya command aaya to relaxed
  // false ho gaya -- par agar target wahi hai jahan servo pehle se
  // hai, to updateMotion kuch likhega hi nahi aur PWM band hi reh
  // jayega. Isliye jaagte hi current position dobara likh dete hain.
  static bool prevRelaxed = false;
  if (prevRelaxed && !relaxed) {
    for (uint8_t i = 0; i < NUM_FINGERS; i++) {
      if (channelLive(i)) writeUs(fingers[i].channel, clampF(i, (int)currentUs[i]));
    }
    lastMoveMs = millis();
  }
  prevRelaxed = relaxed;

  // ---- WiFi par nazar ----
  // setAutoReconnect kabhi-kabhi chook jata hai. Har 20 sec dekhte
  // hain; toota ho to khud reconnect maarte hain. Yahan sirf
  // reconnect() -- mode ya disconnect ko haath NAHI lagate, usi se
  // pichli baar WiFi driver gadbad hua tha (status 255).
  // WiFi.reconnect() stored config se judta hai, aur connection poora
  // tootne par wo config KHALI ho jati hai (net par SSID blank dikhta
  // tha) -- isliye wo kabhi kaam nahi karta tha. Seedha begin() se
  // jodte hain, aur chaar baar fail ho to radio poora band-chalu.
  static unsigned long lastNetChk = 0;
  static uint8_t netFails = 0;
  if (strlen(WIFI_SSID) > 0 && millis() - lastNetChk > 15000) {
    lastNetChk = millis();
    if (WiFi.status() != WL_CONNECTED) {
      netFails++;
      Serial.println();
      Serial.print("  [wifi] toota -- dobara jud rahe hain (koshish ");
      Serial.print(netFails);
      Serial.println(")");

      if (netFails >= 4) {
        // SIRF STA side reset -- mode ko haath nahi lagate.
        // Pehle yahan WIFI_OFF -> WIFI_STA karte the, par ab AP
        // hamesha chalu rehta hai aur mode badalne se wo gir jata,
        // yaani 192.168.4.1 wala pakka raasta bhi tooth jata.
        // disconnect(true) sirf station ko girata hai, AP ko nahi.
        Serial.println("  [wifi] station reset kar rahe hain...");
        WiFi.disconnect(true);
        delay(200);
        setRegionIN();
        netFails = 0;
      }

      WiFi.begin(WIFI_SSID, WIFI_PASS);
    } else {
      netFails = 0;
      String ip = WiFi.localIP().toString();
      if (ip != "0.0.0.0" && ip != wifiIp) {
        wifiIp = ip;
        Serial.println();
        Serial.print("  [wifi] naya IP: http://");
        Serial.println(wifiIp);
      }
    }
  }

  // Chal raha gesture aage badhao
  seqUpdate();

  // Har 20ms mein ungliyan thoda-thoda khiskao
  if (millis() - lastTick >= TICK_MS) {
    lastTick = millis();
    if (!relaxed) updateMotion();
  }

  // ---- AUTO-RELAX ----
  // Kuch der se kuch hila nahi => PWM band. Servo ko lagatar zor
  // lagate rehne se bachata hai (yahi garmi ki asli wajah thi).
  if (idleMs > 0 && !relaxed && !autoRelaxed && lastMoveMs > 0
      && millis() - lastMoveMs > idleMs) {
    for (uint8_t i = 0; i < NUM_FINGERS; i++) {
      if (channelLive(i)) pwm.setPWM(fingers[i].channel, 0, 0);
    }
    relaxed      = true;
    autoRelaxed  = true;
    prevRelaxed  = true;
    Serial.println();
    Serial.println("  [auto-relax] kuch der se kuch hila nahi -- PWM band.");
    Serial.println("  Koi bhi command dene par wapas chalu ho jayega.");
    Serial.print("> ");
  }
}
