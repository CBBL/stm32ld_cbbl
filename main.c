// Loader driver

/*
 * Uses PEAK System PCAN-USB IPEH-002021 and its library
 */

/*
 * Usage:
 * stm32ld [-usart,-can] [device path e.g. /dev/ttyUSB0] [-defaultbaseaddr,[-custombaseaddr, value]] [firmware file] [download file]
 * argument order must be respected
 * firmware file must be present in the file system
 * download file will be created in non-existent or overwritten
 *
 * Example:
 * stm32ld -usart /dev/ttyUSB0 -custombaseaddr 0x08006000 firmware.bin flashmemory.bin
 * stm32ld -can /dev/pcanusb0 -defaultbaseaddr firmware.bin flashmemory.bin
 */

#include "stm32ld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>

static FILE *fp;
static FILE *fflash;
static u32 fpsize;
static u32 fflashsize;

#define BL_VERSION_MAJOR  2
#define BL_VERSION_MINOR  1
#define BL_MKVER( major, minor )    ( ( major ) * 256 + ( minor ) ) 
#define BL_MINVERSION               BL_MKVER( BL_VERSION_MAJOR, BL_VERSION_MINOR )

#define CHIP_ID           0x0414


// ****************************************************************************
// Helper functions and macros

// Get data function
static u32 writeh_read_data( u8 *dst, u32 len )
{
  size_t readbytes = 0;

  if( !feof( fp ) )
    readbytes = fread( dst, 1, len, fp );
  return ( u32 )readbytes;
}

// Progress function
static void writeh_progress( u32 wrote )
{
  unsigned pwrite = ( wrote * 100 ) / fpsize;
  static int expected_next = 10;

  if( pwrite >= expected_next )
  {
    printf( "\n\thost: progress %d%% ", expected_next);
    expected_next += 10;
  }
}

// ****************************************************************************
// Entry point

int main( int argc, const char **argv )
{
  u8 minor, major;
  u16 version;
  long baud  = 115200;
 
  printf("\n==========================");
  printf("\n  CBBL host side loader   ");
  printf("\n--------------------------");
  printf("\nZavatta Marco, Yin Zhining");
  printf("\nPolimi  2011/2012");
  printf("\n==========================");
  printf("\n");

  printf( "\nRequires input: firmware.bin, manual specification of FLASH base address\n for the program to be loaded");
  printf( "\nProduces output: flashmemory.bin" );
  printf( "\n");

  printf("argc = %d \n",argc);
  printf("%s\n",argv[0]);

  // Argument validation
  if( argc != 5 && argc != 6 )
  {
    fprintf( stderr, "usage\nstm32ld [-usart,-can] [device path e.g. /dev/ttyUSB0]"
    		" [firmware file] [download file] [-defaultbaseaddr,[-custombaseaddr, value]]\n" );
    exit( 1 );
  }



  errno = 0;
  //baud = strtol( argv[ 2 ], NULL, 10 );

  // Communication peripheral selection
  devselection = strtol( argv[0], NULL, 10);
  printf("host: devselection: %d", devselection);


  // FLASH base address
  if (argc == 6 && strcmp(argv[4],"-custombaseaddr") == 0) {
	  custombaseaddress = strtol( argv[5], NULL, 10);
	  printf("host: custom base address selected: %x", custombaseaddress);
  }
  else if (argc == 6 && strcmp(argv[5],"-defaultbaseaddr") == 0) {
	  custombaseaddress = STM32_FLASH_START_ADDRESS;
	  printf("host: default base address selected: %x", custombaseaddress);
  }
  else {
	  fprintf( stderr, "Cannot interpret address parameters\n" );
	  exit(1);
  }
  /*
  if( ( errno == ERANGE && ( baud == LONG_MAX || baud == LONG_MIN ) ) || ( errno != 0 && baud == 0 ) || ( baud < 0 ) ) 
  {
    fprintf( stderr, "Invalid baud '%s'\n", argv[ 2 ] );
    exit( 1 );
  }
  */

  //open firmware to be loaded
  if( ( fp = fopen(argv[2], "rb" ) ) == NULL )
  {
	fprintf( stderr, "Unable to open ");
	fprintf( stderr, argv[2]);
	fprintf( stderr, " file\n");
    exit( 1 );
  }
  else
  {
    fseek( fp, 0, SEEK_END );
    fpsize = ftell( fp );
    fseek( fp, 0, SEEK_SET );
  }
  
  // open file for read memory
  // file will be created and opened in write mode if non existing (wb option)
  if( ( fflash = fopen(argv[3], "wb" ) ) == NULL )
  {
    fprintf( stderr, "Unable to open ");
    fprintf( stderr, argv[3]);
    fprintf( stderr, " file\n");
    exit( 1 );
  }
  else
  {
    fseek( fflash, 0, SEEK_END );
    fflashsize = ftell( fp );
    fseek( fflash, 0, SEEK_SET );
  }


  // Ask communication peripheral to use
  /*
  printf("\nhost: Which device do you wanna use (1 USART, 2 CAN) ?");
  printf("\n");
  scanf("%d", &devselection);
  printf("host: devselection: %d", devselection);
  */

  // Connect to bootloader
  // Use /dev/ttyUSB0
  printf( "\nhost: Initializing communication with the device");
  if( stm32_init(argv[1], baud ) != STM32_OK )
  {
    fprintf( stderr, "\nhost: Unable to connect to bootloader" );
    exit( 1 );
  }
    else printf("\nhost: init succeded");

  
  // Get version
  if( stm32_get_version( &major, &minor ) != STM32_OK )
  {
    fprintf( stderr, "host: Unable to get bootloader version" );
    exit( 1 );
  }
  else
  {
    printf( "\nhost: Found bootloader version: %d.%d", major, minor );
    /*
    if( BL_MKVER( major, minor ) < BL_MINVERSION )
    {
      fprintf( stderr, "\n:Unsupported bootloader version" );
      exit( 1 );
    }
    */
  }
  
  // Get chip ID
  if( stm32_get_chip_id( &version ) != STM32_OK )
  {
    fprintf( stderr, "\nhost:Unable to get chip ID" );
    exit( 1 );
  }
  else
  {
    printf( "\nhost: Chip ID: %04X", version );
    /*
    if( version != CHIP_ID )
    {
      fprintf( stderr, "\nhost: Unsupported chip ID" );
      exit( 1 );
    }
    */
  }

  // Write unprotect
  if( stm32_write_unprotect() != STM32_OK )
  {
    fprintf( stderr, "\n:host: Unable to execute write unprotect\n" );
    exit( 1 );
  }
  else
    printf( "\nhost: Cleared write protection." );

  // Erase flash
  if( stm32_erase_flash() != STM32_OK )
  {
    fprintf( stderr, "\nUnable to erase chip" );
    exit( 1 );
  }
  else
    printf( "\nhost: Erased FLASH memory.\n" );

  // Program flash
  setbuf( stdout, NULL );
  printf( "host: Programming flash ... ");
  if( stm32_write_flash( writeh_read_data, writeh_progress ) != STM32_OK )
  {
    fprintf( stderr, "Unable to program FLASH memory.\n" );
    exit( 1 );
  }
  else
    printf( "\nhost: write memory successfully completed." );


  // Read flash
  printf( "host: Reading flash ... ");
  if( stm32_read_flash(fflash) != STM32_OK )
  {
    fprintf( stderr, "Unable to read FLASH memory.\n" );
    fclose( fflash );
    exit( 1 );
  }
  else {
    fseek( fflash, 0, SEEK_END );
    fflashsize = ftell( fflash );
    fseek( fflash, 0, SEEK_SET );
    printf( "\nhost: FLASH memory successfully read (%d bytes). Binary output in flashmemory.bin",fflashsize);
  }


  // Close files
  fclose( fflash );
  fclose( fp );

  // Jump to app
  printf( "\nhost: Jumping to app... ");
  stm32_jump();

  printf( "\n\nhost: Done!");
  return 0;

}
           
