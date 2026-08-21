//Author: Andrew Brown
//Version: 1.00
//Description: The serial number obtaining code, uses registers in interface 1 to get code, although any interface could be used

#include "includes.h"
#include "regdefs.h"
#include "bcc_struct.h"
#include "pci.h"
#include "bccif_serial.h"

extern int debug;                                //Debug level indicator, declared in main.c

/* Allocate the memory for the serial number string */
int Initialise_BCC_Serial(bccif_info_struct *bccif)
{
  //Allocate the memory, 1 memory space is made to hold a copy of ther serial number and all devices are given copies of the pointer so that they can access it
  bccif->IdentString = (char*)kmalloc((MAX_BCC_IDENT_LENGTH+2)*sizeof(char), GFP_KERNEL);

  if(debug >= DEBUG_CRIT) 
    printk(KERN_INFO "BCCIF:Created Serial string space at %lx \n", (unsigned long int) bccif->IdentString);

  //Zero the array
  memset(bccif->IdentString, 0, MAX_BCC_IDENT_LENGTH+2);
      
  //Ensure that the memory was allocated  
    if (bccif->IdentString == NULL)
      return -ENOMEM;
  else
      return 0;  
}

/* Get the serial number string from the BCC interface card */
int Get_BCC_Serial(bccif_info_struct *bccif)
{
  unsigned char result;                        //RMW return value
  int ret = 0;                            //Function return value
  int i;                                //Loop variable
  
  /*   Format of data read from serial PROM.

    Typical example (most significant bit on left down to least
    significant bit on right):

    11111111111111111111111 ... <= Preamble bits.
    XXXXXXX0 <= Start byte.
    DDDDDDDD <= First data byte.
    DDDDDDDD <= Second data byte.
    DDDDDDDD <= Third data byte.
     ...     <= More data bytes.
     ...
    00000000 <= Stop byte.

    If there is no PROM, there will be an infinite number of preamble
    bits. (There will never be a start byte: The hardware will simply
    keep returning '1's forever.)

    If there is a PROM, there may be 0 or more preamble bits. Typically,
    there will be around 128 preamble bits. The maximum number of preamble
    bits is not specified, so technically, it is not possible to detect
    the absence of a serial PROM with 100% certainty.
  
    After the preamble bits, comes the start byte. The start byte
    consists of 7 don't cares and a 0. Since bytes are read least
    significant bit first, the 0 will be read before the don't cares.
    
    After the start byte, come the data bytes. The data is encoded in
    ascii format. There can be anywhere between 0 and infinity data bytes.
  
    After the data bytes comes the stop byte. The stop byte consists of 8
    0s.

    Bytes are read 1 bit at a time, least significant bit first. */
             
  /* Protect the read/writes with the semaphore */
  if(down_interruptible(bccif->sem))
    return -ERESTARTSYS;
     
    /* reset version/serial number PROM */  
    result = ReadPCIByte(bccif->base_Address + BCC_SERIAL_PROM_RESET_OFFSET);
    WritePCIByte(bccif->base_Address + BCC_SERIAL_PROM_RESET_OFFSET, (result | BCC_SERIAL_PROM_RESET_MASK));
      
    /* Discard bits until the start bit is found. */
    for (i=0; ; i++) {      
      if (i>=MAX_PREAMBLE_BITS) {
          ret = -ENXIO;
          printk(KERN_WARNING "BCCIF:Serial Number - no start bit detected\n");
          goto no_start_bit;
      } else if (read_prom_bit(bccif) == 0) {
          break;}
        clock_prom(bccif);}    

  if(debug >= DEBUG_INFORMATION) 
    printk(KERN_INFO "BCCIF:End of preamble detected at %d bits\n", i);        
        
  /* The next 7 bits are don't cares. */
  for (i=0; i<7; i++) {
    clock_prom(bccif);
    read_prom_bit(bccif); }

    /* Store bytes in the buffer until the stop byte if found. */
  for (i=0; ; i++) {
    result = read_prom_byte(bccif);
      bccif->IdentString[i] = (char)result;
    if (i == MAX_BCC_IDENT_LENGTH) {
      bccif->IdentString[i] = '\0';
          printk(KERN_WARNING "BCCIF:Serial Number - no stop byte detected after %d bytes\n", i);
          break;
      }else if (result == 0x00) {
          break; }}
    ret = i;
  
   no_start_bit:
      up(bccif->sem);                              //Release semaphore
      return ret;    
}

/* Read a bit from the prom */
int read_prom_bit(bccif_info_struct *bccif)
{
  unsigned char result;
  
  result = ReadPCIByte((bccif->base_Address + BCC_SERIAL_PROM_DATA_OFFSET));
  if ((result & BCC_SERIAL_PROM_DATA_MASK) != 0)
    return 1;
  else
    return 0;      
}

/* Read 8 bits from the prom and store it as a byte */
unsigned char read_prom_byte(bccif_info_struct *bccif)
{
  int i;                            //Loop variable
  unsigned char byte = 0x00;                  //Temp storage
  for (i = 0; i < 8; i++) {
    clock_prom(bccif);
    byte |= read_prom_bit(bccif) << i;
  }

  if(debug >= DEBUG_INFORMATION && byte >= ' ') 
    printk(KERN_INFO "BCCIF:Serial Byte %c \n", byte);  
    
  return byte;
}

/* Clock the prom */
void clock_prom(bccif_info_struct *bccif)
{
  unsigned char result;
  
  //Perform a RMW cycle on the status register
  result = ReadPCIByte(bccif->base_Address + BCC_SERIAL_PROM_CLOCK_OFFSET);  
  WritePCIByte(bccif->base_Address + BCC_SERIAL_PROM_CLOCK_OFFSET, (result | BCC_SERIAL_PROM_CLOCK_MASK));  
}
