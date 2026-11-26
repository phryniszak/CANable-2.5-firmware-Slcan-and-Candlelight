/*
 * Frequency measurement with TIM2 input capture on PA0 (Arduino A0).
 *
 * TIM2 is a 32 bit timer, clocked with 170 MHz (APB1 timer clock, derived from the 24 MHz HSE quartz).
 * Channel 1 captures every 8th rising edge (hardware input prescaler), the interrupt stores the captured
 * timer value and counts the captures. Every second the main loop calculates
 *
 *     f = (captures * 8) / (timer ticks between the first and the last capture) * 170 MHz
 *
 * The capture is latched by hardware, so the interrupt latency does not influence the result.
 * Resolution: 1 tick in 170 million = 0.006 ppm. The accuracy is that of the Nucleo's HSE quartz.
 *
 * Written with CMSIS register access, because the project contains no LL TIM driver,
 * and this file is not touched when STM32CubeMX regenerates the code.
 * ATTENTION: Do not enable TIM2 in STM32CubeMX, it would generate a second TIM2_IRQHandler.
 */

#include "freq_meas.h"
#include "main.h"
#include <stdio.h>
#include <stdbool.h>

#define TIM_CLOCK_HZ    170000000u // APB1 timer clock (SYSCLK 170 MHz, APB1 prescaler 1)
#define CAPTURE_DIV     8u         // input prescaler: one capture every 8 rising edges
#define WINDOW_MS       1000u      // measurement window

volatile double   freq_hz      = 0;
volatile double   freq_err_ppm = 0;
volatile uint32_t freq_windows = 0;
volatile uint32_t freq_lost    = 0;

static volatile uint32_t cap_count = 0; // captures since start (written by the interrupt)
static volatile uint32_t cap_last  = 0; // timer value of the last capture

static uint32_t prev_count = 0;
static uint32_t prev_last  = 0;
static uint32_t ms_count   = 0;
static bool     started    = false; // prev_count / prev_last are valid
static bool     had_signal = false; // the previous window had captures -> prev_last is a capture of the running signal

void TIM2_IRQHandler(void)
{
    uint32_t sr = TIM2->SR;
    if (sr & TIM_SR_CC1OF) // a capture was overwritten before it was read
    {
        TIM2->SR = ~TIM_SR_CC1OF;
        freq_lost++;
    }
    if (sr & TIM_SR_CC1IF)
    {
        cap_last = TIM2->CCR1; // reading CCR1 clears CC1IF
        cap_count++;
    }
}

void freq_meas_init(void)
{
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
    (void)RCC->APB1ENR1; // wait until the clock is enabled

    // PA0 = alternate function 1 (TIM2_CH1), pull-down so an open input does not produce random edges
    GPIOA->MODER  = (GPIOA->MODER  & ~GPIO_MODER_MODE0)   | (2u << GPIO_MODER_MODE0_Pos);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~GPIO_AFRL_AFSEL0)   | (1u << GPIO_AFRL_AFSEL0_Pos);
    GPIOA->PUPDR  = (GPIOA->PUPDR  & ~GPIO_PUPDR_PUPD0)   | (2u << GPIO_PUPDR_PUPD0_Pos);

    TIM2->CR1   = 0;
    TIM2->PSC   = 0;          // count with 170 MHz
    TIM2->ARR   = 0xFFFFFFFF; // free running 32 bit (wraps every 25 s, the unsigned difference handles that)
    // CC1S = 01: input capture from TI1, IC1PSC = 11: capture every 8 events, IC1F = 0010: filter fCK_INT, N = 4 (24 ns)
    TIM2->CCMR1 = (1u << TIM_CCMR1_CC1S_Pos) | (3u << TIM_CCMR1_IC1PSC_Pos) | (2u << TIM_CCMR1_IC1F_Pos);
    TIM2->CCER  = TIM_CCER_CC1E; // rising edge (CC1P = CC1NP = 0), capture enabled
    TIM2->DIER  = TIM_DIER_CC1IE;
    TIM2->EGR   = TIM_EGR_UG;    // load PSC
    TIM2->SR    = 0;

    NVIC_SetPriority(TIM2_IRQn, 0);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1   = TIM_CR1_CEN;

    printf("\r\nFrequency measurement: TIM2 CH1 input capture on PA0 (A0), timer clock %lu Hz\r\n",
           (unsigned long)TIM_CLOCK_HZ);
}

void freq_meas_process(void)
{
    // SysTick runs without interrupt (LL_Init1msTick), COUNTFLAG is set once per millisecond
    if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        return;
    if (++ms_count < WINDOW_MS)
        return;
    ms_count = 0;

    __disable_irq();
    uint32_t count = cap_count;
    uint32_t last  = cap_last;
    __enable_irq();

    uint32_t d_count = count - prev_count;
    uint32_t d_ticks = last  - prev_last;
    // The window starts at the last capture of the previous window. It is invalid in the first window
    // and when the signal starts or comes back, because then that start capture is missing or old.
    bool     valid   = started && had_signal;
    started    = true;
    had_signal = (d_count != 0);
    prev_count = count;
    prev_last  = last;
    freq_windows++;

    if (d_count == 0 || d_ticks == 0)
    {
        freq_hz      = 0;
        freq_err_ppm = 0;
        printf("No signal on PA0\r\n");
        return;
    }

    if (!valid)
        return; // signal (re)started, the next window is measured

    // integer calculation in millihertz for printing (no float printf required)
    uint64_t edges = (uint64_t)d_count * CAPTURE_DIV;
    uint64_t f_mhz = (edges * TIM_CLOCK_HZ * 1000u + d_ticks / 2) / d_ticks;
    int64_t  err01 = ((int64_t)f_mhz - (int64_t)FREQ_NOMINAL_HZ * 1000) * 10000 / (int64_t)FREQ_NOMINAL_HZ; // 0.1 ppm

    freq_hz      = (double)edges * TIM_CLOCK_HZ / d_ticks;
    freq_err_ppm = (freq_hz - FREQ_NOMINAL_HZ) * 1e6 / FREQ_NOMINAL_HZ;

    int64_t err_abs = err01 < 0 ? -err01 : err01;
    printf("f = %lu.%03lu Hz   err = %c%ld.%01ld ppm   (%lu edges, lost %lu)\r\n",
           (unsigned long)(f_mhz / 1000), (unsigned long)(f_mhz % 1000),
           err01 < 0 ? '-' : '+', (long)(err_abs / 10), (long)(err_abs % 10),
           (unsigned long)edges, (unsigned long)freq_lost);

    LL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

// printf() output to the ST-LINK virtual COM port (USART2, 921600 baud)
int __io_putchar(int ch)
{
    while (!LL_USART_IsActiveFlag_TXE_TXFNF(USART2)) {}
    LL_USART_TransmitData8(USART2, (uint8_t)ch);
    return ch;
}
