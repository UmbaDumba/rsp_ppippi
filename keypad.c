#include "keypad.h"

volatile unsigned *gpio = NULL;

int colPins[4] = {COL1, COL2, COL3, COL4};
int rowPins[4] = {ROW1, ROW2, ROW3, ROW4};

// 키패드 문자 매핑 (col, row)
    // {S16, S12, S8, S4},
    // {S15, S11, S7, S3},
    // {S14, S10, S6, S2},
    // {S13, S9,  S5, S1}
char keypadChar[4][4] = {
    {SEND, '0', ' ', ' '},
    {' ', '9', '6', '3'},
    {' ', '8', '5', '2'},
    {END_SIGN, '7', '4', '1'}
};

static uint8_t prevState[4][4] = {
    {RELEASED, RELEASED, RELEASED, RELEASED},
    {RELEASED, RELEASED, RELEASED, RELEASED},
    {RELEASED, RELEASED, RELEASED, RELEASED},
    {RELEASED, RELEASED, RELEASED, RELEASED}
};

// -------- 입력 버퍼 및 동기화 --------
char input_buf[17] = {0}; // 최대 16자 + 널
int idx = 0;
int is_send = 0;          // 0 : 입력중   1 : 전송
pthread_mutex_t buf_mutex = PTHREAD_MUTEX_INITIALIZER;

// -------- 종료 플래그 --------
volatile int keepRunning = 1;

// -------- Ctrl+C 핸들러 --------
void signalHandler(int sig) {
    keepRunning = 0;
}

// -------- keypad GPIO pin init 함수 -------
void keypad_init(void){
    signal(SIGINT, signalHandler);

    // GPIO/키패드 초기화
    setup_io();
    for (int i = 0; i < 4; i++) {
        INP_GPIO(colPins[i]);
        OUT_GPIO(colPins[i]);
        GPIO_SET = 1 << colPins[i];
    }
    for (int i = 0; i < 4; i++) {
        INP_GPIO(rowPins[i]);
        set_pull_up(rowPins[i]);
    }

    // LCD 2번째 줄 클리어
    lcd_clear_line2();
}

// -------- GPIO/키패드 함수 구현 --------
void setup_io() {
    int mem_fd;
    void *gpio_map;
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0) exit(1);
    gpio_map = mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, GPIO_BASE);
    close(mem_fd);
    if (gpio_map == MAP_FAILED) exit(1);
    gpio = (volatile unsigned *)gpio_map;
}

void set_pull_up(int g) {
    GPIO_PULL = 0x02; // PULL UP
    usleep(500);
    GPIO_PULLCLK0 = (1 << g);
    usleep(500);
    GPIO_PULL = 0;
    GPIO_PULLCLK0 = 0;
    usleep(200);
}

char getKeypadState(int col, int row) {
    char key = 0;
    // 1. 해당 컬럼만 LOW, 나머지는 HIGH
    for (int i = 0; i < 4; i++) {
        if (i == col) GPIO_CLR = 1 << colPins[i];
        else GPIO_SET = 1 << colPins[i];
    }
    usleep(50); // 신호 안정화

    uint8_t curState = (GET_GPIO(rowPins[row]) ? RELEASED : PUSHED); // 풀업 기준

    // 2. 상태 변화 감지 (누름 → 뗌)
    if (curState == RELEASED && prevState[col][row] == PUSHED) {
        key = keypadChar[col][row];
    }
    prevState[col][row] = curState;

    // 3. 컬럼 다시 HIGH
    GPIO_SET = 1 << colPins[col];

    return key;
}

char keypadScan() {
    char data = 0;
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            data = getKeypadState(col, row);
            if (data != 0) return data;
        }
    }
    return 0;
}

// -------- LCD 출력 함수 --------
void lcd_write_line1(const char* str) {
    int fd = open(LCD_DEV, O_WRONLY);
    if (fd < 0) return;
    char buffer[18] = {0};
    buffer[0] = '0'; // 2번째 줄
    strncpy(buffer+1, str, 16);
    write(fd, buffer, strlen(buffer));
    close(fd);
}


void lcd_clear_line1() {
    int fd = open(LCD_DEV, O_WRONLY);
    if (fd < 0) return;
    char buffer[18] = "0 ";
    write(fd, buffer, 17);
    close(fd);
}

void lcd_write_line2(const char* str) {
    int fd = open(LCD_DEV, O_WRONLY);
    if (fd < 0) return;
    char buffer[18] = {0};
    buffer[0] = '1'; // 2번째 줄
    strncpy(buffer+1, str, 16);
    write(fd, buffer, strlen(buffer));
    close(fd);
}

void lcd_clear_line2() {
    int fd = open(LCD_DEV, O_WRONLY);
    if (fd < 0) return;
    char buffer[18] = "1 ";
    write(fd, buffer, 17);
    close(fd);
}

// -------- 키패드 입력 문자열 clear 함수 ------
void clear_keypad_str(void){
    memset(input_buf, 0, sizeof(input_buf));
    idx = 0;
    is_send = 0;
    lcd_clear_line2();
}

// -------- 키패드 입력 스레드 --------
void* keypad_thread(void* arg) {
    while (keepRunning) {
        char key = keypadScan();
        if (key) {
            pthread_mutex_lock(&buf_mutex);

            // 클리어 조건: '+' 또는 16자 초과
            if (key == SEND || idx >= 16) { 
                is_send = 1;
            } else if (key >= '0' && key <= '9') {
                if (idx < 16) {
                    input_buf[idx++] = key;
                    input_buf[idx] = '\0';
                    lcd_write_line2(input_buf);
                }
            }else if(key == END_SIGN){
                keepRunning = 0;
            }
            

            pthread_mutex_unlock(&buf_mutex);
            usleep(200000); // 디바운스
        }
        usleep(10000);
    }
    return NULL;
}

