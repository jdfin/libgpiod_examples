#include <cassert>
#include <cstdint>
#include <errno.h>
#include <signal.h> // ctrl-c handler
#include <stdio.h>
#include <gpiod.h>

extern "C" {
void gpiod_line_config_show(gpiod_line_config*);
void gpiod_chip_show(gpiod_chip*);
void gpiod_request_config_show(gpiod_request_config*);
void gpiod_line_request_show(gpiod_line_request*);
};

// This configures two pins as inputs then polls them to see when they change.

static const char *chip_path = "/dev/gpiochip0";

// GPIOs that will be used as inputs
static const int a_gpio_num = 23;   // GPIO23 is 'a' input
static const int b_gpio_num = 24;   // GPIO24 is 'b' input
static const int gpio_pin_cnt = 2;  // how many pins we're using

static const unsigned long debounce_us = 1000; // debounce time

static bool quitting = false;

static void ctrl_c_handler(int notused)
{
    quitting = true;
}


int main(int argc, char *argv[])
{

    // Allocate a new struct gpiod_line_settings and initialize it with
    // defaults. All userspace (no kernel calls). If lines need to be
    // different (e.g. different debounce time) then there needs to be more
    // than one of these.
    gpiod_line_settings *settings = gpiod_line_settings_new();
    assert(settings != nullptr);

    // Settings are:
    //   direction          input or output
    //   edge_detection     for inputs
    //   bias               for inputs
    //   drive              for outputs
    //   active_low         for inputs or outputs
    //   debounce_period_us for inputs
    //   event_clock        for inputs
    //   output_value       for inputs or outputs
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_NONE);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);
    gpiod_line_settings_set_debounce_period_us(settings, debounce_us);

    // Allocate a new gpiod_line_config structure and initialize it.
    // All userspace, in fact just a malloc and memset.
    gpiod_line_config *line_config = gpiod_line_config_new();
    assert(line_config != nullptr);

    const unsigned int offsets[gpio_pin_cnt] = {
        a_gpio_num,
        b_gpio_num
    };

    // Attach the settings structure to each line we'll be using.
    // This copies the supplied settings structure so it is not needed
    // after this call.
    // Allocations done inside this call are freed in gpiod_line_config_free.
    int r1 = gpiod_line_config_add_line_settings(line_config, offsets, gpio_pin_cnt, settings);
    assert(r1 == 0);

    // offsets[] is used below when printing changes

    // settings is no longer needed
    gpiod_line_settings_free(settings);
    settings = nullptr;

    //gpiod_line_config_show(line_config);

    // Open the chip and save the fd in a newly-allocated chip structure.
    // No gpio-specific kernel calls (just the open).
    // The fd is closed and the structrue freed in gpiod_chip_close.
    gpiod_chip *chip = gpiod_chip_open(chip_path);
    assert(chip != nullptr);

    //gpiod_chip_show(chip);

    // Optional: the gpiod_request_config object can be used to set the
    // "consumer" (inputs or outputs) or the event buffer size (inputs).
    gpiod_request_config *request_config = gpiod_request_config_new();
    assert(request_config != nullptr);

    // always succeeds, but will  truncate consumer name if too long
    gpiod_request_config_set_consumer(request_config, "input_events");

    //gpiod_request_config_show(request_config);

    // This does an ioctl to read the "chip info", then another ioctl to
    // request and configure the lines. Finally, a gpiod_line_request object
    // is allocated, filled in, and returned. request_config can be nullptr if
    // not needed. One of the ioctls on the chip's fd returns (among other
    // things) a new fd that goes in the gpiod_line_request structure, and is
    // the one used to change the lines later.
    gpiod_line_request *request = gpiod_chip_request_lines(chip, request_config, line_config);
    assert(request != nullptr);

    //gpiod_line_request_show(request);

    // Optional request config object no longer needed
    gpiod_request_config_free(request_config);
    request_config = nullptr;

    // Line config object no longer needed.
    gpiod_line_config_free(line_config);
    line_config = nullptr;

    // It might be okay to close the chip at this point (which closes its fd),
    // since the request object contains a different fd that is used to change
    // the outputs. I can't find that as documented though, so close the chip
    // later.
    //gpiod_chip_close(chip);
    //chip = nullptr;

    printf("debounce time = %lu usec\n", debounce_us); // reminder

    // ctrl-c sets 'quitting' flag
    signal(SIGINT, ctrl_c_handler);

    gpiod_line_value values_old[2];
    gpiod_line_value values_new[2];

    int r2 = gpiod_line_request_get_values(request, values_old);
    assert(r2 == 0);

    while (!quitting) {

        int r3 = gpiod_line_request_get_values(request, values_new);
        assert(r3 == 0);

        // print changes
        for (unsigned i = 0; i < 2; i++) {

            if (values_old[i] != values_new[i]) {
                printf("pin %u = %d\n", offsets[i], values_new[i] == GPIOD_LINE_VALUE_ACTIVE ? 1 : 0);
                values_old[i] = values_new[i];
            }
        }

        usleep(1000);

    } // while

    gpiod_line_request_release(request);
    request = nullptr;

    gpiod_chip_close(chip);
    chip = nullptr;

    return 0;

} // main
