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
#define TRIGGER_DELAY 4

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
static uint pio_program_offset;
static int dma_repeat_target = 0;
static int dma_repeat_count = 0;
PIO trigger_pio = pio0;
PIO wave_pio = pio0;

void start_clock(uint OUT_PIN_NUMBER, uint NPINS){ 

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

    PIO pio = trigger_pio;
    if (!trigger_program_loaded) {
        trigger_program_offset = pio_add_program(pio, &trigger_program);
        trigger_program_loaded = true;
    }

    if (trigger_sm == -1) {
        trigger_sm = pio_claim_unused_sm(pio, true);
    } else {
        pio_sm_set_enabled(pio, trigger_sm, false);
    }

    pio_interrupt_clear(trigger_pio, 1u << 0);
    trigger_program_init(pio, trigger_sm, trigger_program_offset, OUT_PIN_NUMBER, NPINS, SM_CLK_FREQ);
    
}

void dma_handler() {
    dma_channel_acknowledge_irq0(dma_chan);
    dma_repeat_count++;
    if (dma_repeat_count >= dma_repeat_target) {
        pio_sm_set_enabled(pio0, pio_sm, false);
        pio_sm_restart     (pio0, pio_sm);
        pio_sm_clear_fifos (pio0, pio_sm);
        dma_channel_abort(dma_chan);
        // printf("DMA complete after %d repeats.\n", dma_repeat_count);
    } else {
        dma_channel_set_read_addr(dma_chan, awg_buff, true);
    }

}

void dma_flash_repeats(int repeats, uint OUT_PIN_NUMBER, uint NPINS, int trigger_delay) {
    // must call stdio_init_all() before using this
    // add in the trigger 

    // const int total_samples = repeats;
    const int dma_transfers = repeats;

    memset(awg_buff, 0, sizeof(awg_buff));

    for (int i = 0; i < dma_transfers; ++i) {
        awg_buff[i] = 0x0000FFFF;
        printf("awg_buff[%d] = 0x%08X\n", i, awg_buff[i]);
    }

    PIO pio = wave_pio;
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
    // channel_config_set_dreq(&cfg, DREQ_PIO0_TX0 + pio_sm); // used for when multiple pio blocks are used
    channel_config_set_dreq(&cfg, DREQ_PIO0_TX0);
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
    
    pio_sm_set_enabled(wave_pio, pio_sm,  true);
    pio_sm_put_blocking(trigger_pio, trigger_sm, trigger_delay);
    pio_sm_set_enabled(trigger_pio, trigger_sm, true);

    dma_channel_start(dma_chan);
    
    
    printf("Started waveform: %d cycles on GPIO %d (%d-bit output)\n",
        repeats, OUT_PIN_NUMBER, NPINS);
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

    PIO pio = wave_pio;
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
    // channel_config_set_dreq(&cfg, DREQ_PIO0_TX0 + pio_sm); // use for multiple pios
    channel_config_set_dreq(&cfg, DREQ_PIO0_TX0); 
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

    pio_sm_set_enabled(wave_pio, pio_sm,  true);
    pio_sm_put_blocking(trigger_pio, trigger_sm, trigger_delay);
    pio_sm_set_enabled(trigger_pio, trigger_sm, true);

    dma_channel_start(dma_chan);
    printf("Started waveform: %d cycles x %d samples/cycle on GPIO %d (%d-bit output)\n",
        repeats, total_samples, OUT_PIN_NUMBER, NPINS);
}

int readline(char *buf, int maxlen) {
    int i = 0;
    while (i < maxlen - 1) {
        int c = getchar_timeout_us(0); 
        if (c == '\r' || c == '\n') {
            putchar('\n'); 
            break;
        } else if (c >= 32 && c <= 126) {  
            buf[i++] = c;
            putchar(c);
        }
    }
    buf[i] = '\0';
    return i;
}


int main() {
    stdio_init_all();
    start_clock(16, 1);
    start_enable(20, 1);

    char inputBuffer[32];
    bool waitingForCommand = true;
    bool invalidShown = false;

    while (true) {
        if (waitingForCommand) {
            printf("Command (1 = flash, 0 = step): \n");
            waitingForCommand = false;
            invalidShown = false;
        }

        int len = readline(inputBuffer, sizeof(inputBuffer));
        if (len == 0 && !invalidShown) {
            printf("Invalid input, enter 0 for step and 1 for flash\n");
            invalidShown = true;
            waitingForCommand = true;
            continue;
        }
        if (strcmp(inputBuffer, "1") == 0) {
            int repeats;
            printf("Enter number of repeats (empty is default as 1): ");
            int len2 = readline(inputBuffer, sizeof(inputBuffer));
            if (len2 > 0 && sscanf(inputBuffer, "%d", &repeats) != 1) {
                repeats = 1;
            }
            dma_flash_repeats(repeats, 0, 16, TRIGGER_DELAY);
            waitingForCommand = true;
        }

        else if (strcmp(inputBuffer, "0") == 0) {
            int repeats;
            printf("Enter number of repeats (empty is default as 1): ");
            int len2 = readline(inputBuffer, sizeof(inputBuffer));
            if (len2 > 0 && sscanf(inputBuffer, "%d", &repeats) != 1) {
                repeats = 1;
            }
            dma_step_repeats(repeats, 0, 16, TRIGGER_DELAY);
            waitingForCommand = true;
        }

        else if (len == 0 && !invalidShown) {
            printf("Invalid input, enter 0 for step and 1 for flash\n");
            invalidShown = true;
            waitingForCommand = true;
        }
    }
}


    