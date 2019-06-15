#include "hal.h"
#include "midi.h"

int main(void) {
    hal_init();
    for(;;);
}

static uint32_t compute_sample(uint32_t t) {
    return t;
}

static float get_freq(float note) {
    static float last_note = 0;
    static float last_frequency;

    int max_note = sizeof(frequency_lookup) / sizeof(*frequency_lookup) - 1;
    if (note > max_note) note = max_note;
    if (note < 0) note = 0;
    if (note == last_note) return last_frequency;
    int note_integer_part = (int)note;
    float note_fractional_part = note - note_integer_part;

    float f1 = frequency_lookup[note_integer_part];
    float f2 = frequency_lookup[note_integer_part + 1];
    float f = f1 * (1 - note_fractional_part) + f2 * note_fractional_part; // Basic linear interpolation

    last_frequency = f;
    return f;
}

void hal_fill(uint8_t *buffer) {
    static float counter;
    static uint32_t last_buttons;
    uint32_t buttons = hal_buttons();

    float note = 60; // Open

    const float frets[] = {
        2,
        1,
        3,
        7,
        12,
    };
    for (int i=0; i<5; i++) {
        if (buttons & (2 << i)) {
            note += frets[i];
        }
    }

    float freq = get_freq(note);

    if (!(last_buttons & 1) && (buttons & 1)) counter = 0;

    for (int i=0; i<BUFFER_SIZE; i++) {
        if (buttons & 1) {
            uint32_t t = counter * (256.f / SAMPLE_RATE) * freq ;
            buffer[i] = compute_sample(t);
        } else {
            buffer[i] = 0;
        }
        counter++;
    }
    last_buttons = buttons;
}

