#include "emoji_maker.h"
#include "config.h"
#include "ui.h"
#include <SD.h>
#include <cmath>

// Emoji Maker state
bool emojiMakerActive = false;

// Grid settings - NEW LAYOUT: Palette left, Grid center, Preview right
static const int GRID_SIZE = 16;   // 16x16 grid for detail
static const int PIXEL_SIZE = 5;   // 5x5 pixels per grid cell = 80x80 grid

// Spacing between elements
static const int ELEMENT_MARGIN = 10;

// Palette on the left
static const int PALETTE_X = 8;  // Move right to align gallery with battery meter
static const int PALETTE_Y = 31;  // Move up 2px
static const int PALETTE_SWATCH_SIZE = 8;
static const int PALETTE_WIDTH = (2 * PALETTE_SWATCH_SIZE) + 2;  // 2 columns + spacing

// Grid (after palette + margin)
static const int GRID_X = PALETTE_X + PALETTE_WIDTH + ELEMENT_MARGIN;
static const int GRID_Y = 31;     // Move up 2px
static const int GRID_WIDTH = GRID_SIZE * PIXEL_SIZE;

// Preview (after grid + margin)
static const int PREVIEW_X = GRID_X + GRID_WIDTH + ELEMENT_MARGIN;
static const int PREVIEW_Y = 31;  // Move up 2px
static const int PREVIEW_SIZE = 16;  // 16x16 preview (1x scale for 16x16 grid)

// Tool buttons (2x3 grid, bottom-aligned with grid)
static const int TOOL_BUTTON_WIDTH = 22;   // Wider buttons for better visibility
static const int TOOL_BUTTON_HEIGHT = 20;  // Taller for icons
static const int TOOL_BUTTON_SPACING = 2;
static const int TOOL_GRID_X = PREVIEW_X;  // Left-aligned with preview
static const int TOOL_GRID_Y = GRID_Y + GRID_WIDTH - (3 * TOOL_BUTTON_HEIGHT) - (2 * TOOL_BUTTON_SPACING) + 2;  // Bottom-aligned with grid + 2px offset

// Gallery (3 columns x 4 rows, to the right of tools)
static const int GALLERY_X = TOOL_GRID_X + (2 * TOOL_BUTTON_WIDTH) + (2 * TOOL_BUTTON_SPACING) + 10;  // After tools
static const int GALLERY_Y = 31;  // Move up 2px
static const int GALLERY_COLS = 3;
static const int GALLERY_ROWS = 4;  // 3 columns x 4 rows = 12 boxes
static const int GALLERY_BOX_SIZE = 16;  // Same as preview
static const int GALLERY_SPACING = 2;

// Cursor position and mode
static int cursorX = 0;
static int cursorY = 0;

// Navigation modes
enum NavMode { NAV_GRID, NAV_PALETTE, NAV_TOOLS, NAV_GALLERY };
static NavMode navMode = NAV_GRID;
static int paletteIndex = 0;  // Which palette color is selected when in palette mode
static int toolIndex = 0;      // Which tool button is selected when in tools mode
static int galleryIndex = 0;   // Which gallery slot is selected when in gallery mode

// Canvas data
static uint16_t canvas[GRID_SIZE][GRID_SIZE];
static uint16_t undoBuffer[GRID_SIZE][GRID_SIZE];  // For undo functionality

// Tool state
enum ToolMode { TOOL_PENCIL, TOOL_BIG_BRUSH, TOOL_FILL, TOOL_CIRCLE, TOOL_LINE, TOOL_ROTATE };
static ToolMode activeTool = TOOL_PENCIL;
static bool toolWaitingForInput = false;  // For multi-click tools (circle, line)
static int toolStartX = 0, toolStartY = 0;  // First click position for multi-click tools
static bool useGhostCenter = false;  // True if circle is centered on ghost point (7.5, 7.5)
static bool moveMode = false;  // True when in move canvas mode (M key)
static bool resizeMode = false;  // True when in resize mode (R key)

// Custom tool icon - paint bucket from paint.emoji
static const uint16_t PAINT_BUCKET_ICON[16][16] = {
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x0000, 0x07E0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x07E0, 0x0000, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x0000, 0x7BEF, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x7BEF, 0x0000, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x0000, 0x7BEF, 0x7BEF, 0x7BEF, 0xFFE0, 0xFFE0, 0xFFE0, 0x7BEF, 0x7BEF, 0x0000, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x0000, 0x7BEF, 0x7BEF, 0x7BEF, 0xFDA0, 0xFDA0, 0xFDA0, 0x7BEF, 0x7BEF, 0x0000, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x7BEF, 0x7BEF, 0xF800, 0xF800, 0xF800, 0x7BEF, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x0000, 0x001F, 0x001F, 0x001F, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x000F, 0x000F, 0x000F, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x000F, 0x000F, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x000F, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
};

// Color palette (16 colors - vibrant and useful)
static const uint16_t palette[] = {
    TFT_BLACK,       // 0
    TFT_WHITE,       // 1
    TFT_RED,         // 2
    TFT_GREEN,       // 3
    TFT_BLUE,        // 4
    TFT_YELLOW,      // 5
    TFT_CYAN,        // 6
    TFT_MAGENTA,     // 7
    TFT_ORANGE,      // 8
    TFT_PURPLE,      // 9
    TFT_PINK,        // 10
    TFT_BROWN,       // 11
    TFT_DARKGREY,    // 12
    TFT_LIGHTGREY,   // 13
    TFT_DARKGREEN,   // 14
    TFT_NAVY         // 15
};
static const int PALETTE_SIZE = 16;
static int selectedColor = 12;  // Start with dark grey

// Emoji shortcut name
static String emojiShortcut = "";
static bool editingShortcut = false;

// UI state
static bool needsFullRedraw = true;
static bool showSaveMenu = false;
static bool showLoadMenu = false;
static int loadMenuIndex = 0;
static std::vector<String> savedEmojis;

// Gallery cache - stores up to 12 emojis (slot 0 is always strawberry sample)
static uint16_t galleryCache[12][GRID_SIZE][GRID_SIZE];
static int galleryCacheCount = 1;  // Start at 1 because slot 0 is strawberry sample

// Forward declarations
static void drawGrid();
static void drawPalette();
static void drawPreview();
static void drawToolButtons();
static void drawHints();
static void drawGallery();
static void drawSaveMenu();
static void drawLoadMenu();
static void clearCanvas();
static void saveEmojiToLabChat();
static void exportEmojiToLabChat();
static void loadSavedEmojis();
static void loadEmojiFromFile(const String& filename);
static void loadGalleryCache();
static bool canvasHasContent();

void enterEmojiMaker() {
    emojiMakerActive = true;
    needsFullRedraw = true;
    showSaveMenu = false;
    showLoadMenu = false;
    cursorX = GRID_SIZE / 2;
    cursorY = GRID_SIZE / 2;
    selectedColor = 12;  // Dark grey
    emojiShortcut = "";
    editingShortcut = false;
    navMode = NAV_GRID;
    paletteIndex = 12;  // Start at dark grey
    galleryIndex = 0;
    activeTool = TOOL_PENCIL;  // Reset tool to pencil on entry
    moveMode = false;  // Clear move mode
    resizeMode = false;  // Clear resize mode

    // Clear canvas (white background)
    clearCanvas();

    // Load gallery cache (includes creating strawberry sample)
    loadGalleryCache();

    Serial.print("Gallery cache loaded: ");
    Serial.print(galleryCacheCount);
    Serial.println(" emojis");

    M5Cardputer.Display.fillScreen(TFT_WHITE);
    drawEmojiMaker();
}

void exitEmojiMaker() {
    emojiMakerActive = false;
    needsFullRedraw = true;
}

// Transparency color (lime green) - will be transparent in LabChat
#define TRANSPARENCY_COLOR 0x07E0  // Bright lime green (RGB565)

void clearCanvas() {
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            canvas[y][x] = TRANSPARENCY_COLOR;  // Lime green = transparent
        }
    }
}

void updateEmojiMaker() {
    if (!emojiMakerActive) return;

    // Input is handled in main keyboard loop, not here
    // Drawing happens on-demand when changes occur
}

void drawEmojiMaker() {
    M5Cardputer.Display.fillScreen(TFT_WHITE);
    drawStatusBar(false);

    // Draw all components
    drawPalette();
    drawGrid();
    drawPreview();
    drawToolButtons();
    drawGallery();
    drawHints();

    if (showSaveMenu) {
        drawSaveMenu();
    } else if (showLoadMenu) {
        drawLoadMenu();
    }
}

static void drawGrid() {
    // Draw 16x16 grid with white background and grid lines
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int screenX = GRID_X + (x * PIXEL_SIZE);
            int screenY = GRID_Y + (y * PIXEL_SIZE);

            // Draw pixel
            M5Cardputer.Display.fillRect(screenX, screenY, PIXEL_SIZE, PIXEL_SIZE, canvas[y][x]);

            // Draw very thin grid lines (only right and bottom edges)
            if (x < GRID_SIZE - 1) {
                M5Cardputer.Display.drawFastVLine(screenX + PIXEL_SIZE - 1, screenY, PIXEL_SIZE, 0x8410);  // Very light grey
            }
            if (y < GRID_SIZE - 1) {
                M5Cardputer.Display.drawFastHLine(screenX, screenY + PIXEL_SIZE - 1, PIXEL_SIZE, 0x8410);  // Very light grey
            }

            // Draw cursor (thick border in selected color)
            if (x == cursorX && y == cursorY) {
                M5Cardputer.Display.drawRect(screenX, screenY, PIXEL_SIZE, PIXEL_SIZE, palette[selectedColor]);
                M5Cardputer.Display.drawRect(screenX + 1, screenY + 1, PIXEL_SIZE - 2, PIXEL_SIZE - 2, palette[selectedColor]);
            }
        }
    }

    // Draw live preview for circle/line tools
    if (toolWaitingForInput && navMode == NAV_GRID) {
        if (activeTool == TOOL_CIRCLE) {
            // Preview circle from center to cursor
            int dx = cursorX - toolStartX;
            int dy = cursorY - toolStartY;
            int radius = (int)sqrt(dx*dx + dy*dy);

            // Draw preview circle with semi-transparent effect (use palette color)
            int x = 0, y = radius;
            int d = 3 - 2 * radius;

            auto drawPreviewPoint = [&](int px, int py) {
                if (px >= 0 && px < GRID_SIZE && py >= 0 && py < GRID_SIZE) {
                    int screenX = GRID_X + (px * PIXEL_SIZE);
                    int screenY = GRID_Y + (py * PIXEL_SIZE);
                    M5Cardputer.Display.drawRect(screenX, screenY, PIXEL_SIZE, PIXEL_SIZE, TFT_YELLOW);
                }
            };

            auto drawCirclePreviewPoints = [&](int x, int y) {
                drawPreviewPoint(toolStartX + x, toolStartY + y);
                drawPreviewPoint(toolStartX - x, toolStartY + y);
                drawPreviewPoint(toolStartX + x, toolStartY - y);
                drawPreviewPoint(toolStartX - x, toolStartY - y);
                drawPreviewPoint(toolStartX + y, toolStartY + x);
                drawPreviewPoint(toolStartX - y, toolStartY + x);
                drawPreviewPoint(toolStartX + y, toolStartY - x);
                drawPreviewPoint(toolStartX - y, toolStartY - x);
            };

            drawCirclePreviewPoints(x, y);
            while (y >= x) {
                x++;
                if (d > 0) {
                    y--;
                    d = d + 4 * (x - y) + 10;
                } else {
                    d = d + 4 * x + 6;
                }
                drawCirclePreviewPoints(x, y);
            }
        } else if (activeTool == TOOL_LINE) {
            // Preview line from start to cursor
            int x0 = toolStartX, y0 = toolStartY;
            int x1 = cursorX, y1 = cursorY;
            int dx = abs(x1 - x0), dy = abs(y1 - y0);
            int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
            int err = dx - dy;

            while (true) {
                if (x0 >= 0 && x0 < GRID_SIZE && y0 >= 0 && y0 < GRID_SIZE) {
                    int screenX = GRID_X + (x0 * PIXEL_SIZE);
                    int screenY = GRID_Y + (y0 * PIXEL_SIZE);
                    M5Cardputer.Display.drawRect(screenX, screenY, PIXEL_SIZE, PIXEL_SIZE, TFT_YELLOW);
                }
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 > -dy) { err -= dy; x0 += sx; }
                if (e2 < dx) { err += dx; y0 += sy; }
            }
        }
    }

    // Outer border (black)
    M5Cardputer.Display.drawRect(GRID_X - 1, GRID_Y - 1, (GRID_SIZE * PIXEL_SIZE) + 2, (GRID_SIZE * PIXEL_SIZE) + 2, TFT_BLACK);
}

static void drawPalette() {
    // Draw color palette (8x2 vertical stack on left)
    for (int i = 0; i < PALETTE_SIZE; i++) {
        int row = i / 2;
        int col = i % 2;
        int x = PALETTE_X + (col * (PALETTE_SWATCH_SIZE + 2));
        int y = PALETTE_Y + (row * (PALETTE_SWATCH_SIZE + 2));

        // Draw color swatch
        M5Cardputer.Display.fillRect(x, y, PALETTE_SWATCH_SIZE, PALETTE_SWATCH_SIZE, palette[i]);

        // Highlight based on mode
        if (navMode == NAV_PALETTE && i == paletteIndex) {
            // Active selection in palette mode (yellow)
            M5Cardputer.Display.drawRect(x - 1, y - 1, PALETTE_SWATCH_SIZE + 2, PALETTE_SWATCH_SIZE + 2, TFT_YELLOW);
            M5Cardputer.Display.drawRect(x - 2, y - 2, PALETTE_SWATCH_SIZE + 4, PALETTE_SWATCH_SIZE + 4, TFT_YELLOW);
        } else if (i == selectedColor) {
            // Current color (even when not in palette mode)
            M5Cardputer.Display.drawRect(x - 1, y - 1, PALETTE_SWATCH_SIZE + 2, PALETTE_SWATCH_SIZE + 2, TFT_BLUE);
        } else {
            M5Cardputer.Display.drawRect(x - 1, y - 1, PALETTE_SWATCH_SIZE + 2, PALETTE_SWATCH_SIZE + 2, TFT_DARKGREY);
        }
    }
}

static void drawPreview() {
    // Draw preview (1x scale = 16x16 for 16x16 grid) - NO LABEL
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int screenX = PREVIEW_X + x;
            int screenY = PREVIEW_Y + y;

            // Draw pixel at 1x scale
            M5Cardputer.Display.drawPixel(screenX, screenY, canvas[y][x]);
        }
    }

    // Border
    M5Cardputer.Display.drawRect(PREVIEW_X - 1, PREVIEW_Y - 1, PREVIEW_SIZE + 2, PREVIEW_SIZE + 2, TFT_BLACK);
}

static void drawToolButtons() {
    // Draw 2x3 grid of tool buttons with icons, bottom-aligned with grid
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 2; col++) {
            int x = TOOL_GRID_X + (col * (TOOL_BUTTON_WIDTH + TOOL_BUTTON_SPACING));
            int y = TOOL_GRID_Y + (row * (TOOL_BUTTON_HEIGHT + TOOL_BUTTON_SPACING));
            int toolIdx = row * 2 + col;

            // Highlight selected tool in NAV_TOOLS mode
            bool isSelected = (navMode == NAV_TOOLS && toolIndex == toolIdx);
            uint16_t bgColor = isSelected ? TFT_YELLOW : TFT_LIGHTGREY;
            uint16_t borderColor = isSelected ? TFT_BLACK : TFT_DARKGREY;

            // Draw button background
            M5Cardputer.Display.fillRoundRect(x, y, TOOL_BUTTON_WIDTH, TOOL_BUTTON_HEIGHT, 2, bgColor);
            M5Cardputer.Display.drawRoundRect(x, y, TOOL_BUTTON_WIDTH, TOOL_BUTTON_HEIGHT, 2, borderColor);

            // Draw tool icons (centered in button)
            int cx = x + TOOL_BUTTON_WIDTH / 2;
            int cy = y + TOOL_BUTTON_HEIGHT / 2;

            switch (toolIdx) {
                case TOOL_PENCIL:  // Pencil - small dot
                    M5Cardputer.Display.fillRect(cx - 1, cy - 1, 3, 3, TFT_BLACK);
                    break;

                case TOOL_BIG_BRUSH:  // Big brush - larger square
                    M5Cardputer.Display.fillRect(cx - 4, cy - 4, 8, 8, TFT_BLACK);
                    break;

                case TOOL_FILL:  // Fill - paint bucket from custom emoji
                    // Draw the 16x16 paint bucket icon, centered in button
                    for (int py = 0; py < 16; py++) {
                        for (int px = 0; px < 16; px++) {
                            uint16_t color = PAINT_BUCKET_ICON[py][px];
                            if (color != TRANSPARENCY_COLOR) {  // Skip transparent pixels
                                M5Cardputer.Display.drawPixel(
                                    x + 3 + px,
                                    y + 2 + py,
                                    color
                                );
                            }
                        }
                    }
                    break;

                case TOOL_CIRCLE:  // Circle - hollow circle (from screenshot 2)
                    M5Cardputer.Display.drawCircle(cx, cy, 5, TFT_BLACK);
                    M5Cardputer.Display.drawCircle(cx, cy, 4, TFT_BLACK);
                    break;

                case TOOL_LINE:  // Line - thick diagonal line
                    // Draw thick line by drawing multiple parallel lines
                    for (int i = -1; i <= 1; i++) {
                        M5Cardputer.Display.drawLine(cx - 5 + i, cy + 5, cx + 5 + i, cy - 5, TFT_BLACK);
                        M5Cardputer.Display.drawLine(cx - 5, cy + 5 + i, cx + 5, cy - 5 + i, TFT_BLACK);
                    }
                    break;

                case TOOL_ROTATE:  // Rotate - circular arrows (from screenshot 3)
                    // Draw circular arrow
                    M5Cardputer.Display.drawCircle(cx, cy, 4, TFT_BLACK);
                    M5Cardputer.Display.drawCircle(cx, cy, 3, TFT_BLACK);
                    // Erase bottom half
                    M5Cardputer.Display.fillRect(cx - 5, cy + 1, 11, 6, bgColor);
                    // Arrow head at top right
                    M5Cardputer.Display.fillTriangle(
                        cx + 4, cy - 2,  // tip pointing right
                        cx + 2, cy - 4,  // top
                        cx + 2, cy,      // bottom
                        TFT_BLACK
                    );
                    break;
            }

            // Highlight active tool with green border
            if ((ToolMode)toolIdx == activeTool) {
                M5Cardputer.Display.drawRoundRect(x - 1, y - 1, TOOL_BUTTON_WIDTH + 2, TOOL_BUTTON_HEIGHT + 2, 2, TFT_GREEN);
            }
        }
    }
}

static void drawHints() {
    // Hints at the bottom
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_DARKGREY);

    int hintX = 10;
    int hintY = 125;

    // Show transparency indicator with lime green square
    M5Cardputer.Display.fillRect(2, 118, 6, 6, TRANSPARENCY_COLOR);
    M5Cardputer.Display.drawRect(1, 117, 8, 8, TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_DARKGREY);
    M5Cardputer.Display.drawString("=Transparent", 10, 118);

    if (moveMode) {
        M5Cardputer.Display.drawString("MOVE MODE: Arrows=Move M=Exit", hintX, hintY);
    } else if (resizeMode) {
        M5Cardputer.Display.drawString("RESIZE MODE: +=Grow -=Shrink R=Exit", hintX, hintY);
    } else {
        M5Cardputer.Display.drawString("Spc=Erase Del=Undo M=Move R=Resize S=Save", hintX, hintY);
    }
}

static void drawGallery() {
    // Draw gallery grid (3 columns x 4 rows) to the right of tools
    for (int i = 0; i < GALLERY_ROWS * GALLERY_COLS; i++) {
        int row = i / GALLERY_COLS;
        int col = i % GALLERY_COLS;
        int x = GALLERY_X + (col * (GALLERY_BOX_SIZE + GALLERY_SPACING));
        int y = GALLERY_Y + (row * (GALLERY_BOX_SIZE + GALLERY_SPACING));

        if (i < galleryCacheCount) {
            // Draw the emoji from cache (1x scale - 16x16 fits in 16x16 box)
            for (int py = 0; py < GRID_SIZE; py++) {
                for (int px = 0; px < GRID_SIZE; px++) {
                    int screenX = x + px;
                    int screenY = y + py;
                    M5Cardputer.Display.drawPixel(screenX, screenY, galleryCache[i][py][px]);
                }
            }
        } else {
            // Draw empty box (white background)
            M5Cardputer.Display.fillRect(x, y, GALLERY_BOX_SIZE, GALLERY_BOX_SIZE, TFT_WHITE);
        }

        // Draw border (yellow if selected in gallery mode)
        uint16_t borderColor = (navMode == NAV_GALLERY && galleryIndex == i) ? TFT_YELLOW : TFT_DARKGREY;
        M5Cardputer.Display.drawRect(x - 1, y - 1, GALLERY_BOX_SIZE + 2, GALLERY_BOX_SIZE + 2, borderColor);
    }
}

static void drawSaveMenu() {
    // Save menu overlay
    int menuX = 50;
    int menuY = 35;
    int menuW = 140;
    int menuH = 70;

    // Background
    M5Cardputer.Display.fillRoundRect(menuX, menuY, menuW, menuH, 5, TFT_LIGHTGREY);
    M5Cardputer.Display.drawRoundRect(menuX, menuY, menuW, menuH, 5, TFT_BLACK);

    // Title
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_BLACK);
    M5Cardputer.Display.drawString("Save Emoji", menuX + 40, menuY + 5);

    // Name input label
    M5Cardputer.Display.setTextColor(TFT_DARKGREY);
    M5Cardputer.Display.drawString("Name:", menuX + 10, menuY + 22);

    // Name input box
    M5Cardputer.Display.drawRect(menuX + 45, menuY + 20, 80, 12, TFT_DARKGREY);
    M5Cardputer.Display.fillRect(menuX + 47, menuY + 22, 76, 8, TFT_WHITE);

    // Draw name (blue if editing)
    M5Cardputer.Display.setTextColor(editingShortcut ? TFT_BLUE : TFT_BLACK);
    M5Cardputer.Display.drawString(":" + emojiShortcut, menuX + 49, menuY + 22);

    // Instructions
    M5Cardputer.Display.setTextColor(TFT_DARKGREY);
    if (editingShortcut) {
        M5Cardputer.Display.drawString("Type name, Enter=Done", menuX + 15, menuY + 45);
    } else {
        M5Cardputer.Display.drawString("N=Edit Name", menuX + 15, menuY + 45);
        M5Cardputer.Display.drawString("Enter=Save E=Export", menuX + 15, menuY + 55);
        M5Cardputer.Display.drawString("`=Cancel", menuX + 15, menuY + 65);
    }
}

static void drawLoadMenu() {
    // Load menu overlay
    int menuX = 40;
    int menuY = 30;
    int menuW = 160;
    int menuH = 80;

    // Background
    M5Cardputer.Display.fillRoundRect(menuX, menuY, menuW, menuH, 5, TFT_LIGHTGREY);
    M5Cardputer.Display.drawRoundRect(menuX, menuY, menuW, menuH, 5, TFT_BLACK);

    // Title
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_BLACK);
    M5Cardputer.Display.drawString("Load Emoji", menuX + 50, menuY + 5);

    if (savedEmojis.size() == 0) {
        M5Cardputer.Display.setTextColor(TFT_DARKGREY);
        M5Cardputer.Display.drawString("No saved emojis", menuX + 40, menuY + 35);
    } else {
        // Show list of saved emojis
        int startIdx = max(0, loadMenuIndex - 3);
        int endIdx = min((int)savedEmojis.size(), startIdx + 6);

        for (int i = startIdx; i < endIdx; i++) {
            int yPos = menuY + 20 + ((i - startIdx) * 10);

            String name = savedEmojis[i];
            name.replace(".emoji", "");

            if (i == loadMenuIndex) {
                M5Cardputer.Display.setTextColor(TFT_BLUE);
                M5Cardputer.Display.drawString(">", menuX + 5, yPos);
            } else {
                M5Cardputer.Display.setTextColor(TFT_BLACK);
            }

            M5Cardputer.Display.drawString(name, menuX + 15, yPos);
        }
    }

    M5Cardputer.Display.setTextColor(TFT_DARKGREY);
    M5Cardputer.Display.drawString("ENTER=Load  ESC=Cancel", menuX + 20, menuY + 95);
}

// Tool helper functions
static void saveUndo() {
    memcpy(undoBuffer, canvas, sizeof(canvas));
}

static void performUndo() {
    memcpy(canvas, undoBuffer, sizeof(canvas));
}

static void floodFill(int x, int y, uint16_t targetColor, uint16_t replacementColor) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) return;
    if (canvas[y][x] != targetColor) return;
    if (targetColor == replacementColor) return;

    canvas[y][x] = replacementColor;

    floodFill(x + 1, y, targetColor, replacementColor);
    floodFill(x - 1, y, targetColor, replacementColor);
    floodFill(x, y + 1, targetColor, replacementColor);
    floodFill(x, y - 1, targetColor, replacementColor);
}

static void drawCircle(int cx, int cy, int radius, uint16_t color) {
    // If using ghost center, draw from 7.5, 7.5 (between pixels)
    if (useGhostCenter && cx == 7 && cy == 7) {
        // Draw circle centered at 7.5, 7.5 using floating point math
        for (int py = 0; py < GRID_SIZE; py++) {
            for (int px = 0; px < GRID_SIZE; px++) {
                // Calculate distance from 7.5, 7.5
                float dx = (float)px - 7.5f;
                float dy = (float)py - 7.5f;
                float dist = sqrt(dx*dx + dy*dy);

                // Draw if within 0.5 pixels of the radius
                if (dist >= radius - 0.5f && dist <= radius + 0.5f) {
                    canvas[py][px] = color;
                }
            }
        }
        return;
    }

    // Standard Bresenham circle algorithm for non-ghost center
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;

    auto drawCirclePoints = [&](int cx, int cy, int x, int y) {
        if (cx + x >= 0 && cx + x < GRID_SIZE && cy + y >= 0 && cy + y < GRID_SIZE) canvas[cy + y][cx + x] = color;
        if (cx - x >= 0 && cx - x < GRID_SIZE && cy + y >= 0 && cy + y < GRID_SIZE) canvas[cy + y][cx - x] = color;
        if (cx + x >= 0 && cx + x < GRID_SIZE && cy - y >= 0 && cy - y < GRID_SIZE) canvas[cy - y][cx + x] = color;
        if (cx - x >= 0 && cx - x < GRID_SIZE && cy - y >= 0 && cy - y < GRID_SIZE) canvas[cy - y][cx - x] = color;
        if (cx + y >= 0 && cx + y < GRID_SIZE && cy + x >= 0 && cy + x < GRID_SIZE) canvas[cy + x][cx + y] = color;
        if (cx - y >= 0 && cx - y < GRID_SIZE && cy + x >= 0 && cy + x < GRID_SIZE) canvas[cy + x][cx - y] = color;
        if (cx + y >= 0 && cx + y < GRID_SIZE && cy - x >= 0 && cy - x < GRID_SIZE) canvas[cy - x][cx + y] = color;
        if (cx - y >= 0 && cx - y < GRID_SIZE && cy - x >= 0 && cy - x < GRID_SIZE) canvas[cy - x][cx - y] = color;
    };

    drawCirclePoints(cx, cy, x, y);
    while (y >= x) {
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
        drawCirclePoints(cx, cy, x, y);
    }
}

static void drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    // Bresenham line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x0 >= 0 && x0 < GRID_SIZE && y0 >= 0 && y0 < GRID_SIZE) {
            canvas[y0][x0] = color;
        }

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void rotateCanvas90() {
    uint16_t temp[GRID_SIZE][GRID_SIZE];
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            temp[x][GRID_SIZE - 1 - y] = canvas[y][x];
        }
    }
    memcpy(canvas, temp, sizeof(canvas));
}

void handleEmojiMakerInput() {
    // DON'T call update() - main loop already does it
    // DON'T check isChange() - main keyboard handler already did
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

    // Handle shortcut name editing
    if (editingShortcut) {
        // Backspace (DEL key)
        if (status.del) {
            if (emojiShortcut.length() > 0) {
                emojiShortcut.remove(emojiShortcut.length() - 1);
                drawEmojiMaker();
            }
            return;
        }

        // Enter to finish editing
        if (status.enter) {
            editingShortcut = false;
            drawEmojiMaker();
            return;
        }

        // Check for ESC (backtick `) to cancel editing
        for (auto key : status.word) {
            if (key == '`') {
                editingShortcut = false;
                drawEmojiMaker();
                return;
            }
            // Add characters (limit to 12 chars)
            if (emojiShortcut.length() < 12 && ((key >= 'a' && key <= 'z') || (key >= '0' && key <= '9') || key == '_')) {
                emojiShortcut += key;
                drawEmojiMaker();
            }
        }
        return;
    }

    // Check for ESC (backtick `) to exit or close menus
    for (auto key : status.word) {
        if (key == '`') {
            if (showSaveMenu || showLoadMenu) {
                showSaveMenu = false;
                showLoadMenu = false;
                drawEmojiMaker();
            } else {
                exitEmojiMaker();
            }
            return;
        }
    }

    // Handle save menu
    if (showSaveMenu) {
        // N to start editing name in save dialog
        for (auto key : status.word) {
            if (key == 'n' && !editingShortcut) {
                editingShortcut = true;
                drawEmojiMaker();
                return;
            } else if (key == 'e' && !editingShortcut) {
                // E to export to LabChat
                if (emojiShortcut.length() > 0) {
                    exportEmojiToLabChat();
                }
                showSaveMenu = false;
                drawEmojiMaker();
                return;
            }
        }

        // Enter to save (if not editing and has name)
        if (status.enter && !editingShortcut) {
            if (emojiShortcut.length() > 0) {
                saveEmojiToLabChat();
            }
            showSaveMenu = false;
            drawEmojiMaker();
        }
        return;
    }

    // Handle load menu
    if (showLoadMenu) {
        // Up/Down navigation
        for (auto key : status.word) {
            if (key == ';') {  // Up arrow
                if (loadMenuIndex > 0) {
                    loadMenuIndex--;
                    drawEmojiMaker();
                }
            }
            if (key == '.') {  // Down arrow
                if (loadMenuIndex < (int)savedEmojis.size() - 1) {
                    loadMenuIndex++;
                    drawEmojiMaker();
                }
            }
        }

        // Enter to load
        if (status.enter && savedEmojis.size() > 0) {
            loadEmojiFromFile(savedEmojis[loadMenuIndex]);
            showLoadMenu = false;
            drawEmojiMaker();
        }
        return;
    }

    // Navigation handling based on current mode
    for (auto key : status.word) {
        // UP arrow
        if (key == ';') {
            if (moveMode && navMode == NAV_GRID) {
                // Move canvas up (shift all pixels up)
                saveUndo();
                uint16_t temp[GRID_SIZE];
                // Save top row
                for (int x = 0; x < GRID_SIZE; x++) {
                    temp[x] = canvas[0][x];
                }
                // Shift all rows up
                for (int y = 0; y < GRID_SIZE - 1; y++) {
                    for (int x = 0; x < GRID_SIZE; x++) {
                        canvas[y][x] = canvas[y + 1][x];
                    }
                }
                // Wrap top row to bottom
                for (int x = 0; x < GRID_SIZE; x++) {
                    canvas[GRID_SIZE - 1][x] = temp[x];
                }
                drawEmojiMaker();
            } else if (navMode == NAV_GRID) {
                cursorY = (cursorY - 1 + GRID_SIZE) % GRID_SIZE;
            } else if (navMode == NAV_PALETTE) {
                if (paletteIndex >= 2) paletteIndex -= 2;
            } else if (navMode == NAV_TOOLS) {
                if (toolIndex >= 2) toolIndex -= 2;  // Move up one row
            } else if (navMode == NAV_GALLERY) {
                if (galleryIndex >= GALLERY_COLS) galleryIndex -= GALLERY_COLS;
            }
            drawEmojiMaker();
        }

        // DOWN arrow
        if (key == '.') {
            if (moveMode && navMode == NAV_GRID) {
                // Move canvas down (shift all pixels down)
                saveUndo();
                uint16_t temp[GRID_SIZE];
                // Save bottom row
                for (int x = 0; x < GRID_SIZE; x++) {
                    temp[x] = canvas[GRID_SIZE - 1][x];
                }
                // Shift all rows down
                for (int y = GRID_SIZE - 1; y > 0; y--) {
                    for (int x = 0; x < GRID_SIZE; x++) {
                        canvas[y][x] = canvas[y - 1][x];
                    }
                }
                // Wrap bottom row to top
                for (int x = 0; x < GRID_SIZE; x++) {
                    canvas[0][x] = temp[x];
                }
                drawEmojiMaker();
            } else if (navMode == NAV_GRID) {
                cursorY = (cursorY + 1) % GRID_SIZE;
            } else if (navMode == NAV_PALETTE) {
                if (paletteIndex < PALETTE_SIZE - 2) paletteIndex += 2;
            } else if (navMode == NAV_TOOLS) {
                if (toolIndex < 4) toolIndex += 2;  // Move down one row (max 6 tools)
            } else if (navMode == NAV_GALLERY) {
                if (galleryIndex + GALLERY_COLS < GALLERY_ROWS * GALLERY_COLS) galleryIndex += GALLERY_COLS;
            }
            drawEmojiMaker();
        }

        // LEFT arrow
        if (key == ',') {
            if (moveMode && navMode == NAV_GRID) {
                // Move canvas left (shift all pixels left)
                saveUndo();
                uint16_t temp[GRID_SIZE];
                // Save left column
                for (int y = 0; y < GRID_SIZE; y++) {
                    temp[y] = canvas[y][0];
                }
                // Shift all columns left
                for (int y = 0; y < GRID_SIZE; y++) {
                    for (int x = 0; x < GRID_SIZE - 1; x++) {
                        canvas[y][x] = canvas[y][x + 1];
                    }
                }
                // Wrap left column to right
                for (int y = 0; y < GRID_SIZE; y++) {
                    canvas[y][GRID_SIZE - 1] = temp[y];
                }
                drawEmojiMaker();
            } else if (navMode == NAV_GRID) {
                if (cursorX == 0) {
                    // Move to palette RIGHT column (closest to grid)
                    navMode = NAV_PALETTE;
                    // Map cursorY (0-15) to palette index (0-15, arranged in 8 rows x 2 cols)
                    int paletteRow = (cursorY * 8) / GRID_SIZE;  // Map to 8 palette rows
                    paletteRow = min(paletteRow, 7);  // Clamp to valid range
                    paletteIndex = paletteRow * 2 + 1;  // Right column (closest to grid)
                } else {
                    cursorX--;
                }
            } else if (navMode == NAV_PALETTE) {
                // Move left within palette
                if (paletteIndex % 2 == 1) {
                    paletteIndex--;  // Move from right to left column
                }
            } else if (navMode == NAV_TOOLS) {
                if (toolIndex % 2 == 1) {
                    toolIndex--;  // Move from right to left column
                } else {
                    // Already in left column, go back to grid
                    navMode = NAV_GRID;
                }
            } else if (navMode == NAV_GALLERY) {
                if (galleryIndex % GALLERY_COLS == 0) {
                    // Move back to tools
                    navMode = NAV_TOOLS;
                } else {
                    galleryIndex--;
                }
            }
            drawEmojiMaker();
        }

        // RIGHT arrow
        if (key == '/') {
            if (moveMode && navMode == NAV_GRID) {
                // Move canvas right (shift all pixels right)
                saveUndo();
                uint16_t temp[GRID_SIZE];
                // Save right column
                for (int y = 0; y < GRID_SIZE; y++) {
                    temp[y] = canvas[y][GRID_SIZE - 1];
                }
                // Shift all columns right
                for (int y = 0; y < GRID_SIZE; y++) {
                    for (int x = GRID_SIZE - 1; x > 0; x--) {
                        canvas[y][x] = canvas[y][x - 1];
                    }
                }
                // Wrap right column to left
                for (int y = 0; y < GRID_SIZE; y++) {
                    canvas[y][0] = temp[y];
                }
                drawEmojiMaker();
            } else if (navMode == NAV_GRID) {
                if (cursorX == GRID_SIZE - 1) {
                    // Move to tools - map grid Y position to tool row
                    navMode = NAV_TOOLS;
                    int toolRow = (cursorY * 3) / GRID_SIZE;  // Map to 3 tool rows
                    toolRow = min(toolRow, 2);
                    toolIndex = toolRow * 2;  // Left column of tools
                } else {
                    cursorX++;
                }
            } else if (navMode == NAV_PALETTE) {
                // Move right within palette or back to grid
                if (paletteIndex % 2 == 0) {
                    paletteIndex++;  // Move from left to right column
                } else {
                    // Already in right column (closest to grid), go back to grid
                    navMode = NAV_GRID;
                }
            } else if (navMode == NAV_TOOLS) {
                if (toolIndex % 2 == 0) {
                    toolIndex++;  // Move from left to right column
                } else {
                    // Already in right column, go to gallery
                    navMode = NAV_GALLERY;
                    int toolRow = toolIndex / 2;
                    int galleryRow = (toolRow * GALLERY_ROWS) / 3;  // Map tool row to gallery row
                    galleryRow = min(galleryRow, GALLERY_ROWS - 1);
                    galleryIndex = galleryRow * GALLERY_COLS;
                }
            } else if (navMode == NAV_GALLERY) {
                if (galleryIndex % GALLERY_COLS < GALLERY_COLS - 1) galleryIndex++;
            }
            drawEmojiMaker();
        }

        // Number keys to select color (0-9)
        if (key >= '0' && key <= '9') {
            int colorIndex = (key - '0');
            if (colorIndex < PALETTE_SIZE) {
                selectedColor = colorIndex;
                drawEmojiMaker();
            }
        }

        // Spacebar to erase (paint transparent/lime green on current pixel)
        if (key == ' ') {
            if (navMode == NAV_GRID) {
                saveUndo();
                canvas[cursorY][cursorX] = TRANSPARENCY_COLOR;
                drawEmojiMaker();
            }
        }

        // C to clear canvas
        if (key == 'c') {
            saveUndo();
            clearCanvas();
            drawEmojiMaker();
        }

        // S to save
        if (key == 's') {
            showSaveMenu = true;
            drawEmojiMaker();
        }

        // N to edit name
        if (key == 'n') {
            editingShortcut = true;
            drawEmojiMaker();
        }

        // M to toggle move mode
        if (key == 'm') {
            moveMode = !moveMode;
            drawEmojiMaker();
        }

        // R to toggle resize mode
        if (key == 'r') {
            resizeMode = !resizeMode;
            drawEmojiMaker();
        }

        // + to grow (2x upscale - each pixel becomes 2x2)
        if (key == '+' || key == '=') {
            if (resizeMode) {
                saveUndo();
                uint16_t newCanvas[GRID_SIZE][GRID_SIZE];
                // Fill with transparency first
                for (int y = 0; y < GRID_SIZE; y++) {
                    for (int x = 0; x < GRID_SIZE; x++) {
                        newCanvas[y][x] = TRANSPARENCY_COLOR;
                    }
                }
                // Scale up 2x (each pixel becomes 2x2), centered
                for (int y = 0; y < GRID_SIZE / 2; y++) {
                    for (int x = 0; x < GRID_SIZE / 2; x++) {
                        uint16_t color = canvas[y][x];
                        // Place 2x2 block in center of grid
                        int newX = x * 2 + GRID_SIZE / 4;
                        int newY = y * 2 + GRID_SIZE / 4;
                        if (newX < GRID_SIZE - 1 && newY < GRID_SIZE - 1) {
                            newCanvas[newY][newX] = color;
                            newCanvas[newY][newX + 1] = color;
                            newCanvas[newY + 1][newX] = color;
                            newCanvas[newY + 1][newX + 1] = color;
                        }
                    }
                }
                // Copy back to canvas
                for (int y = 0; y < GRID_SIZE; y++) {
                    for (int x = 0; x < GRID_SIZE; x++) {
                        canvas[y][x] = newCanvas[y][x];
                    }
                }
                drawEmojiMaker();
            }
        }

        // - to shrink (2x downscale - each 2x2 becomes 1 pixel)
        if (key == '-' || key == '_') {
            if (resizeMode) {
                saveUndo();
                uint16_t newCanvas[GRID_SIZE][GRID_SIZE];
                // Fill with transparency first
                for (int y = 0; y < GRID_SIZE; y++) {
                    for (int x = 0; x < GRID_SIZE; x++) {
                        newCanvas[y][x] = TRANSPARENCY_COLOR;
                    }
                }
                // Scale down 2x (sample every other pixel), centered
                for (int y = 0; y < GRID_SIZE; y += 2) {
                    for (int x = 0; x < GRID_SIZE; x += 2) {
                        // Sample the pixel (could average 2x2 block but let's just take top-left)
                        uint16_t color = canvas[y][x];
                        int newX = x / 2 + GRID_SIZE / 4;
                        int newY = y / 2 + GRID_SIZE / 4;
                        if (newX < GRID_SIZE && newY < GRID_SIZE) {
                            newCanvas[newY][newX] = color;
                        }
                    }
                }
                // Copy back to canvas
                for (int y = 0; y < GRID_SIZE; y++) {
                    for (int x = 0; x < GRID_SIZE; x++) {
                        canvas[y][x] = newCanvas[y][x];
                    }
                }
                drawEmojiMaker();
            }
        }
    }

    // Delete key for undo
    if (status.del && navMode == NAV_GRID) {
        performUndo();
        drawEmojiMaker();
    }

    // ENTER key actions
    if (status.enter) {
        if (navMode == NAV_GRID) {
            // Apply active tool
            saveUndo();  // Save state before any modification

            if (activeTool == TOOL_PENCIL) {
                canvas[cursorY][cursorX] = palette[selectedColor];
                drawEmojiMaker();
            } else if (activeTool == TOOL_BIG_BRUSH) {
                // Paint 2x2 block
                for (int dy = 0; dy < 2 && cursorY + dy < GRID_SIZE; dy++) {
                    for (int dx = 0; dx < 2 && cursorX + dx < GRID_SIZE; dx++) {
                        canvas[cursorY + dy][cursorX + dx] = palette[selectedColor];
                    }
                }
                drawEmojiMaker();
            } else if (activeTool == TOOL_FILL) {
                uint16_t targetColor = canvas[cursorY][cursorX];
                floodFill(cursorX, cursorY, targetColor, palette[selectedColor]);
                drawEmojiMaker();
            } else if (activeTool == TOOL_CIRCLE) {
                if (!toolWaitingForInput) {
                    // First click - check if near ghost center (7,7) or (7,8) or (8,7) or (8,8)
                    // If cursor is on any of the 4 center pixels, snap to ghost center
                    useGhostCenter = false;
                    if ((cursorX == 7 || cursorX == 8) && (cursorY == 7 || cursorY == 8)) {
                        useGhostCenter = true;
                        toolStartX = 7;  // Will use 7.5 in drawing
                        toolStartY = 7;  // Will use 7.5 in drawing
                    } else {
                        toolStartX = cursorX;
                        toolStartY = cursorY;
                    }
                    toolWaitingForInput = true;
                } else {
                    // Second click - draw circle
                    int dx = cursorX - toolStartX;
                    int dy = cursorY - toolStartY;
                    int radius = (int)sqrt(dx*dx + dy*dy);
                    drawCircle(toolStartX, toolStartY, radius, palette[selectedColor]);
                    toolWaitingForInput = false;
                    useGhostCenter = false;
                    drawEmojiMaker();
                }
            } else if (activeTool == TOOL_LINE) {
                if (!toolWaitingForInput) {
                    // First click - store start
                    toolStartX = cursorX;
                    toolStartY = cursorY;
                    toolWaitingForInput = true;
                } else {
                    // Second click - draw line
                    drawLine(toolStartX, toolStartY, cursorX, cursorY, palette[selectedColor]);
                    toolWaitingForInput = false;
                    drawEmojiMaker();
                }
            } else if (activeTool == TOOL_ROTATE) {
                rotateCanvas90();
                drawEmojiMaker();
            }
        } else if (navMode == NAV_PALETTE) {
            // Select color
            selectedColor = paletteIndex;
            navMode = NAV_GRID;
            drawEmojiMaker();
        } else if (navMode == NAV_TOOLS) {
            // Activate selected tool
            activeTool = (ToolMode)toolIndex;
            navMode = NAV_GRID;
            drawEmojiMaker();
        } else if (navMode == NAV_GALLERY) {
            // Load emoji from gallery
            if (galleryIndex < galleryCacheCount) {
                memcpy(canvas, galleryCache[galleryIndex], sizeof(canvas));
                navMode = NAV_GRID;
                drawEmojiMaker();
            }
        }
    }
}

static void saveEmojiToLabChat() {
    // Create /labchat/emojis directory if it doesn't exist
    if (!SD.exists("/labchat")) {
        SD.mkdir("/labchat");
    }
    if (!SD.exists("/labchat/emojis")) {
        SD.mkdir("/labchat/emojis");
    }

    // Save with shortcut name
    String filename = "/labchat/emojis/" + emojiShortcut + ".emoji";

    // Save canvas data
    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
        M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_RED);
        M5Cardputer.Display.setTextColor(TFT_WHITE);
        M5Cardputer.Display.drawString("Save failed!", 10, 5);
        delay(1000);
        return;
    }

    // Write 16x16 grid of uint16_t colors
    file.write((uint8_t*)canvas, sizeof(canvas));
    file.close();

    // Success message
    M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_GREEN);
    M5Cardputer.Display.setTextColor(TFT_BLACK);
    M5Cardputer.Display.drawString("Saved: :" + emojiShortcut, 10, 5);
    delay(1500);

    // Reload gallery cache to show new emoji
    loadGalleryCache();

    needsFullRedraw = true;
}

static void exportEmojiToLabChat() {
    // Create /labchat/system_emojis directory if it doesn't exist
    if (!SD.exists("/labchat")) {
        SD.mkdir("/labchat");
    }
    if (!SD.exists("/labchat/system_emojis")) {
        SD.mkdir("/labchat/system_emojis");
    }

    // Show converting message
    M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_BLUE);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.drawString("Converting...", 80, 5);
    delay(300); // Give user visual feedback

    // Check if we already have 20 system emojis
    File dir = SD.open("/labchat/system_emojis");
    int emojiCount = 0;
    if (dir && dir.isDirectory()) {
        File file = dir.openNextFile();
        while (file) {
            if (!file.isDirectory() && String(file.name()).endsWith(".emoji")) {
                emojiCount++;
            }
            file = dir.openNextFile();
        }
        dir.close();
    }

    // Count strawberry as slot 0 if it exists
    bool hasStrawberry = SD.exists("/labchat/system_emojis/strawberry.emoji");
    if (!hasStrawberry) emojiCount++; // Reserve slot 0 for strawberry

    if (emojiCount >= 20) {
        M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_RED);
        M5Cardputer.Display.setTextColor(TFT_WHITE);
        M5Cardputer.Display.drawString("Error: 20 slots full!", 50, 5);
        delay(2000);
        return;
    }

    // Save system emoji with shortcut name
    String filename = "/labchat/system_emojis/" + emojiShortcut + ".emoji";

    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
        M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_RED);
        M5Cardputer.Display.setTextColor(TFT_WHITE);
        M5Cardputer.Display.drawString("Export failed!", 70, 5);
        delay(1000);
        return;
    }

    // Write 16x16 grid of uint16_t colors (512 bytes)
    file.write((uint8_t*)canvas, sizeof(canvas));
    file.flush(); // Force write to SD card
    file.close();

    // Update manifest file
    File manifest = SD.open("/labchat/system_emojis/manifest.txt", FILE_WRITE);
    if (manifest) {
        manifest.println(emojiShortcut);
        manifest.flush(); // Force write to SD card
        manifest.close();
    }

    // CRITICAL: Wait for SD card to finish writing before reload
    delay(100);

    // Success message
    M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_GREEN);
    M5Cardputer.Display.setTextColor(TFT_BLACK);
    M5Cardputer.Display.drawString("Exported: :" + emojiShortcut, 50, 5);

    // Reload system emojis so new emoji is immediately available in LabCHAT
    extern void reloadSystemEmojis();
    reloadSystemEmojis();

    // Show success message after reload completes
    delay(1500);

    needsFullRedraw = true;
}

static void loadSavedEmojis() {
    savedEmojis.clear();

    if (!SD.exists("/labchat/emojis")) {
        return;
    }

    File dir = SD.open("/labchat/emojis");
    if (!dir || !dir.isDirectory()) {
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".emoji")) {
                savedEmojis.push_back(name);
            }
        }
        file = dir.openNextFile();
    }
    dir.close();
}

static void loadEmojiFromFile(const String& filename) {
    String fullPath = "/labchat/emojis/" + filename;

    File file = SD.open(fullPath, FILE_READ);
    if (!file) {
        return;
    }

    // Read canvas data
    file.read((uint8_t*)canvas, sizeof(canvas));
    file.close();

    // Extract shortcut name
    emojiShortcut = filename;
    emojiShortcut.replace(".emoji", "");

    needsFullRedraw = true;
}

static void loadGalleryCache() {
    // Always start with strawberry sample in slot 0
    galleryCacheCount = 1;

    // Create system strawberry emoji (16x16) - matches ui.cpp drawEmojiIcon
    // Red circle (radius 3*scale=3*5.33≈16px diameter) + green rectangle on top
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            galleryCache[0][y][x] = TRANSPARENCY_COLOR;  // Start with transparent background
        }
    }

    // Draw green leaf rectangle (top center)
    // Original: x+2*scale to x+6*scale, y+1*scale to y+3*scale (where scale≈1.33 for 16x16)
    for (int y = 1; y < 4; y++) {
        for (int x = 3; x < 10; x++) {
            galleryCache[0][y][x] = TFT_GREEN;
        }
    }

    // Draw red circle (center at x=8, y=9, radius=5)
    int cx = 8, cy = 9, radius = 5;
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx*dx + dy*dy <= radius*radius) {
                galleryCache[0][y][x] = TFT_RED;
            }
        }
    }

    // Load saved emojis from SD card (if any)
    if (!SD.exists("/labchat/emojis")) {
        return;
    }

    File dir = SD.open("/labchat/emojis");
    if (!dir || !dir.isDirectory()) {
        return;
    }

    // Get all emoji files
    std::vector<String> emojiFiles;
    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".emoji")) {
                emojiFiles.push_back(name);
            }
        }
        file = dir.openNextFile();
    }
    dir.close();

    // Load up to 11 most recent (slot 0 is strawberry, so 1-11 for saved emojis)
    int startIdx = max(0, (int)emojiFiles.size() - 11);
    for (int i = startIdx; i < emojiFiles.size() && galleryCacheCount < 12; i++) {
        String fullPath = "/labchat/emojis/" + emojiFiles[i];
        File f = SD.open(fullPath, FILE_READ);
        if (f) {
            f.read((uint8_t*)galleryCache[galleryCacheCount], sizeof(galleryCache[0]));
            f.close();
            galleryCacheCount++;
        }
    }
}

static bool canvasHasContent() {
    // Check if canvas has any non-white pixels
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            if (canvas[y][x] != TFT_WHITE) {
                return true;
            }
        }
    }
    return false;
}
