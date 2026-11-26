/*
 * Frequency measurement with TIM2 input capture on PA0 (Arduino A0).
 * Measures the MCO output of the CANable firmware (HSI16 / 16 = 1 MHz on its PA8).
 */
#pragma once

#include <stdint.h>

// Expected frequency, only used to calculate the deviation in ppm
#define FREQ_NOMINAL_HZ      1000000u

// Results of the last 1 second window (for Live Watch in the debugger)
extern volatile double   freq_hz;      // measured frequency, 0 = no signal
extern volatile double   freq_err_ppm; // deviation from FREQ_NOMINAL_HZ
extern volatile uint32_t freq_windows; // count of completed measurement windows
extern volatile uint32_t freq_lost;    // count of lost captures (over-capture), should stay 0

void freq_meas_init(void);
void freq_meas_process(void); // call from the main loop as often as possible
