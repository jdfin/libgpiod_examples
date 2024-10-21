#include <cassert>
#include <signal.h> // ctrl-c handler
#include <unistd.h> // sleep()
#include <gpiod.h>

extern "C" {
void gpiod_line_config_show(gpiod_line_config*);
void gpiod_chip_show(gpiod_chip*);
void gpiod_request_config_show(gpiod_request_config*);
void gpiod_line_request_show(gpiod_line_request*);
};

// This will configure one pin as output then toggle it repeatedly.

static const char *chip_path = "/dev/gpiochip0";

// GPIO that will be used as output
// gpiod calls expect an unsigned int
static const unsigned int gpio_num = 23;

static bool quitting = false;

static void ctrl_c_handler(int notused)
{
    quitting = true;
}


int main(int argc, char *argv[])
{

    // Allocate a new gpiod_line_config structure and initialize it.
    // All userspace, in fact just a malloc and memset.
    gpiod_line_config *line_config = gpiod_line_config_new();
    assert(line_config != nullptr);

    // Allocate a new gpiod_line_settings structure and initialize it with
    // defaults. All userspace (no kernel calls).
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
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_drive(settings, GPIOD_LINE_DRIVE_PUSH_PULL);

    // Attach the settings structure to the line we'll be using.
    // This copies the supplied settings structure so it is not needed
    // after this call.
    // Allocations done inside this call are freed in gpiod_line_config_free.
    int r1 = gpiod_line_config_add_line_settings(line_config, &gpio_num, 1, settings);
    assert(r1 == 0);

    // settings are no longer needed
    gpiod_line_settings_free(settings);
    settings = nullptr;

    // Initial value for GPIO.
    const gpiod_line_value init_value = GPIOD_LINE_VALUE_INACTIVE;
    
    // Copy output value from init_value to the line_config structure.
    // All userspace, and no allocations are done here.
    int r2 = gpiod_line_config_set_output_values(line_config, &init_value, 1);
    assert(r2 == 0);

    //gpiod_line_config_show(line_config);

    // init_value is no longer needed

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
    gpiod_request_config_set_consumer(request_config, "output1_simple");

    //gpiod_request_config_show(request_config);

    // This does an ioctl to read the "chip info", then another ioctl to
    // request and configure the line. Finally, a gpiod_line_request object
    // is allocated, filled in, and returned. request_config can be nullptr if
    // not needed. One of the ioctls on the chip's fd returns (among other
    // things) a new fd that goes in the gpiod_line_request structure, and is
    // the one used to change the line later.
    gpiod_line_request *request = gpiod_chip_request_lines(chip, request_config, line_config);
    assert(request != nullptr);

    //gpiod_line_request_show(request);

    // Optional request config object no longer needed
    gpiod_request_config_free(request_config);
    request_config = nullptr;

    // Line config object no longer needed.
    gpiod_line_config_free(line_config);
    line_config = nullptr;

    // The gpiod example programs close the chip at this point,
    // leaving only the request object.
    gpiod_chip_close(chip);
    chip = nullptr;

    // map [0, 1] to [GPIOD_LINE_VALUE_INACTIVE, GPIOD_LINE_VALUE_ACTIVE]
    gpiod_line_value code_values[2] = {
        GPIOD_LINE_VALUE_INACTIVE, GPIOD_LINE_VALUE_ACTIVE
    };
    int code = 0; // 0, 1

    // ctrl-c sets 'quitting'
    signal(SIGINT, ctrl_c_handler);

    while (!quitting) {

        sleep(1);

        // This does an ioctl using the request object's fd to set
        // the new values
        gpiod_line_request_set_value(request, gpio_num, code_values[code]);

        code = 1 - code;

    } // while

    // set output low
    gpiod_line_request_set_value(request, gpio_num, code_values[0]);

    // inputs (with no pull) would be more polite

    gpiod_line_request_release(request);
    request = nullptr;

    return 0;

} // main
