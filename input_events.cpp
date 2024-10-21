#include <cassert>
#include <cstdint>
#include <inttypes.h>
#include <errno.h>
#include <signal.h> // ctrl-c handler
#include <stdio.h>
#include <gpiod.h>

// This configures two pins as inputs then print messages as they change.

static const char *chip_path = "/dev/gpiochip0";

// GPIOs that will be used as inputs
static const int a_gpio_num = 23;   // GPIO23 is 'a' input
static const int b_gpio_num = 24;   // GPIO24 is 'b' input
static const int gpio_pin_cnt = 2;  // how many pins we're using

static const int max_events = 32;   // max events to buffer

static const unsigned long debounce_us = 1000; // debounce time

static bool quitting = false;

static void ctrl_c_handler(int notused)
{
    quitting = true;
}


int main(int argc, char *argv[])
{

    // Allocate event buffer. An event buffer is a control structure with
    // pointers to two buffers: one used to read raw event data (array of
    // struct gpio_v2_line_event) from the request fd, and another used to
    // hold reformatted event data (struct gpiod_edge_event) that is user-
    // visible.
    gpiod_edge_event_buffer *events = gpiod_edge_event_buffer_new(max_events);
    assert(events != nullptr);

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
    gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);
    gpiod_line_settings_set_debounce_period_us(settings, debounce_us);
    gpiod_line_settings_set_event_clock(settings, GPIOD_LINE_CLOCK_MONOTONIC);

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

    // offsets[] and settings are no longer needed
    gpiod_line_settings_free(settings);
    settings = nullptr;

#if GPIOD_INCLUDE_SHOW
    gpiod_line_config_show(line_config);
#endif

    // Open the chip and save the fd in a newly-allocated chip structure.
    // No gpio-specific kernel calls (just the open).
    // The fd is closed and the structrue freed in gpiod_chip_close.
    gpiod_chip *chip = gpiod_chip_open(chip_path);
    assert(chip != nullptr);

#if GPIOD_INCLUDE_SHOW
    gpiod_chip_show(chip);
#endif

    // Optional: the gpiod_request_config object can be used to set the
    // "consumer" (inputs or outputs) or the event buffer size (inputs).
    gpiod_request_config *request_config = gpiod_request_config_new();
    assert(request_config != nullptr);

    // always succeeds, but will  truncate consumer name if too long
    gpiod_request_config_set_consumer(request_config, "input_events");

#if GPIOD_INCLUDE_SHOW
    gpiod_request_config_show(request_config);
#endif

    // This does an ioctl to read the "chip info", then another ioctl to
    // request and configure the lines. Finally, a gpiod_line_request object
    // is allocated, filled in, and returned. request_config can be nullptr if
    // not needed. One of the ioctls on the chip's fd returns (among other
    // things) a new fd that goes in the gpiod_line_request structure, and is
    // the one used to change the lines later.
    gpiod_line_request *request = gpiod_chip_request_lines(chip, request_config, line_config);
    assert(request != nullptr);

#if GPIOD_INCLUDE_SHOW
    gpiod_line_request_show(request);
#endif

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

    uint64_t last_ns = 0;

    // ctrl-c sets 'quitting' flag
    signal(SIGINT, ctrl_c_handler);

    while (!quitting) {

        // Wait for events. Arg 2 is int64_t timeout in nanoseconds, or zero
        // to return immediately, or negative to wait forever. Returns 1 for
        // event available, -1 for error, 0 for timeout.
        // XXX what about signal interrupt?
        int r2 = gpiod_line_request_wait_edge_events(request, -1);
        if (r2 < 0 && errno == EINTR)
            break; // ctrl-c
        assert(r2 == 1);

        // Read events. This does not append to events buffer, it starts
        // writing at the beginning each time called. Returns number of events
        // written to events buffer.
        int num_events = gpiod_line_request_read_edge_events(request, events, max_events);
        assert(num_events > 0);

        // Print all events received.
        for (unsigned i = 0; i < gpiod_edge_event_buffer_get_num_events(events); i++) {
            // this returns a pointer into events
            gpiod_edge_event *event = gpiod_edge_event_buffer_get_event(events, i);
            unsigned long global_seqno = gpiod_edge_event_get_global_seqno(event);
            unsigned long line_seqno = gpiod_edge_event_get_line_seqno(event);
            unsigned int pin_num = gpiod_edge_event_get_line_offset(event);
            unsigned int pin_val =
                gpiod_edge_event_get_event_type(event) == GPIOD_EDGE_EVENT_RISING_EDGE ? 1 : 0;
            uint64_t timestamp_ns = gpiod_edge_event_get_timestamp_ns(event);
            printf("%lu:%lu pin %u = %u @ %" PRIu64, global_seqno, line_seqno,
                    pin_num, pin_val, timestamp_ns);
            if (last_ns != 0)
                printf(" +%" PRIu64, timestamp_ns - last_ns);
            last_ns = timestamp_ns;
            printf("\n");
        }
        // Extra blank line here groups events received in the same read call.
        printf("\n");

    } // while

    gpiod_line_request_release(request);
    request = nullptr;

    gpiod_chip_close(chip);
    chip = nullptr;

    return 0;

} // main
