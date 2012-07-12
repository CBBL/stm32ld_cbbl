// Loader driver

/*
 * Uses PEAK System PCAN-USB IPEH-002021 and its library
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
  int wantwrite = 0;  //not writing unless specified
  int wantread = 0;   //not reading unless specified
  int wanterase = 1;  //erasing unless specified not to do so
  int argind = 0;
 
  printf("\n==========================");
  printf("\n  CBBL host side loader   ");
  printf("\n--------------------------");
  printf("\nZavatta Marco, Yin Zhining");
  printf("\nPolimi  2011/2012");
  printf("\n==========================");
  printf("\n");

  /*
  printf("Number of arguments argc = %d \n",argc);
  printf("First argument: %s\n",argv[0]);
  */

  /******************************************** Argument handling *************************************/
  // Case of no arguments
  if (argc==1) {
	  fprintf( stderr, "Use -help for details.\n\n");
	  exit(1);
  }

  // Help argument handler
  if (strcmp(argv[1],"-help")==0)
  {
	fprintf( stderr, "Program usage:./stm32ld_cbbl {-usart,-can} {device path e.g. /dev/ttyUSB0}"
			" [-write, firmware file] [-read, download file] [-noerase] {-defaultbaseaddr,(-custombaseaddr, value)}\n"
			"arguments marked with {} are mandatory unless going for -help\n"
			"arguments marked with [] are optional\n"
			"order of the first two arguments should be respected\n"
			"examples:\n"
			"stm32ld_cbbl -usart /dev/ttyUSB0 -write firmwaretowrite.bin -defaultbaseaddr\n"
			"stm32ld_cbbl -can /dev/pcanusb0 -custombaseaddr 0x08007000 -read readflashmemory.bin -write firmwaretowrite.bin\n"
			"switches description:\n"
			"-write	write specified file into Flash memory from given address\n"
			"-read	read Flash memory into specified file from given address\n"
		    " neither -write nor -read	jump to specified memory address and execute\n"
			"-defaultbaseaddr use hard-coded base address 0x0800 6000 as the first\n"
		    "\tFlash address where the write/read/jump operations will begin\n"
			"-custombaseaddr use the specified value as the base address\n"
		    "\tvalue must be in the format 0xY\n"
		    "-noerase do not erase the Flash memory"
			"\n\n" );
	exit( 1 );
	}

  // Communication peripheral selection
  if (strcmp(argv[1],"-usart")==0)  devselection = 1;
  else if (strcmp(argv[1],"-can")==0) devselection = 2;
  else {
	  fprintf( stderr, "host: cannot interpret device selection parameter\n\n" );
	  exit(1);
  }

  // FLASH base address selection
  int found = 0;
  while (argind<argc) {
	  if (strcmp(argv[argind],"-custombaseaddr")==0 || strcmp(argv[argind],"-defaultbaseaddr")==0) {
		  found = 1;
		  if (strcmp(argv[argind],"-custombaseaddr") == 0) {
		  	  custombaseaddress = strtoul( argv[argind+1], NULL, 0);
		  	  printf("host: custom base address selected: %x\n", custombaseaddress);
		    }
		    else if (strcmp(argv[argind],"-defaultbaseaddr") == 0) {
		  	  custombaseaddress = STM32_FLASH_START_ADDRESS;
		  	  printf("host: default base address selected: %x\n", custombaseaddress);
		    }
	  }
	  argind++;
  }
  if (found==0) {
	  fprintf( stderr, "host: cannot interpret address parameters\n\n" );
	  exit(1);
  }


  /*
  if( ( errno == ERANGE && ( baud == LONG_MAX || baud == LONG_MIN ) ) || ( errno != 0 && baud == 0 ) || ( baud < 0 ) ) 
  {
    fprintf( stderr, "Invalid baud '%s'\n", argv[ 2 ] );
    exit( 1 );
  }
  */

  // Want to write?
  argind=0;
  while (argind<argc) {
	  if (strcmp(argv[argind],"-write")==0) {
		  wantwrite=1;
		  break;
	  }
  	  argind++;
  }
  // If yes, open firmware file to be written
  if (wantwrite) {
	  printf("host: write selected\n");
	  if( ( fp = fopen(argv[argind+1], "rb" ) ) == NULL )
	  {
		fprintf( stderr, "Unable to open ");
		fprintf( stderr, argv[argind+1]);
		fprintf( stderr, " file\n");
		exit( 1 );
	  }
	  else
	  {
		printf("host: firmware file %s opened successfully\n", argv[argind+1]);
		fseek( fp, 0, SEEK_END );
		fpsize = ftell( fp );
		fseek( fp, 0, SEEK_SET );
	  }
  }
  
  // Want to read?
  argind=0;
  while (argind<argc) {
  	if (strcmp(argv[argind],"-read")==0) {
  		wantread=1;
  	 	break;
  	}
    argind++;
  }
  // If yes, open destination file for data from memory
  // file will be created and opened in write mode if non existing (wb option)
  if (wantread) {
	  printf("host: read selected\n");
	  if( ( fflash = fopen(argv[argind+1], "wb" ) ) == NULL )
	  {
		fprintf( stderr, "Unable to open ");
		fprintf( stderr, argv[argind+1]);
		fprintf( stderr, " file\n");
		exit( 1 );
	  }
	  else
	  {
		printf("host: flash memory download file %s opened successfully\n",argv[argind+1]);
		fseek( fflash, 0, SEEK_END );
		fflashsize = ftell( fflash );
		fseek( fflash, 0, SEEK_SET );
	  }
  }

  // Want to erase?
  argind=0;
  while (argind<argc) {
  	 if (strcmp(argv[argind],"-noerase")==0) {
  	    wanterase=0;
  		break;
  	  }
      argind++;
  }
  if (wanterase)
	  printf("host: erase selected\n");
  else printf("host: erase deactivated\n");


  /******************************************** Loader workflow *************************************/
  // Connect to bootloader
  printf( "host: Initializing communication with the device\n");
  if( stm32_init(argv[2], (u32)SER_BAUD ) != STM32_OK )
  {
    fprintf( stderr, "host: Unable to connect to bootloader\n\n" );
    exit( 1 );
  }
    else printf("host: init succeded\n");


  // Get version
  if( stm32_get_version( &major, &minor ) != STM32_OK )
  {
    fprintf( stderr, "host: Unable to get bootloader version\n\n" );
    exit( 1 );
  }
  else
  {
    printf( "host: Found bootloader version: %d.%d\n", major, minor );
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
    fprintf( stderr, "host:Unable to get chip ID\n\n" );
    exit( 1 );
  }
  else
  {
    printf( "host: Chip ID: %04X\n", version );
    /*
    if( version != CHIP_ID )
    {
      fprintf( stderr, "\nhost: Unsupported chip ID" );
      exit( 1 );
    }
    */
  }

  // Write unprotect
  if (wantread || wantwrite) {
	  if( stm32_write_unprotect() != STM32_OK )
	  {
		fprintf( stderr, ":host: Unable to execute write unprotect\n\n" );
		exit( 1 );
	  }
	  else
		printf( "host: Cleared write protection.\n\n" );
  }

  // Erase flash
  if (wantwrite && wanterase) {
	  if( stm32_erase_flash() != STM32_OK )
	  {
		fprintf( stderr, "Unable to erase chip\n\n" );
		exit( 1 );
	  }
	  else
		printf( "host: Erased FLASH memory.\n" );
  }

  // Program flash
  if (wantwrite) {
	  setbuf( stdout, NULL );
	  printf( "host: Programming flash ... \n ");
	  if( stm32_write_flash( writeh_read_data, writeh_progress ) != STM32_OK )
	  {
		fprintf( stderr, "Unable to program FLASH memory.\n\n" );
		exit( 1 );
	  }
	  else {
		printf( "host: write memory successfully completed.\n" );
	  	fclose( fp );
	  }
  }

  // Read flash
  if (wantread) {
	  printf( "host: Reading flash ... \n");
	  if( stm32_read_flash(fflash) != STM32_OK )
	  {
		fprintf( stderr, "Unable to read FLASH memory.\n\n" );
		fclose( fflash );
		exit( 1 );
	  }
	  else {
		fseek( fflash, 0, SEEK_END );
		fflashsize = ftell( fflash );
		fseek( fflash, 0, SEEK_SET );
		printf( "\nhost: FLASH memory successfully read (%d bytes).\n",fflashsize);
		fclose( fflash );
	  }
  }

  // Jump to app
  printf( "host: Jumping to app...\n");
  stm32_jump();

  printf( "\nhost: Done!\n\n");
  return 0;

}

/*******************************************************************************************/
           
