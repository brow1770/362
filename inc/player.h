#ifndef PLAYER_INCLUDE
#define PLAYER_INCLUDE

#include "midi.h"

void init_wavetable(void);
void init_TIM2(int arr_init);
void init_TIM6();
void init_DAC();
void TIM2_IRQHandler();
void TIM6_DAC_IRQHandler();
void set_tempo(int time, int value, const MIDI_Header *hdr);
void note_on(int time, int chan, int key, int velo);
void note_off(int time, int chan, int key, int velo);
#endif
