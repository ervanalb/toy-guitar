#include "hal.h"
#include <libopencm3/stm32/dac.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/l4/nvic.h>

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 64

static uint16_t samples[BUFFER_SIZE * 2];

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
    const uint32_t sysclk_freq = 80000000;

    // Enable peripheral clocks
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_DAC1);
	rcc_periph_clock_enable(RCC_TIM2);
	rcc_periph_clock_enable(RCC_DMA1);

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

static void hal_fill(uint16_t *buffer) {
    static uint32_t counter = 0;
    const int n = 10;
    for (int i=0; i<BUFFER_SIZE; i++) {
        buffer[i] = (counter % n < n / 2) ? (1 << 16) - 1 : 0;
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
