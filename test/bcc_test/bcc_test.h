//Author: Andrew Brown
//Version: 1.00
//Description: Test program for the bcc interface

/* System include files */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

/* Include files from the driver source */
#include "../../bcc_ioctrl.h"						//IOCTL commands for the bcc interface
#include "../../regdefs.h"							//Register locations in the WB address space

#define BCCIF_NAME				"bccif"					//Name of the bccif devices in the /dev directory
#define BCCIF_BLOCKS 			4						//Number of interfaces for the file system, is used to error check the suffix for the device name while BCCIF_NAME is the prefix
#define MAX_LINE_LENGTH			80						//Generic string buffer length
#define MAX_INFO_LENGTH			10						//Length of buffers allocated to displaying status of bcc interfaces
#define DEFAULT_DEVICE 			1						//Default device to use on startup
#define HW_DELAY				87						//Number of PCI clocks the HW delay is set for, configured through XILINX VHDL files

//////////////////////////////////////////////////////////////////////////////////////
//                               Device Structure                                   //
//////////////////////////////////////////////////////////////////////////////////////

typedef struct {
	int device;											//Number of the active interface
	int mode;											//Current mode of the open interface
	int handle;											//Handle of the open device
	char *serial_no;									//Pointer to a char array containing the serial number
} if_data_t;

//////////////////////////////////////////////////////////////////////////////////////
//                                  Prototypes                                      //
//////////////////////////////////////////////////////////////////////////////////////

/* General Functions */
int displayMainMenu(if_data_t **);
char * get_input(int, char* );
void open_interface(if_data_t **);
if_data_t * bcc_test_init(void);
void close_interface(if_data_t *);
void clear_screen(void);

/* Menu Display Functions */
void do_interface_change(if_data_t **);
void do_interrupt_menu(if_data_t **);
void do_timeout_menu(if_data_t **);
void do_mode_menu(if_data_t **);
void do_hw_reset(if_data_t **);
void do_change_options(if_data_t **);
void do_config_dump (if_data_t **);
void do_data_write(if_data_t **);
void do_data_read(if_data_t **);
void do_interactive_debug(if_data_t **);




