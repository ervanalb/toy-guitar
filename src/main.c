#include "hal.h"
#include "midi.h"
#define BIRB_SHORTHAND
#include "birb/birb.h"

// (t*5&t>>7)|(t*3&t>>10)
static uint8_t current_program[64] = {
    T, 5, MUL,
    T, 7, SHR,
        AND,
    T, 3, MUL,
    T, A, SHR,
        AND,
            OR,
    END,
};

static enum {PROGRAMMING, PLAY} mode;

int main(void) {
    hal_init();
    DEBOUNCE_CYCLES = 100;
    for(;;);
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

static uint8_t silence(uint32_t t) {
    (void)t;
    return 0;
}

static uint8_t bleep(uint32_t t) {
    return t * (t < 1000);
}

static uint8_t bloop(uint32_t t) {
    return 2 * t * (t < 1000);
}

static void hal_fill_programming_mode(uint8_t *buffer) {
    uint32_t buttons = hal_buttons();
    static int program_counter;
    static uint32_t last_buttons;
    static uint8_t (*sfx)(uint32_t t) = silence;
    static uint32_t counter;
    static int zero_counter;

    if (!(last_buttons & STRUM) && (buttons & STRUM)) {
        uint8_t opcode = (buttons >> FRETS_OFFSET) & ((1 << N_FRETS) - 1);
        if (opcode == 0) {
            if (zero_counter == 2) {
                program_counter -= zero_counter;
                opcode = END;
                mode = PLAY;
                DEBOUNCE_CYCLES = 10;
                sfx = bleep;
                counter = 0;
            }
            zero_counter++;
        } else {
            zero_counter = 0;
        }
        sfx = bloop;
        counter = 0;
        current_program[program_counter] = opcode;
        program_counter++;
    }

    for (int i=0; i<BUFFER_SIZE; i++) {
        buffer[i] = sfx(counter++);
    }
    if (counter > 100000) {
      sfx = silence;
    }

    last_buttons = buttons;
}

static void hal_fill_play_mode(uint8_t *buffer) {
    static uint32_t counter;
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
    for (int i=0; i<N_FRETS; i++) {
        if (buttons & (1 << (FRETS_OFFSET + i))) {
            note += frets[i];
        }
    }

    float freq = get_freq(note);

    if (!(last_buttons & STRUM) && (buttons & STRUM)) counter = 0;

    for (int i=0; i<BUFFER_SIZE; i++) {
        if (buttons & STRUM) {
            uint32_t t = counter * (256.f / SAMPLE_RATE) * freq ;
            buffer[i] = birb_eval(current_program, t, counter);
        } else {
            buffer[i] = 0;
        }
        counter++;
    }
    last_buttons = buttons;
}

void hal_fill(uint8_t *buffer) {
    switch (mode) {
        case PROGRAMMING:
            hal_fill_programming_mode(buffer);
            break;
        case PLAY:
            hal_fill_play_mode(buffer);
            break;
    }
}

