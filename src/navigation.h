#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "config.h"

void navigateUp();
void navigateLeft();
void navigateRight();
void handleSelect();
void handleBack();
void drawStillStar();
void updateStarGifPlayback();
void returnToMainMenu(); // Helper to return to main menu with CPU scaling

#endif