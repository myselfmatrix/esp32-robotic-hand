/* ============================================================
   PHASE 2 + 3  :  I2C CHECK  +  CH0/CH1/CH2 SERVO TEST
   ------------------------------------------------------------
   Yeh program do kaam karta hai:

   STEP A) I2C bus scan karke PCA9685 (0x40) dhoondhta hai.
           Agar na mile -> servo ko chhuta bhi nahi, error print
           karke ruk jaata hai. (Safety)

           STEP B) Mile to CH0, CH1, CH2 ke servo ko ek-ek karke
           DHEERE-DHEERE ghumata hai, ek CHHOTI safe range mein.

   ------------------------------------------------------------
   WIRING (already done):
     ESP32 3V3    -> PCA9685 VCC    (logic power)
     ESP32 GND    -> PCA9685 GND    (common ground)
     ESP32 GPIO22 -> PCA9685 SDA
     ESP32 GPIO23 -> PCA9685 SCL
     SMPS +5V     -> PCA9685 V+     (servo power, 5.01V verified)
     SMPS -V      -> PCA9685 GND
     Servo        -> CH0  (brown=GND, red=V+, orange=PWM)

   ------------------------------------------------------------
   SAFETY: Pehli baar hum poori 0-180 range use NAHI kar rahe.
   Sirf 1200us se 1800us tak (~lagbhag 54 degree ka jhoola).
   Isse servo mechanical stop se nahi takrayega. Range baad
   mein, confirm hone par, badhayenge.
   ============================================================ */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ---------------- Settings ----------------
#define SDA_PIN      22
#define SCL_PIN      23
#define PCA_ADDRESS  0x40
#define SERVO_COUNT  3

// SG90 pulse widths (microseconds)
#define US_CENTER    1500     // ~90 degree
#define US_SAFE_MIN  1200     // ~63 degree  (conservative)
#define US_SAFE_MAX  1800     // ~117 degree (conservative)

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA_ADDRESS, Wire);

bool pcaFound = false;

// Servo ko dheere-dheere ek position se dusri tak le jao
void slowMove(uint8_t channel, int fromUs, int toUs, int stepUs = 4, int delayMs = 18) {
  if (fromUs < toUs) {
    for (int us = fromUs; us <= toUs; us += stepUs) {
      pwm.writeMicroseconds(channel, us);
      delay(delayMs);
    }
  } else {
    for (int us = fromUs; us >= toUs; us -= stepUs) {
      pwm.writeMicroseconds(channel, us);
      delay(delayMs);
    }
  }
  pwm.writeMicroseconds(channel, toUs);
}

void sweepServo(uint8_t channel) {
  Serial.printf("\n--- CH%d sweep ---\n", channel);
  slowMove(channel, US_CENTER, US_SAFE_MIN);
  delay(500);
  slowMove(channel, US_SAFE_MIN, US_CENTER);
  delay(500);
  slowMove(channel, US_CENTER, US_SAFE_MAX);
  delay(500);
  slowMove(channel, US_SAFE_MAX, US_CENTER);
  delay(700);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("==========================================");
  Serial.println("  ESP32 + PCA9685 + SG90   CH0/1/2 TEST");
  Serial.println("  SDA=GPIO22  SCL=GPIO23   addr=0x40");
  Serial.println("==========================================");

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  // ---------------- STEP A : I2C SCAN ----------------
  Serial.println();
  Serial.println("[STEP A] I2C bus scan kar rahe hain...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      found++;
      Serial.printf("   Device mila at 0x%02X", addr);
      if (addr == PCA_ADDRESS) {
        Serial.print("   <<< PCA9685  (YEH CHAHIYE THA)");
        pcaFound = true;
      }
      if (addr == 0x70) Serial.print("   <<< PCA9685 all-call (normal)");
      Serial.println();
    }
  }

  if (found == 0) {
    Serial.println("   >> Koi I2C device nahi mila.");
  }

  if (!pcaFound) {
    Serial.println();
    Serial.println("  **** FAIL: PCA9685 0x40 par nahi mila ****");
    Serial.println("  Servo ko touch nahi kar rahe. Yeh check karo:");
    Serial.println("    1. PCA9685 VCC  <- ESP32 3V3");
    Serial.println("    2. PCA9685 GND  <- ESP32 GND");
    Serial.println("    3. PCA9685 SDA  <- GPIO22");
    Serial.println("    4. PCA9685 SCL  <- GPIO23");
    Serial.println("    5. Jumper wires dheele hain?");
    return;              // Yahin ruk jao
  }

  Serial.println();
  Serial.println("  **** PASS: PCA9685 mil gaya! ****");

  // ---------------- STEP B : SERVO TEST ----------------
  Serial.println();
  Serial.println("[STEP B] PCA9685 ko 50Hz par set kar rahe hain...");
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);   // PCA9685 internal clock
  pwm.setPWMFreq(50);                     // SG90 ko 50Hz chahiye
  delay(100);

  Serial.println("   CH0, CH1, CH2 ko CENTER (1500us) par la rahe hain...");
  for (uint8_t channel = 0; channel < SERVO_COUNT; ++channel) {
    pwm.writeMicroseconds(channel, US_CENTER);
    delay(500);
  }
  Serial.println("   >>> Ab CH0, CH1, CH2 ek-ek karke hilne chahiye. <<<");
}

void loop() {
  if (!pcaFound) {
    delay(5000);
    return;
  }

  Serial.println();
  Serial.println("--- Sweep cycle: CH0, CH1, CH2 ek-ek karke ---");
  for (uint8_t channel = 0; channel < SERVO_COUNT; ++channel) {
    Serial.printf("  CH%d: center -> left -> center -> right -> center\n", channel);
    slowMove(channel, US_CENTER, US_SAFE_MIN);
    delay(500);
    slowMove(channel, US_SAFE_MIN, US_CENTER);
    delay(500);
    slowMove(channel, US_CENTER, US_SAFE_MAX);
    delay(500);
    slowMove(channel, US_SAFE_MAX, US_CENTER);
    delay(700);
  }
  Serial.println("--- Cycle poora. Dobara shuru... ---");
}
