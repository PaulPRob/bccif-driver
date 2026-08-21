//Author: Andrew Brown
//Version: 1.00
//Description: The main header for the bcc pci driver. This driver has been 
//         designed to work with Linux Kernel version 2.2 up

//////////////////////////////////////////////////////////////////////////////////////
//                               Main include file                                  //
//////////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDES_H
#define INCLUDES_H

#include <linux/version.h>
#define VERSION_CODE(vers,rel,seq) ( ((vers)<<16) | ((rel)<<8) | (seq) )

/* Define the Multiprocessor constant */
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
#include <linux/config.h>
#endif


//////////////////////////////////////////////////////////////////////////////////////
//                                    C Includes                                    //
//////////////////////////////////////////////////////////////////////////////////////

/* Linux Module Includes */
#if LINUX_VERSION_CODE >= VERSION_CODE(2,6,0)
#include <linux/autoconf.h>
#include <linux/moduleparam.h>            //Required for 2.6 for parameters
#include <linux/netdevice.h>      // SET_MODULE_OWNER()
#else
#include <linux/modversions.h>
#include <linux/module.h>
#endif

#include <linux/fs.h>
#include <linux/errno.h>
//#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/delay.h>
#if LINUX_VERSION_CODE < VERSION_CODE(2,2,0)
#include <linux/isdnif.h>
#include <linux/bios32.h>               /* pci stuff */
#else
#include <linux/poll.h>                 /* replaces 'select' stuff */
#endif /* LINUX_VERSION_CODE < VERSION_CODE(2,2,0) */

#include <linux/pci.h>                  //PCI functions and config register definitions
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/system.h>
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,17)
#include <asm/semaphore.h>
#endif /* LINUX_VERSION_CODE < VERSION_CODE(2,7,17) */
#include <asm/atomic.h>

/* Generic files used to mainatain compatability with older kernels */
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
#include "sysdep.h"
#endif

#ifdef CONFIG_SMP
  #define __SMP__
#endif

#if defined(CONFIG_MODVERSIONS)
  #define MODVERSIONS
#endif

//////////////////////////////////////////////////////////////////////////////////////
//                               Device Constants                                   //
//////////////////////////////////////////////////////////////////////////////////////

/* PCI Core Implementation Specifications */
#define BCCIF_BLOCKS       4  // Number of external interfaces located on the PCI card
                              // The 4th unit is special and not a normat IF. ERD 10/11/07
#define PCI_IMAGE0            // Configuration access to bridge for regdefs file
#define PCI_IMAGE1            // External BCC interface access for regdefs file

/* Driver Specific Options */
#define MAX_OPEN_IF        5            //Each interface can only be opened 5 times by the 1 user
#define MAX_DEVICES        5            //Can have at max 5 cards in the system, of which a driver can be install for each, means there can be 15 interfaces on 1 PC
#define DEFAULT_MAJOR       0            //Default major number to use, 0 for auto

/* Prom specific definitions */
#define MAX_PREAMBLE_BITS 256              // Assume that there is no PROM after reading this many preamble bits.
#define MAX_BCC_IDENT_LENGTH  0x100          //Max of 256 characters of serial number

/* PCI Card Specifications */
#define VENDOR_ID       0x2321            //Generic vendor id
#define DEVICE_ID       0x0001            //BCC device ID
#define CONFIG_BAR      0x0000            //Location of address of config register access
#define CONFIG_REGION_SIZE  0x1000            //Size of that area
#define BCCIF_BAR       0x0001            //Location of the interface access
#define BCCIF_REGION_SIZE  0x1000            //Size of that data region

/* Debug Level Definitions */
#define DEBUG_ALL      0x6              //All debug info - will fill the console
#define  DEBUG_PCI      0x5              //Display all PCI reads/writes - should not be used, will overflow logs
#define DEBUG_IFRW      0x4              //Display all interface reads/writes - if enabled will dramatically reduce the throughput of the driver
#define DEBUG_IF      0x3              //Display all interface transactiosn except reads/writes  
#define DEBUG_INFORMATION  0x2              //Display all information
#define DEBUG_CRIT      0x1              //Display critical information
#define DEBUG_OFF      0x0              //No debug info


#endif /* #ifndef INCLUDES_H */




