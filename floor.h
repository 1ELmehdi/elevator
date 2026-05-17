/**
 * @file floor.h
 * @brief Floor/Elevator component management functions.
 */
#ifndef _FLOOR_H
#define _FLOOR_H

#include <Adafruit_LEDBackpack.h>

#define FLOOR_FEEDBACK_PERIOD 100 

#define I2C_LCD       0x20
#define I2C_7SEG_BASE	0x70

#define PIN_LEDS      15
#define PIN_CALLBTNS  A0
#define PIN_KEYMAP_ROWS { 13, 12, 11, 10 }
#define PIN_KEYMAP_COLS {  9,  8,  7,  6 }

#define KEYMAP "123A" "456B" "789C" "*0#D"

#define MAX_LEDS         19
#define MAX_LCD_COLS     16
#define MAX_FLOOR_TITLE  MAX_LCD_COLS+1

/**
 * @struct floor_info
 * @brief Holds configuration and state information for a specific floor.
 */
struct floor_info {
  char title[MAX_FLOOR_TITLE]; 
  char key; 
  int displayVal; 
  bool def; 
  int ledUp, ledDown; 
  int callUp, callDown; 
  int pressed; 
  Adafruit_7segment upper7seg;
};

int  floor_init     (floor_info floors[], size_t floor_num);
void floor_readbtns ();
void floor_feedback (int current, const char *status = nullptr);
int  floor_requested(int from);

/**
 * @brief Checks if the STOP button has been pressed.
 * @return true if STOP was pressed, false otherwise.
 */
bool floor_stop_pressed();

/**
 * @brief Checks if the open door button has been pressed.
 * @return true if open button was pressed, false otherwise.
 */
bool floor_open_pressed();

/**
 * @brief Checks if the close door button has been pressed.
 * @return true if close button was pressed, false otherwise.
 */
bool floor_close_pressed();

#endif