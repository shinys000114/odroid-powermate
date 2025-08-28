//
// Created by vl011 on 2025-08-28.
//

#ifndef ODROID_POWER_MATE_SW_H
#define ODROID_POWER_MATE_SW_H
#include <stdbool.h>

void init_sw();
void trig_power();
void trig_reset();
void set_main_load_switch(bool on);
void set_usb_load_switch(bool on);
bool get_main_load_switch();
bool get_usb_load_switch();

#endif //ODROID_POWER_MATE_SW_H
