#include "stm32f0xx.h"
#include "midi.h"
#include "step-array.h"
#include "player.h"
#include <math.h>

struct {
    uint8_t note;
    uint8_t chan;
    uint8_t volume;
    int     step;
    int     offset;
} voice[15];

short int wavetable[N];

void init_wavetable(void)
{
    for (int i = 0; i < N; i ++)
    {
        wavetable[i] = 32767 * sin(2 * M_PI * i / N);
    }
}

void init_TIM2(int arr_init)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; // turn on RCC clock for TIM2
    TIM2->PSC = 48-1;                   // set PSC to divide by 48
    TIM2->ARR = arr_init - 1;
    TIM2->DIER |= TIM_DIER_UIE;         // enable update interrupt
    TIM2->CR1 |= TIM_CR1_CEN;           // enable counter
    NVIC->ISER[0] |= 1<<TIM2_IRQn;      // unmask NVIC interrupt
}

void init_TIM6()
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN; // turn on RCC clock for TIM6
    TIM6->PSC = 1-1;                    // timer must trigger at RATE times per second
    TIM6->ARR = 48000000 / RATE - 1;
    TIM6->DIER |= TIM_DIER_UIE;         // enable update interrupt
    TIM6->CR1 |= TIM_CR1_CEN;           // enable counter
    NVIC->ISER[0] |= 1<< TIM6_DAC_IRQn; // unmask NVIC interrupt
}

void init_DAC()
{
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;  // turn on RCC clock for DAC
    DAC->CR |= DAC_CR_TSEL1;            // select software trigger
    DAC->CR |= DAC_CR_TEN1;             // trigger enable
    DAC->CR |= DAC_CR_EN1;              // enable DAC
    // set up port - DAC1 out is pa4
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;  // enable RCC clock for GPIOA
    GPIOA->MODER |= 0x00000300;         // configure PA4 for analog
}

void TIM2_IRQHandler()
{
    TIM2->SR &=~TIM_SR_UIF;             // acknowledge interrupt
    midi_play();                        // fetches next note
}

void TIM6_DAC_IRQHandler()
{
    TIM6->SR &=~TIM_SR_UIF;             // acknowledge interrupt
    int sample = 0;                     // taken from piazza/course website
    while ((DAC->SWTRIGR & DAC_SWTRIGR_SWTRIG1)== DAC_SWTRIGR_SWTRIG1);
    DAC->SWTRIGR |= DAC_SWTRIGR_SWTRIG1;// trigger DAC
    for(int i=0; i < sizeof voice / sizeof voice[0]; i++)
    {
        if (voice[i].step != 0)         // check that voice is active
        {
            sample += (wavetable[voice[i].offset>>16] * voice[i].volume/* * (100/voice[i].note)*/) /*<< 4*/;
            voice[i].offset += voice[i].step;
            if ((voice[i].offset >> 16) >= sizeof wavetable / sizeof wavetable[0])
                voice[i].offset -= (sizeof wavetable / sizeof wavetable[0]) << 16;
        }
    }
    sample = (sample >> 16) + 2048;
    DAC->DHR12R1 = sample;

}



void set_tempo(int time, int value, const MIDI_Header *hdr)
{
    // This assumes that the TIM2 prescaler divides by 48.
    // It sets the timer to produce an interrupt every N
    // microseconds, where N is the new tempo (value) divided by
    // the number of divisions per beat specified in the MIDI header.
    TIM2->ARR = value/hdr->divisions - 1;
}

void note_on(int time, int chan, int key, int velo)
{
    if (velo != 0)
    {
        for(int i=0; i < sizeof voice / sizeof voice[0]; i++)
            if (voice[i].step == 0)
            {
                // configure this voice to have the right step and volume
                voice[i].note = key;
                voice[i].step = step[key];
                voice[i].volume = velo;
                voice[i].chan = chan;
                break;
            }
    }
    else
    {
        note_off(time, chan, key, velo);
    }

}

void note_off(int time, int chan, int key, int velo)
{
    for(int i=0; i < sizeof voice / sizeof voice[0]; i++)
        if (voice[i].step != 0 && voice[i].note == key)
        {
            // turn off this voice
            voice[i].step = 0;
            voice[i].note = 0;
            voice[i].volume = 0;
            //voice[i].chan = 0;
            break;
        }
}
