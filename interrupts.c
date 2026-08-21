//Author: Andrew Brown
//Version: 1.01
//Description: The interrupt handling code, includes initialisation, destruction and handling

#include "includes.h"
#include "bcc_struct.h"
#include "regdefs.h"
#include "interrupts.h"
#include "pci.h"

extern int debug;                                  //Debug constant declared in main.c

//////////////////////////////////////////////////////////////////////////////////////
//                                  Interrupt Code                                  //
//////////////////////////////////////////////////////////////////////////////////////

/* The interurpt handler itself */
void BCC_Interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  bccif_info_struct *bccif;                            //Local copy of the device data structure  
  unsigned int data, mask;                            //32 bit variables used to pass information back between the PCI core and the driver
  unsigned short int intr;                            //16 bit variable used for data passing between the driver and the 
  
  //Get the data structure to determine which device we are dealing with
  bccif = dev_id;
  
  //Read the PCI core to determine if it is an error interrupt or a generated interrupt
  data = ReadPCIDWord(bccif->config_Address + ISR);
  mask = ReadPCIDWord(bccif->config_Address + ICR);
  data = data & mask;                                //Mask out any bits that are not enabled
  
  //If it wasn't our interrupt then quit, will only apply on a shared irq line
  //  if (data == 0)    
    //    return;
  
  //It was our interrupt, can only be either a wishbone error or an interface interrupt 
  if ((data & WB_EINT) != 0){                            //Then it was a wishbone error 
    if ((data & ERR_SIG) != 0) {                        //Was wb error signalled, if not then we have no idea what happened
      WritePCIDWord(bccif->config_Address + W_ERR_CS, ERR_SIG);        //It was so clear it
    
      //Set the wishbone error flag in the flags variable 
      spin_lock(&bccif->flaglock);                      //Use the spin lock to protect the variable
      bccif->flags |= WB_ERROR;                        //Set the error flag
      spin_unlock(&bccif->flaglock);  
          
      if(debug >= DEBUG_CRIT)  
        printk(KERN_INFO "BCCIF:Wishbone error interrupt occured\n");} }    
    intr = ReadPCIWord(bccif->base_Address + BCC_STATUS);            //Load the interrupts occured, they will be cleared here
    if(debug >= DEBUG_CRIT)  
      printk(KERN_INFO "BCCIF:General interrupt occured %04x\n", intr);      
    
    //Handle the overflow interrupt
    if ((BCC_OVERFLOW_INTERRUPT_MASK & intr) != 0){                //It was the oveflow interrupt that occured
      if ((BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_MASK & intr) != 0) {      //And it is enabled
        if(debug >= DEBUG_INFORMATION)  
          printk(KERN_INFO "BCCIF:Overflow interrupt occured\n");        
        spin_lock(&bccif->flaglock);
        bccif->flags |= OVERFLOW;                      //Set the overflow flag
        spin_unlock(&bccif->flaglock);      
    }}
    
    //Handle a timeout interrupt
    if ((BCC_BUS_TIMEOUT_INTERRUPT_MASK & intr) != 0) {              //It was the timeout interrupt
      if ((BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_MASK & intr) != 0) {        //The timout interrupt is enabled
        if(debug >= DEBUG_INFORMATION)  
          printk(KERN_INFO "BCCIF:Timeout interrupt occured\n");  
        spin_lock(&bccif->flaglock);
        bccif->flags |= TIMEOUT;                      //Set the timeout flag
        spin_unlock(&bccif->flaglock);
    }}
}

/* Install an implementation of the ISR based on the passed device info structure */
/* Structure of this is that there is 1 ISR handler for every open called */
/* Enables interrupts on the PCI bridge */
int Install_ISR(bccif_info_struct *bccif)
{
  int result;                                    //Variable used to handle function calls return values
  
  //Install the interrupt handler with sharing enabled
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
    result = request_irq(bccif->irq, BCC_Interrupt, SA_SHIRQ, "BCC Interface", bccif);  
#else
    result = request_irq(bccif->irq, (void *) BCC_Interrupt, IRQF_SHARED, "BCC Interface", bccif);  
#endif
  
  if (result != 0) {
    printk(KERN_ERR "BCCIF:Can't assign IRQ = %d\n", bccif->irq);
    bccif->irq = NO_IRQ;
    return result;
    }
    
  //Enable interrupts in the PCI core, protect the next section with the semaphore
  if (down_interruptible(bccif->sem))
    return -ERESTARTSYS;
  
  //Clear the interrupt registers in interface
  WritePCIWord(bccif->base_Address + BCC_STATUS, 0);                //Zero the status register of the interface  
  WritePCIDWord(bccif->config_Address + ICR, INT_PROP_EN | WB_EINT_EN);      //Enable Wishbone interrupts and error interrupts on PCI core
  
  //Configure the wishbone error registers
  WritePCIDWord(bccif->config_Address + W_ERR_CS, ERR_EN);            //Enable Wishbone error reporting  
  
  //End of critical section    
  up(bccif->sem);
  
  if(debug >= DEBUG_CRIT)  
    printk(KERN_INFO "BCCIF:Interrupt handler installed for device MAJOR:%d MINOR:%d\n",  bccif->major, bccif->minor);  
  
  return 0;
}

/* Uninstall the interrupt handler for the specified device info structure */
void Remove_ISR(bccif_info_struct *bccif)
{
  /* free the interrupt handler */
    if (bccif->irq != NO_IRQ) {
    if(debug >= DEBUG_CRIT)  
      printk(KERN_INFO "BCCIF:Interrupt handler uninstalled for device MAJOR:%d MINOR:%d\n",  bccif->major, bccif->minor);
        free_irq(bccif->irq, bccif); }
}

