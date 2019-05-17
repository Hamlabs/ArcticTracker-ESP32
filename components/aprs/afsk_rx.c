/*
 * AFSK Demodulation. 
 * 
 * Based on code from BertOS AFSK decoder. 
 * Originally by Develer S.r.l. (http://www.develer.com/), GPLv2 licensed.
 * 
 */

      
#include <string.h>
#include "defines.h"
#include "afsk.h"
#include "ui.h"
#include "fifo.h"
#include "radio.h"
#include "system.h"


#define SAMPLESPERBIT (AFSK_SAMPLERATE / AFSK_BITRATE)   // How many DAC/ADC samples constitute one bit (8).

/* Phase sync constants */
#define PHASE_BITS   8                              // How much to increment phase counter each sample
#define PHASE_INC    1                              // Nudge by an eigth of a sample each adjustment
#define PHASE_MAX    (SAMPLESPERBIT * PHASE_BITS)   // Resolution of our phase counter = 64
#define PHASE_THRESHOLD  (PHASE_MAX / 2)            // Target transition point of our phase window

/* Detect transition */
#define BITS_DIFFER(bits1, bits2) (((bits1)^(bits2)) & 0x01)
#define TRANSITION_FOUND(bits) BITS_DIFFER((bits), (bits) >> 1)

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define FIR_MAX_TAPS 16


/* Queue of decoded bits. To be used by HDLC packet decoder */
static QueueHandle_t iq; 



/*********************************************************
 * This is our primary modem struct. It defines
 * the values we need to demodulate data.
 *********************************************************/

typedef struct AfskRx
{
   int16_t iirX[2];         // Filter X cells
   int16_t iirY[2];         // Filter Y cells
  
   uint8_t sampled_bits;    // Bits sampled by the demodulator (at ADC speed)
   int8_t  curr_phase;      // Current phase of the demodulator
   uint8_t found_bits;      // Actual found bits at correct bitrate
  
   bool    cd;              // Carrier detect 
   uint8_t cd_state;
  
} AfskRx;

static AfskRx afsk;


static void add_bit(bool bit);

static int8_t delay_buf[100];
static int8_t delay_buf1[100]; 

static fifo_t fifo, fifo1;

static void afsk_process_sample(int8_t curr_sample);




/*****************************
 * Sampler ISR 
 *****************************/

uint32_t cp0_regs[18];

void afsk_rxSampler(void *arg) 
{
    // get FPU state
    uint32_t cp_state = xthal_get_cpenable();
  
    /* Save FPU registers and enable FPU in ISR */
    if(cp_state)
        xthal_save_cp0(cp0_regs);
    else 
        xthal_set_cpenable(1);
    
  
    clock_clear_intr(AFSK_TIMERGRP, AFSK_TIMERIDX);
    afsk_process_sample((int8_t) (adc_sample()/14));
    
    /* Restore FPU registers and turn it back off */
    if(cp_state) 
        xthal_restore_cp0(cp0_regs);
    else 
        xthal_set_cpenable(0);
    
}


/*******************************************
  Modem Initialization                             
 *******************************************/

QueueHandle_t afsk_rx_init() 
{ 
//  clock_init(AFSK_RX_TIMERGRP, AFSK_RX_TIMERIDX, CLOCK_DIVIDER, afsk_rxSampler, false);
  
  /* Allocate memory for struct */
  memset(&afsk, 0, sizeof(afsk));
  
  fifo_init(&fifo,  delay_buf,  sizeof(delay_buf)); 
  fifo_init(&fifo1, delay_buf1, sizeof(delay_buf1));  
  
  /* Fill sample FIFO with 0 */
  for (int i = 0; i < SAMPLESPERBIT / 2; i++) {
    fifo_push(&fifo, 0); 
    fifo_push(&fifo1, 0);
  }
  
  iq =  xQueueCreate(AFSK_RX_QUEUE_SIZE, 1);
  return iq;
}



   

/************************************************
 * FIR filtering 
 ************************************************/

typedef struct FIR
{
  int8_t taps;
  int8_t coef[FIR_MAX_TAPS];
  int16_t mem[FIR_MAX_TAPS];
} FIR;


enum fir_filters
{
  FIR_1200_BP=0,
  FIR_2200_BP=1,
  FIR_1200_LP=2
};


static FIR fir_table[] =
{
  [FIR_1200_BP] = {
    .taps = 11,
    .coef = {
      -12, -16, -15, 0, 20, 29, 20, 0, -15, -16, -12
    },
    .mem = {
      0,
    },
  },
  [FIR_2200_BP] = {
    .taps = 11,
    .coef = {
      11, 15, -8, -26, 4, 30, 4, -26, -8, 15, 11
    },
    .mem = {
      0,
    },
  },
  [FIR_1200_LP] = {
    .taps = 8,
    .coef = {
      -9, 3, 26, 47, 47, 26, 3, -9
    },
    .mem = {
      0,
    },
  },
};



static int8_t fir_filter(int8_t s, enum fir_filters f)
{
  int8_t Q = fir_table[f].taps - 1;
  int8_t *B = fir_table[f].coef;
  int16_t *Bmem = fir_table[f].mem;
  
  int8_t i;
  int16_t y;
  
  Bmem[0] = s;
  y = 0;
  
  for (i = Q; i >= 0; i--)
  {
    y += Bmem[i] * B[i];
    Bmem[i + 1] = Bmem[i];
  }
  
  return (int8_t) (y / 128);
}



/******************************************************************************
   Automatic gain control. 
   
*******************************************************************************/

#define DECAY 5

static uint8_t peak_mark = 250;
static uint8_t peak_space = 250;


static int8_t agc (int8_t in, uint8_t *ppeak)
{
    if (*ppeak > 100) 
       *ppeak -= DECAY; 
    if (in*2 > *ppeak)
       *ppeak = in*2;
    
    float factor = 250.0f / *ppeak;        
    return (int8_t) (factor * (float) in);
}



/***************************************************************
  This routine should be called 9600
  times each second to analyze samples taken from
  the physical medium. 
****************************************************************/

static void afsk_process_sample(int8_t curr_sample) 
{ 
    afsk.iirY[0] = fir_filter(curr_sample, FIR_1200_BP);
    afsk.iirY[1] = fir_filter(curr_sample, FIR_2200_BP);
    
    afsk.iirY[1] = (int8_t) (afsk.iirY[1] * 1.2f); 
    
    afsk.iirY[0] = ABS(afsk.iirY[0]);
    afsk.iirY[1] = ABS(afsk.iirY[1]);
    
    afsk.iirY[0] = agc(afsk.iirY[0], &peak_mark);
    afsk.iirY[1] = agc(afsk.iirY[1], &peak_space);
    
    
    afsk.sampled_bits <<= 1;
    afsk.sampled_bits |= fir_filter(afsk.iirY[1] - afsk.iirY[0], FIR_1200_LP) > 0;
    
    /* 
     * If there is a transition, adjust the phase of our sampler
     * to stay in sync with the transmitter. 
     */ 
    if (TRANSITION_FOUND(afsk.sampled_bits)) {
        if (afsk.curr_phase < PHASE_THRESHOLD) {
            afsk.curr_phase += PHASE_INC;
        } else {
            afsk.curr_phase -= PHASE_INC;
        }
    }

    afsk.curr_phase += PHASE_BITS;

    /* Check if we have reached the end of
     * our sampling window.
     */ 
    if (afsk.curr_phase >= PHASE_MAX) 
    { 
        afsk.curr_phase %= PHASE_MAX;

        /* Shift left to make room for the next bit */
        afsk.found_bits <<= 1;

        /*
         * Determine bit value by reading the last 3 sampled bits.
         * If the number of ones is two or greater, the bit value is a 1,
         * otherwise is a 0.
         * This algorithm presumes that there are 8 samples per bit.
         */
        uint8_t bits = afsk.sampled_bits & 0x07;
        if ( bits == 0x07     // 111, 3 bits set to 1
              || bits == 0x06 // 110, 2 bits
              || bits == 0x05 // 101, 2 bits
              || bits == 0x03 // 011, 2 bits
           )
           afsk.found_bits |= 1;

        /* 
         * Now we can pass the actual bit to the HDLC parser.
         * We are using NRZI coding, so if 2 consecutive bits
         * have the same value, we have a 1, otherwise a 0.
         * We use the TRANSITION_FOUND function to determine this.
         */
        add_bit( !TRANSITION_FOUND(afsk.found_bits) );
    }
} 



/*********************************************************
 * Send a single bit to the HDLC decoder
 *********************************************************/

static uint8_t bit_count = 0;

static void add_bit(bool bit)
{     
    static uint8_t octet;
    octet = (octet >> 1) | (bit ? 0x80 : 0x00);
    bit_count++;
  
    if (bit_count == 8) 
    {       
        if (!xQueueIsQueueFullFromISR(iq))
            xQueueSendFromISR(iq, &octet, NULL);
        bit_count = 0;
    }
}

