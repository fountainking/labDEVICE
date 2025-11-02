#include "gb_emulator.h"

#ifdef ENABLE_GB_EMULATOR

#include <SD.h>
#include <vector>

// Peanut-GB core
#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#include "peanut_gb.h"

// GB state
bool gbActive = false;
static struct gb_s* gb = nullptr;  // Lazy allocation
static String selectedROM = "";
static bool romLoaded = false;
static unsigned long lastFrameTime = 0;
static const int FRAME_TIME_US = 16742; // ~59.7Hz

// Private data structure
struct priv_t {
    uint8_t *rom;
    uint8_t *cart_ram;
    uint32_t fb[144][160];  // Frame buffer - 92KB!
};

static struct priv_t* priv = nullptr;  // Lazy allocation

// DMG palette (original Game Boy green)
static const uint16_t dmg_palette[4] = {
    0xFFFF,  // White
    0xAD55,  // Light gray
    0x52AA,  // Dark gray
    0x0000   // Black
};

// Callback: Read ROM
uint8_t gb_rom_read(struct gb_s* gb, const uint_fast32_t addr) {
    struct priv_t* p = (struct priv_t*)gb->direct.priv;
    return p->rom[addr];
}

// Callback: Read cartridge RAM
uint8_t gb_cart_ram_read(struct gb_s* gb, const uint_fast32_t addr) {
    struct priv_t* p = (struct priv_t*)gb->direct.priv;
    if (p->cart_ram == NULL) return 0xFF;
    return p->cart_ram[addr];
}

// Callback: Write cartridge RAM
void gb_cart_ram_write(struct gb_s* gb, const uint_fast32_t addr, const uint8_t val) {
    struct priv_t* p = (struct priv_t*)gb->direct.priv;
    if (p->cart_ram != NULL) {
        p->cart_ram[addr] = val;
    }
}

// Callback: Draw scanline to framebuffer
void lcd_draw_line(struct gb_s* gb, const uint8_t pixels[160], const uint_fast8_t line) {
    struct priv_t* p = (struct priv_t*)gb->direct.priv;
    for (unsigned int x = 0; x < 160; x++) {
        p->fb[line][x] = dmg_palette[pixels[x] & 3];
    }
}

// Error handler (ignore errors)
void gb_error(struct gb_s* gb, const enum gb_error_e gb_err, const uint16_t addr) {
    // Ignore errors for now
    (void)gb;
    (void)gb_err;
    (void)addr;
}

// Load ROM from SD card
static bool loadROM(const String& filename) {
    String fullPath = "/gameboy/" + filename;

    File file = SD.open(fullPath, FILE_READ);
    if (!file) {
        Serial.printf("Failed to open ROM: %s\n", fullPath.c_str());
        return false;
    }

    size_t romSize = file.size();
    Serial.printf("Loading ROM: %s (%d bytes)\n", filename.c_str(), romSize);

    // Allocate ROM memory
    priv->rom = (uint8_t*)malloc(romSize);
    if (priv->rom == NULL) {
        Serial.println("Failed to allocate ROM memory");
        file.close();
        return false;
    }

    // Read ROM into memory
    file.read(priv->rom, romSize);
    file.close();

    // Allocate cartridge RAM (32KB should cover most games)
    priv->cart_ram = (uint8_t*)malloc(0x8000);
    if (priv->cart_ram != NULL) {
        memset(priv->cart_ram, 0, 0x8000);
    }

    // Initialize framebuffer
    memset(priv->fb, 0, sizeof(priv->fb));

    // Initialize emulator
    enum gb_init_error_e ret = gb_init(gb, &gb_rom_read, &gb_cart_ram_read,
                                        &gb_cart_ram_write, &gb_error, priv);

    if (ret != GB_INIT_NO_ERROR) {
        Serial.printf("GB init error: %d\n", ret);
        free(priv->rom);
        if (priv->cart_ram) free(priv->cart_ram);
        return false;
    }

    gb_init_lcd(gb, &lcd_draw_line);

    Serial.println("ROM loaded successfully");
    return true;
}

// ROM file picker
bool selectGBROM() {
    if (!SD.exists("/gameboy")) {
        M5Cardputer.Display.fillScreen(TFT_BLACK);
        M5Cardputer.Display.setTextColor(TFT_RED);
        M5Cardputer.Display.drawString("No /gameboy directory!", 10, 50);
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.drawString("Create it and add .gb ROMs", 10, 70);
        M5Cardputer.Display.drawString("Press any key to exit", 10, 100);

        while (true) {
            M5Cardputer.update();
            if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
                return false;
            }
            delay(10);
        }
    }

    // Get list of .gb files
    std::vector<String> romFiles;
    File root = SD.open("/gameboy");
    File file = root.openNextFile();

    while (file) {
        String name = file.name();
        if (name.endsWith(".gb") || name.endsWith(".GB")) {
            romFiles.push_back(name);
        }
        file = root.openNextFile();
    }

    if (romFiles.empty()) {
        M5Cardputer.Display.fillScreen(TFT_BLACK);
        M5Cardputer.Display.setTextColor(TFT_RED);
        M5Cardputer.Display.drawString("No .gb ROM files found!", 10, 50);
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.drawString("Add ROMs to /gameboy/", 10, 70);
        M5Cardputer.Display.drawString("Press any key to exit", 10, 100);

        while (true) {
            M5Cardputer.update();
            if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
                return false;
            }
            delay(10);
        }
    }

    // File selection menu
    int selectedIndex = 0;

    while (true) {
        M5Cardputer.Display.fillScreen(TFT_BLACK);
        M5Cardputer.Display.setTextColor(TFT_RED);
        M5Cardputer.Display.drawString("SELECT GAME BOY ROM", 10, 5);

        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.drawString("UP/DOWN: Navigate", 10, 110);
        M5Cardputer.Display.drawString("ENTER: Load", 10, 120);

        // Display ROM list (show 5 at a time)
        int startIdx = max(0, selectedIndex - 2);
        int endIdx = min((int)romFiles.size(), startIdx + 5);

        for (int i = startIdx; i < endIdx; i++) {
            int y = 25 + ((i - startIdx) * 15);

            if (i == selectedIndex) {
                M5Cardputer.Display.setTextColor(TFT_BLACK, TFT_RED);
                M5Cardputer.Display.fillRect(5, y - 2, 230, 14, TFT_RED);
            } else {
                M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            }

            M5Cardputer.Display.drawString(romFiles[i], 10, y);
        }

        M5Cardputer.update();

        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                if (status.del) {  // ESC/Back
                    return false;
                }

                for (auto key : status.word) {
                    if (key == ';') {  // UP
                        selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : romFiles.size() - 1;
                    } else if (key == '.') {  // DOWN
                        selectedIndex = (selectedIndex < romFiles.size() - 1) ? selectedIndex + 1 : 0;
                    }
                }

                if (status.enter) {  // ENTER
                    selectedROM = romFiles[selectedIndex];
                    return true;
                }
            }
        }

        delay(50);
    }
}

void enterGB() {
    // Check available heap
    size_t freeHeap = ESP.getFreeHeap();
    size_t gbSize = sizeof(struct gb_s);
    size_t privSize = sizeof(struct priv_t);

    Serial.printf("Free heap: %d bytes\n", freeHeap);
    Serial.printf("GB size: %d bytes\n", gbSize);
    Serial.printf("Priv size: %d bytes\n", privSize);
    Serial.printf("Total needed: %d bytes\n", gbSize + privSize);

    // Allocate GB structures (109KB total - only when needed!)
    if (gb == nullptr) {
        gb = (struct gb_s*)malloc(sizeof(struct gb_s));
        if (gb == nullptr) {
            M5Cardputer.Display.fillScreen(TFT_BLACK);
            M5Cardputer.Display.setTextColor(TFT_RED);
            M5Cardputer.Display.drawString("Out of memory for GB!", 10, 40);
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(TFT_WHITE);
            M5Cardputer.Display.drawString(String("Need: ") + String(gbSize) + " bytes", 10, 60);
            M5Cardputer.Display.drawString(String("Free: ") + String(freeHeap) + " bytes", 10, 75);
            delay(3000);
            return;
        }
        Serial.printf("GB allocated at %p\n", gb);
    }

    if (priv == nullptr) {
        priv = (struct priv_t*)malloc(sizeof(struct priv_t));
        if (priv == nullptr) {
            free(gb);
            gb = nullptr;
            M5Cardputer.Display.fillScreen(TFT_BLACK);
            M5Cardputer.Display.setTextColor(TFT_RED);
            M5Cardputer.Display.drawString("Out of memory for framebuffer!", 10, 40);
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(TFT_WHITE);
            M5Cardputer.Display.drawString(String("Need: ") + String(privSize) + " bytes", 10, 60);
            M5Cardputer.Display.drawString(String("Free: ") + String(ESP.getFreeHeap()) + " bytes", 10, 75);
            delay(3000);
            return;
        }
        memset(priv, 0, sizeof(struct priv_t));
        Serial.printf("Priv allocated at %p\n", priv);
    }

    // Select ROM
    if (!selectGBROM()) {
        return;
    }

    // Load selected ROM
    if (!loadROM(selectedROM)) {
        M5Cardputer.Display.fillScreen(TFT_BLACK);
        M5Cardputer.Display.setTextColor(TFT_RED);
        M5Cardputer.Display.drawString("Failed to load ROM!", 10, 50);
        M5Cardputer.Display.drawString("Press any key to exit", 10, 70);

        while (true) {
            M5Cardputer.update();
            if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
                return;
            }
            delay(10);
        }
    }

    romLoaded = true;
    gbActive = true;
    lastFrameTime = micros();

    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void exitGB() {
    gbActive = false;
    romLoaded = false;

    // Free ROM/RAM memory
    if (priv && priv->rom) {
        free(priv->rom);
        priv->rom = NULL;
    }
    if (priv && priv->cart_ram) {
        free(priv->cart_ram);
        priv->cart_ram = NULL;
    }

    // Free GB structures (reclaim 109KB!)
    if (priv) {
        free(priv);
        priv = nullptr;
    }
    if (gb) {
        free(gb);
        gb = nullptr;
    }
}

void updateGB() {
    if (!romLoaded) return;

    M5Cardputer.update();

    // Check for exit
    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            if (status.del) {  // ESC to exit
                exitGB();
                return;
            }
        }
    }

    // Map keyboard to Game Boy buttons (check if keys are currently pressed)
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

    // D-pad
    gb->direct.joypad_bits.up = 0;
    gb->direct.joypad_bits.down = 0;
    gb->direct.joypad_bits.left = 0;
    gb->direct.joypad_bits.right = 0;

    // Buttons
    gb->direct.joypad_bits.a = 0;
    gb->direct.joypad_bits.b = 0;
    gb->direct.joypad_bits.start = 0;
    gb->direct.joypad_bits.select = 0;

    // Check all currently pressed keys
    for (auto key : status.word) {
        if (key == 'e') gb->direct.joypad_bits.up = 1;       // E = Up
        if (key == 'd') gb->direct.joypad_bits.down = 1;     // D = Down
        if (key == 's') gb->direct.joypad_bits.left = 1;     // S = Left
        if (key == 'f') gb->direct.joypad_bits.right = 1;    // F = Right
        if (key == 'l') gb->direct.joypad_bits.a = 1;        // L = A
        if (key == 'k') gb->direct.joypad_bits.b = 1;        // K = B
        if (key == '1') gb->direct.joypad_bits.start = 1;    // 1 = Start
        if (key == '2') gb->direct.joypad_bits.select = 1;   // 2 = Select
    }

    // Frame timing (59.7Hz)
    unsigned long currentTime = micros();
    if (currentTime - lastFrameTime >= FRAME_TIME_US) {
        lastFrameTime = currentTime;

        // Run emulator for one frame (~70224 cycles)
        gb_run_frame(gb);

        // Draw frame
        drawGB();
    }
}

void drawGB() {
    // Scale 160x144 GB display to fit 240x135 Cardputer screen
    // Center horizontally: (240 - 160) / 2 = 40
    // Scale vertically: 144 -> 135 (skip every ~10th line)

    int xOffset = 40;

    for (int y = 0; y < 135; y++) {
        int srcY = (y * 144) / 135;  // Map destination Y to source Y

        for (int x = 0; x < 160; x++) {
            uint16_t color = priv->fb[srcY][x];
            M5Cardputer.Display.drawPixel(x + xOffset, y, color);
        }
    }
}

#endif // ENABLE_GB_EMULATOR
