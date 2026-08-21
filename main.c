//Author: Andrew Brown
//Version: 1.04
//Description: The main module for the BCC driver

/* Main include files */
#include "includes.h"
#include "bcc_struct.h"      //Contains information structure of BCC card
#include "main.h"
#include "bcc_date.h"
#include "regdefs.h"         //Contains register definitions and masks
#include "pci.h"             //Conatins the pci read/write stuff
#include "file_ops.h"
#include "interrupts.h"
#include "bccif_serial.h"

/* Error if there is no PCI support */
#ifndef CONFIG_PCI
  #error "This driver needs PCI support to be available"
#endif

//////////////////////////////////////////////////////////////////////////////////////
//                               Global Variables                                   //
//////////////////////////////////////////////////////////////////////////////////////

static int major = DEFAULT_MAJOR;  //Default to the default major number in includes.h
static int card = 1;      //By default find the first card in the system
bccif_info_struct Bccif_Info;   //The master copy of the device info structure,
                              // all file opens use this copy as a template for
                              // device info
int debug = 0;   //Default to no debug messages

static struct file_operations fops = {
  read: read,
  write: write,
  ioctl: ioctl,
  open: open,
  release: release,
};                                           //FOPS structure

//////////////////////////////////////////////////////////////////////////////////////
//                               Module Parameters                                  //
//////////////////////////////////////////////////////////////////////////////////////

#if LINUX_VERSION_CODE > VERSION_CODE(2,6,0)
  module_param(major, int, 0);
  module_param(debug, int, 0);
  module_param(card, int, 0);
#else
  MODULE_PARM(major, "i");
  MODULE_PARM_DESC(major, "[major=X]\t"
       "If X=0, automatically detect and use the first available major number.\n"
       "If 1<=X<=255, use major number X.\n"
       "(major=0)\n");
  MODULE_PARM(debug, "i");
  MODULE_PARM_DESC(debug, "[debug=Y]\t"
       "If Y=0, all debug messages are truned off.\n"
       "If 1<=Y<=6, different levels of debug messages are enabled.\n"
       "(debug=0)\n");            
  MODULE_PARM(card, "i");
  MODULE_PARM_DESC(card, "[card=Z]\t"
       "The driver is installed for the (Z)th card detected.\n"
       "(card=1)\n");    
#endif     

/* Module information embedded in the object file */
MODULE_AUTHOR("Andrew Brown, <Andrew.J.Brown@uts.edu.au>");
MODULE_DESCRIPTION("CSIRO ATNF Block Control Computer (BCC) PCI Char Driver");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("CSIRO ATNF PCI BCC CARD");

//////////////////////////////////////////////////////////////////////////////////////
//                                Kernel 2.6 Changes                                //
//////////////////////////////////////////////////////////////////////////////////////

#if LINUX_VERSION_CODE >= VERSION_CODE(2,6,0)
int start_module(void);
void end_module(void);
module_init(start_module);
module_exit(end_module);
#endif


//////////////////////////////////////////////////////////////////////////////////////
//                          Module Entry & Exit Points                              //
//////////////////////////////////////////////////////////////////////////////////////

/* Entry point for module */
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
int init_module(void)
#else
int start_module(void)
#endif
{
int devices;                                  //Count of the number of detected cards  
int result;                                    //Return value from function calls
bccif_info_struct *bccif;     //Information structure containing all the info about detected card



#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
    EXPORT_NO_SYMBOLS;                                //Not exporting any symbols to the kernel
#elif  LINUX_VERSION_CODE < VERSION_CODE(2,6,17)
    SET_MODULE_OWNER(&fops);  
#endif /* LINUX_VERSION_CODE < VERSION_CODE(2,6,0) */
  
    printk(KERN_NOTICE "BCCIF:Driver Compilation Date: %s\n", COMPILE_TIME);
    printk(KERN_NOTICE "BCCIF:Debug level set to: %d\n", debug);  
    printk(KERN_NOTICE "BCCIF:Number devices supported by this driver: %d\n",
	   BCCIF_BLOCKS);  
   
    //Setup the master info strcuture
    bccif = InitSysInfo(&Bccif_Info);
    devices = 0;

    if (Find_BCC(&devices, bccif) != 0) {      //Initiate a search for a card
    printk(KERN_ERR "BCCIF:Failed to detect a PCI bus\n");
    return -ENODEV; }                          //No PCI bus detected
    
  if (devices == 0) {
    printk(KERN_ERR "BCCIF:Failed to detect a PCI BCC card\n");
    return -ENODEV;  }                              //No card detected
  
  //Initialise the BCC card and its file system
  result = Initialise_BCC_Card(bccif);    
    
  if (result != 0) {
    printk(KERN_ERR "BCCIF:Failed to initialise the BCC card\n");
    return result; }                              //No such device or other error
                
  return 0;
}

/* Exit point for module */
/* Can only occur if the module count is 0 */
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
void cleanup_module(void)
#else
void end_module(void)
#endif
{
  int i;                                      //Loop variable
  bccif_info_struct *bccif;                            //Pointer to global structure
  
  bccif = &Bccif_Info; 
  
    printk(KERN_NOTICE "BCCIF:Unregistering BCC device major no. %d\n", bccif->major);
   
    /* unregister the device with the kernel */
    unregister_chrdev(bccif->major, "bccif");

  //Unmap the IO of the PCI bus
    iounmap((char *) bccif->base_Address);
  iounmap((char *) bccif->config_Address);
  
  //Destroy Serial number string and the semaphore space
  if (bccif->IdentString != NULL) kfree(bccif->IdentString);
  kfree(bccif->sem);
  
  //Destroy memory for block specific components of the bcc_struct structure
  for (i=0 ;i<BCCIF_BLOCKS; i++) {
    kfree(bccif->countlock[i]);         
    kfree(bccif->owner[i]);
    kfree(bccif->open_count[i]);}
    
}

//////////////////////////////////////////////////////////////////////////////////////
//                             Initialisation Routines                              //
//////////////////////////////////////////////////////////////////////////////////////

/* initialise the identification array */
bccif_info_struct *InitSysInfo(bccif_info_struct *bccif)
{
  int i;                                      //Loop variable
  
  memset(bccif, 0, sizeof(bccif_info_struct));                  //Write all zeros to the control structures  

  //Allocate common space for the semaphore, so all devices have atomic access to bus
  bccif->sem = (struct semaphore *)kmalloc(sizeof(struct semaphore), GFP_KERNEL);   
  
  //Initialise the semaphore
  sema_init(bccif->sem, 1);  

  //Allocate memory for block specific components of the bcc_struct structure
  for (i = 0 ; i < BCCIF_BLOCKS; i++) {
    
  //Allocate a common space for each block for the spin lock
  bccif->countlock[i] = (spinlock_t *)kmalloc(sizeof(spinlock_t), GFP_KERNEL); 
  
  //Initilaise the spin lock
  spin_lock_init(bccif->countlock[i]);        
    
  //Allocate common space fpr the owner and open count variables
  bccif->owner[i] = (int*) kmalloc(sizeof(int), GFP_KERNEL);
   *bccif->owner[i] = 0;
  bccif->open_count[i] = (int*) kmalloc(sizeof(int), GFP_KERNEL);
  *bccif->open_count[i] = 0;
  }

  return bccif;
}

/* Find the BCC device on the PCI bus, parameter returns the number of devices found on PCI bus */
/* Function returns <0 if there is no PCI bus detected */
int Find_BCC(int *devices, bccif_info_struct *bccif)
{
struct pci_dev *dev = NULL;      //Info structure used to return info about the device
unsigned long int base_addr;     //Temp variable used to store the PCI address prior to remapping
int n, Device_Count;             //Loop variable, count of how many devices have been detected

#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
    if (!pci_present()) return -ENODEV;      //If there is no pci bus then error
#endif /* LINUX_VERSION_CODE < VERSION_CODE(2,6,0) */
  
    if (card < 0) card = 1;   //If the card variable is invalid, make it valid

    for(Device_Count = 0; Device_Count < MAX_DEVICES; ) {   //Keep looking for max devices for each specified vendor id
      dev = pci_find_device(VENDOR_ID, DEVICE_ID, dev);
      if (!dev) break;                              //When we run out of devices
      
      Device_Count++;                                //Increment the device count    
      
      //Indicate a device has been detected
      if (debug >= DEBUG_INFORMATION)
	printk(KERN_INFO "BCCIF:BCCIF Found, attempting initialisation VendorID:%x DeviceID:%x\n",
	       dev->vendor, dev->device);  

      if (Device_Count != card) continue;  // If we aren't installing a driver for this card ignore it
                                      
#if LINUX_VERSION_CODE < VERSION_CODE(2,4,0)    // Versions before 2.4 had a different pc_dev structure
      if(debug >= DEBUG_INFORMATION) {
        printk(KERN_INFO "BCCIF:Possible addresses:\n");
        for(n = 0; n < 6; ++n) 
          printk(KERN_INFO "BCCIF:     Address[%d]: %lx\n", n, (*dev).base_address[n]); }
      
      //Map the config space first
      base_addr = (*dev).base_address[CONFIG_BAR];
      if(debug >= DEBUG_CRIT)
        printk(KERN_INFO "BCCIF:Remapping Config Base address = %lx\n", base_addr);
    
      //Remap the PCI bus space to the CPU memory space
      bccif->config_Address = (char *) ioremap(base_addr, CONFIG_REGION_SIZE);
    
      //Map the interface space second, as per above
      base_addr = (*dev).base_address[BCCIF_BAR];
      if(debug >= DEBUG_CRIT)
        printk(KERN_INFO "BCCIF:Remapping Interface Base address = %lx\n", base_addr);    
    
      //Remap the PCI bus space to the CPU memory space
      bccif->base_Address = (char *) ioremap(base_addr, BCCIF_REGION_SIZE);
    
#else /* Version >= 2.4.0 */
      if(debug >= DEBUG_INFORMATION) {
        printk(KERN_INFO "BCCIF:Possible addresses:\n");
        for(n = 0; n < 6; ++n) 
          printk(KERN_INFO "BCCIF:     Address[%d]: %lx\n", n,
		 (long unsigned int) pci_resource_start(dev, n)); }

      //Map the config space first
      base_addr = pci_resource_start(dev, CONFIG_BAR);
      if(debug >= DEBUG_CRIT)
        printk(KERN_INFO "BCCIF:Remapping Config Base address = %lx\n", base_addr);
      
      //Remap the PCI bus space to the CPU memory space
      bccif->config_Address = (unsigned long int) ioremap(base_addr, CONFIG_REGION_SIZE);

      //Then map the interface space
      base_addr = pci_resource_start(dev, BCCIF_BAR);
      if(debug >= DEBUG_CRIT)
        printk(KERN_INFO "BCCIF:Remapping Interface Base address = %lx\n", base_addr);
      
      //Remap the PCI bus space to the CPU memory space
      bccif->base_Address = (unsigned long int) ioremap(base_addr, BCCIF_REGION_SIZE);      
#endif
  
      //Examine the re-mapped addresses
      if(bccif->base_Address == (unsigned long int) NULL) {
	printk(KERN_ERR "BCCIF:ioremap of interface address failed\n");
	return -ENOMEM; }
      else 
	if(debug >= DEBUG_CRIT)  
	  printk(KERN_INFO "BCCIF:Interface mapped address = %lx\n", (unsigned long) bccif->base_Address);  

      if(bccif->config_Address == (unsigned long int) NULL) {
	printk(KERN_ERR "BCCIF:ioremap of config address failed\n");
	return -ENOMEM; }
      else 
	if(debug >= DEBUG_CRIT)  
	  printk(KERN_INFO "BCCIF:Config mapped address = %lx\n", (unsigned long) bccif->config_Address);  

      //Get the IRQ    
      bccif->irq = dev->irq;
      printk(KERN_INFO "BCCIF:IRQ line = %d\n", bccif->irq);
    
      //Increment the number of devices found on PCI bus
      (*devices)++;
    }

  //return an error if no devices were detected  
  if(debug >= DEBUG_INFORMATION) 
    printk(KERN_INFO "BCCIF:Detected %d BCC card(s)\n", (*devices));
  
  return 0;                
}

/* Initialises the ifile strcuture of the driver */
/* Loads the serial prom */
int Initialise_BCC_Card(bccif_info_struct *bccif)
{
  int result;                                    //Return value variable
  
  //Get the serial number of the BCC card
  result = Initialise_BCC_Serial(bccif);                      //Allocate a memory buffer
  if (result != 0) {
    printk(KERN_ERR "BCCIF: Couldn't start PROM interface \n");
    return result;}
  
  result = Get_BCC_Serial(bccif);                          //Actually load the serial number
  if(result <= 0)
    strcpy(bccif->IdentString, "NOT AVAILABLE");
   
  printk(KERN_NOTICE "BCCIF:Serial Number: '%s' \n", bccif->IdentString);      //Print it out  
    
  //Allocate a major number
  result = register_chrdev(major, "bccif", &fops);  
  
  //Check response
    if (result < 0) {
      printk(KERN_ERR "BCCIF: Couldn't get major %d\n", major);
      return result; } 
    else if (result > 0) {
    if (major == 0)
        bccif->major = result;                      //Store assigned major number
      else
        bccif->major = major;}                       //Store the requested major number
  
    printk(KERN_INFO "BCCIF:Major %d Assigned\n", bccif->major);  
  return 0;  
}

