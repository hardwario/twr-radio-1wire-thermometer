#include <at.h>
#include <bcl.h>

static struct
{
    twr_led_t *led;
    twr_cmwx1zzabz_t *lora;
    char tmp[36];

} _at;

void at_init(twr_led_t *led)
{
    _at.led = led;
}

bool at_blink(void)
{
    twr_led_blink(_at.led, 3);

    return true;
}

bool at_led_set(twr_atci_param_t *param)
{
    if (param->length != 1)
    {
        return false;
    }

    if (param->txt[0] == '1')
    {
        twr_led_set_mode(_at.led, TWR_LED_MODE_ON);

        return true;
    }

    if (param->txt[0] == '0')
    {
        twr_led_set_mode(_at.led, TWR_LED_MODE_OFF);

        return true;
    }

    return false;
}

bool at_led_help(void)
{
    twr_atci_printfln("$LED: (0,1)");

    return true;
}
