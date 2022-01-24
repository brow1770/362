#include "stm32f0xx.h"
#include "gps.h"
#include "oled.h"
#include "player.h"
#include "rtc.h"
#include "lcd.h"

int alarm_menu = 0;

//DEBOUNCE KEYPAD

void enable_keypad_ports(void){
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC -> MODER &= ~0x0000ffff;
    GPIOC -> MODER |= 0x00005500;
    GPIOC -> PUPDR &= ~0x000000ff;
    GPIOC -> PUPDR |= 0x000000aa;
}

const char keymap[] = "DCBA#9630852*741";
uint8_t hist[16];
uint8_t col;
char queue[2];
int qin;
int qout;

void drive_column(int c)
{
    c = c & 3;
    GPIOC->BSRR = 0xf00000 | (1 << (c + 4));
}

int read_rows()
{
    return GPIOC->IDR & 0xf;
}

void push_queue(int n) {
    n = (n & 0xff) | 0x80;
    queue[qin] = n;
    qin ^= 1;
}

uint8_t pop_queue() {
    uint8_t tmp = queue[qout] & 0x7f;
    queue[qout] = 0;
    qout ^= 1;
    return tmp;
}

void update_history(int c, int rows)
{
    for(int i = 0; i < 4; i++) {
        hist[4*c+i] = (hist[4*c+i]<<1) + ((rows>>i)&1);
        if (hist[4*c+i] == 1)
          push_queue(4*c+i);
    }
}

void init_tim7(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
    TIM7->PSC = 48-1;
    TIM7->ARR = 1000-1;
    TIM7->DIER |= TIM_DIER_UIE;
    TIM7->CR1 |= TIM_CR1_CEN;
    NVIC->ISER[0] |= 1<<TIM7_IRQn;
}

void TIM7_IRQHandler(void){
    TIM7->SR &= ~TIM_SR_UIF;
    int rows = read_rows();
    update_history(col, rows);
    col = (col + 1) & 3;
    drive_column(col);
}

char get_keypress() {
    for(;;) {
        asm volatile ("wfi" : :);   // wait for an interrupt
        if (queue[qout] == 0)
            continue;
        return keymap[pop_queue()];
    }
}
//END KEYPAD DEBOUNCE

RTC_InitTypeDef rtc_init;
//RTC_TimeTypeDef rtc_time;
RTC_TimeTypeDef rtc_time;
RTC_DateTypeDef rtc_date;
RTC_AlarmTypeDef rtc_alarm_gps;
RTC_AlarmTypeDef rtc_alarm_prog;
uint8_t alarm_mode;
uint8_t is_alarm_on = 0;
uint8_t is_alarm_set = 0;
char* time_str;

short oled_memory_map[34] = {0x02, 0x220, 0x220, 0x220, 0x220, 0x23a, 0x220, 0x220, 0x23a, 0x220, 0x220, 0x220, 0x220, 0x24d, 0x24d, 0x220, 0x220,
                            0xc0, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220};

short alarm_memory_map[34] = {0x02, 0x220, 0x220, 0x220, 0x253, 0x265, 0x274, 0x220, 0x241, 0x26c, 0x261, 0x272, 0x26d, 0x3a, 0x23a, 0x220, 0x220,
                             0xc0, 0x220, 0x220, 0x25f, 0x25f, 0x23a, 0x25f, 0x25f, 0x23a, 0x25f, 0x25f, 0x220, 0x25f, 0x24d, 0x220, 0x220, 0x220};

u16 old_hrx;
u16 old_hry;
u16 old_minx;
u16 old_miny;
u16 old_secx;
u16 old_secy;
int hrx[72] = {150, 154, 158, 162, 165, 169, 171, 174, 176, 177, 179, 179, 180, 179, 179, 177, 176, 174, 171, 169, 165, 162, 158, 154, 150, 145, 140, 135, 130, 125, 120, 114, 109, 104, 99, 94, 90, 85, 81, 77, 74, 70, 68, 65, 63, 62, 60, 60, 60, 60, 60, 62, 63, 65, 68, 70, 74, 77, 81, 85, 89, 94, 99, 104, 109, 114, 119, 125, 130, 135, 140, 145};
int hry[72] = {109, 111, 115, 118, 122, 126, 131, 135, 140, 145, 150, 155, 161, 166, 171, 176, 181, 186, 191, 195, 199, 203, 206, 210, 212, 215, 217, 218, 220, 220, 221, 220, 220, 218, 217, 215, 212, 210, 206, 203, 199, 195, 191, 186, 181, 176, 171, 166, 161, 155, 150, 145, 140, 135, 131, 126, 122, 118, 115, 111, 109, 106, 104, 103, 101, 101, 101, 101, 101, 103, 104, 106};
int minx[60] = {120, 127, 135, 143, 150, 157, 164, 170, 175, 180, 184, 188, 191, 193, 194, 195, 194, 193, 191, 188, 184, 180, 175, 170, 164, 157, 150, 143, 135, 127, 119, 112, 104, 96, 89, 82, 75, 69, 64, 59, 55, 51, 48, 46, 45, 45, 45, 46, 48, 51, 55, 59, 64, 69, 75, 82, 89, 96, 104, 112};
int miny[60] = {86, 86, 87, 89, 92, 96, 100, 105, 110, 116, 123, 130, 137, 145, 153, 161, 168, 176, 184, 191, 198, 205, 211, 216, 221, 225, 229, 232, 234, 235, 236, 235, 234, 232, 229, 225, 221, 216, 211, 205, 198, 191, 184, 176, 168, 161, 153, 145, 137, 130, 123, 116, 110, 105, 100, 96, 92, 89, 87, 86};
int secx[60] = {120, 129, 138, 147, 156, 165, 172, 180, 186, 192, 197, 202, 205, 208, 209, 210, 209, 208, 205, 202, 197, 192, 186, 180, 172, 165, 156, 147, 138, 129, 119, 110, 101, 92, 83, 74, 67, 59, 53, 47, 42, 37, 34, 31, 30, 30, 30, 31, 34, 37, 42, 47, 53, 59, 67, 75, 83, 92, 101, 110};
int secy[60] = {71, 71, 72, 75, 78, 83, 88, 94, 100, 108, 116, 124, 133, 142, 151, 161, 170, 179, 188, 197, 206, 213, 221, 227, 233, 238, 243, 246, 249, 250, 251, 250, 249, 246, 243, 238, 233, 227, 221, 213, 205, 197, 188, 179, 170, 161, 151, 142, 133, 124, 116, 108, 100, 94, 88, 83, 78, 75, 72, 71};


void init_tim3(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    TIM3->PSC = 48000-1;
    TIM3->ARR = 500 - 1; // 500 = 1/2 hz
    TIM3->DIER |= TIM_DIER_UIE;
    TIM3->CR1 |= TIM_CR1_CEN;
    NVIC->ISER[0] |= 1<<TIM3_IRQn;
}

void TIM3_IRQHandler(void) {
    TIM3->SR &= ~TIM_SR_UIF;
    uint32_t rtc_tr = (uint32_t)(RTC->TR & 0x007F7F7F);

    int ht = (rtc_tr >> 20 & 0x3);
    int hu = (rtc_tr >> 16 & 0xF);
    int mnt = (rtc_tr >> 12 & 0x7);
    int mnu = (rtc_tr >> 8 & 0xF);
    int st = (rtc_tr >> 4 & 0x7);
    int su = (rtc_tr & 0xF);

    oled_memory_map[3] = 0x200 | (ht + 0x30);
    oled_memory_map[4] = 0x200 | (hu + 0x30);
    oled_memory_map[6] = 0x200 | (mnt + 0x30);
    oled_memory_map[7] = 0x200 | (mnu + 0x30);
    oled_memory_map[9] = 0x200 | (st + 0x30);
    oled_memory_map[10] = 0x200 | (su + 0x30);

    oled_memory_map[18] = 0x200 | time_str[0];
    oled_memory_map[19] = 0x200 | time_str[1];
    oled_memory_map[20] = 0x200 | time_str[2];

    oled_memory_map[22] = 0x200 | time_str[4];
    oled_memory_map[23] = 0x200 | time_str[5];
    oled_memory_map[24] = 0x200 | time_str[6];

    oled_memory_map[26] = 0x200 | time_str[8];
    oled_memory_map[27] = 0x200 | time_str[9];
    oled_memory_map[28] = 0x200 | ',';
    oled_memory_map[30] = 0x200 | time_str[20];
    oled_memory_map[31] = 0x200 | time_str[21];
    oled_memory_map[32] = 0x200 | time_str[22];
    oled_memory_map[33] = 0x200 | time_str[23];


        if ((rtc_tr >> 22) & 0x1) {
            oled_memory_map[13] = 0x250; // PM
        } else {
            oled_memory_map[13] = 0x241; // AM
        }
        if (!is_alarm_on) {
            int hr = ht * 10 + hu;
            int min = mnt * 10 + mnu;
            int sec = st * 10 + su;

            LCD_DrawLine(120,161,old_hrx,old_hry,WHITE); //removes old hour hand
            LCD_DrawLine(120,161,old_minx,old_miny,WHITE); //removes old minute hand
            LCD_DrawLine(120,161,old_secx,old_secy,WHITE);
            //draws new hands
            old_hrx = hrx[hr * 6 + mnt - 6];
            old_hry = hry[hr * 6 + mnt - 6];
            old_minx = minx[min];
            old_miny = miny[min];
            old_secx = secx[sec];
            old_secy = secy[sec];
            LCD_DrawLine(120, 161, old_hrx, old_hry, BLACK); //moves hour hand
            LCD_DrawLine(120,161,old_minx, old_miny, BLUE); //moves minute hand
            LCD_DrawLine(120,161,old_secx, old_secy, RED); //moves second hand
            LCD_Circle(120,161,5,1, BLUE); //center dot. Must go on top of hands
        }
}

int main(void) {
    RCC -> AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC -> MODER &= ~(0x000c3000);
    GPIOC -> MODER |= 0x00041000;
    GPIOC -> ODR &= 0x00000000;
    GPIOC -> ODR ^= 1<<9;

    // Initialize GPS
    init_usart1();

    // Select LSI as clk; 40k Hz -> 1Hz, Async + 1 = 400, Sync + 1 = 100
    RTC_Clock_Enable();

    // Init RTC and various structs
    RTC_Init_Structs();
    RTC_Init(&rtc_init);

    // Get GPS time/date, Set time/date
    RTC_Update_DateTime(); // ***

    // Init DAC, Timers for MIDI
    init_wavetable();
    init_DAC();
    init_TIM6();
//    NVIC_SetPriority(TIM6_DAC_IRQn, 0);
//    NVIC_SetPriority(TIM2_IRQn, 0);

    // Set GPS Alarm by default
    RTC_Alarm_Enable();
    RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
    RTC_SetAlarm(RTC_Format_BCD, RTC_Alarm_A, &rtc_alarm_gps);
    RTC_ClearITPendingBit(RTC_IT_ALRA);
    RTC_ITConfig(RTC_IT_ALRA, ENABLE);
    RTC_AlarmCmd(RTC_Alarm_A, ENABLE);

    init_spi2();
    init2_oled();
    LCD_Setup();
    makeClockFace();
    init_tim3();

    init_dma1_c5();

    //BEN ADDING TO MAIN
    enable_keypad_ports();
    init_tim7();

//    int key = 0;
        int alarm_index = 20;
        int set_index = 0;
        uint8_t set_alarm_arr[7] = {0};
        int set_alarm_arr_cnt = 0;

        for(;;) {
            char key = get_keypress();
                if (key == 'A' && alarm_menu == 0){ //if normal clock is running and 'A' is pressed (set alarm case)
                    alarm_menu = 1;
                    DMA1_Channel5->CMAR = (uint32_t) alarm_memory_map;
                }
                else if(key=='A' && alarm_menu == 1){ //if alarm setting is running and 'A' is pressed (go back to clock case)
                    if(set_alarm_arr_cnt >= 7){
                        RTC_SetAlarm_Prog(set_alarm_arr);
                    }
                    //some check that set_alarm_arr is full
                    alarm_menu = 0;
                    DMA1_Channel5->CMAR = (uint32_t) oled_memory_map;

                    //send data to henry right here, or else alarm data will be lost
                    alarm_index = 20;
                    set_alarm_arr_cnt = 0;
                    set_index = 0;
                    alarm_memory_map[20] = 0x25f;
                    alarm_memory_map[21] = 0x25f;
                    alarm_memory_map[23] = 0x25f;
                    alarm_memory_map[24] = 0x25f;
                    alarm_memory_map[26] = 0x25f;
                    alarm_memory_map[27] = 0x25f;
                    alarm_memory_map[29] = 0x25f;
                    alarm_memory_map[30] = 0x24d;
                }
                else if(key >= '0' && key <= '9' && alarm_menu == 1){ //if numbers are pressed, add them to correct index in alarm MM -> still need to actually set alarm
                    if(alarm_index == 22 || alarm_index == 25){
                        alarm_index += 1;                       //this if statement increments alarm_index if it hits a colon
                    }
                    if(alarm_index == 20 && (key == '0' || key == '1')){
                        set_alarm_arr[set_index] = key - 0x30; //arr passed to henry where key - 0x30 = value to be used
                        set_index += 1;
                        alarm_memory_map[alarm_index] = key | 0x200; //char write to memory
                        alarm_index += 1;
                        set_alarm_arr_cnt++;
                    }
                    else if(alarm_index == 21){
                        if(set_alarm_arr[0] == 1){
                            //u can only write 0, 1, 2
                            if(key == '0' || key == '1' || key == '2'){
                                set_alarm_arr[set_index] = key - 0x30; //arr passed to henry where key - 0x30 = value to be used
                                set_index += 1;
                                alarm_memory_map[alarm_index] = key | 0x200; //char write to memory
                                alarm_index += 1;
                                set_alarm_arr_cnt++;
                            }
                        }
                        else{
                            //you can write 0-9
                           if(key >= '1' && key <= '9'){
                                set_alarm_arr[set_index] = key - 0x30; //arr passed to henry where key - 0x30 = value to be used
                                set_index += 1;
                                alarm_memory_map[alarm_index] = key | 0x200; //char write to memory
                                alarm_index += 1;
                                set_alarm_arr_cnt++;
                            }

                        }
                    }
                    else if(alarm_index == 23 || alarm_index == 26){ //tens place of minutes and seconds
                        if(key >= '0' && key <= '5'){
                            set_alarm_arr[set_index] = key - 0x30; //arr passed to henry where key - 0x30 = value to be used
                            set_index += 1;
                            alarm_memory_map[alarm_index] = key | 0x200; //char write to memory
                            alarm_index += 1;
                            set_alarm_arr_cnt++;
                        }
                    }
                    else if(alarm_index == 24 || alarm_index == 27){ //ones place mins/secs
                        if(key >= '0' && key <= '9'){
                            set_alarm_arr[set_index] = key - 0x30; //arr passed to henry where key - 0x30 = value to be used
                            set_index += 1;
                            alarm_memory_map[alarm_index] = key | 0x200; //char write to memory
                            alarm_index += 1;
                            set_alarm_arr_cnt++;
                        }
                    }

                }
                else if(key == 'B' && alarm_menu == 1){ //set to AM
                    alarm_memory_map[29] = 0x241;
                    alarm_memory_map[30] = 0x24d;
                    set_alarm_arr[6] = 0x00;
                    set_alarm_arr_cnt++;

                }
                else if(key == 'C' && alarm_menu == 1){ //set to PM
                    alarm_memory_map[29] = 0x250;
                    alarm_memory_map[30] = 0x24d;
                    set_alarm_arr[6] = 0x40;
                    set_alarm_arr_cnt++;
                }
                else if(key == 'D' && is_alarm_on == 1){ //is_alarm_on indicates if the alarm is actually sounding, while alarm_menu refers to the set alarm screen
                    RTC_TurnAlarmOff();
                }

                if(alarm_index > 27){
                    alarm_index = 20;
                }
            }
        //END KEYPAD TEST
}
