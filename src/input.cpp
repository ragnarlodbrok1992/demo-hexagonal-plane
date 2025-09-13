#ifndef _H_INPUT
#define _H_INPUT

#include <windef.h>

typedef struct Input {
  POINT last_mouse_pos;
  bool left_mouse_button_down = false;
} Input;

#endif /* _H_INPUT */

