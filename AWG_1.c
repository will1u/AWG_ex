#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "AWG_1.pio.h"
#include <string.h>

// Our assembled program:
#include "AWG_1.pio.h"

#define PI 3.1428592654
#define SM_CLK_FREQ 1000000


typedef struct {
    int repeats;
    int count;
    int chan_a;
    int chan_b;
} dma_pingpong_state_t;

dma_pingpong_state_t dma_state;

// void dma_handler() {
//     // Check which channel triggered the interrupt
//     if (dma_channel_get_irq0_status(dma_state.chan_a)) {
//         dma_channel_acknowledge_irq0(dma_state.chan_a);
//     }

//     if (dma_channel_get_irq0_status(dma_state.chan_b)) {
//         dma_channel_acknowledge_irq0(dma_state.chan_b);
//     }

//     dma_state.count++;
    
//     if (dma_state.count >= dma_state.repeats) {
//         dma_channel_abort(dma_state.chan_a);
//         dma_channel_abort(dma_state.chan_b);
//         printf("Done ping-ponging %d times.\n", dma_state.repeats);
//     }
// }

// void dma_flash(int repeats, uint OUT_PIN_NUMBER, uint NPINS, uint bufdepth) {
//     // must run stdio_init_all() before calling function
// 	float factor;
	
// 	// Choose which PIO instance to use (there are two instances)
//     PIO pio = pio0;
// 	uint sm = pio_claim_unused_sm(pio, true);
// 	uint offset = pio_add_program(pio, &pio_byte_out_program);
//     pio_byte_out_program_init(pio, sm, offset, OUT_PIN_NUMBER, NPINS, SM_CLK_FREQ);

// 	// wave_dma_chan_a and wave_dma_chan_b loads AWG buffer table to PIO in ping pong method
// 	int wave_dma_chan_a = dma_claim_unused_channel(true);
//     int wave_dma_chan_b = dma_claim_unused_channel(true);

//     dma_state.chan_a = wave_dma_chan_a;
//     dma_state.chan_b = wave_dma_chan_b;
// 	dma_state.repeats = repeats;
//     dma_state.count = 0;

// 	// define the waveform buffer to hold the waveform
//     uint8_t awg_buff[256] __attribute__((aligned(256))); 

//     int pulse_width = 2;  // Number of samples the pulse stays high
//     int period = 4;       // Total samples in one period
//     for (int i = 0; i < bufdepth; ++i) {
//         if ((i % period) < pulse_width)
//             awg_buff[i] = 0xFF; 
//         else
//             awg_buff[i] = 0x00; 
//     }
		
// 	//set up the wave_dma_chan_a DMA channel
//     dma_channel_config wave_dma_chan_a_config = dma_channel_get_default_config(wave_dma_chan_a);
//     // Transfers 32-bits at a time, increment read address so we pick up a new table value each
//     // time, don't increment write address so we always transfer to the same PIO register.
//     channel_config_set_transfer_data_size(&wave_dma_chan_a_config, DMA_SIZE_32); //Transfer 32 bits at a time (4 bytes)
//     channel_config_set_read_increment(&wave_dma_chan_a_config, true);
//     channel_config_set_write_increment(&wave_dma_chan_a_config, false);
// 	channel_config_set_chain_to(&wave_dma_chan_a_config, wave_dma_chan_b); //after block has been transferred, wave_dma_chan b
//     channel_config_set_dreq(&wave_dma_chan_a_config, DREQ_PIO0_TX0);// Transfer when PIO asks for a new value
// 	channel_config_set_ring(&wave_dma_chan_a_config, false, 8); //wrap every 256 bytes
	
// 	// Setup the wave_dma_chan_b DMA channel
//     dma_channel_config wave_dma_chan_b_config = dma_channel_get_default_config(wave_dma_chan_b);
//     // Transfers 32-bits at a time, increment read address so we pick up a new wave value each
//     // time, don't increment writes address so we always transfer to the same PIO register.
//     channel_config_set_transfer_data_size(&wave_dma_chan_b_config, DMA_SIZE_32); //Transfer 32 bits at a time (4 bytes)
//     channel_config_set_read_increment(&wave_dma_chan_b_config, true);
//     channel_config_set_write_increment(&wave_dma_chan_b_config, false);
// 	channel_config_set_chain_to(&wave_dma_chan_b_config, wave_dma_chan_a);//after block has been transferred, wave_dma_chan a
// 	channel_config_set_dreq(&wave_dma_chan_b_config, DREQ_PIO0_TX0); // Transfer when PIO asks for a new value
// 	channel_config_set_ring(&wave_dma_chan_b_config, false, 8);	//wrap every 256 bytes (2**8)
	
//     // Setup the first wave DMA channel for PIO output
//     dma_channel_configure(
//         wave_dma_chan_a,
//         &wave_dma_chan_a_config,
//         &pio0_hw->txf[sm], // Write address (sm1 transmit FIFO)
//         awg_buff, // Read values from fade buffer
//         bufdepth, // 256 values to copy
//         false // Don't start yet.
// 	);
//     // Setup the second wave DMA channel for PIO output
//     dma_channel_configure(
//         wave_dma_chan_b,
//         &wave_dma_chan_b_config,
//         &pio0_hw->txf[sm], // Write address (sm1 transmit FIFO)
//         awg_buff, // Read values from fadeoff buffer
//         bufdepth, // 256 values to copy
//         false //  Don't start yet.
//     );

//     dma_channel_set_irq0_enabled(wave_dma_chan_a, true);
//     dma_channel_set_irq0_enabled(wave_dma_chan_b, true);
//     irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
//     irq_set_enabled(DMA_IRQ_0, true);
	
//     // Everything is ready to go. Now start the first DMA
//     dma_start_channel_mask(1u << wave_dma_chan_a);

//     // This section for communicating with terminal; either debugging or future control
// 	for (int k = 0; k < repeats; ++k) {
//         printf("awg_buff[%i]: %i sm: %i \n", k, awg_buff[k], sm);
//         printf(" wave_dma_chan_a: %i wave_dma_chan_b: %i \n", wave_dma_chan_a, wave_dma_chan_b);
//         //printf("ch#: %i &dma_hw->ch[wave_dma_chan_b].al2_write_addr_trig: %i \n", dma_hw->ch[wave_dma_chan_b].al2_write_addr_trig, &dma_hw->ch[wave_dma_chan_b].al2_write_addr_trig);
//         sleep_ms(100);
//     }

//     return;
// }

static uint32_t awg_buff[256] __attribute__((aligned(256)));
static int dma_chan = -1;
static int pio_sm = -1;
static bool pio_program_loaded = false;
static int dma_repeat_target = 0;
static int dma_repeat_count = 0;

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

void dma_flash_repeats(int repeats, uint OUT_PIN_NUMBER, uint NPINS, uint period_length) {
    // must call stdio_init_all() before using this
    
    if (repeats <= 0 || period_length <= 0 || period_length > 128) {
        printf("Invalid input. period_length must be >0 and <= 128.\n");
        return;
    }

    const int total_samples = repeats * period_length / 2;
    if (total_samples % 2 != 0) {
        printf("Error: total_samples must be even (2 x 16-bit samples per DMA word).\n");
        return;
    }
    const int dma_transfers = total_samples / 2;

    memset(awg_buff, 0, sizeof(awg_buff));
    for (int i = 0; i < dma_transfers; ++i) {
        awg_buff[i] = 0x0000FFFF;
        printf("awg_buff[%d] = 0x%08X\n", i, awg_buff[i]);
    }

    static bool pio_program_loaded = false;
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

    dma_channel_start(dma_chan);
    printf("Started waveform: %d cycles x %d samples/cycle on GPIO %d (%d-bit output)\n",
        repeats, period_length, OUT_PIN_NUMBER, NPINS);
}

void dma_step_repeats(int repeats, uint OUT_PIN_NUMBER, uint NPINS) {
    // must call stdio_init_all() before using this
    
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

    static bool pio_program_loaded = false;
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

    dma_channel_start(dma_chan);
    printf("Started waveform: %d cycles x %d samples/cycle on GPIO %d (%d-bit output)\n",
        repeats, total_samples, OUT_PIN_NUMBER, NPINS);
}



int main() {
    stdio_init_all();  // set up to print out
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

            dma_flash_repeats(repeats, 0,16,4);
            waitingForCommand = true;
        } 

        else if (userInput == '0') {
            int repeats = 1;
            printf("Enter number of repeats (empty is default as 1): ");
            scanf("%d", &repeats);

            dma_step_repeats(repeats, 0,16);
            waitingForCommand = true;


        }
    }
}

    