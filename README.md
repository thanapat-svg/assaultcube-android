# AssaultCube for Android

พอร์ต [AssaultCube](https://github.com/assaultcube/AC) 1.3 (เกม FPS โอเพนซอร์ส) มาเล่นบนมือถือ Android
เล่นออฟไลน์กับบอทได้ ใช้ปุ่มสัมผัสแบบ Call of Duty Mobile

## ดาวน์โหลด
ไปที่แท็บ **Releases** แล้วโหลดไฟล์ `.apk` ล่าสุด → ติดตั้งในมือถือ (อนุญาต "ติดตั้งแอปที่ไม่รู้จัก")
เปิดครั้งแรกจะช้ากว่าปกติ เพราะแตกไฟล์เกม (~60 MB) ลงเครื่อง

## วิธีเล่นกับบอท
เมนูหลัก → **Singleplayer** → เลือกโหมด (เช่น Bot Deathmatch) → เลือกแมพ / ระดับบอท

## ปุ่มควบคุม
| ปุ่ม | หน้าที่ |
|---|---|
| จอยซ้าย (แตะตรงไหนก็ได้ครึ่งซ้าย) | เดิน |
| ลากนิ้วครึ่งขวา | หันกล้อง / เล็ง |
| FIRE (ใหญ่ขวา + เล็กซ้าย) | ยิง (กดค้างแล้วลากเพื่อเล็งไปด้วยได้) |
| JUMP / CROUCH | กระโดด / หมอบ (กดสลับ) |
| RELOAD, NADE, มีด, SWAP | รีโหลด, ปาระเบิด, ฟันมีด, สลับปืน |
| SCOPE | ส่องกล้อง (ขึ้นเฉพาะตอนถือสไนเปอร์) |
| ปุ่มบนซ้าย | เมนู / ตารางคะแนน |
| ในเมนู | แตะรายการเพื่อเลือก, ลากขึ้นลงเพื่อเลื่อน, ปุ่มลูกศร/OK/BACK ด้านขวา |

ปรับได้ในคอนโซล: `touchsens` (ความไวการเล็ง), `touchsize` (ขนาดปุ่ม %), `touchalpha` (ความทึบ %),
`touchleftfire 0/1`, `touchcrouchtoggle 0/1`

## Build
GitHub Actions build APK ให้อัตโนมัติทุกครั้งที่ push ขึ้น `main` (ดู `.github/workflows/build-apk.yml`)
Build เองบนเครื่อง: ต้องมี Android SDK + NDK 26.3 แล้วรัน
```
scripts/fetch-deps.sh
scripts/prepare-assets.sh
cd android && ./gradlew assembleRelease
```

## โครงสร้าง
- `source/src` ซอร์ส AssaultCube (+ `touch.cpp` ปุ่มสัมผัส, `android_main.cpp` จุดเริ่มต้นบน Android)
- `config/ packages/ bot/ mods/` ข้อมูลเกม
- `android/` โปรเจกต์ Android (SDL2 + gl4es + OpenAL Soft + libvorbis)

## License
โค้ดเอนจินเป็น zlib license (ดู `LICENSE-cube-engine.txt`) ส่วนไฟล์ภาพ/เสียง/แมพมี license แยกตามแต่ละชิ้น
(ดู `docs-package_copyrights.txt`) บางชิ้นห้ามใช้เชิงพาณิชย์ — โปรเจกต์นี้สำหรับเล่นส่วนตัว
