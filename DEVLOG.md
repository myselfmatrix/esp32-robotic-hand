> ## Haath khol ke le ja rahe ho?
> **[WIRING.md](WIRING.md)** mein saare taar, channel, calibration aur WiFi likhe hain.
> Dobara jodne ke liye bas wahi file kaafi hai.
>
> Yaad rakhna: channel ka kram **ULTA** hai -- CH4 = THUMB, CH0 = PINKY.

# Robotic Hand — ESP32 + PCA9685 + SG90

Paanch-ungli ka humanoid palm. ESP32 PCA9685 ko I2C se chalata hai, PCA9685 har
ungli ke servo ko PWM deta hai. Sab kuch phase-wise verify karke aage badhaya ja
raha hai — har phase hardware par apni aankhon se dekhne ke baad hi agla phase.

---

## Hardware

| Cheez | Detail |
|---|---|
| Board | ESP32 DevKit V1 (ESP-WROOM-32) |
| Port | COM6 — Silicon Labs CP210x (`VID:PID 10C4:EA60`) |
| Servo driver | PCA9685, I2C address `0x40` |
| Servo | SG90, 50Hz |
| Servo power | SMPS +5V (5.01V par verified) |

### Wiring

```
ESP32 3V3    -> PCA9685 VCC          (sirf logic power)
ESP32 GND    -> PCA9685 GND          (6-pin header wala)
ESP32 GPIO22 -> PCA9685 SDA
ESP32 GPIO23 -> PCA9685 SCL
ESP32 GND    -> PCA9685 OE           (dusra GND pin)
ESP32 VIN    -> kuch nahi            (KHAALI rehna chahiye)

SMPS +5V     -> CH14 ka LAAL pin     (V+ row = servo power)
SMPS -V      -> CH14 ka KAALA pin    (GND row)
```

Teen baatein jo bhoolne par ghanto debug hota hai -- teenon yahan ho chuki hain:

- **Green screw terminal mein dupont pin mat daalna.** Dupont ke male pin gol hote
  hain, aur screw terminal ki clamp unhe pakad nahi pati. Bahar se bilkul juda hua
  dikhta hai, andar se contact zero -- aur kasne se koi farak nahi padta. V+ rail
  isi wajah se shuru se murda thi. Isiliye ab power **channel header ki laal (V+)
  row** se di jati hai, jo asli male pin hai aur female dupont use kas ke pakadta
  hai. CH14 par servo mat lagana.

- **6-pin header wale `V+` pin se power mat dena.** Uske bilkul bagal mein `VCC`
  hai. Ek pin khisak gaya to 5V seedha ESP32 ki 3.3V rail par chala jayega.
  (Header ka kram: `GND OE SCL SDA VCC V+`.)

- **SMPS aur VIN ek saath mat jodna.** Dono V+ rail se jud gaye to do supply
  aamne-saamne aa jayengi aur SMPS ka current ESP32 hokar USB ki taraf push karega.
  USB sirf ESP32 ki logic ke liye, SMPS sirf servo ke liye.

Common ground apne aap ban jata hai: SMPS ka `-V` channel ki GND row par jata hai,
aur woh row board ke andar 6-pin wale GND se judi hai, jo ESP32 ke GND se juda hai.
Yeh zaroori hai -- iske bina servo ko PWM ka reference nahi milta aur woh chup
reh jata hai.

**USB sirf ek servo jhel sakta hai.** Laptop ka USB ~0.5A deta hai. Teen servo ek
saath jodne par port apni current limit par band ho jata hai, serial `A device
attached to the system is not functioning` dene lagta hai, aur kuch nahi hilta --
bilkul wiring fault jaisa dikhta hai. USB nikaalo, load hatao, sab wapas theek.
Ek se zyada servo = SMPS zaroori.

### I2C pins default nahi hain

ESP32 par default `Wire` pins SDA=21, SCL=22 hote hain. Yahan **22 aur 23** use ho
rahe hain, jo bilkul theek hai (ESP32 mein I2C GPIO matrix se jata hai, koi bhi
output pin chalega; 22/23 na strapping pin hain na input-only na flash wale).
Par iska matlab hai: koi bhi library ya example code jo khaali `Wire.begin()`
likhta hai, woh 21/22 pakdega aur **device nahi milega**. Hamesha pins pass karo:

```cpp
Wire.begin(22, 23);
```

### Servo channels

| CH | Ungli |
|---|---|
| 0 | THUMB (angutha) |
| 1 | INDEX (tarjani) |
| 2 | MIDDLE (bich wali) |
| 3 | RING (anamika) |
| 4 | PINKY (chhoti) |

---

## Current ka hisaab — sabse zaroori baat

Paanch SG90 ek saath chalein to peak current **2.5–3A** tak ja sakta hai. 1A wala
SMPS ESP32 ko brown-out se reboot karata rahega, aur yeh aisa fault hai jo code ki
galti jaisa dikhta hai.

Code mein do cheezein isi liye hain:

1. **Startup par ungliyan ek-ek kar ke** (400ms gap) position par aati hain, saath
   nahi — taaki shuruati inrush current flat rahe.
2. **Motion dheemi hai**: har 20ms mein sirf ~4µs. `speed` command se badla ja
   sakta hai, par board reboot karne lage to isi ko kam karo.

---

## Build aur upload

`pio` PATH par nahi hai, poora path use karo:

```
C:\Users\Hp\.platformio\penv\Scripts\pio.exe
```

Do environments hain, kyunki dono programs mein apna `setup()`/`loop()` hai — ek
saath compile karne par linker "multiple definition" error dega.

| Env | File | Kaam |
|---|---|---|
| `palm` *(default)* | `src/palm/main.cpp` | 5 ungli, gestures, calibration |
| `ch0_test` | `src/ch0_test/main.cpp` | Phase 2+3 — I2C scan + ek servo ka sweep |

```bash
pio run -e palm -t upload -t monitor    # palm chadhao + monitor kholo
pio run -e palm                         # sirf build (hardware ki zarurat nahi)
pio run -e ch0_test -t upload           # purana test, wiring par shaq ho to
```

Common settings (board, COM6, lib_deps, upload speed) `platformio.ini` ke `[env]`
section mein hain — ek jagah badlo, dono envs par lag jata hai.

### Upload na chade to

`Failed to connect to ESP32: Timed out waiting for packet header` aaye to board ko
manually download mode mein daalo:

1. **BOOT** dabaye rakho
2. **EN** ek baar daba ke chhod do
3. **BOOT** chhod do
4. Turant upload chalao

Upload ke beech mein **EN mat dabao** — flash aadhi likhi reh jayegi.

---

## Abhi kahan tak pahuncha hai

- [x] Phase 1 — wiring, SMPS 5.01V verified
- [x] Phase 2 — I2C scan, PCA9685 `0x40` par milna
- [x] Phase 3 — CH0 par ek servo ka safe sweep (1200–1800µs)
- [x] Phase 4 — 5-ungli firmware: gestures, non-blocking motion, calibration
- [ ] **CH0 par pehli harkat aankhon se confirm karna** (abhi yahin hain)
- [ ] Har ungli ki calibration (asli open/closed µs)
- [ ] Baaki 4 servo jodna, `FINGERS_CONNECTED` badhana
- [ ] Phone app — ESP32 par WiFi web UI (install-free, kisi bhi phone par)

`FINGERS_CONNECTED` abhi **1** hai, kyunki sirf CH0 par servo laga hai. Code un
channels par pulse bhejta hi nahi jahan servo nahi hai. Ungli jodte jao, yeh number
badhate jao — bas ek line badalni hai.

---

## Serial commands

Monitor 115200 baud par. `?` dabane par list dikh jati hai.

### Gestures

| Cmd | Kaam |
|---|---|
| `o` | OPEN — haath khola |
| `f` | FIST — muthhi |
| `p` | POINT — ek ungli |
| `v` | PEACE |
| `t` | THUMBS UP |
| `k` | ROCK (sing) |
| `n` | PINCH (pakadna) |
| `0`–`5` | utni ungli dikhao (ginti) |

### Manual control

| Cmd | Kaam |
|---|---|
| `a 60` | saari ungliyan 60% mod do |
| `s 0 60` | sirf ch0 ko 60% |
| `u 0 1450` | ch0 ko seedha 1450µs par |
| `speed 3` | raftaar (1–15, default 4) |
| `r` | RELAX — PWM off, servo dheela |
| `w` | WHERE — abhi kahan hai |

`0` = ungli poori khuli, `100` = poori mudi. Yeh percent har ungli ke apne
`usOpen`/`usClosed` se nikalta hai, isliye calibration badalne par **gestures khud
sahi ho jate hain** — har gesture alag se theek nahi karna padta.

---

## Calibration — asli numbers kaise nikalein

`1600`/`1300` sirf andaza hai. Asli numbers aapke mechanism par depend karte hain,
aur har ungli ke alag honge. Calibration **flash (NVS) mein save hoti hai**, toh
reboot ke baad bhi yaad rehti hai — code edit karne ki zarurat nahi.

Ek ungli ka tarika:

```
cal 0          # ch0 chuna; ungli uski abhi ki 'open' position par aa jati hai
+              # 10us aage
+ 50           # 50us aage
-              # 10us peeche
setopen        # jahan ungli POORI SEEDHI lage, wahan yeh maaro
+ 50           # ab mudne ki taraf khiskao
setclose       # jahan ungli ARAAM SE mudi ho (zor lagta hua NAHI), wahan yeh
save           # flash mein pakka
table          # numbers dekho (code mein paste karne layak format)
cal 1          # agli ungli
exit           # calibration mode band
```

Teen cheezein dhyan rakhni hain:

- **Zor lagta hua dikhe to wahin ruk jao.** Servo stall ho raha hai — garam hoga,
  current kheenchega, aur gear kha sakta hai. "Araam se mudi" wali position hi
  `setclose` hai, "zyada se zyada mudi" nahi.
- **Ungli ulti taraf mude** to us ungli ke `usOpen` aur `usClosed` aapas mein badal
  do (swap). Code mein ulta range bhi theek kaam karta hai.
- **Safe range se bahar jaana ho** to `limits 1100 1900` jaise command se badhao,
  dheere-dheere. Absolute deewar `1000–2000` hai, usse aage code jaane hi nahi
  dega — SG90 wahan mechanical stop se takrata hai.

Baaki calibration commands: `load` (flash se wapas padho), `defaults` (code ke
shuruati numbers), `table` (abhi ke numbers).

---

## Troubleshooting

**`PCA9685 0x40 par nahi mila`** — code servo ko chhuta bhi nahi, ruk jata hai.
Check: VCC↔3V3, GND↔GND (common ground), SDA↔GPIO22, SCL↔GPIO23, dheele jumper.
I2C scan har device ka address print karta hai, toh `0x40` ke bajaye kuch aur dikhe
to board ke address jumpers dekho.

**Board baar-baar reboot karta hai** — brown-out. SMPS chhota hai ya `speed` zyada
hai. `speed 2` karo; na banay to bada SMPS.

**`pio` ImportError deta hai (compile error nahi)** — PlatformIO ka Python env toota
hai, project theek hai. Fix:

```bash
C:\Users\Hp\.platformio\penv\Scripts\python.exe -m pip install --ignore-installed --no-deps certifi charset_normalizer
```

(Yeh ek baar ho chuka hai — `certifi` folder khali ho gaya tha aur har `pio` command
`cannot import name 'where' from 'certifi'` par mar raha tha.)

**Servo jhatke maarta hai ya hil-hil karta hai** — V+ par power theek nahi, ya
ground common nahi. Pehle multimeter se V+ par 5V check karo servo chalte waqt.
