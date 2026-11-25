/* ============================================================
   DIAGNOSTIC : SERVO SEEDHA ESP32 SE  (PCA9685 beech se hata ke)
   ------------------------------------------------------------
   Maqsad: yeh pata karna ki servo aur power theek hain ya nahi.
   Yahan PCA9685 ka koi role nahi -- PWM seedha ESP32 ke GPIO se
   nikalta hai (LEDC hardware timer se).

   ------------------------------------------------------------
   WIRING -- sirf EK taar hilana hai:

     Servo ORANGE  -> ESP32 D18        <-- yeh badla
     Servo RED     -> PCA9685 ki V+ row   (jaisa tha, waisa hi)
     Servo BROWN   -> PCA9685 ki GND row  (jaisa tha, waisa hi)

   Power aur ground bilkul mat chhedna. ESP32 ka GND aur SMPS ka
   -V pehle se jude hain (PCA9685 ke through), isliye signal ko
   sahi reference mil jayega.

   ------------------------------------------------------------
   NATEEJA KA MATLAB:
     Servo hila      -> servo + power theek. Kharabi PCA9685 mein.
     Servo nahi hila -> PCA9685 bekasoor. Power ya servo mein kharabi.
   ============================================================ */

#include <Arduino.h>

#define SERVO_PIN    18        // D18
#define LEDC_CH      0
#define LEDC_FREQ    50        // servo ko 50Hz chahiye
#define LEDC_BITS    16        // 65536 steps, 50Hz par aaram se chalta hai

#define PERIOD_US    20000UL   // 50Hz = 20ms ka period

// Safe range -- SG90 ko mechanical stop se bachane ke liye
#define MIN_US       1200
#define MAX_US       1800

uint16_t currentUs = 1500;

// Pulse width ko LEDC duty mein badlo aur bhej do
void writeUs(uint16_t us) {
  if (us < MIN_US) us = MIN_US;
  if (us > MAX_US) us = MAX_US;
  uint32_t duty = ((uint32_t)us * ((1UL << LEDC_BITS) - 1)) / PERIOD_US;
  ledcWrite(LEDC_CH, duty);
  currentUs = us;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("==========================================");
  Serial.println("  SERVO DIRECT TEST  --  bina PCA9685 ke");
  Serial.printf ("  Signal pin: D%d    Freq: %dHz\n", SERVO_PIN, LEDC_FREQ);
  Serial.printf ("  Range: %d - %d us\n", MIN_US, MAX_US);
  Serial.println("==========================================");
  Serial.println();
  Serial.println("  WIRING CHECK:");
  Serial.printf ("    servo ORANGE -> ESP32 D%d\n", SERVO_PIN);
  Serial.println("    servo RED    -> PCA9685 V+ row (jaisa tha)");
  Serial.println("    servo BROWN  -> PCA9685 GND row (jaisa tha)");
  Serial.println();

  ledcSetup(LEDC_CH, LEDC_FREQ, LEDC_BITS);
  ledcAttachPin(SERVO_PIN, LEDC_CH);

  Serial.println("  CENTER (1500us) par la rahe hain...");
  writeUs(1500);
  delay(1000);

  Serial.println("  >>> AB SERVO KHUD-BA-KHUD JHOOLNA CHAHIYE <<<");
  Serial.println("  (commands: u <us> , c = center , s = stop)");
  Serial.println();
}

// Dheere-dheere ek position se dusri tak
void sweepTo(uint16_t toUs, int stepUs = 8, int delayMs = 15) {
  int from = currentUs;
  if (from < toUs) for (int us = from; us <= toUs; us += stepUs) { writeUs(us); delay(delayMs); }
  else             for (int us = from; us >= toUs; us -= stepUs) { writeUs(us); delay(delayMs); }
  writeUs(toUs);
}

bool autoSweep = true;

void loop() {
  // Serial se command aaye to pehle woh dekho
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd.startsWith("u")) {
      int us = cmd.substring(1).toInt();
      autoSweep = false;
      writeUs(us);
      Serial.printf("  -> %dus (auto sweep band)\n", currentUs);
    } else if (cmd == "c") {
      autoSweep = false;
      writeUs(1500);
      Serial.println("  -> CENTER 1500us");
    } else if (cmd == "s") {
      autoSweep = !autoSweep;
      Serial.printf("  Auto sweep %s\n", autoSweep ? "CHALU" : "BAND");
    } else if (cmd.length()) {
      Serial.println("  commands: u <us> | c | s");
    }
  }

  if (!autoSweep) { delay(50); return; }

  Serial.println("  LEFT 1200us");
  sweepTo(1200);
  delay(600);

  Serial.println("  RIGHT 1800us");
  sweepTo(1800);
  delay(600);
}
