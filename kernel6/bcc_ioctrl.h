//////////////////////////////////////////////////////////////////////////////////////
//                                   IOCTRL Constants                               //
//////////////////////////////////////////////////////////////////////////////////////

/* This file should be included in any custom applications written using the BBC driver */

#ifndef BCC_IOCTRL_H
#define BCC_IOCTRL_H

#include<linux/ioctl.h>

/* Use 'b' as the magic number */
#define BCC_IOC_MAGIC  'b'

/* All commands return 0 on sucess and -1 on failure */

/* Commands to modify functionality and individual bits in the interface */
#define BCC_RESET_INTERFACES		_IO(BCC_IOC_MAGIC, 1)								//Reset all the external interfaces  - No parameters
#define BCC_SET_MK2_MODE		_IO(BCC_IOC_MAGIC, 2)								//Set the mode of the particular interface to MK2, HW override must be disabled - No parameters
#define BCC_SET_MK1_MODE		_IO(BCC_IOC_MAGIC, 3)								//Set the mode of the particular interface to MK1, HW override must be disabled - No parameters
#define BCC_SET_TIMEOUT_DELAY		_IOW(BCC_IOC_MAGIC, 4, unsigned short int*) //Sets the 5 bit software delay, parameter is a pointer to a short int representing the new delay
#define BCC_SET_SOFTWARE_TIMEOUT	_IO(BCC_IOC_MAGIC, 5) //Set the timeout mode to programable delay - No parameters
#define BCC_CLR_SOFTWARE_TIMEOUT	_IO(BCC_IOC_MAGIC, 6)	//Clear the programable delay timeout mode, changes back to hw only mode - No parameters
#define BCC_SET_INCREMENT_ENABLE	_IO(BCC_IOC_MAGIC, 7)	//Set the auto incrementing feature of the address register  - No parameters
#define BCC_CLR_INCREMENT_ENABLE	_IO(BCC_IOC_MAGIC, 8)	//Clears the auto incrementing feature of the address register  - No parameters
#define BCC_ENABLE_GLOBAL_INTERRUPTS	_IO(BCC_IOC_MAGIC, 9)	//Enable the global interrupts  - No parameters
#define BCC_DISABLE_GLOBAL_INTERRUPTS	_IO(BCC_IOC_MAGIC, 10)	//Disable the global interrupts  - No parameters
#define BCC_ENABLE_TIMEOUT_INTERRUPT	_IO(BCC_IOC_MAGIC, 11)	//Enable the interface timeout interrupt  - No parameters
#define BCC_DISABLE_TIMEOUT_INTERRUPT	_IO(BCC_IOC_MAGIC, 12)	//Disable the interface timeout interrupt - No parameters
#define BCC_ENABLE_OVERFLOW_INTERRUPT	_IO(BCC_IOC_MAGIC, 13)	//Enable the address overflow interrupt i.e. interrupt on 7FFF -> 8000 - No parameters
#define BCC_DISABLE_OVERFLOW_INTERRUPT	_IO(BCC_IOC_MAGIC, 14)								//Disable the address overflow interrupt - No parameters
#define BCC_CLR_OVERFLOW_OCCURED_STATE	_IO(BCC_IOC_MAGIC, 15)								//Clear the overflow occured flag
#define BCC_CLR_TIMEOUT_OCCURED_STATE	_IO(BCC_IOC_MAGIC, 16)								//Clear the timeout occured flag

/* Commands to read status of interface */
#define BCC_GET_TIMEOUT_DELAY		_IOR(BCC_IOC_MAGIC, 17, unsigned short int*)		//Returns the current value of the 5 bit delay - Parameter is an integer pointer to a memory location to recieve the short int
#define BCC_GET_SOFTWARE_TIMEOUT	_IOR(BCC_IOC_MAGIC, 18, unsigned short int*)		//Returns the current value of the software enable bit
#define BCC_GET_INCREMENT_ENABLE	_IOR(BCC_IOC_MAGIC, 19, unsigned short int*)		//Returns the current state of the address increment bit - parameter is a pointer to an short int, a non-zero value represents the increment is active
#define BCC_GET_GLOBAL_INTERRUPTS_STATE	_IOR(BCC_IOC_MAGIC, 20, unsigned short int*)		//Returns the current state of the global interrupt enable bit - parameter is a pointer to an short int, a non-zero value represents the increment is active
#define BCC_GET_TIMEOUT_INTERRUPT_STATE	_IOR(BCC_IOC_MAGIC, 21, unsigned short int*)		//Returns the current state of the interface timeout inetrrupt enable bit - parameter is a pointer to a short in, a non-zero value represents timeout interrupt active
#define BCC_GET_OVERFLOW_INTERRUPT_STATE _IOR(BCC_IOC_MAGIC, 22, unsigned short int*)		//Returns the current state of the interface overflow interrupt enable bit - parameter is a pointer to a short in, a non-zero value represents overflow interrupt active
#define BCC_GET_TIMEOUT_OCCURED_STATE	 _IOR(BCC_IOC_MAGIC, 23, unsigned short int*)		//Returns the state of the timeout interrupt occur bit - parameter is a pointer to a short in, a non-zero value represents timeout interrupt occured
#define BCC_GET_OVERFLOW_OCCURED_STATE	 _IOR(BCC_IOC_MAGIC, 24, unsigned short int*)		//Returns the state of the overflow interrupt occur bit - parameter is a pointer to a short in, a non-zero value represents overflow interrupt occured
#define BCC_GET_MK_MODE			_IOR(BCC_IOC_MAGIC, 25, unsigned short int*)		//Returns the operating mode of the interface - parameter is a pointer to a short in, a non-zero value represents MK2 and 0 represents MK1

/* Additional read/write commands to other registers */
#define BCC_SET_ADDRESS		_IOW(BCC_IOC_MAGIC, 26, unsigned short int*)	//Set the address register of the current interface - parameter is a pointer to a short int address
#define BCC_GET_ADDRESS		_IOR(BCC_IOC_MAGIC, 27, unsigned short int*)	//Read the address register of the current interface - parameter is a pointer to a short int
#define BCC_READ_IF_REGISTER	_IOR(BCC_IOC_MAGIC, 28, unsigned short int*)	//Reads from given register address of the current interface. parameter is a pointer to a data struct of type Bccif_data
#define BCC_READ_REGISTER_0	_IOR(BCC_IOC_MAGIC, 29, unsigned short int*)	//Reads register 0 of the current interface, only available in MK1 mode - parameter is a pointer to a short int 
#define BCC_READ_REGISTER_1	_IOR(BCC_IOC_MAGIC, 30, unsigned short int*)	//Reads register 1 of the current interface, only available in MK1 mode - parameter is a pointer to a short int 
#define BCC_READ_REGISTER_2	_IOR(BCC_IOC_MAGIC, 31, unsigned short int*)	//Reads register 2 of the current interface, only available in MK1 mode - parameter is a pointer to a short int 
#define BCC_READ_REGISTER_3     _IOR(BCC_IOC_MAGIC, 32, unsigned short int*)	//Reads register 3 of the current interface, only available in MK1 mode - parameter is a pointer to a short int 
#define BCC_READ_REGISTER_4	_IOR(BCC_IOC_MAGIC, 33, unsigned short int*)	//Reads register 4 of the current interface, only available in MK1 mode - parameter is a pointer to a short int 
#define BCC_READ_REGISTER_5	_IOR(BCC_IOC_MAGIC, 34, unsigned short int*)	//Reads register 5 of the current interface, only available in MK1 mode - parameter is a pointer to a short int 
#define BCC_WRITE_IF_REGISTER	_IOR(BCC_IOC_MAGIC, 35, unsigned short int*)	//Write to given register address of the current interface. parameter is a pointer to a data struct of type Bccif_data
#define BCC_WRITE_REGISTER_0	_IOW(BCC_IOC_MAGIC, 36, unsigned short int*)	//Write to register 0 of the current interface, only available when in MK1 mode - parameter is a pointer to a short int
#define BCC_WRITE_REGISTER_1	_IOW(BCC_IOC_MAGIC, 37, unsigned short int*)	//Write to register 1 of the current interface, only available when in MK1 mode - parameter is a pointer to a short int
#define BCC_WRITE_REGISTER_2	_IOW(BCC_IOC_MAGIC, 38, unsigned short int*)	//Write to register 2 of the current interface, only available when in MK1 mode - parameter is a pointer to a short int
#define BCC_WRITE_REGISTER_3	_IOW(BCC_IOC_MAGIC, 39, unsigned short int*)	//Write to register 3 of the current interface, only available when in MK1 mode - parameter is a pointer to a short int
#define BCC_WRITE_REGISTER_4	_IOW(BCC_IOC_MAGIC, 40, unsigned short int*)	//Write to register 4 of the current interface, only available when in MK1 mode - parameter is a pointer to a short int
#define BCC_WRITE_REGISTER_5	_IOW(BCC_IOC_MAGIC, 41, unsigned short int*)	//Write to register 5 of the current interface, only available when in MK1 mode - parameter is a pointer to a short int

/* Serial number commands */
#define BCC_GET_SERIAL_NUMBER	_IOR(BCC_IOC_MAGIC, 42, char *)		//Returns the serial number string as obtained from the prom - Parameter is a pointer to a buffer, use the following call to get correct length
#define BCC_GET_SERIAL_NUMBER_LENGTH	_IOR(BCC_IOC_MAGIC, 43, int*)	//Returns the length of the serial number string - Parameter is a pointer to an integer to recieve the data

/* Generic Read/write commands to the registers */
#define BCC_WRITE_WORD_IF_REGISTER	_IOW(BCC_IOC_MAGIC, 44, Bccif_data*)	//Writes data to any register of the current interface - parameter is a pointer to type Bccif_data
#define BCC_READ_WORD_IF_REGISTER	_IOR(BCC_IOC_MAGIC, 45, Bccif_data*)	//Reads data from any register in the current interface - parameter is a pointer to type Bccif_data, the data member will be overwritten

#define BCC_WRITE_BYTE_IF_REGISTER	_IOW(BCC_IOC_MAGIC, 46, Bccif_data*)	//Writes data to any register of the current interface - parameter is a pointer to type Bccif_data
#define BCC_READ_BYTE_IF_REGISTER	_IOR(BCC_IOC_MAGIC, 47, Bccif_data*)	//Reads data from any register in the current interface - parameter is a pointer to type Bccif_data, the data member will be overwritten

#define BCC_WRITE_JTAG_DEVICE	_IOR(BCC_IOC_MAGIC, 48, char *)	//Writes data pointed from buffer pointed to by first parameter. first 32 bits of buffer is the number of bytes to follow.

/* Custome data type for write_if and read_if commands */
typedef struct {
  unsigned short int address;	//Address in the internal interface address space of the register to read/write
  unsigned short int data;	//The data being inputted or outputted from the regsiter identified above
} Bccif_data;

#endif
