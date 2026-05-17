#include "floor.h"
#include "cabin.h"

#define TIME_OPENED 6000
#define TIME_DOORS  1700
#define TIME_FLOOR_SHORT 7150
#define TIME_FLOOR_LONG  7700

#define FLOOR_NUM 6

floor_info building[FLOOR_NUM] = {
  //                                --led--   --btn--
  // title          key disp  def   up  down  up   down  pressed
  { "Parking   " , '*', -1, false ,  9,   0,  84,   -1,   0 },
  { "RDC       " , '0',  0, true  , 11,  10, 101,   93,   0 },
  { "1er etage " , '1',  1, false , 13,  12, 118,  110,   0 },
  { "2eme etage" , '2',  2, false , 15,  14, 133,  126,   0 },
  { "3eme etage" , '3',  3, false , 17,  16, 149,  141,   0 },
  { "4eme etage" , '4',  4, false ,  0,  18,  -1,  156,   0 }
};

enum states {
  STATE_OPENED,
  STATE_MOVING,
  STATE_OPENING,
  STATE_CLOSING
};

states state = STATE_OPENED;
timer_ms timer;
int target = -1;

unsigned long movetime();

void setup() {
  Serial.begin(9600);
  cabin_init(
    floor_init(building, FLOOR_NUM)
  );
}

void loop() {
  const char* status = nullptr;

  floor_readbtns();
  switch(state) {
    case STATE_OPENED:
      target = floor_requested(cabin_current_floor());
      status = "(waiting...)   ";
      if(timer_elapsed(timer, TIME_OPENED) && target>=0) {
        cabin_door(CABIN_DOOR_CLOSE);
        state = STATE_CLOSING;
      }
      break;
    case STATE_CLOSING:
      cabin_door(CABIN_DOOR_CLOSE);
      status = "(closing doors)";
      if(timer_elapsed(timer, TIME_DOORS)) {
        cabin_door(CABIN_DOOR_STOP);
        state = STATE_MOVING;
      }
      break;
    case STATE_MOVING: 
      status = "(moving)       ";
      if(cabin_move(timer, target, movetime()) == target) {
        cabin_stop();
        state = STATE_OPENING;
      }
      break;
    case STATE_OPENING:
      status = "(opening doors)";
      cabin_door(CABIN_DOOR_OPEN);
      if(timer_elapsed(timer, TIME_DOORS)) {
        cabin_door(CABIN_DOOR_STOP);
        state = STATE_OPENED;  
      }
      break;
  }
  floor_feedback(cabin_current_floor(), status);
}

unsigned long movetime() {
  auto cur = cabin_current_floor();
  auto odd = (cur % 2) != 0;

  if(target < cur && odd || target > cur && !odd) {
    return TIME_FLOOR_SHORT;
  }
  else {
    return TIME_FLOOR_LONG;
  }
}