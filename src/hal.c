#include "hal.h"
#include <libopencm3/stm32/dac.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/l4/nvic.h>
#include <math.h>

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 64

static uint16_t samples[BUFFER_SIZE * 2];

const struct {
    uint32_t gpioport;
    uint16_t gpio;
} button_gpios[] = {
    {GPIOB, GPIO6}, // Strum
    {GPIOA, GPIO8}, // 1
    {GPIOB, GPIO1}, // 2
    {GPIOA, GPIO11}, // 3
    {GPIOB, GPIO4}, // 4
    {GPIOB, GPIO5}, // 5
};

void hal_init(void) {
	rcc_osc_on(RCC_HSI16);
	
	flash_prefetch_enable();
	flash_set_ws(4);
	flash_dcache_enable();
	flash_icache_enable();
	/* 16MHz / 4 = > 4 * 40 = 160MHz VCO => 80MHz main pll  */
	rcc_set_main_pll(RCC_PLLCFGR_PLLSRC_HSI16, 4, 40,
			0, 0, RCC_PLLCFGR_PLLR_DIV2);
	rcc_osc_on(RCC_PLL);
	/* either rcc_wait_for_osc_ready() or do other things */
    rcc_wait_for_osc_ready(RCC_PLL);
    rcc_set_sysclk_source(RCC_CFGR_SW_PLL);
    const uint32_t sysclk_freq = 80000000;

    // Enable peripheral clocks
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_DAC1);
	rcc_periph_clock_enable(RCC_TIM2);
	rcc_periph_clock_enable(RCC_DMA1);

    // Set up GPIO for buttons
    for (int i=0; i<sizeof(button_gpios) / sizeof(*button_gpios); i++) {
        gpio_mode_setup(button_gpios[i].gpioport, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, button_gpios[i].gpio);
    }

    // Set up DMA for audio output
    nvic_set_priority(NVIC_DMA1_CHANNEL3_IRQ, 0);
	nvic_enable_irq(NVIC_DMA1_CHANNEL3_IRQ);

	dma_channel_reset(DMA1, DMA_CHANNEL3);
	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL3);
	dma_enable_half_transfer_interrupt(DMA1, DMA_CHANNEL3);
	dma_set_memory_size(DMA1, DMA_CHANNEL3, DMA_CCR_MSIZE_16BIT);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL3, DMA_CCR_PSIZE_16BIT);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL3);
	dma_enable_circular_mode(DMA1, DMA_CHANNEL3);
    dma_set_read_from_memory(DMA1, DMA_CHANNEL3);
	dma_set_peripheral_address(DMA1, DMA_CHANNEL3, (uint32_t) &DAC_DHR12L1);
	dma_set_memory_address(DMA1, DMA_CHANNEL3, (uint32_t) samples);
	dma_set_number_of_data(DMA1, DMA_CHANNEL3, BUFFER_SIZE * 2);
    DMA1_CSELR |= 0x6 << 8; // Set CSELR to 0b0110 for channel 3
	dma_enable_channel(DMA1, DMA_CHANNEL3);

    // Set up TIM2 to trigger DAC
	timer_set_period(TIM2, sysclk_freq / SAMPLE_RATE);
	timer_set_master_mode(TIM2, TIM_CR2_MMS_UPDATE); // Connect TIM2 to the DAC
	timer_enable_counter(TIM2);

    // Set up DAC
	gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO4);
    dac_dma_enable(CHANNEL_1);
    dac_trigger_enable(CHANNEL_1);
	dac_set_trigger_source(DAC_CR_TSEL1_T2); // Trigger on timer 2
	dac_enable(CHANNEL_1);
}

uint32_t hal_buttons() {
    uint32_t buttons = 0;
    for (int i=0; i<sizeof(button_gpios) / sizeof(*button_gpios); i++) {
        buttons |= (!gpio_get(button_gpios[i].gpioport, button_gpios[i].gpio)) << i;
    }
    return buttons;
}

static void hal_fill(uint16_t *buffer) {
    static float counter = 0;
    const float freqs[] = {
        16.f / 15.f,
        9.f / 8.f,
        5.f / 4.f,
        3.f / 2.f,
        2.f,
    };

    float n = 100;
    uint32_t buttons = hal_buttons();
    for (int i=0; i<5; i++) {
        if (buttons & (2 << i)) {
            n /= freqs[i];
        }
    }
    for (int i=0; i<BUFFER_SIZE; i++) {
        if (buttons & 1) {
            buffer[i] = (counter < n / 2) ? (1 << 16) - 1 : 0;
            if (counter >= n) counter -= n;
        } else {
            buffer[i] = 0;
        }
        counter++;
    }
}

void dma1_channel3_isr() {
	if (DMA1_ISR & DMA_ISR_TCIF3) {
		DMA1_IFCR |= DMA_IFCR_CTCIF3;
        hal_fill(&samples[BUFFER_SIZE]);
	}
	else if (DMA1_ISR & DMA_ISR_HTIF3) {
		DMA1_IFCR |= DMA_IFCR_CHTIF3;
        hal_fill(&samples[0]);
    }
}
