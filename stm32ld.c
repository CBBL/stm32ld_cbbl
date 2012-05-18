// STM32 bootloader client

#include <stdio.h>
#include "serial.h"
#include "type.h"
#include "stm32ld.h"

static ser_handler stm32_ser_id = ( ser_handler )-1;

#define STM32_RETRY_COUNT             10

// ****************************************************************************
// Helper functions and macros

// Check initialization
#define STM32_CHECK_INIT\
  if( stm32_ser_id == ( ser_handler )-1 )\
    return STM32_NOT_INITIALIZED_ERROR

// Check received byte
#define STM32_EXPECT( expected )\
  if(stm32h_read_byte() != expected )\
    return STM32_COMM_ERROR;

#define STM32_READ_AND_CHECK( x )\
  if( ( x = stm32h_read_byte() ) == -1 )\
    return STM32_COMM_ERROR;

// Helper: send a command to the STM32 chip
static int stm32h_send_command( u8 cmd )
{
  ser_write_byte( stm32_ser_id, cmd );
  ser_write_byte( stm32_ser_id, ~cmd );
}

// Helper: read a byte from STM32 with timeout
static int stm32h_read_byte()
{
  return ser_read_byte( stm32_ser_id );
}

// Helper: append a checksum to a packet and send it
static int stm32h_send_packet_with_checksum( u8 *packet, u32 len )
{
  u8 chksum = 0;
  u32 i, res;

  for( i = 0; i < len; i ++ )
    chksum ^= packet[ i ];
  if (len==4) printf("\n\t\thost: actual packet (N, N+1) length: %d, data: %x %x %x %x", len, *packet, *(packet+1), *(packet+2), *(packet+3));
  else printf("\n\t\thost: actual packet (N, N+1) length: %d, data: %x %x %x %x %x ...", len, *packet, *(packet+1), *(packet+2), *(packet+3), *(packet+4));
  res = ser_write( stm32_ser_id, packet, len );
  printf("\n\t\thost: bytes actually sent: %d", res);
  printf("\n\t\thost: checksum: %x", chksum);
  res = ser_write_byte( stm32_ser_id, chksum );
  printf("\n\t\thost: bytes actually sent: %d", res);
  return STM32_OK;
}

// Helper: send an address to STM32
static int stm32h_send_address( u32 address )
{
  u8 addr_buf[ 4 ];

  addr_buf[ 0 ] = address >> 24;
  addr_buf[ 1 ] = ( address >> 16 ) & 0xFF;
  addr_buf[ 2 ] = ( address >> 8 ) & 0xFF;
  addr_buf[ 3 ] = address & 0xFF;
  return stm32h_send_packet_with_checksum( addr_buf, 4 );
}

// Helper: intiate BL communication
static int stm32h_connect_to_bl()
{
  int res;
  int log;

  // Flush all incoming data
  ser_set_timeout_ms( stm32_ser_id, SER_NO_TIMEOUT );
  while( stm32h_read_byte() != -1 );
  ser_set_timeout_ms( stm32_ser_id, STM32_COMM_TIMEOUT );

  // Initiate communication
  ser_write_byte( stm32_ser_id, STM32_CMD_INIT );
  printf("\nhost: init byte sent");
  res = stm32h_read_byte();
  while( (log = stm32h_read_byte()) != -1 )
     printf("%c", log);
  //printf("\n");
  //printf("res: %c", res);
  //return res == STM32_COMM_ACK || res == STM32_COMM_NACK ? STM32_OK : STM32_INIT_ERROR;
  if (res == STM32_COMM_ACK) {
	  printf("\nhost: init succeded");
	  return STM32_OK;

  }
  else return STM32_INIT_ERROR;
}

void delay(int a) {
	int i;
	while (i<a) i++;
}
// ****************************************************************************
// Implementation of the protocol

int stm32_init( const char *portname, u32 baud )
{
  // Open port
  if( ( stm32_ser_id = ser_open( portname ) ) == ( ser_handler )-1 )
    return STM32_PORT_OPEN_ERROR;

  // Setup port
  ser_setup( stm32_ser_id, baud, SER_DATABITS_8, SER_PARITY_NONE, SER_STOPBITS_1 );

  // Connect to bootloader
  return stm32h_connect_to_bl();
}

// Get bootloader version
// Expected response: ACK version OPTION1 OPTION2 ACK
int stm32_get_version( u8 *major, u8 *minor )
{
  u8 i, version;
  int temp, total;
  int tries = STM32_RETRY_COUNT;  

  STM32_CHECK_INIT;
  stm32h_send_command( STM32_CMD_GET_COMMAND );
  STM32_EXPECT( STM32_COMM_ACK );
  STM32_READ_AND_CHECK( total );
  for( i = 0; i < total + 1; i ++ )
  {
    STM32_READ_AND_CHECK( temp );
    if( i == 0 )
      version = ( u8 )temp;
  }
  *major = version >> 4;
  *minor = version & 0x0F;
  STM32_EXPECT( STM32_COMM_ACK );
  return STM32_OK;
}

// Get chip ID
int stm32_get_chip_id( u16 *version )
{
  u8 temp;
  int vh, vl;

  STM32_CHECK_INIT;
  stm32h_send_command( STM32_CMD_GET_ID );
  STM32_EXPECT( STM32_COMM_ACK );
  STM32_EXPECT( 1 );
  STM32_READ_AND_CHECK( vh );
  STM32_READ_AND_CHECK( vl );
  STM32_EXPECT( STM32_COMM_ACK );
  *version = ( ( u16 )vh << 8 ) | ( u16 )vl;
  return STM32_OK;
}

// Write unprotect
int stm32_write_unprotect()
{
  printf("\nhost: starting write unprotect sequence");
  STM32_CHECK_INIT;
  //printf("\n\thost: CHECK_INIT succeded");
  stm32h_send_command( STM32_CMD_WRITE_UNPROTECT );
  //printf("\n\thost: write unprotect command sent, waiting for acks");
  STM32_EXPECT( STM32_COMM_ACK );
  printf("\n\thost: ack received (write unprotect request)");
  STM32_EXPECT( STM32_COMM_ACK );
  printf("\n\thost: ack received (Flash unprotected successfully)");
  printf("\n\thost: reinitializing due to device reset");
  // At this point the system got a reset, so we need to re-enter BL mode
  //int i;
  //while(i<99) i++;
  return stm32h_connect_to_bl();
}

// Erase flash
int stm32_erase_flash()
{
  u8 temp;
  int cbbltest;
  STM32_CHECK_INIT;
  printf("\nhost: starting erase flash sequence");
  //delay(9);
  stm32h_send_command( STM32_CMD_ERASE_FLASH );
  cbbltest = stm32h_read_byte();
  printf("\n\thost: received value %x", cbbltest);
  if(cbbltest != STM32_COMM_ACK) return STM32_COMM_ERROR;
  //delay(9);
  printf("\n\thost: ack received (erase memory request)");
  ser_write_byte( stm32_ser_id, 0xFF );
  //ser_write_byte( stm32_ser_id, 0x00 );
  delay(99);
  cbbltest = stm32h_read_byte();
  /*
  if (cbbltest == -1) printf("\n\tread byte failed, %x, %d", cbbltest, cbbltest);
  else printf("\n\thost: received value %x, %d", cbbltest, cbbltest);
  */
  if(cbbltest != STM32_COMM_ACK) return STM32_COMM_ERROR;
  printf("\n\thost: ack received (erase procedure successful)");
  return STM32_OK;
}

// Program flash
// Requires pointers to two functions: get data and progress report
int stm32_write_flash( p_read_data read_data_func, p_progress progress_func )
{
  u32 wrote = 0;
  u8 data[ STM32_WRITE_BUFSIZE + 1 ];
  u32 datalen, address = STM32_FLASH_START_ADDRESS;
  int cbbltest;
  printf("\nhost: starting to write memory");

  printf("\n");
  printf("host: Type flash base address (default 0x08005000 6!!!!!!):\n");
  scanf("%x", &address);
  printf("host: programming Flash starting from: %x", address, address);

  STM32_CHECK_INIT; 
  while( 1 )
  {
	//delay(9);
    // Read data to program
    if( ( datalen = read_data_func( data + 1, STM32_WRITE_BUFSIZE ) ) == 0 ) {
    printf("\n\thost: bin code packet length is %d", datalen);
      break;
    }
    printf("\n\thost: bin code packet length is %d", datalen);
    data[ 0 ] = ( u8 )( datalen - 1 );

    // Send write request
    //delay(9);
    printf("\n\thost: sending write request command, 0x31");
    stm32h_send_command( STM32_CMD_WRITE_FLASH );
    STM32_EXPECT( STM32_COMM_ACK );
    printf("\n\thost: ack received (write request ack)");
    
    // Send address
    //delay(9);
    printf("\n\thost: sending address: %x", address);
    stm32h_send_address( address );
    STM32_EXPECT( STM32_COMM_ACK );
    printf("\n\thost: ack received (address ok)");

    // Send data
    //delay(9);
    printf("\n\thost: sending data...");
    stm32h_send_packet_with_checksum( data, datalen + 1 );
    //delay(9);
    cbbltest = stm32h_read_byte();
    if (cbbltest == -1) printf("\n\tread byte failed, %x, %d", cbbltest, cbbltest);
    if(cbbltest != STM32_COMM_ACK) return STM32_COMM_ERROR;
    printf("\n\thost: ack received (data packet ok)");

    // Call progress function (if provided)
    wrote += datalen;
    if( progress_func )
      progress_func( wrote );

    // Advance to next data
    address += datalen;
  }
  printf("\n\thost: returning, write successful");
  return STM32_OK;
}

int stm32_jump() {
	u32 address;
	printf("\n");
	printf("host: Type address to jump to(default 0x08005000):\n");
	scanf("%x", &address);
	printf("host: jumping to: %x", address, address);
	stm32h_send_command( STM32_CMD_GO );
	STM32_EXPECT( STM32_COMM_ACK );
	printf("\n\thost: ack received (jump request)");
	stm32h_send_address( address );
	STM32_EXPECT( STM32_COMM_ACK );
	printf("\n\thost: ack received (address ok)");
	return STM32_OK;
}

