
/*
 * AFSK Demodulation. 
 * 
 * Partly based on code from BertOS AFSK decoder. 
 *    Originally by Develer S.r.l. (http://www.develer.com/), GPLv2 licensed.
 * 
 */
#include "defines.h"
#if !defined(ARCTIC4_UHF)

#include <string.h>
#include "afsk.h"
#include "hdlc.h"
#include "ui.h"
#include "fifo.h"
#include "radio.h"
#include "system.h"

#define TAG "afsk-rx"


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
#define FIR_MAX_TAPS 60


/* Queue of decoded bits. To be used by HDLC packet decoder */
static fifo_t iq; 


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
  FIR_NONE=-1,
  FIR_1200_BP=0,
  FIR_2200_BP=1,
  FIR_12_22_BP=2,
  FIR_1200_LP=3,
  FIR_PREEMP=4,
  FIR_DEEMP=5,
  FIR_1200_BP2=6,
  FIR_2200_BP2=7,
};


/************************************************
 * Static functions
 ************************************************/

static void add_bit(bool bit);
static void afsk_process_sample(int8_t curr_sample);
static void afsk_rxdecoder(void* arg);
static void doFrame(enum fir_filters f);

   


// Tool for designing filters: http://t-filter.engineerjs.com/

static FIR fir_table[] =
{
   /* 1200 Hz bandpass filter */
  [FIR_1200_BP] = {
    .taps = 11,
    .coef = {
      -12, -16, -15, 0, 20, 29, 20, 0, -15, -16, -12
    },
    .mem = {
      0,
    },
  },
  
  /* 2200 Hz bandpass filter */
  [FIR_2200_BP] = {
    .taps = 11,
    .coef = {
      11, 15, -8, -26, 4, 30, 4, -26, -8, 15, 11
    },
    .mem = {
      0,
    },
  },

  /* 1200-2200 Hz bandpass filter */
  [FIR_12_22_BP] = {
    .taps = 30,
    .coef = {
        3,4,-2,-5,-3,1,-2,-3,7,19,8,-24,-37,-4,38,38,-4,-37,-24,8,19,7,-3,-2,1,-3,-5,-2,4,3
    },
    .mem = {
      0,
    },
  },
  
  
  /* Lowpass filter to 1200 Hz */
  [FIR_1200_LP] = {
    .taps = 8,
    .coef = {
      -9, 3, 26, 47, 47, 26, 3, -9
    },
    .mem = {
      0,
    },
  },  
  
  /* Pre-emphasis filter */
  [FIR_PREEMP] = {
    .taps = 5,
    .coef = {
      -18, -29, 93, -29, -18
    },
    .mem = {
      0,
    },
  },  
  
  /* De-emphasis filter */
  [FIR_DEEMP] = {
    .taps = 5,
    .coef = {
      6, 39, 60, 39, 6
    },
    .mem = {
      0,
    },
  },
  

  /* 1200 Hz bandpass filter */
  [FIR_1200_BP2] = {
    .taps = 40,
    .coef = {
       -1,-1,0,1,3,3,0,-4,-6,-5,1,7,10,6,-2,-10,-12,-6,5,12,12,5,-6,-12,-10,-2,6,10,7,1,-5,-6,-4,0,3,3,1,0,-1,-1
    },
    .mem = {
      0,
    },
  },
  
  /* 2200 Hz bandpass filter */
  [FIR_2200_BP2] = {
    .taps = 40,
    .coef = {
      -1,0,2,1,-2,-1,3,3,-3,-5,3,6,-1,-8,-1,9,3,-8,-5,7,7,-5,-8,3,9,-1,-8,-1,6,3,-5,-3,3,3,-1,-2,1,2,0,-1
    },
    .mem = {
      0,
    },
  },
  
};


/*
 * FIR filter function. Apply sample to the given filter
 */
static int8_t fir_filter(int8_t s, enum fir_filters f)
{
  if (f==FIR_NONE)
    return s;
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




/************************************************
  DCD 
  Return true if there is a AFSK signal present
  FIXME: This still need some work.. 
 ************************************************/

#if DEVICE == T_TWR
#define SIGNAL_THRESHOLD 25
#else
#define SIGNAL_THRESHOLD 75
#endif


static uint16_t flevel = 0, ndcd=0;
static bool prev_dcd=false, prev2_dcd=false, dcd=false, result=false;

bool afsk_dcd(int8_t inp) {

    /* 
     * Put the sample through the bandpass filter.
     */ 
    int8_t fsample = fir_filter(inp, FIR_12_22_BP);
    flevel = flevel * 0.5 + (fsample * fsample) * 0.5; 

    dcd = (flevel > SIGNAL_THRESHOLD);
    if (!dcd && !prev_dcd && !prev2_dcd)
      result = false; 
    else if (dcd && prev_dcd && prev2_dcd)
      result = true; 
    prev2_dcd = prev_dcd;
    prev_dcd = dcd;
    
    return result; 
}




void afsk_dcd_reset() {
}



/*******************************************
  Decoding thread
 *******************************************/

void print_samples();

static void afsk_rxdecoder(void* arg) 
{
    while (true) {
        /* Wait for squelch to be opened */
        if (!afsk_isSquelchOff() && !radio_getSquelch() ) {
            rxSampler_stop(); 
            sem_down(afsk_frames);
        }
      
        /* Get the frame from ADC sampler */
        rxSampler_start(); 
        int n = rxSampler_getFrame();
        rxSampler_readLast();
        ESP_LOGI(TAG, "Frame: %d samples", n); 
        if (n <= 2500)
           continue;
        hdlc_next_frame();
        
        /* Decode the frame */
        doFrame(FIR_NONE);
        sleepMs(5);
        ESP_LOGD(TAG, "Decode attempt 1: %s", (hdlc_isSuccess() ? "YES": "NO"));
        if (hdlc_isSuccess())
          continue;
        
        doFrame(FIR_PREEMP);
        sleepMs(5);
        ESP_LOGD(TAG, "Decode attempt 2: %s", (hdlc_isSuccess() ? "YES": "NO"));
        if (hdlc_isSuccess())
          continue;
        
        doFrame(FIR_DEEMP);
        sleepMs(10);
        ESP_LOGD(TAG, "Decode attempt 3: %s", (hdlc_isSuccess() ? "YES": "NO"));
    }
}


/* Signal that we can start to receive a frame */
void afsk_rx_newFrame() {
    sem_up(afsk_frames);
}



/***************************************************
  Process the samples to decode the current frame. 
  A FIR-filter may be used before the decoding. 
  FIR_NONE means no filtering. 
 ***************************************************/

static void doFrame(enum fir_filters filt) {
    rxSampler_reset();
    while (!rxSampler_eof()) {     
        int8_t sample = rxSampler_get();
        int8_t filtered = fir_filter(sample, filt);
        afsk_process_sample(filtered);
    }
    /* 
     * Add 0-samples after the end of the frame to flush filters
     */
    for (int i=0; i<16; i++)
        afsk_process_sample(0);
}



/***************************************************************
  This routine should be called for each sample taken from 
  the radio receiver audio. 
****************************************************************/

static void afsk_process_sample(int8_t sample) 
{
    afsk.iirY[0] = fir_filter(sample, FIR_1200_BP);
    afsk.iirY[1] = fir_filter(sample, FIR_2200_BP);
    
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

#endif

