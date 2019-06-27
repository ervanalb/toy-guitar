#include "hal.h"
#include "midi.h"
#define BIRB_SHORTHAND
#include "birb/birb.h"

#define ROM __attribute__ ((section (".rodata")))
#include "audio/0.h"
#include "audio/1.h"
#include "audio/2.h"
#include "audio/3.h"
#include "audio/4.h"
#include "audio/5.h"
#include "audio/6.h"
#include "audio/7.h"
#include "audio/8.h"
#include "audio/9.h"

#define SAMPLE_RATE_HIGH 44100
static uint32_t audio_lengths[32];
static const unsigned char *audios[32];

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
#define AUDIOS X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9)
#define CONCAT(x, y) x ## y
#define X(N) audio_lengths[N] = sizeof(CONCAT(audio_, N)); audios[N] = CONCAT(audio_, N);
    AUDIOS
#undef X

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
                hal_set_sample_rate(44100);
                DEBOUNCE_CYCLES = 10;
                sfx = bleep;
                //counter = 0;
            }
            zero_counter++;
        } else {
            zero_counter = 0;
        }
        sfx = bloop;
        //counter = 0;
        current_program[program_counter] = opcode;
        program_counter++;
    }

    static uint32_t digit = 0;
    for (int i=0; i<BUFFER_SIZE; i++) {
        if (counter < audio_lengths[digit])
            buffer[i] = audios[digit][counter++];
        else
            buffer[i] = 0;
    }
    if (counter >= audio_lengths[digit]) {
        counter = 0;
        digit++;
        if (digit >= 10) digit = 0;
    }
    /*
    if (counter > 100000) {
      sfx = silence;
    }
    */

    last_buttons = buttons;
}

static void hal_fill_play_mode(uint8_t *buffer) {
    static uint32_t last_buttons;
    uint32_t buttons = hal_buttons();
    static int32_t open = 60;
    static int32_t mod = 60;
    static uint64_t t;
    static uint64_t u;
    static uint64_t t1;
    static uint64_t t2;
    static uint64_t t3;
    static enum {
        MODE_MEL_PITCH,
        MODE_MEL_MOD,
        MODE_HAR_PITCH,
        MODE_HAR_MOD,
        N_MODES,
    } play_mode;

    if (play_mode == MODE_MEL_PITCH || play_mode == MODE_HAR_PITCH) {
        open += hal_encoder();
    } else if (play_mode == MODE_MEL_MOD || play_mode == MODE_HAR_MOD) {
        mod += hal_encoder();
    }

    const int frets[] = {
        2,
        1,
        3,
        7,
        12,
    };

    int fretting = 0;
    for (int i=0; i<N_FRETS; i++) {
        if (buttons & (1 << (FRETS_OFFSET + i))) {
            fretting += frets[i];
        }
    }

    if (play_mode == MODE_MEL_PITCH || play_mode == MODE_MEL_MOD) {
        float note = open; // Open
        note += fretting;

        float note_freq = get_freq(note);
        float mod_freq = get_freq(mod);

        if (!(last_buttons & STRUM) && (buttons & STRUM)) {
            t = 0;
            u = 0;
        }

        for (int i=0; i<BUFFER_SIZE; i++) {
            if (buttons & STRUM) {
                t += (int32_t)(((float)(1 << 16)) * (256.f / SAMPLE_RATE_HIGH) * note_freq);
                u += (int32_t)(((float)(1 << 16)) * (256.f / SAMPLE_RATE_HIGH) * mod_freq);
                buffer[i] = birb_eval(current_program, t >> 16, u >> 16);
            } else {
                buffer[i] = 0;
            }
        }
    } else if (play_mode == MODE_HAR_PITCH || play_mode == MODE_HAR_MOD) {
        const int chords[][3] = {
            {0, 4, 7}, // C
            {1, 4, 7}, // C#dim
            {2, 5, 9}, // Dm
            {3, 7, 10}, // Eb
            {4, 7, 11}, // Em
            {5, 9, 12}, // F
            {2, 6, 9}, // D
            {7, 11, 14}, // G
            {5, 8, 12}, // Fm
            {9, 12, 16}, // Am
            {5, 10, 14}, // Bb
            {4, 8, 11}, // E
        };
        float n1;
        float n2;
        float n3;
        if (fretting < 0 || fretting >= (int)(sizeof(chords) / sizeof(*chords))) {
            n1 = open + fretting;
            n2 = open + fretting;
            n3 = open + fretting;
        } else {
            n1 = open + chords[fretting][0];
            n2 = open + chords[fretting][1];
            n3 = open + chords[fretting][2];
        }

        float f1 = get_freq(n1);
        float f2 = get_freq(n2);
        float f3 = get_freq(n3);
        float mod_freq = get_freq(mod);

        if (!(last_buttons & STRUM) && (buttons & STRUM)) {
            t1 = 0;
            t2 = 0;
            t3 = 0;
            u = 0;
        }

        for (int i=0; i<BUFFER_SIZE; i++) {
            if (buttons & STRUM) {
                t1 += (int32_t)(((float)(1 << 16)) * (256.f / SAMPLE_RATE_HIGH) * f1);
                t2 += (int32_t)(((float)(1 << 16)) * (256.f / SAMPLE_RATE_HIGH) * f2);
                t3 += (int32_t)(((float)(1 << 16)) * (256.f / SAMPLE_RATE_HIGH) * f3);
                u += (int32_t)(((float)(1 << 16)) * (256.f / SAMPLE_RATE_HIGH) * mod_freq);
                uint8_t s1 = birb_eval(current_program, t1 >> 16, u >> 16);
                uint8_t s2 = birb_eval(current_program, t2 >> 16, u >> 16);
                uint8_t s3 = birb_eval(current_program, t3 >> 16, u >> 16);
                buffer[i] = 0.33f * (s1 + s2 + s3);
            } else {
                buffer[i] = 0;
            }
        }
    }

    if (!(last_buttons & WHAMMY) && (buttons & WHAMMY)) {
        play_mode++;
        if (play_mode >= N_MODES) play_mode = 0;
    }
    last_buttons = buttons;
}

void hal_fill(uint8_t *buffer) {
    switch (mode) {
        case PROGRAMMING:
            hal_set_sample_rate(8000);
            hal_fill_programming_mode(buffer);
            break;
        case PLAY:
            hal_set_sample_rate(SAMPLE_RATE_HIGH);
            hal_fill_play_mode(buffer);
            break;
    }
}

