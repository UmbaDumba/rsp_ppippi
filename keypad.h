#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>

// -------- 키패드 GPIO 관련 매크로 및 변수 --------
#define BCM2711_PERI_BASE 0xFE000000
#define GPIO_BASE (BCM2711_PERI_BASE + 0x200000)
#define BLOCK_SIZE (4*1024)

// 4x4 키패드 핀 (BCM 번호)
#define COL1 21
#define COL2 20
#define COL3 16
#define COL4 12
#define ROW1 13
#define ROW2 19
#define ROW3 26
#define ROW4 18

extern int colPins[4];
extern int rowPins[4];

extern volatile unsigned *gpio;
#define INP_GPIO(g)    *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g)    *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define GPIO_SET       *(gpio+7)
#define GPIO_CLR       *(gpio+10)
#define GET_GPIO(g)    (*(gpio+13)&(1<<g))
#define GPIO_PULL      *(gpio+37)
#define GPIO_PULLCLK0  *(gpio+38)

#define PUSHED 0
#define RELEASED 1

#define SEND        'v' // 입력한거 전송할때 사용
#define END_SIGN    'e' // 프로그램 종료신호

// 키패드 문자 매핑 (col, row)
    // {S16, S12, S8, S4},
    // {S15, S11, S7, S3},
    // {S14, S10, S6, S2},
    // {S13, S9,  S5, S1}
extern char keypadChar[4][4];

// -------- LCD 관련 --------
#define LCD_DEV "/dev/mylcd"

// -------- 입력 버퍼 및 동기화 --------
extern char input_buf[17]; // 최대 16자 + 널
extern int idx;
extern int is_send;
extern pthread_mutex_t buf_mutex;

// -------- 종료 플래그 --------
extern volatile int keepRunning;

// -------- 함수 선언 --------
void keypad_init(void);
void setup_io();
void set_pull_up(int g);
char getKeypadState(int col, int row);
char keypadScan();
void clear_keypad_str(void);
void* keypad_thread(void* arg);
void lcd_write_line1(const char* str);
void lcd_clear_line1();
void lcd_write_line2(const char* str);
void lcd_clear_line2();

#endif