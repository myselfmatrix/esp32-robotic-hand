# WIRING — dobara jodne ke liye poori list

Haath khol ke le jaa rahe ho. Yahan **sab kuch** likha hai — taar, channel,
calibration, WiFi. College mein bas yeh file khol lena.

> **Sabse zaroori baat:** channel ka kram **SEEDHA** hai (naya haath, 22 Nov 2026).
> CH0 par THUMB, CH4 par PINKY. Purane haath mein ULTA tha — agar gesture
> ulta chale to sabse pehle yahi check karna.

---

## 1. ESP32 ↔ PCA9685 (4 taar)

| ESP32 | → | PCA9685 | Kaam |
|---|---|---|---|
| `3V3` | → | `VCC` | logic power |
| `GND` | → | `GND` | **common ground — ye zaroori hai** |
| `GPIO22` (D22) | → | `SDA` | I2C data |
| `GPIO23` (D23) | → | `SCL` | I2C clock |

PCA9685 ka address `0x40` hai (koi address jumper solder nahi karna).

---

## 2. Servo power — SMPS (5V 5A)

⚠️ **Green screw terminal KHARAB hai — usme mat lagana.** Power seedha
channel ke pins par jaati hai:

| SMPS | → | Kahan |
|---|---|---|
| `+5V` | → | **CH15 ka LAAL pin** (beech wali row = V+) |
| `−V` | → | **CH15 ka KAALA pin** (neeche wali row = GND) |

`V+` poore board par **ek hi rail** hai — CH15 par diya hua 5V saare
channels ke laal pin tak apne aap pahunch jata hai.

---

## 3. ESP32 ki power

**USB se** — laptop, phone charger, ya power bank. Koi bhi 5V USB chalega.

❌ **VIN se mat karna.** Try kiya tha — board boot hi nahi hota, brown-out
hoke hang ho jata hai. Time barbaad mat karna.

---

## 4. Servo channels — ULTA KRAM

```
PCA9685
┌─────────────────────────────────────────────────┐
│  ch0    ch1    ch2    ch3    ch4  ...  ch15     │
│   ▌      ▌      ▌      ▌      ▌         ▌       │  peela/orange = PWM
│   ▌      ▌      ▌      ▌      ▌         ▌       │  LAAL         = V+  (5V)
│   ▌      ▌      ▌      ▌      ▌         ▌       │  KAALA/BROWN  = GND
│   │      │      │      │      │         ▲       │
│ THUMB  INDEX MIDDLE  RING   PINKY     SMPS      │
└─────────────────────────────────────────────────┘
```

| Channel | Ungli |
|---|---|
| `CH0` | **THUMB** (angutha) |
| `CH1` | **INDEX** (tarjani) |
| `CH2` | **MIDDLE** (bich wali) |
| `CH3` | **RING** (anamika) |
| `CH4` | **PINKY** (chhoti) |
| `CH15` | SMPS power input |

Servo ka taar: **brown → GND**, **red → V+**, **orange/yellow → PWM**
(connector seedha chadhao, GND wala pin board ke kinare ki taraf).

Agar taar badal jayein to code mein sirf ek table badalni hai —
[`src/palm/main.cpp`](src/palm/main.cpp) mein `Finger fingers[]` ka `ch` column.
Saare signs/gestures ungli ke **naam** se likhe hain, channel se nahi.

---

## 5. Servo horn aur thread

- Saare horn **15°** par, **ungliyon ki taraf** point karte hue lagaye gaye the
- Thread horn ke **sabse bahar wale hole** mein, ungli **seedhi** rakh ke bandhi
  thi — itni tight ki dheeli na rahe, par kheench ke nahi
- `15°` = ungli khuli. Yahi har ungli ka **OPEN** hai.

Dobara jodte waqt: servo ko pehle **15° par le jao**, phir horn lagao, phir thread
bandho. (Serial se: `deg 0 15`, `deg 1 15` … `deg 4 15`)

---

## 6. Calibration — flash mein saved hai

Ye numbers **flash (NVS) mein** aur **code ke defaults** mein, dono jagah hain.
Flash erase ho jaye tab bhi code se wapas aa jayenge.

| Ungli | CH | OPEN | CLOSED |
|---|---|---|---|
| THUMB | 0 | 15° (658us) | **110°** (1661us) |
| INDEX | 1 | 15° (658us) | **140°** (1978us) |
| MIDDLE | 2 | 15° (658us) | **175°** (2347us) |
| RING | 3 | 15° (658us) | **175°** (2347us) |
| PINKY | 4 | 15° (658us) | **158°** (2168us) |

⚠️ MIDDLE aur RING apne 180° stop ke 5° ke andar band hote hain — **unka
margin khatam hai**. Unke thread doosron se dheele bandhe hain; kas dene par
wo bhi 140° ke aas-paas aa jayenge. Thread thodi dheeli bandhi gayi ya cardboard baith gaya to wo poori band
nahi hongi, aur software se theek nahi kar paoge. Us haalat mein horn lamba karna
padega (15mm → 30mm), phir dobara calibrate.

---

## 7. WiFi — teen raaste, teesra hamesha chalta hai

Board ek saath **do** network par rehta hai: tumhare router/hotspot par, aur
apna khud ka hotspot bana ke. Isliye teen tarike se khul jata hai:

| # | Link | Kab |
|---|---|---|
| 1 | `http://<IP>` | router se juda ho. **IP badalta rehta hai** |
| 2 | `http://hand.local` | router se juda ho. IP badle to bhi chalega |
| 3 | `http://192.168.4.1` | **hamesha.** Pehle WiFi `RoboticHand` / `hand12345` se judo |

**College mein #3 use karo.** Wahan ka WiFi credentials nahi honge, aur #3 ko
router ki zaroorat hi nahi — board khud hotspot deta hai. IP kabhi nahi badalta
kyunki DHCP board khud chalata hai.

#2 (`hand.local`) mDNS hai. Windows aur iPhone mein built-in hai, test kiya hua
hai. Android Chrome mein kabhi chalta hai kabhi nahi — isliye #3 rakha hai.

### ⚠️ IP khud badalta rehta hai — ye normal hai

Android hotspot jab band-chalu hota hai to **poora subnet naya bana deta hai**.
Ek hi din mein teen baar badla tha:

```
10.120.3.20  ->  172.22.138.20  ->  10.231.202.20
```

Purana link marr jata hai, **code kharab nahi hua hota**. Isiliye #2 aur #3
banaye gaye. Pakka IP chahiye to serial par `net` maaro.

### ⚠️ Channel 12/13 ki dikkat

ESP32 default **US/FCC** region par hota hai = sirf **channel 1-11**. Android
hotspot apne aap channel chunta hai aur aksar **12-13** par chala jata hai.
Us haalat mein ESP32 ko network **`scan` mein DIKH jata hai par JUD nahi
pata** — ye sabse confusing symptom hai, lagta hai password galat hai.

Code mein region **IN (channel 1-13)** set kar diya hai (`setRegionIN()`), to
ye theek hai. Par agar college mein kabhi "dikhta hai par judta nahi" ho, to
wajah yahi hogi — ya to hotspot ka channel 1-11 mein force karo, ya #3 wala
raasta lo.

### Naya WiFi daalna ho

[`src/palm/main.cpp`](src/palm/main.cpp) mein `WIFI_SSID` / `WIFI_PASS` badlo
aur upload karo.

⚠️ ESP32 ka antenna kamzor hai. **−70 dBm se kamzor signal par nahi judega.**
Ghar par router `−77` par tha, isiliye fail ho raha tha; phone hotspot `−30` par
tha, turant jud gaya. Dikkat aaye to board ko router ke paas le jao, ya #3
wala raasta lo.

---

## 8. Chalane ke commands

```bash
# upload
C:\Users\Hp\.platformio\penv\Scripts\pio.exe run -e palm -t upload

# serial monitor (COM6, 115200)
C:\Users\Hp\.platformio\penv\Scripts\pio.exe run -e palm -t monitor
```

Serial commands:

| Command | Kaam |
|---|---|
| `signs` | saare naam dekho |
| `sign peace` | peace, middle, rock, ok, point, callme, gun, three, four, open, fist, thumbsup |
| `seq wave` | wave, hello, count, grab, drum |
| `stop` | chalta gesture band |
| `deg <0-4> <0-180>` | ek ungli ko degree par (0=THUMB … 4=PINKY) |
| `w` | abhi kahan hai |
| `net` | **IP, hand.local, hotspot ka link — sab ek jagah** |
| `scan` | aas-paas ke WiFi (⚠️ isme padosiyon ke router naam aate hain — demo video mein mat dikhao) |
| `r` | relax (PWM off) |
| `?` | poori list |

---

## 9. Dikkat aaye to

| Lakshan | Wajah | Ilaaj |
|---|---|---|
| **Purana link nahi khul raha** | **hotspot restart par IP badal gaya** | **`http://hand.local` ya `http://192.168.4.1` kholo** |
| WiFi `scan` mein dikhta hai par judta nahi | hotspot channel 12/13 par | region IN set hai; na chale to hotspot ko ch 1-11 par lao |
| Site nahi khul rahi, power hai | abhi boot ho raha hai | 20 sec ruko, refresh |
| Phir bhi nahi | board hang | ESP32 par **EN button** dabao |
| `PCA9685 nahi mila` | I2C taar | VCC/GND/SDA(22)/SCL(23) check karo |
| Board baar-baar reboot | brown-out | servo aur ESP32 ki power alag karo |
| Ungli ulti mudti hai | kuch nahi | us ungli ka OPEN/CLOSED aapas mein badlo |
| Servo garam + buzz | stall | wo apne stop se ragad raha hai — CLOSED kam karo |
| Gesture ke baad PWM band | **normal hai** | auto-relax, 8 sec. Koi bhi command par wapas chalu |

---

## 10. Jodne ka kram (checklist)

1. ESP32 ↔ PCA9685 ke 4 taar (3V3, GND, SDA=22, SCL=23)
2. SMPS → CH15 ke laal/kaale pin
3. Paanch servo: **CH0=THUMB, CH1=INDEX, CH2=MIDDLE, CH3=RING, CH4=PINKY**
4. ESP32 ko USB se power
5. SMPS chalu
6. Serial monitor kholo → `PCA9685 mil gaya` aana chahiye, aur **teen link** chhapenge
   (router IP, `hand.local`, aur `192.168.4.1`)
7. `w` maaro — paanchon `15°` par dikhni chahiye
8. `sign peace` → ✌️ banna chahiye
9. `seq wave` → lehar chalni chahiye

Agar 8 aur 9 theek hain to sab jud gaya.
