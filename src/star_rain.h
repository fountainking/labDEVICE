#ifndef STAR_RAIN_H
#define STAR_RAIN_H

#include <Arduino.h>
#include <vector>

// Star rain configuration
#define MAX_STARS 400  // Heavy rain effect - reduced from 1500 to fit with 32KB canvas buffer
#define STAR_COLUMNS 40  // 240px width / 6px per char = 40 columns
#define STAR_SPEED_BASE 3  // Base speed for rain
#define STAR_SPAWN_CHANCE 98  // Very frequent spawning for heavy rain

// Star structure
struct FallingStar {
  int x;           // X position (column number)
  int y;           // Y position (pixels)
  int prevX;       // Previous X position (for erasing)
  int prevY;       // Previous Y position (for erasing)
  int speed;       // Fall speed (pixels per frame)
  uint16_t color;  // Star color
  char symbol;     // Character to display ('*' or variations)
  bool active;     // Is this star slot in use
  int trail;       // Trail length (1-5) for varied raindrop streaks
  int trailOpacity; // How visible the trail is
};

// Star rain mode
enum StarRainMode {
  STARRAIN_SCREENSAVER,
  STARRAIN_TERMINAL
};

// Global star rain state
extern std::vector<FallingStar> fallingStars;
extern StarRainMode currentStarMode;
extern bool starRainActive;
extern float gravityX;  // Horizontal tilt
extern float gravityY;  // Vertical tilt

// Star rain functions
void initStarRain(StarRainMode mode);
void updateStarRain();
void drawStarRain();
void stopStarRain();
void updateStarGravity();  // Read accelerometer and update gravity
void spawnStar();

// Helper functions
uint16_t getRandomStarColor();
int getRandomStarSpeed();

#endif
