#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include <Keypad.h>
#include "timer.h"

#include "floor.h"

#define BTN_UP    0b0001 
#define BTN_DOWN  0b0010 
#define BTN_PANEL 0b0100 

#define IS_PRESSED(val, flag) (((val)&(flag))==(flag))

static byte rows[] = PIN_KEYMAP_ROWS;
static byte cols[] = PIN_KEYMAP_COLS;

static Keypad _keys(const_cast<char*>(KEYMAP), rows, cols, sizeof(rows)/sizeof(byte), sizeof(cols)/sizeof(byte));
static Adafruit_NeoPixel _leds(MAX_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);
static LiquidCrystal_I2C _lcd(I2C_LCD , MAX_LCD_COLS, 2);

static floor_info *_floors;
static size_t _floors_size;

static bool ladder_pressed(int expected, int actual);
static void neopixel_switch(int led, bool on);

int floor_init(floor_info floors[], size_t floor_num) {
  int start = 0;

  _floors = floors;
  _floors_size = floor_num;
  for(int i=0; i<_floors_size; i++) {
    _floors[i].upper7seg.begin(I2C_7SEG_BASE + i);
    if(_floors[i].def) {
      start = i;
    }
  }
  _lcd.init();
  _lcd.backlight();
  _leds.begin();
  floor_feedback(start);
  return start;
}

void floor_feedback(int current, const char* status) {
  static timer_ms refresh = 0L;
  static int last_floor = -1;
  static const char* last_status = nullptr;
  
  if(!timer_elapsed(refresh, FLOOR_FEEDBACK_PERIOD)) {
    return;
  }

  // Efface l'écran si l'étage ou le statut a changé
  if(current != last_floor || status != last_status) {
    _lcd.clear();
    last_floor = current;
    last_status = status;
  }

  _lcd.setCursor(0, 0);
  _lcd.print(_floors[current].title);

  if(status) {
    _lcd.setCursor(0, 1);
    _lcd.print(status);
  }

  for(int i=0; i<_floors_size; i++) {
    floor_info& info = _floors[i];
    auto btns = info.pressed;

    neopixel_switch(info.ledUp  , IS_PRESSED(btns, BTN_UP));
    neopixel_switch(info.ledDown, IS_PRESSED(btns, BTN_DOWN));
    neopixel_switch(i           , IS_PRESSED(btns, BTN_PANEL));
    info.upper7seg.printNumber(_floors[current].displayVal, 10);
    info.upper7seg.writeDisplay();
  }
  _leds.show();
}

void floor_readbtns() {
  int btn = analogRead(PIN_CALLBTNS);
  char key = _keys.getKey();
  
  for(int i=0; i < _floors_size; i++) {
    if(_floors[i].key == key) {
      _floors[i].pressed |= BTN_PANEL;
    }
    if(ladder_pressed(_floors[i].callDown, btn)) {
      _floors[i].pressed |= BTN_DOWN;
    }
    else if(ladder_pressed(_floors[i].callUp, btn)) {
      _floors[i].pressed |= BTN_UP;
    }
  }
}

int floor_requested(int from) {
  _floors[from].pressed = 0;
  for(int i = 0; i < _floors_size; i++) {
    int index = (from + i)%_floors_size;
    
    if(_floors[index].pressed) {
      return index;
    }
  }
  return -1;
}

bool ladder_pressed(int expected, int actual) {
  return expected !=-1 
      && expected-5 < actual && actual < expected+5;
}

void neopixel_switch(int led, bool on) {
  uint32_t color;

  if(on) {
    color = Adafruit_NeoPixel::Color(255, 64, 0);
  }
  else {
    color = Adafruit_NeoPixel::Color(0, 0, 0);
  }
  _leds.setPixelColor(led, color);
}