// ============================================================
//  secrets.h  --  apna WiFi yahan daalo
//  ------------------------------------------------------------
//  Is file ko "secrets.h" naam se copy karo (usi include/ folder
//  mein) aur apne router ka naam-password bhar do.
//
//      cp include/secrets.example.h include/secrets.h
//
//  secrets.h .gitignore mein hai, isliye wo kabhi commit nahi
//  hogi -- tumhara password repo mein nahi jayega.
//
//  Ye file BANAYE BINA bhi project chalta hai: tab ESP32 router
//  se judne ki koshish hi nahi karega aur seedha apna hotspot
//  bana lega -> http://192.168.4.1
// ============================================================
#pragma once

#define WIFI_SSID  "YOUR_WIFI_NAME"
#define WIFI_PASS  "YOUR_WIFI_PASSWORD"
