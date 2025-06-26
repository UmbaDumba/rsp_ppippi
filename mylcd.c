#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/delay.h>

#define DEV_NAME "mylcd"
#define I2C_BUS_NUM    1       // I2C-1 버스 사용
#define BH1750_ADDR   0x27    // 디바이스 슬레이브 주소 <- sudo i2cdetect -y 1 로 확인

#define I2C_ENABLE  0x04     // enable bit : xxxx xExx
#define I2C_DATA    0x08    // data -> RS : 1      [x][enable][rw][rs]
#define I2C_COMMAND 0x09    // command -> RS : 0   [x][enable][rw][rs]

static struct i2c_adapter *i2c_adap;
static struct i2c_client *i2c_client;
static int major_num;


/* i2c, lcd 함수 선언 */
void i2c_send_data(uint8_t data);
void i2c_send_command(uint8_t data);

void I2C_LCD_write_string(char *string);
void I2C_LCD_goto_XY(uint8_t row, uint8_t col);
void I2C_LCD_write_string_XY(uint8_t row, uint8_t col, char *string);

void lcd_init_seq(void);



// 현재의 xy좌표에 printf처럼 스트링 값을 출력 
void I2C_LCD_write_string(char *string)
{
	uint8_t i;
	for(i=0; string[i]; i++){
        //I2C_LCD_write_data(string[i]);
        if(string[i] < 32){
            continue;
            // \n 같은 친구들 출력불가
        }
        i2c_send_data(string[i]);
    }
}

// 커서를 x,y좌표로 이동
void I2C_LCD_goto_XY(uint8_t row, uint8_t col)
{
	col %= 16;
	row %= 2;
	
	uint8_t address = (0x40 * row) + col;
	uint8_t command = 0x80 + address;
	
	//I2C_LCD_write_command(command);
    i2c_send_command(command);
}

// x,y좌표로 이동을 하고 string값을 출력 한다. 
void I2C_LCD_write_string_XY(uint8_t row, uint8_t col, char *string)
{
	I2C_LCD_goto_XY(row, col);
	I2C_LCD_write_string(string);
}


void i2c_send_data(uint8_t data){
    // data : aaaa bbbb
    // control : 0000 xxxx
    //              -> 백라이트 enable R/W RS

    uint8_t tmp[4];

    // 상위 nibble 전송 -> aaaa xxxx
    uint8_t hnibble = (data & 0xF0);
    tmp[0] = hnibble|0x0D;    //0000 1101        0000 0100 + 0000 1001   
    tmp[1] = hnibble|0x09;

    // 하위 nibble 전송 -> bbbb xxxx
    uint8_t lnibble = ((data << 4) & 0xF0);
    tmp[2] = lnibble|0x0D;
    tmp[3] = lnibble|0x09;
    
    i2c_master_send(i2c_client, tmp, 4);
}

void i2c_send_command(uint8_t data){
    // data : aaaa bbbb
    // control : 0000 xxxx
    //              -> 백라이트 enable R/W RS

    uint8_t tmp[4];

    // 상위 nibble 전송 -> aaaa xxxx
    uint8_t hnibble = (data & 0xF0);
    tmp[0] = hnibble|0x0C;
    tmp[1] = hnibble|0x08;

    // 하위 nibble 전송 -> bbbb xxxx
    uint8_t lnibble = ((data << 4) & 0xF0);
    tmp[2] = lnibble|0x0C;
    tmp[3] = lnibble|0x08;
    
    i2c_master_send(i2c_client, tmp, 4);
}

void lcd_init_seq(void){

   
    // i2c_send_command(0x33);
    // i2c_send_command(0x32);
    // i2c_send_command(0x28);
    // i2c_send_command(0x0c);
    // i2c_send_command(0x06);
    // i2c_send_command(0x01);

    i2c_send_command(0x33);
    i2c_send_command(0x32);
    i2c_send_command(0x28);
    i2c_send_command(0x08); // display off
    i2c_send_command(0x0C); // display on (커서 안보이는 모드)
    i2c_send_command(0x01);
    i2c_send_command(0x06);
    msleep(2000);
}

static ssize_t dev_write (struct file *file, const char __user *buf, size_t len, loff_t *offset){
    
    char kbuf[19] = {0, };

    if (copy_from_user(kbuf, buf, len+1)) return -EFAULT;

    // 문자열 뒷부분은 " " 으로 채우기
    for(int i = strlen(kbuf); i<=16; i++){
        kbuf[i] = ' ';
    }
    kbuf[16] = 0;

    if(kbuf[0] == '0')          I2C_LCD_write_string_XY(0, 0, kbuf + 1);
    else if(kbuf[0] == '1')     I2C_LCD_write_string_XY(1, 0, kbuf + 1);
    else{
        i2c_send_command(0x01); // 디스플레이 clear
        I2C_LCD_write_string_XY(0, 0, "bbi bbi!!!!");
    }
    

    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = dev_write,
};

static int __init lcd_init(void)
{
    struct i2c_board_info board_info = {
        I2C_BOARD_INFO("lcd", BH1750_ADDR)
    };

    // 1. I2C 어댑터 획득
    i2c_adap = i2c_get_adapter(I2C_BUS_NUM);
    if (!i2c_adap) {
        pr_err("I2C adapter not found\n");
        return -ENODEV;
    }

    // 2. I2C 클라이언트 생성
    i2c_client = i2c_new_client_device(i2c_adap, &board_info);
    if (!i2c_client) {
        pr_err("Device registration failed\n");
        i2c_put_adapter(i2c_adap);
        return -ENODEV;
    }

    // 3. major 번호 받아오기
    major_num = register_chrdev(0, DEV_NAME, &fops);
    if (major_num < 0) {
        pr_err("Device registration failed\n");
        return major_num;
    }
    pr_info("Major number: %d\n", major_num);

    // 4. 초기화
    msleep(15);

    lcd_init_seq();

    msleep(1000);
    
    pr_info("lcd initialized\n");

    I2C_LCD_write_string_XY(0,0,"goooood");

    

    return 0;
}

static void __exit bh1750_exit(void)
{
    i2c_unregister_device(i2c_client);
    i2c_put_adapter(i2c_adap);
    pr_info("lcd removed\n");
}

module_init(lcd_init);
module_exit(bh1750_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("BH1750 I2C Driver");
