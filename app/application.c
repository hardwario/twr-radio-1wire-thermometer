#include <application.h>
#include <at.h>

#define SEND_DATA_INTERVAL        (15 * 60 * 1000)
#define MEASURE_INTERVAL               (30 * 1000)

#define DS18B20_SENSOR_COUNT 10
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer_0, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer_1, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer_2, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer_3, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer_4, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer_5, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer_6, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer_7, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer_8, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))
TWR_DATA_STREAM_FLOAT_BUFFER(sm_temperature_buffer_9, (SEND_DATA_INTERVAL / MEASURE_INTERVAL))

twr_data_stream_t sm_temperature_0;
twr_data_stream_t sm_temperature_1;
twr_data_stream_t sm_temperature_2;
twr_data_stream_t sm_temperature_3;
twr_data_stream_t sm_temperature_4;
twr_data_stream_t sm_temperature_5;
twr_data_stream_t sm_temperature_6;
twr_data_stream_t sm_temperature_7;
twr_data_stream_t sm_temperature_8;
twr_data_stream_t sm_temperature_9;

twr_data_stream_t *sm_temperature[] =
{
    &sm_temperature_0,
    &sm_temperature_1,
    &sm_temperature_2,
    &sm_temperature_3,
    &sm_temperature_4,
    &sm_temperature_5,
    &sm_temperature_6,
    &sm_temperature_7,
    &sm_temperature_8,
    &sm_temperature_9
};

TWR_DATA_STREAM_FLOAT_BUFFER(sm_voltage_buffer, 8)

twr_data_stream_t sm_voltage;

// LED instance
twr_led_t led;
// Button instance
twr_button_t button;
// ds18b20 library instance
static twr_ds18b20_t ds18b20;
// ds18b20 sensors array
static twr_ds18b20_sensor_t ds18b20_sensors[DS18B20_SENSOR_COUNT];

twr_scheduler_task_id_t battery_measure_task_id;

enum {
    HEADER_BOOT         = 0x00,
    HEADER_UPDATE       = 0x01,
    HEADER_BUTTON_CLICK = 0x02,
    HEADER_BUTTON_HOLD  = 0x03,

} header = HEADER_BOOT;


void handler_battery(twr_module_battery_event_t e, void *p);

void handler_ds18b20(twr_ds18b20_t *s, uint64_t device_id, twr_ds18b20_event_t e, void *p);

void climate_module_event_handler(twr_module_climate_event_t event, void *event_param);

void switch_to_normal_mode_task(void *param);

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == TWR_BUTTON_EVENT_CLICK)
    {
        header = HEADER_BUTTON_CLICK;

        twr_scheduler_plan_now(0);
    }
    else if (event == TWR_BUTTON_EVENT_HOLD)
    {
        header = HEADER_BUTTON_HOLD;

        twr_scheduler_plan_now(0);
    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    (void) event_param;

    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        float voltage = NAN;

        twr_module_battery_get_voltage(&voltage);

        twr_data_stream_feed(&sm_voltage, &voltage);
    }
}

void battery_measure_task(void *param)
{
    (void) param;

    if (!twr_module_battery_measure())
    {
        twr_scheduler_plan_current_now();
    }
}

void handler_ds18b20(twr_ds18b20_t *self, uint64_t device_address, twr_ds18b20_event_t event, void *event_param)
{
    (void) event_param;

    float value = NAN;

    if (event == TWR_DS18B20_EVENT_UPDATE)
    {
        twr_ds18b20_get_temperature_celsius(self, device_address, &value);
        int device_index = twr_ds18b20_get_index_by_device_address(self, device_address);

        //twr_log_debug("UPDATE %" PRIx64 "(%d) = %f", device_address, device_index, value);

        twr_data_stream_feed(sm_temperature[device_index], &value);
    }
}

bool at_send(void)
{
    twr_scheduler_plan_now(0);

    return true;
}


bool at_status(void)
{
    float value_avg = NAN;

    if (twr_data_stream_get_average(&sm_voltage, &value_avg))
    {
        twr_atci_printfln("$STATUS: \"Voltage\",%.1f", value_avg);
    }
    else
    {
        twr_atci_printfln("$STATUS: \"Voltage\",");
    }

    int sensor_found = twr_ds18b20_get_sensor_found(&ds18b20);

    for (int i = 0; i < sensor_found; i++)
    {
        value_avg = NAN;

        if (twr_data_stream_get_average(sm_temperature[i], &value_avg))
        {
            twr_atci_printfln("$STATUS: \"Temperature%d\",%.1f", i, value_avg);
        }
        else
        {
            twr_atci_printfln("$STATUS: \"Temperature%d\",", i);
        }
    }

    return true;
}

void application_init(void)
{
    // twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    battery_measure_task_id = twr_scheduler_register(battery_measure_task, NULL, 2020);

    // Initialize Sensor Module
    twr_module_sensor_init();

    // Initialize 1-Wire temperature sensors
    twr_ds18b20_init_multiple(&ds18b20, ds18b20_sensors, DS18B20_SENSOR_COUNT, TWR_DS18B20_RESOLUTION_BITS_12);
    twr_ds18b20_set_event_handler(&ds18b20, handler_ds18b20, NULL);
    twr_ds18b20_set_update_interval(&ds18b20, MEASURE_INTERVAL);

    // Init stream buffers for averaging
    twr_data_stream_init(&sm_voltage, 1, &sm_voltage_buffer);
    twr_data_stream_init(&sm_temperature_0, 1, &sm_temperature_buffer_0);
    twr_data_stream_init(&sm_temperature_1, 1, &sm_temperature_buffer_1);
    twr_data_stream_init(&sm_temperature_2, 1, &sm_temperature_buffer_2);
    twr_data_stream_init(&sm_temperature_3, 1, &sm_temperature_buffer_3);
    twr_data_stream_init(&sm_temperature_4, 1, &sm_temperature_buffer_4);
    twr_data_stream_init(&sm_temperature_5, 1, &sm_temperature_buffer_5);
    twr_data_stream_init(&sm_temperature_6, 1, &sm_temperature_buffer_6);
    twr_data_stream_init(&sm_temperature_7, 1, &sm_temperature_buffer_7);
    twr_data_stream_init(&sm_temperature_8, 1, &sm_temperature_buffer_8);
    twr_data_stream_init(&sm_temperature_9, 1, &sm_temperature_buffer_9);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_pairing_request("1wire-thermometer", VERSION);

    // Initialize AT command interface
    at_init(&led);
    static const twr_atci_command_t commands[] = {
            {"$SEND", at_send, NULL, NULL, NULL, "Immediately send packet"},
            {"$STATUS", at_status, NULL, NULL, NULL, "Show status"},
            AT_LED_COMMANDS,
            TWR_ATCI_COMMAND_CLAC,
            TWR_ATCI_COMMAND_HELP
    };
    twr_atci_init(commands, TWR_ATCI_COMMANDS_LENGTH(commands));

    // Plan task 0 (application_task) to be run after 10 seconds
    twr_scheduler_plan_relative(0, 10 * 1000);
    
    twr_led_pulse(&led, 2000);
}


void application_task(void)
{
    twr_scheduler_plan_relative(battery_measure_task_id, 0);

    float voltage_avg = NAN;

    twr_data_stream_get_average(&sm_voltage, &voltage_avg);

    bc_radio_pub_battery(&voltage_avg);

    int sensor_found = twr_ds18b20_get_sensor_found(&ds18b20);

    for (int i = 0; i < sensor_found; i++)
    {
        float temperature_avg = NAN;

        twr_data_stream_get_average(sm_temperature[i], &temperature_avg);

        if (!isnan(temperature_avg))
        {
            bc_radio_pub_temperature(i, &temperature_avg);
        }
        else
        {
            bc_radio_pub_temperature(i, NULL);
        }
    }

    header = HEADER_UPDATE;

    twr_scheduler_plan_current_relative(SEND_DATA_INTERVAL);
}
