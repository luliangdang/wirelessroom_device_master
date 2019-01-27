#ifndef __KEYBOARD_H
#define __KEYBOARD_H
#include "sys.h"

void key_load_ui(u16 x,u16 y);
void key_staset(u16 x,u16 y,u8 keyx,u8 sta);
u8 get_keynum(u16 x,u16 y);

#endif


