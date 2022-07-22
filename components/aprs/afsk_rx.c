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
static fifo_t iq; 
static fifo_t delay;

/* Semaphore to signal availability of afsk frames */
static semaphore_t afsk_frames; 




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
static void afsk_process_sample(int8_t curr_sample);
static void afsk_rxdecoder(void* arg);


/*******************************************
  Modem Initialization                             
 *******************************************/

fifo_t* afsk_rx_init() 
{ 
  /* Allocate memory for struct */
  memset(&afsk, 0, sizeof(afsk));

  afsk_frames = sem_create(0);
  fifo_init(&iq, AFSK_RX_QUEUE_SIZE); 
  
  rxSampler_init();
  
  xTaskCreatePinnedToCore(&afsk_rxdecoder, "AFSK RX decoder", 
        STACK_AFSK_RXDECODER, NULL, NORMALPRIO, NULL, CORE_AFSK_RXDECODER);

  return &iq;
}



static void frameInfo() {      
    int cnt=0, cnt100=0, cnt40=0, cnt10=0;
    rxSampler_reset();
    while (!rxSampler_eof()) {
        cnt++;
        int8_t sample = rxSampler_get();
        if (sample < -100 || sample > 100)
            cnt100++;
        else if (sample < -40 || sample > 40)
            cnt40++;
    }
}



static void doFrame() {
    rxSampler_reset();
    while (!rxSampler_eof())
        afsk_process_sample(rxSampler_get()); 
}


/* Decoding thread */
static void afsk_rxdecoder(void* arg) 
{
    while (true) {
        sem_down(afsk_frames);
        doFrame(1);    
    }
}


void afsk_rx_newFrame() {
    sem_up(afsk_frames);
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



/***************************************************************
  This routine should be called 9600
  times each second to analyze samples taken from
  the physical medium. 
****************************************************************/

static void afsk_process_sample(int8_t curr_sample) 
{ 
    afsk.iirY[0] = fir_filter(curr_sample, FIR_1200_BP);
    afsk.iirY[1] = fir_filter(curr_sample, FIR_2200_BP);
    
    afsk.iirY[1] = (int8_t) (afsk.iirY[1] * 1.2); 
    
    afsk.iirY[0] = ABS(afsk.iirY[0]);
    afsk.iirY[1] = ABS(afsk.iirY[1]);
    
    afsk.sampled_bits <<= 1;
    afsk.sampled_bits |= (fir_filter(afsk.iirY[1] - afsk.iirY[0], FIR_1200_LP) > 0 ? 1 : 0);

    
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
        uint8_t bits = afsk.sampled_bits & 0x0f;
        if ( bits == 0x07       // 0111, 3 bits set to 1
                || bits == 0x0f // 1111 
                || bits == 0x0b // 1011
                || bits == 0x0d // 1101
                || bits == 0x0e // 1110
                || bits == 0x03
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

uint8_t octet = 0; 
int bit_count = 0;

/*********************************************************
 * Send a single bit to the HDLC decoder
 *********************************************************/

static void add_bit(bool bit)
{     
    octet = (octet >> 1) | (bit ? 0x80 : 0x00);
    bit_count++;

    if (bit_count == 8) 
    {      
        fifo_put(&iq, octet);
        bit_count = 0;
        
    }
}

