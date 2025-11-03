# Gruntz (1999) .PID Converter Plugin for Dragon UnPACKer 5
<img src="https://blogger.googleusercontent.com/img/b/R29vZ2xl/AVvXsEiMBUh9su8-HYI7FkeI0XuMM9v705qBKd4cNweY6e5qJnPWIwjbbrqJ30DsCbGQfBXhXZKoDOvSlqYhE26k-ybNeN8Bt2WNQ6Xx3y_pbr1Gg1pPxgUV48LMtHrQUOGVYsLYW1NuTtwcMzP686PNvpaHADpTlz6b_aevvRA6GSJnsN-BbRbLLS-p7aPBdA3e/s968/Gruntz-gun_hd.png" alt="Gruntz Logo" width="300" />
**Author:** Paweł C. (PaweX3)  
**Version:** 0.82  
**DUCI Compatibility:** v3 / v4  
**Supported Formats:** `.PID` (8-bit paletted, RLE-compressed) -> **BMP**, **TGA**, **PNG**

---

## Overview

This plugin enables **full support** for `.PID` image files from the 1999 game **Gruntz** in **Dragon UnPACKer 5**. It allows:

- **Preview** (when plugin is loaded and prioritized)
- **Export** to **BMP (24-bit)**, **TGA (8-bit indexed)**, and **PNG (8/24/32-bit with alpha)**
- **Transparency support** (index 0 -> transparent)
- **RLE decompression**
- **Mirror / invert** handling via flags
- **Embedded or default palette** support
- **Configurable PNG output mode** (8/24/32 bpp)

> **Note:** The plugin works **even if DU5 crashes on preview** — you can still **right-click -> Export** to convert files successfully.

---

## Known Issue: Preview Crash in Dragon UnPACKer

> **Error:**
> ```
> Cannot preview... Error while loading images from stream 0016F4C8 (file format: tga).
> Exception Message: Access violation at address 00591941 in module 'drgunpack5.exe'. Read of address 00000000
> ```

### This happens **even without the plugin installed**.

**Cause:**  
Dragon UnPACKer 5 incorrectly identifies some `.PID` files as **TGA** due to similar headers or internal driver bugs. It attempts to use its **built-in TGA previewer**, which crashes on invalid data.

### **Workarounds:**

1. **Use Right-Click -> Export**  
   This plugin can **still export** these files to **BMP / TGA / PNG** — **this works perfectly**.
