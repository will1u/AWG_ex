// add parameter after x clock cycles (after signal pattern) to send out the trigger signal
// integrate clock and trigger pio together

// oh wait this is super easy ill just add it onto the waveform

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "AWG_1.pio.h"
#include "clock.pio.h"
#include "trigger.pio.h"
#include "enable.pio.h"
#include <string.h>

#define PI 3.1428592654
#define SM_CLK_FREQ 10000000
#define TRIGGER_DELAY 6

static uint32_t awg_buff[256] __attribute__((aligned(256)));
static int dma_chan = -1;
static int pio_sm = -1;
static int clock_sm = -1;
static int trigger_sm = -1;
static int enable_sm = -1;
static bool pio_program_loaded = false;
static bool clock_program_loaded = false;
static bool trigger_program_loaded = false;
static bool enable_program_loaded = false;
static int dma_repeat_target = 0;
static int dma_repeat_count = 0;


void start_clock(uint OUT_PIN_NUMBER, uint NPINS){ // good

    static uint pio_program_offset;

    PIO pio = pio0;
    if (!clock_program_loaded) {
        pio_program_offset = pio_add_program(pio, &clock_program);
        clock_program_loaded = true;
    }

    if (clock_sm == -1) {
        clock_sm = pio_claim_unused_sm(pio, true);
    } else {
        pio_sm_set_enabled(pio, clock_sm, false);
    }

    clock_program_init(pio, clock_sm, pio_program_offset, OUT_PIN_NUMBER, NPINS, SM_CLK_FREQ);

}

void start_enable(uint OUT_PIN_NUMBER, uint NPINS){ // good

    static uint pio_program_offset;

    PIO pio = pio0;
    if (!enable_program_loaded) {
        pio_program_offset = pio_add_program(pio, &enable_program);
        enable_program_loaded = true;
    }

    if (enable_sm == -1) {
        enable_sm = pio_claim_unused_sm(pio, true);
    } else {
        pio_sm_set_enabled(pio, enable_sm, false);
    }

    enable_program_init(pio, enable_sm, pio_program_offset, OUT_PIN_NUMBER, NPINS, SM_CLK_FREQ);

}


void ready_trigger(uint OUT_PIN_NUMBER, uint NPINS) {

    static uint trigger_program_offset;

    PIO pio = pio1;
    if (!trigger_program_loaded) {
        trigger_program_offset = pio_add_program(pio, &trigger_program);
        trigger_program_loaded = true;
    }

    if (trigger_sm == -1) {
        trigger_sm = pio_claim_unused_sm(pio, true);
    } else {
        pio_sm_set_enabled(pio, trigger_sm, false);
    }

    trigger_program_init(pio, trigger_sm, trigger_program_offset, OUT_PIN_NUMBER, NPINS, SM_CLK_FREQ);
    
}

void dma_handler() {
    dma_channel_acknowledge_irq0(dma_chan);
    dma_repeat_count++;
    if (dma_repeat_count >= dma_repeat_target) {
        dma_channel_abort(dma_chan);
        // printf("DMA complete after %d repeats.\n", dma_repeat_count);
    } else {
        dma_channel_set_read_addr(dma_chan, awg_buff, true);
    }

}

void dma_flash_repeats(int repeats, uint OUT_PIN_NUMBER, uint NPINS, uint period_length, int trigger_delay) {
    // must call stdio_init_all() before using this
    // add in the trigger 

    const int total_samples = repeats * period_length / 2;
    const int dma_transfers = total_samples/2;

    memset(awg_buff, 0, sizeof(awg_buff));

    for (int i = 0; i < dma_transfers; ++i) {
        awg_buff[i] = 0x0000FFFF;
        printf("awg_buff[%d] = 0x%08X\n", i, awg_buff[i]);
    }

    static uint pio_program_offset;

    PIO pio = pio0;
    if (!pio_program_loaded) {
        pio_program_offset = pio_add_program(pio, &pio_byte_out_program);
        pio_program_loaded = true;
    }

    if (pio_sm == -1) {
        pio_sm = pio_claim_unused_sm(pio, true);
    } else {
        pio_sm_set_enabled(pio, pio_sm, false);
    }
    pio_byte_out_program_init(pio, pio_sm, pio_program_offset, OUT_PIN_NUMBER, NPINS, SM_CLK_FREQ);
    ready_trigger(17, 1);
     

    // pio_interrupt_clear(pio1, 1u << 0);
    pio_sm_clear_fifos(pio1, trigger_sm);
    pio_sm_put_blocking(pio1, trigger_sm, trigger_delay);
    pio_sm_set_enabled(pio1, trigger_sm, true);
    // pio_interrupt_set(pio1, 1u << 0 );
    pio_sm_set_enabled(pio0, pio_sm,  true);
    
    

    if (dma_chan == -1) {
        dma_chan = dma_claim_unused_channel(true);
    } else {
        dma_channel_abort(dma_chan);
    }

    dma_repeat_target = 1;
    dma_repeat_count = 0;

    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, DREQ_PIO0_TX0 + pio_sm);
    channel_config_set_ring(&cfg, false, 0);

    dma_channel_configure(
        dma_chan,
        &cfg,
        &pio0_hw->txf[pio_sm],
        awg_buff,
        dma_transfers,
        false
    );

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    
    dma_channel_start(dma_chan);
    printf("Started waveform: %d cycles x %d samples/cycle on GPIO %d (%d-bit output)\n",
        repeats, period_length, OUT_PIN_NUMBER, NPINS);
}

void dma_step_repeats(int repeats, uint OUT_PIN_NUMBER, uint NPINS, int trigger_delay) {
    // must call stdio_init_all() before using this
    // add in trigger waveform

    if (repeats <= 0 ) {
        printf("Invalid input. period_length must be >0.\n");
        return;
    }

    const int total_samples = repeats * 8;
    if (total_samples % 2 != 0) {
        printf("Error: total_samples must be even (2 x 16-bit samples per DMA word).\n");
        return;
    }

    uint32_t value_array[8];
    uint32_t value = 0x00020001;

    for (int i = 0 ; i < 8 ; i++) {
        value_array[i] = value;
        value = value * 4;
    }

    memset(awg_buff, 0, sizeof(awg_buff));
    for (int i = 0; i < total_samples; ++i) {
        awg_buff[i] = value_array[i%8];
        printf("awg_buff[%d] = 0x%08X\n", i, awg_buff[i]);
    }

    static uint pio_program_offset;

    PIO pio = pio0;
    if (!pio_program_loaded) {
        pio_program_offset = pio_add_program(pio, &pio_byte_out_program);
        pio_program_loaded = true;
    }
    

    if (pio_sm == -1) {
        pio_sm = pio_claim_unused_sm(pio, true);
    } else {
        pio_sm_set_enabled(pio, pio_sm, false);
    }
    pio_byte_out_program_init(pio, pio_sm, pio_program_offset, OUT_PIN_NUMBER, NPINS, SM_CLK_FREQ);
    ready_trigger(17, 1);

    pio_interrupt_clear(pio0, 1u << 0);
    pio_sm_put_blocking(pio1, trigger_sm, trigger_delay);
    
    pio_sm_set_enabled(pio1, trigger_sm, true); 
    pio_sm_set_enabled(pio0, pio_sm,  true);

    if (dma_chan == -1) {
        dma_chan = dma_claim_unused_channel(true);
    } else {
        dma_channel_abort(dma_chan);
    }

    dma_repeat_target = 1;
    dma_repeat_count = 0;

    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, DREQ_PIO0_TX0 + pio_sm);
    channel_config_set_ring(&cfg, false, 0);

    dma_channel_configure(
        dma_chan,
        &cfg,
        &pio0_hw->txf[pio_sm],
        awg_buff,
        total_samples,
        false
    );

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_start(dma_chan);
    printf("Started waveform: %d cycles x %d samples/cycle on GPIO %d (%d-bit output)\n",
        repeats, total_samples, OUT_PIN_NUMBER, NPINS);
}


int main() {
    stdio_init_all();  // set up to print out
    start_clock(16, 1);
    start_enable(20, 1);

	char userInput = '\0';
    bool invalidInputDisplayed = false;
    bool waitingForCommand = true;

    while (true) {
        if (waitingForCommand) {
            printf("Command (1 = flash, 0 = step): \n");
            waitingForCommand = false; 
        }
        
        userInput = getchar_timeout_us(10000);
        
        if (userInput == '1') {
            int repeats = 1;
            printf("Enter number of repeats (empty is default as 1): ");
            scanf("%d", &repeats);

            dma_flash_repeats(repeats, 0,16, 4, TRIGGER_DELAY);
            waitingForCommand = true;
        } 

        else if (userInput == '0') {
            int repeats = 1;
            printf("Enter number of repeats (empty is default as 1): ");
            scanf("%d", &repeats);

            dma_step_repeats(repeats, 0,16, TRIGGER_DELAY);
            waitingForCommand = true;


        }
    }
}

    