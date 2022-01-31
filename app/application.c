#include <application.h>
#include <at.h>

#define SEND_DATA_INTERVAL            (15 * 60 * 1000)
#define MEASURE_INTERVAL                   (30 * 1000)
#define BATTERY_UPDATE_INTERVAL       (60 * 60 * 1000)

#define DS18B20_SENSOR_COUNT 10

#define NUMBER_OF_SAMPLES (SEND_DATA_INTERVAL / MEASURE_INTERVAL)

float sm_temperature_buffer_feed[DS18B20_SENSOR_COUNT][NUMBER_OF_SAMPLES];
float sm_temperature_buffer_sort[DS18B20_SENSOR_COUNT][NUMBER_OF_SAMPLES];
twr_data_stream_buffer_t sm_temperature_buffer[DS18B20_SENSOR_COUNT];
twr_data_stream_t sm_temperature[DS18B20_SENSOR_COUNT];

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

        float voltage_avg = NAN;
        twr_data_stream_get_average(&sm_voltage, &voltage_avg);
        twr_radio_pub_battery(&voltage_avg);
    }
}

void handler_ds18b20(twr_ds18b20_t *self, uint64_t device_address, twr_ds18b20_event_t event, void *event_param)
{
    (void) event_param;

    float value = NAN;

    if (device_address == 0)
    {
        twr_log_debug("handler_ds18b20 event: %d", event);
        return;
    }

    int device_index = twr_ds18b20_get_index_by_device_address(self, device_address);

    if (event == TWR_DS18B20_EVENT_UPDATE)
    {
        twr_ds18b20_get_temperature_celsius(self, device_address, &value);
        // twr_log_debug("UPDATE %" PRIx64 "(%d) = %f", device_address, device_index, value);
        twr_data_stream_feed(&sm_temperature[device_index], &value);
    } else {
        twr_data_stream_reset(&sm_temperature[device_index]);
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

        if (twr_data_stream_get_average(&sm_temperature[i], &value_avg))
        {
            twr_atci_printfln("$STATUS: \"Temperature\",%016" PRIx64 ",%.1f", ds18b20_sensors[i]._device_address, value_avg);
        }
        else
        {
            twr_atci_printfln("$STATUS: \"Temperature\",%016" PRIx64 ",", ds18b20_sensors[i]._device_address);
        }
    }

    return true;
}

void application_init(void)
{
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize Sensor Module
    twr_module_sensor_init();

    // Initialize 1-Wire temperature sensors
    twr_ds18b20_init_multiple(&ds18b20, ds18b20_sensors, DS18B20_SENSOR_COUNT, TWR_DS18B20_RESOLUTION_BITS_12);
    twr_ds18b20_set_event_handler(&ds18b20, handler_ds18b20, NULL);
    twr_ds18b20_set_update_interval(&ds18b20, MEASURE_INTERVAL);
    // twr_ds18b20_set_power_dynamic(&ds18b20, true);

    // Init stream buffers for averaging
    twr_data_stream_init(&sm_voltage, 1, &sm_voltage_buffer);

    for (int i = 0; i < DS18B20_SENSOR_COUNT; i++) {
        sm_temperature_buffer[i].feed = &sm_temperature_buffer_feed[i];
        sm_temperature_buffer[i].sort = &sm_temperature_buffer_sort[i];
        sm_temperature_buffer[i].number_of_samples = NUMBER_OF_SAMPLES;
        sm_temperature_buffer[i].type = TWR_DATA_STREAM_TYPE_FLOAT;
        twr_data_stream_init(&sm_temperature[i], 1, &sm_temperature_buffer[i]);
    }

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
    int sensor_found = twr_ds18b20_get_sensor_found(&ds18b20);

    for (int i = 0; i < sensor_found; i++)
    {
        float temperature_avg = NAN;

        twr_data_stream_get_average(&sm_temperature[i], &temperature_avg);

        static char topic[64];
        snprintf(topic, sizeof(topic), "thermometer/%016" PRIx64 "/temperature", ds18b20_sensors[i]._device_address);

        if (!isnan(temperature_avg))
        {
            twr_radio_pub_float(topic, &temperature_avg);
        }
        else
        {
            twr_radio_pub_float(topic, NULL);
        }
    }

    header = HEADER_UPDATE;

    twr_scheduler_plan_current_relative(SEND_DATA_INTERVAL);
}
