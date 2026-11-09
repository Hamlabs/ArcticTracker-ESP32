#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>


#define MIN_INTERVAL         2    // 2200 Hz is ~2.18 samples
#define MAX_INTERVAL         4    // 1200 Hz is exactly 4 samples

// Hysteresis parameters based on the last 16 zero-crossings

#define RECENT_WINDOW_SIZE   16
#define DCD_ON_THRESHOLD     13
#define DCD_OFF_THRESHOLD     7  


typedef struct {
    int8_t last_sample;
    uint32_t sample_counter;
    
    // A 16-bit shift register representing the last 16 zero crossings.
    // A '1' bit means a valid AFSK interval, a '0' bit means an invalid interval/noise.
    uint16_t history_shift_reg;
    uint8_t valid_count; // Tracks how many bits are currently '1'
    
    bool dcd_detected;
} DCD_Detector;



static DCD_Detector _dcd;


void dcd_init() {
    _dcd.last_sample = 0;
    _dcd.sample_counter = 0;
    _dcd.history_shift_reg = 0;
    _dcd.valid_count = 0;
    _dcd.dcd_detected = false;
}



bool dcd_test(int8_t new_sample) {
    _dcd.sample_counter++;

    // Prevent counter overflow during prolonged silence or long flat signals
    if (_dcd.sample_counter > 50) {
        _dcd.sample_counter = 50; 
    }

    // Detect zero crossing (sign change)
    if ((_dcd.last_sample >= 0 && new_sample < 0) || (_dcd.last_sample < 0 && new_sample >= 0)) {
        uint32_t interval = _dcd.sample_counter;
        _dcd.sample_counter = 0; 

        // Determine if this specific interval matches our AFSK targets (1200Hz/2200Hz)
        bool is_valid_interval = (interval >= MIN_INTERVAL && interval <= MAX_INTERVAL);

        // Check the oldest bit that is about to drop out of our 16-crossing window
        bool oldest_bit = (_dcd.history_shift_reg & (1 << (RECENT_WINDOW_SIZE - 1))) != 0;

        // Shift the register and insert our new observation at the lowest bit
        _dcd.history_shift_reg <<= 1;
        if (is_valid_interval) {
            _dcd.history_shift_reg |= 1;
        }

        // Update the running tally of valid crossings within our window
        if (is_valid_interval && !oldest_bit) {
            _dcd.valid_count++;
        } else if (!is_valid_interval && oldest_bit) {
            _dcd.valid_count--;
        }

        // Apply hysteresis evaluation
        if (!_dcd.dcd_detected) {
            if (_dcd.valid_count >= DCD_ON_THRESHOLD) {
                _dcd.dcd_detected = true;
            }
        } else {
            if (_dcd.valid_count <= DCD_OFF_THRESHOLD) {
                _dcd.dcd_detected = false;
            }
        }
    }

    _dcd.last_sample = new_sample;
    return _dcd.dcd_detected;
}




