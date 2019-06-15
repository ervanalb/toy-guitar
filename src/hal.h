#pragma once
#include <stdint.h>

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 64

void hal_init(void);
void hal_fill(uint8_t *buffer);
uint32_t hal_buttons(void);
