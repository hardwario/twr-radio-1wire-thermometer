#ifndef _AT_H
#define _AT_H

#include <bcl.h>

#define AT_LED_COMMANDS {"$BLINK", at_blink, NULL, NULL, NULL, "LED blink 3 times"},\
                        {"$LED", NULL, at_led_set, NULL, at_led_help, "LED on/off"}

void at_init(twr_led_t *led);

bool at_blink(void);
bool at_led_set(twr_atci_param_t *param);
bool at_led_help(void);

#endif // _AT_H
