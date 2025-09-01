//
// Created by shinys on 25. 7. 29.
//

#ifndef LED_H
#define LED_H

enum blink_type
{
    BLINK_SLOW = 0,
    BLINK_FAST,
    BLINK_DOUBLE,
    BLINK_TRIPLE,
    BLINK_SOLID,
    BLINK_MAX,
};

enum blink_led
{
    LED_RED = 0,
    LED_BLU = 1,
    LED_MAX,
};

void init_led(void);
void led_set(enum blink_led led, enum blink_type type);
void led_off(enum blink_led led);

#endif // LED_H
