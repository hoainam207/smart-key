#include "STM8S.h"
#include "stm8s103_serial.h"
#include "stm8s_i2c.h"
#include "stm8s_gpio.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h> 
bool isPasswordSet = false;
bool shouldEraseEEPROM = false;
#define MAX_PASSWORD_LENGTH 6
#define EMERGENCY_PASSWORD "111111" 
#define EEPROM_ADDRESS 0xA0 // EEPROM AT24C256 address (A2, A1, A0 are all set to logic LOW)
#define EEPROM_SIZE 32768
#define MAX_ID_LENGTH 8 // S? lu?ng ký t? t?i da cho ID
char id[MAX_ID_LENGTH + 1]; // Buffer d? luu ID
bool idEntered = false; // Bi?n dánh d?u ID dã du?c nh?p
uint8_t firstPasswordByte;
uint8_t I2C_ReadByte(uint16_t address);
void eraseEEPROM(uint16_t startAddress, uint16_t endAddress);
void I2C_Configuration(void);
void I2C_WriteByte(uint16_t address, uint8_t data);
void delay_ms(int ms);
void readPasswordFromEEPROM(uint16_t address, char *password);
void writePasswordToEEPROM(uint16_t address, const char *password);
char password[MAX_PASSWORD_LENGTH + 1] = "111111";
char input[MAX_PASSWORD_LENGTH + 1]; // Buffer to store the input password
int passwordLength = 6;
int i = 0;

 void generateRandomPassword(char *password, int length) {
   int carry = 2;
    int i;
    for (i = passwordLength - 1; i >= 0; i--) {
        int digit = password[i] - '0' + carry;
        carry = digit / 10;
        password[i] = (digit % 10) + '0';
    }
  }
void delay_ms (int ms) //Function Definition 
{
int i =0 ;
int j=0;
for (i=0; i<=ms; i++)
{
for (j=0; j<120; j++) // Nop = Fosc/4
_asm("nop"); //Perform no operation //assembly code <span style="white-space:pre"> </span>
}
}
void I2C_Configuration(void) {
    // Configure I2C pins (PB4 as SDA, PB5 as SCL)
    GPIO_Init(GPIOB, GPIO_PIN_4, GPIO_MODE_OUT_OD_HIZ_FAST);
    GPIO_Init(GPIOB, GPIO_PIN_5, GPIO_MODE_OUT_OD_HIZ_FAST);
    GPIO_Init(GPIOD, GPIO_PIN_2, GPIO_MODE_OUT_PP_LOW_FAST);
    // I2C clock frequency configuration (16MHz clock, I2C running at 100kHz)
    I2C_DeInit();
    CLK_PeripheralClockConfig(CLK_PERIPHERAL_I2C, ENABLE);
    I2C_Init(100000, EEPROM_ADDRESS, I2C_DUTYCYCLE_2, I2C_ACK_CURR, I2C_ADDMODE_7BIT, 16);
    I2C_Cmd(ENABLE);
}
void TIM2_Configuration(void) {
    TIM2_DeInit();
    TIM2_TimeBaseInit(TIM2_PRESCALER_256, 255); // Ð?t chu k? cho TIM2 (d?m t? 0 d?n 255)
    TIM2_ITConfig(TIM2_IT_UPDATE, ENABLE); // Cho phép ng?t khi d?m d?t d?n giá tr? cài d?t
    TIM2_Cmd(ENABLE);
}
void I2C_WriteByte(uint16_t address, uint8_t data) {
    // Wait until I2C bus is not busy
    while (I2C_GetFlagStatus(I2C_FLAG_BUSBUSY));

    // Generate start condition
    I2C_GenerateSTART(ENABLE);

    // Wait until start condition is generated
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT));

    // Send EEPROM address and write bit
    I2C_Send7bitAddress(EEPROM_ADDRESS, I2C_DIRECTION_TX);

    // Wait until address is sent
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    // Send the high byte of the memory address
    I2C_SendData((uint8_t)(address >> 8));

    // Wait until data is sent
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // Send the low byte of the memory address
    I2C_SendData((uint8_t)address);

    // Wait until data is sent
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // Send the data to be written
    I2C_SendData(data);

    // Wait until data is sent
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // Generate stop condition
    I2C_GenerateSTOP(ENABLE);
}

uint8_t I2C_ReadByte(uint16_t address) {
    uint8_t data;

    // Wait until I2C bus is not busy
    while (I2C_GetFlagStatus(I2C_FLAG_BUSBUSY));

    // Generate start condition
    I2C_GenerateSTART(ENABLE);

    // Wait until start condition is generated
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT));

    // Send EEPROM address and write bit
    I2C_Send7bitAddress(EEPROM_ADDRESS, I2C_DIRECTION_TX);

    // Wait until address is sent
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
		// Send the high byte of the memory address
    I2C_SendData((uint8_t)(address >> 8));

    // Wait until data is sent
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // Send the low byte of the memory address
    I2C_SendData((uint8_t)address);

    // Wait until data is sent
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // Generate repeated start condition
    I2C_GenerateSTART(ENABLE);

    // Wait until repeated start condition is generated
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT));

    // Send EEPROM address and read bit
    I2C_Send7bitAddress(EEPROM_ADDRESS, I2C_DIRECTION_RX);

    // Wait until address is sent
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    // Disable ACK
    I2C_AcknowledgeConfig(I2C_ACK_NONE);

    // Generate stop condition
    I2C_GenerateSTOP(ENABLE);

    // Wait for data to be received
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_RECEIVED));

    // Read the data from the I2C data register
    data = I2C_ReceiveData();

    // Re-enable ACK
    I2C_AcknowledgeConfig(I2C_ACK_CURR);

    return data;
}
void writePasswordToEEPROM(uint16_t address, const char *password) {
    int i;
    for (i = 0; i < MAX_PASSWORD_LENGTH; i++) {
        I2C_WriteByte(address + i, password[i]);
        delay_ms(5);
    }
}
void readPasswordFromEEPROM(uint16_t address, char *password) {
  int i;
    uint8_t data;
    for (i = 0; i < MAX_PASSWORD_LENGTH; i++) {
        data = I2C_ReadByte(address + i);
        password[i] = (char)data;
    }
    password[MAX_PASSWORD_LENGTH] = '\0'; // Null-terminate the string
                    Serial_newline();
}
void eraseEEPROM(uint16_t startAddress, uint16_t endAddress) {
    uint16_t address;
    for (address = startAddress; address <= endAddress; address++) {
        I2C_WriteByte(address, 0xFF);
        delay_ms(5); // Delay to ensure proper EEPROM write
    }
    Serial_print_string("Da xoa du lieu.");
    Serial_newline();
}
int main() {
      uint16_t passwordAddress = 0x0010;
    Serial_begin(9600);
    I2C_Configuration();
  firstPasswordByte = I2C_ReadByte(passwordAddress);
    if (firstPasswordByte == 0xFF) {
        // First password is not set, set it to "111111"
        writePasswordToEEPROM(passwordAddress, EMERGENCY_PASSWORD);
        // Wait a short time to ensure EEPROM write completes
        delay_ms(10); // You can adjust the delay time if needed
    } else {
        // Read the existing password from EEPROM
        readPasswordFromEEPROM(passwordAddress, password);
    }
        Serial_print_string("PASSWORD: ");
    while (1) {
        if (Serial_available()) {
            char ch = Serial_read_char();
            Serial_print_char(ch); // In '*' d? ?n m?t kh?u dã nh?p
            if (ch == '\n') {
                Serial_newline();
                input[i] = '\0'; // K?t thúc chu?i nh?p
                if (i == MAX_PASSWORD_LENGTH && strncmp(input, password, MAX_PASSWORD_LENGTH) == 0) {
                    Serial_print_string("Dang nhap thanh cong");
                    generateRandomPassword(password, sizeof(password) - 1);
                    Serial_print_string("Mat khau moi: ");
                    Serial_print_string(password);
                    Serial_newline();
                    writePasswordToEEPROM(passwordAddress, password);
                    Serial_newline();
                    Serial_print_string("chot cua da mo");
          GPIO_WriteHigh(GPIOD, GPIO_PIN_2);
         delay_ms(2000);
                    Serial_newline();
                    Serial_print_string("chot cua da dong");
                  GPIO_WriteLow(GPIOD, GPIO_PIN_2);
                    Serial_newline();
                    
                } else {
                    Serial_newline();
                    Serial_print_string("Sai mat khau");
                      GPIO_WriteLow(GPIOD, GPIO_PIN_2);
                    delay_ms(2000);
                }                
                Serial_newline();
                Serial_print_string("Nhap mat khau: ");
								i = 0;
            } 
             else if (ch == 'k') {
                // Khi nh?n ký t? 'k', d?t c? d? b?t d?u xóa EEPROM
                shouldEraseEEPROM = true;
                Serial_print_string("Da nhan 'k', se xoa EEPROM sau khi nhap 'r'.");
                Serial_newline();
            } else if (ch == 'r') {
                // Khi nh?n ký t? 'r' và c? du?c d?t, th?c hi?n xóa EEPROM
                if (shouldEraseEEPROM) {
                    eraseEEPROM(0x0000, 0x7FFF);
                    shouldEraseEEPROM = false;
                    Serial_print_string("Da xoa toan bo du lieu EEPROM.");
                    Serial_newline();
                  }
                }
                    else {
                input[i] = ch;
                i++;
                
                if (i >= sizeof(input)) {
                    i = 0;
                }
            }
           
        }
}
}
INTERRUPT_HANDLER(TIM2_UPD_OVF_IRQHandler, 13) {
    TIM2_ClearFlag(TIM2_FLAG_UPDATE); // Xóa c? ng?t
    if (shouldEraseEEPROM) {
        static uint16_t currentAddress = 0x0000;
        if (currentAddress <= 0x7FFF) {
            I2C_WriteByte(currentAddress, 0xFF); // G?i l?nh xóa cho byte hi?n t?i
            currentAddress++;
        } else {
            shouldEraseEEPROM = false; // Khi dã xóa h?t EEPROM, t?t c? xóa
            Serial_print_string("Da xoa toan bo du lieu EEPROM.");
            Serial_newline();
        }
    }
}