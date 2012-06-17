// STM32 loader

#ifndef __STM32LD_H__
#define __STM32LD_H__

#include "type.h"
#include <fcntl.h>
#include <libpcan.h>

// Global variable for the device to be used
#define CAN 2
#define USART 1

// Global variable to be assigned a value either CAN or USART
int devselection;

// Error codes
enum
{
  STM32_OK = 0,
  STM32_PORT_OPEN_ERROR,
  STM32_COMM_ERROR,
  STM32_INIT_ERROR,
  STM32_TIMEOUT_ERROR,
  STM32_NOT_INITIALIZED_ERROR
};

// Communication data
#define STM32_COMM_ACK      0x79
#define STM32_COMM_NACK     0x1F
#define STM32_COMM_TIMEOUT  2000000
#define STM32_WRITE_BUFSIZE 256

#define SER_BAUD (115200)

// Device FLASH memory data
#define STM32_FLASH_START_ADDRESS 0x08006000
#define STM32_FLASH_END_ADDRESS 0x08020000
#define STM32_FLASH_PAGES_NUM 128 //absolute number
#define STM32_FLASH_PAGES_SIZE 1024 //bytes

// Global variable for the custom FLASH base address
u32 custombaseaddress;

enum
{
  STM32_CMD_INIT = 0x7F,
  STM32_CMD_GET_COMMAND = 0x00,
  STM32_CMD_ERASE_FLASH = 0x43,
  STM32_CMD_GET_ID = 0x02,
  STM32_CMD_WRITE_FLASH = 0x31,
  STM32_CMD_WRITE_UNPROTECT = 0x73,
  STM32_CMD_READ_FLASH = 0x11,
  STM32_CMD_GO = 0x21
};

// Function types for stm32_write_flash
typedef u32 ( *p_read_data )( u8 *dst, u32 len );
typedef void ( *p_progress )( u32 wrote );

// Loader functions
int stm32_init( const char* portname, u32 baud );
int stm32_get_bl_version( u8 *major, u8 *minor ); 
int stm32_get_chip_id( u16 *version );
int stm32_write_unprotect();
int stm32_erase_flash();
int stm32_write_flash( p_read_data read_data_func, p_progress progress_func );
int stm32_jump();
u8 stm32h_CANread_byte();
void stm32h_CANwrite_byte(u8 data);
int stm32_CAN_init ();

// Utils
#define STM32_RETRY_COUNT	10

#endif

