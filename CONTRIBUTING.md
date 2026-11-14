# Contributing Guidelines

Thank you for your interest in contributing to this project. We welcome improvements, bug fixes, hardware optimizations, and additional feature modules.

---

## 1. Code Style and Conventions

### 1.1. Firmware (C++ / Arduino)
- Non-Blocking Execution: Never use `delay()` in the primary `loop()` or web request handlers. Use `millis()`-based timestamps for task scheduling.
- Resource Conservation: The ESP32 shares RAM between the network stack, TLS buffers, and display drivers. Avoid heap fragmentation; prefer static buffers or lightweight parsers over heavy external dynamic libraries.
- Headers and Modular Design: Place feature-specific logic in dedicated header files (`.h`) within the `esp32_wifi_and_display` directory.
- PROGMEM Placement: All bitmap arrays, font tables, and static HTML templates must be marked with `PROGMEM` to reside in Flash rather than consuming SRAM.

### 1.2. Web Companion and Embedded UI (HTML / CSS / JavaScript)
- Lightweight Footprint: Web assets served directly from ESP32 Flash should be compact and dependency-free (vanilla JavaScript and CSS).
- Cross-Browser Compatibility: Ensure responsive layouts function reliably on both desktop and mobile viewports.
- Security and Standards: When handling credentials or geolocation, maintain strict compliance with browser security contexts (HTTPS requirements for Geolocation).

---

## 2. Adding Custom Animation Sequences

To integrate a new animation into the display engine:

### 2.1. Frame Preparation
1. Create or export your animation as a sequence of monochrome bitmap images.
2. Dimensions must be exactly 128 pixels wide by 64 pixels high.
3. Color format must be 1-bit monochrome (black and white).

### 2.2. XBM Array Generation
1. Convert each frame to XBM (X BitMap) format. Tools such as LCD Image Converter, GIMP, or ImageMagick can export C arrays.
2. Store the frames as PROGMEM arrays:
   ```cpp
   const unsigned char frame_00[] PROGMEM = { /* 1024 bytes */ };
   const unsigned char frame_01[] PROGMEM = { /* 1024 bytes */ };
   ```
3. Create a master array of pointers:
   ```cpp
   const unsigned char* const customFrames[] = {
     frame_00,
     frame_01,
     // ...
   };
   #define CUSTOM_FRAME_COUNT 10
   ```

### 2.3. Firmware Registration
1. Save the frames into a new header file (e.g., `custom_frames.h`).
2. Include the header inside `esp32_wifi_and_display.ino`.
3. Add a frame counter variable, frame rate definition, and update the rendering `switch(activeAnim)` block.
4. Add a corresponding button and endpoint handler in `web_ui.h`.

---

## 3. Pull Request Process

1. Fork the repository and create your feature branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```
2. Test your changes thoroughly on physical ESP32 hardware with an SSD1306 OLED panel.
3. Ensure the project builds cleanly without warnings under Arduino IDE and PlatformIO.
4. Verify flash size does not exceed the 3MB partition allocation.
5. Commit your changes with clear, descriptive commit messages:
   ```bash
   git commit -m "Add custom frame rate support for clock refresh"
   ```
6. Push to your branch and submit a Pull Request against `main`.
7. Provide a detailed summary in your PR description explaining the changes and testing performed.
