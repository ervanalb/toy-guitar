#pragma once
#include <stdint.h>

#define BUFFER_SIZE 64

#define STRUM 1
#define WHAMMY (1 << 6)
#define FRETS_OFFSET 1
#define N_FRETS 5

extern int DEBOUNCE_CYCLES;

void hal_init(void);
void hal_set_sample_rate(int sample_rate);
void hal_fill(uint8_t *buffer);
uint32_t hal_buttons(void);
int hal_encoder(void);
