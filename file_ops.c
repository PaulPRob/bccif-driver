//Author: Andrew Brown
//Version: 1.12
//Description: The file operations handler, only 1 user can open a minor at once, but it can be opened many times

#include "includes.h"
#include "bcc_struct.h"
#include "regdefs.h"
#include "bcc_ioctrl.h"
#include "interrupts.h"
#include "pci.h"
#include "file_ops.h"
#include "main.h"

extern bccif_info_struct Bccif_Info;												//Master copy of the device infor structure, all file opens use this copy as a template for device info, declared in main.c
extern int debug;																	//Debug status variable, declared in main.c

//////////////////////////////////////////////////////////////////////////////////////
//                                File Read/Write                                   //
//////////////////////////////////////////////////////////////////////////////////////

/*****************************************************************************************
 Handle a read command to an interface, always reads from data reg
 arg is a pointer to an array of unsigned short ints (16 bit). Format is:
    arg[0] - address of far end
    arg[1] - data[0]
    arg[2] - data[1]
....

count is number data words
*****************************************************************************************/

/* Will return errors if interupts are enabled for interface, will always return wishbone error 
   and will return timeout or overflow if their interrupt and the global one are enabled */
ssize_t read(struct file *filp, char *arg, size_t count, loff_t *off) 
{
int transferred = 0; //Number of bytes transferred
bccif_info_struct *bccif = NULL; //Data structure of the file
short int* data;	//Pointer to the kernel copy of the data to be written
int result, ret;      	//Return value variable
unsigned long irqsave;	//Saves the current status of the irq's between spin lock calls
	
    //Get this device's particular data structure
    bccif = filp->private_data;
	
    //Create the kernel memory space for the output
    data = (short int *) kmalloc((count + 1) * sizeof(short int), GFP_KERNEL);

    //If the memory space was unavailable then exit with a memory warning
    if (data == NULL)
      return -ENOMEM;		
					
    //Copy the address value to kernel space
    result = copy_from_user(data, arg, sizeof(short int));

    //If we can't get the data to write then exit
    if (result != 0) {
      ret = -EFAULT;
      goto exit; }
			
    //Start semaphore protection
    if(down_interruptible(bccif->sem)) {
      ret = -ERESTARTSYS;
      goto exit; }

    //Ensure the exit flags are cleared before we start, and disable interrupts while we clear the flags
    spin_lock_irqsave(&bccif->flaglock, irqsave);
    bccif->flags &= FLAGS_MASK - (TIMEOUT + OVERFLOW + WB_ERROR);	
    spin_unlock_irqrestore(&bccif->flaglock, irqsave);			
				
    //Read from the data register count times
    for(transferred = 0; transferred < (count + 1); transferred++) {

      if (transferred == 0)		//First word contains the address data
	WritePCIWord(bccif->base_Address + BCC_ADDRESS_OFFSET, data[transferred]);
      else      //Loop until all data has been received or an error occurs
	data[transferred] = ReadPCIWord(bccif->base_Address + BCC_DATA);			
		
      //Check if an interrupt has occured, at full speed there is about a 5 word interrupt lag,
      // between when the errors occur and when it is detected		
      spin_lock_irqsave(&bccif->flaglock, irqsave);
      if ((bccif->flags & TIMEOUT) != 0 || (bccif->flags & OVERFLOW) != 0 || (bccif->flags & WB_ERROR) != 0){								//Test for an exit command from the ISR
	if ((bccif->flags & WB_ERROR) != 0)
	  transferred = -EBADMSG;
	else
	  transferred = 0;
	spin_unlock_irqrestore(&bccif->flaglock, irqsave);							
	if(debug >= DEBUG_INFORMATION)	
	  printk(KERN_INFO "BCCIF:Dropped out of write loop early\n");				
	break;}
	
      spin_unlock_irqrestore(&bccif->flaglock, irqsave);}
				
    //Release semaphore
    up(bccif->sem);	
		
    //Copy the data to user space
    result = copy_to_user(arg, data, transferred*sizeof(short int));

    //If the data sould not be copied bomb out
    if (result != 0) {
      ret = -EFAULT;
      goto exit; }	
				
    ret = transferred;

    if(debug >= DEBUG_IFRW)	
      printk(KERN_INFO "BCCIF:Read completed on MAJOR:%d MINOR:%d, read %d WORDS\n",  bccif->major, bccif->minor, transferred);					
			
exit:
    kfree(data);  		
    return ret;	
}

/*****************************************************************************************
 Handle a write command to an interface, always writes to data reg
 arg is a pointer to an arrya of unsugned short ints (16 bit). Format is:
    arg[0] - address of far end
    arg[1] - data[0]
    arg[2] - data[1]
....

count is number data words
*****************************************************************************************/

/* Will return errors if interupts are enabled for interface, will always return wishbone error 
   and will return timeout or overflow if their interrupt and the global one are enabled */
ssize_t write(struct file *filp, const char *arg, size_t count, loff_t *off) 
{
int transferred = 0;			//Number of bytes transferred
bccif_info_struct *bccif = NULL; 	//Data structure of the file
unsigned short int* data; 			//Pointer to the kernel copy of the data to be written
int result, ret;			//Return value variable
unsigned long irqsave;                  //Saves the current status of the irq's between spin lock calls
				
    bccif = filp->private_data;
	
    //Create the memory space
    data = (short int *) kmalloc((count + 1) * sizeof(short int), GFP_KERNEL);
	
    //If memory cannot be allocated then exit
    if (data == NULL)
      return -ENOMEM;
		
    //Copy the data to kernel space
    result = copy_from_user(data, arg, (count + 1) * sizeof(short int));

    //printk("<1>BCCIF: Write: %x %x\n", data[0], data[1]);

    //If we can't get the data to write then exit
    if (result != 0) {
      ret = -EFAULT;
      goto exit; }
			
    //Start semaphore protection of the pci transaction
    if(down_interruptible(bccif->sem)) {
      ret = -ERESTARTSYS;
      goto exit; }
		
    //Ensure the exit flags are cleared before we start, and disable interrupts
    spin_lock_irqsave(&bccif->flaglock, irqsave);
    bccif->flags &= FLAGS_MASK - (TIMEOUT + OVERFLOW + WB_ERROR);	
    spin_unlock_irqrestore(&bccif->flaglock, irqsave);			
		
    //The input data is of the form 16bits of Address and then remainder data
    for(transferred = 0; transferred < (count + 1); transferred++) {
      //Loop until all data has been transferred, exits on timeout error
      if (transferred == 0) {		//First word contains the address data
	// printk("<1>BCCIF:WRITE: Setting Address reg to: %x\n", data[transferred]);
	WritePCIWord(bccif->base_Address + BCC_ADDRESS_OFFSET, data[transferred]);
      }
      else  { //Everything else is data, only writes to data buffer through here, others selected through ioctl
	// printk("<1>BCCIF:WRITE: Setting data reg to: %x\n", data[transferred]);
	WritePCIWord(bccif->base_Address + BCC_DATA, data[transferred]);
      }				

      //Check if an interrupt has occured, there is about a 6 word lag on the interrupt
      spin_lock_irqsave(&bccif->flaglock, irqsave);
      if ((bccif->flags & (TIMEOUT + OVERFLOW + WB_ERROR)) != 0) {	//Test for an exit command from the ISR
	if ((bccif->flags & WB_ERROR) != 0) {
	  transferred = -EBADMSG;
	  // printk("<1>BCCIF: WRITE: error:WB_ERROR\n");
	}
	else {
	  // printk("<1>BCCIF: WRITE: error:TIMEOUT + OVERFLOW\n");
	  transferred = 0;
	}			
	spin_unlock_irqrestore(&bccif->flaglock, irqsave);							
	if(debug >= DEBUG_INFORMATION)	
	  printk(KERN_INFO "BCCIF:Dropped out of write loop early\n");			
	break;}		
      spin_unlock_irqrestore(&bccif->flaglock, irqsave);
    }
    
    //Release semaphore
    up(bccif->sem);	
	
    //return number of words read	
    ret = transferred;
    // printk("<1>BCCIF: WRITE: retval:%x\n", ret);

    if(debug >= DEBUG_IFRW)
      printk(KERN_INFO "BCCIF:Write completed on MAJOR:%d MINOR:%d, wrote %d WORDS\n",
	     bccif->major, bccif->minor, transferred);			
    
 exit:
    kfree(data);
    return(ret);
}

//////////////////////////////////////////////////////////////////////////////////////
//                                 IOCTL Command                                    //
//////////////////////////////////////////////////////////////////////////////////////

/* Handle IO commands to an interface device */
/* The commands are stored in the bcc_ioctrl.h file */
int ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
  int type;	// The type and operation command
  int ret = 0;	// Error variable
  int addr = 0;
  int loops, speed;
  int minor = 0;
  unsigned long int btf, br, nb;
  unsigned short int result;   //Storage variable used for Read Modify Write cycles
  unsigned int resultD;		//DWORD length storage variable
  unsigned char resultB;		//Byte length storage variable
  unsigned char *bp;		//Byte pointer
  unsigned short int data;	//Word length storage variable
  bccif_info_struct *bccif = NULL;  //Data structure of the open file
  Bccif_data bccif_data;
	
    //Get a pointer to the device's data structure
    bccif = filp->private_data;
	   
    //Get the type of command, i.e. the magic number
    type =_IOC_TYPE(cmd);

    if(debug >= DEBUG_IF)
    	printk(KERN_INFO "BCCIF:Ioctl being called: type = %x, op = %x\n", type, _IOC_NR(cmd));  
    
    //Only respond to commands directed at it
    if (type != BCC_IOC_MAGIC) return(-ENOTTY);

    //Check the direction of the command, and that the memory address supplied is located in user space
    if (_IOC_DIR(cmd) & _IOC_READ)
    	ret = !access_ok(VERIFY_WRITE, (void *) arg, _IOC_SIZE(cmd)); //write is from the kernel's point of view, so backwards
    else if  (_IOC_DIR(cmd) & _IOC_WRITE)
    	ret = !access_ok(VERIFY_READ, (void *) arg, _IOC_SIZE(cmd));
	
    //If somthing funny is going on then bomb out
    if (ret) return -EFAULT;	

    //Assume sucess initially	
    ret = 0;
	
    //Handle the request, all writes to the status register are bytes, so as not to clear the interrupts
    switch (cmd) {

/////////////////////////////////////////////////////////////////////////////////////
//                 Handle general commands, mode select, reset, etc                //
/////////////////////////////////////////////////////////////////////////////////////	    
    case BCC_RESET_INTERFACES:  //Signal a reset to the wishbone bus
      if (! capable(CAP_SYS_ADMIN)) {	//Make sure the user has admin privilages to reset the interfaces
	ret = -EPERM;
	break;
      }
	    		
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break;
      }

      //Clear interrupt flags before using them
      clearallflags(bccif);
						
      //Perform a RMW cycle
      resultD = ReadPCIDWord(bccif->config_Address + ICR);
      resultD |= SW_RST;						
      WritePCIDWord(bccif->config_Address + ICR, resultD);				//Set the reset flag
			
      //Do delay to create reset pulse
      udelay(1); 	//Delay for 1 uS
      resultD = ReadPCIDWord(bccif->config_Address + ICR);
      resultD &= (0xFFFFFFFF - SW_RST);						
      WritePCIDWord(bccif->config_Address + ICR, resultD);				//Clear the reset flag
      //Check for wb error
      ret = checkwbflag(bccif);
												
      //End of critical section
      up(bccif->sem);			
		
      if(debug >= DEBUG_IF){
	if (ret == 0)	
	  printk(KERN_INFO "BCCIF:Wishbone bus reset\n"); 			
	else
	  printk(KERN_INFO "BCCIF:Wishbone bus reset failed - error %d\n", ret);
      }
	      
      break;
    case BCC_SET_MK2_MODE:	//Enable MK2 mode on this interface
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }

      //Clear interrupt flags before using them
      clearallflags(bccif);			
						
      //Perform a RMW cycle
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);
      result |= BCC_MK2_MODE_SELECT_MASK;
      WritePCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET, result);

      //Check for wb error
      ret = checkwbflag(bccif);	
						
      //End of critical section
      up(bccif->sem);			
		
      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:MK2 mode enabled\n"); 
	else
	  printk(KERN_INFO "BCCIF:MK2 mode set failed - error %d\n", ret); }
      
      break;
    case BCC_SET_MK1_MODE:	//Enable MK1 mode on this interface
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break;
      }

      //Clear interrupt flags before using them
      clearallflags(bccif);			
      
      //Perform a RMW cycle
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);
      result &= (0xFFFF - BCC_MK2_MODE_SELECT_MASK);
      WritePCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET, result);

      //Check for wb error
      ret = checkwbflag(bccif);	
						
      //End of critical section
      up(bccif->sem);			
		
      if(debug >= DEBUG_IF) {
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:MK1 mode enabled\n"); 
	else
	  printk(KERN_INFO "BCCIF:MK1 mode set failed - error %d\n", ret);}
      
      break;
    case BCC_GET_MK_MODE: //Get the actual mode of the interface, if overriden by jumpers it will still return the operating mode
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break;
      }		

      //Clear interrupt flags before using them
      clearallflags(bccif);			
					
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);

      //Check for wb error
      ret = checkwbflag(bccif);		
						
      //End of critical section
      up(bccif->sem);	
						
      result &= BCC_MK2_MODE_SELECT_MASK;			
			
      //Remap the result to be 0 or 1
      if (result != 0)
	data = 1;
      else
	data = 0;
			
      //Put the data into the user space
      if (ret == 0) ret = __put_user(data, (int*) arg);

      if(debug >= DEBUG_IF) {
	if (ret == 0)	
	  printk(KERN_INFO "BCCIF:MK2 determined to be:%d\n", data);			
	else
	  printk(KERN_INFO "BCCIF:MK2 mode get failed - error %d\n", ret);
      }
      
      break;			
				    	
/////////////////////////////////////////////////////////////////////////////////////
//             Handle the auto increment setting, clearing and reading             //
/////////////////////////////////////////////////////////////////////////////////////
    case BCC_SET_INCREMENT_ENABLE:  //Set the auto increment flag		
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break;
      }

      //Clear interrupt flags before using them
      clearallflags(bccif);				
						
      //Perform a RMW cycle
      result = ReadPCIWord(bccif->base_Address + BCC_ADDRESS_INCREMENT_ENABLE_OFFSET);
      result |= BCC_ADDRESS_INCREMENT_ENABLE_MASK;
      WritePCIWord(bccif->base_Address + BCC_ADDRESS_INCREMENT_ENABLE_OFFSET, result);

      //Check for wb error
      ret = checkwbflag(bccif);		
						
      //End of critical section
      up(bccif->sem);			
		
      if(debug >= DEBUG_IF) {
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Address increment enabled\n");
	else
	  printk(KERN_INFO "BCCIF:Address increment set failed - error %d\n", ret); }					
      break;

    case BCC_CLR_INCREMENT_ENABLE: //Clear the auto increment flag		
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break;
      }

      //Clear interrupt flags before using them
      clearallflags(bccif);			
						
      //Perform a RMW cycle
      result = ReadPCIWord(bccif->base_Address + BCC_ADDRESS_INCREMENT_ENABLE_OFFSET);
      result &= (0xFFFF - BCC_ADDRESS_INCREMENT_ENABLE_MASK);
      WritePCIWord(bccif->base_Address + BCC_ADDRESS_INCREMENT_ENABLE_OFFSET, result);

      //Check for wb error
      ret = checkwbflag(bccif);		
						
      //End of critical section
      up(bccif->sem);			

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Address increment disabled\n"); 
	else
	  printk(KERN_INFO "BCCIF:Address increment clear failed - error %d\n", ret);
      }
      break;

    case BCC_GET_INCREMENT_ENABLE:  //Get the state of the auto increment flag
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break;
      }	

      //Clear interrupt flags before using them
      clearallflags(bccif);			
					
      result = ReadPCIWord(bccif->base_Address + BCC_ADDRESS_INCREMENT_ENABLE_OFFSET);

      //Check for wb error
      ret = checkwbflag(bccif);			
						
      //End of critical section
      up(bccif->sem);	
						
      result &= BCC_ADDRESS_INCREMENT_ENABLE_MASK;			
			
      //Remap the response to be 0 or 1
      if (result != 0)
	data = 1;
      else
	data = 0;
			
      //Put the data into the user space
      if (ret == 0) ret = __put_user(data, (int*) arg);

      if(debug >= DEBUG_IF) {
	if (ret == 0)			
	  printk(KERN_INFO "BCCIF:Address increment determined to be:%d\n", data);
	else
	  printk(KERN_INFO "BCCIF:Address increment get failed - error %d\n", ret);
      }
			
      break;
			
/////////////////////////////////////////////////////////////////////////////////////
//           Handle the software timeout setting, clearing and reading             //
/////////////////////////////////////////////////////////////////////////////////////			
    case BCC_SET_TIMEOUT_DELAY: //Set the software delay in the mode register
      ret = __get_user(data, (int *) arg);  //Get the data from user space 
      if (ret != 0) break;
			
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break;
      }

      //Clear interrupt flags before using them
      clearallflags(bccif);			
						
      //Perform a RMW cycle
      result = ReadPCIWord(bccif->base_Address + BCC_SOFTWARE_DELAY_OFFSET);
      result &= (0xFFFF - BCC_SOFTWARE_DELAY_MASK);	//Mask out the old delay
      result |= (BCC_SOFTWARE_DELAY_MASK & data);
      WritePCIWord(bccif->base_Address + BCC_SOFTWARE_DELAY_OFFSET, result);

      //Check for wb error
      ret = checkwbflag(bccif);			
						
      //End of critical section
      up(bccif->sem);			

      if(debug >= DEBUG_IF) {
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Software delay set to:%d\n", data);
	else
	  printk(KERN_INFO "BCCIF:Software delay set failed - error %d\n", ret);
      }      
      break;

    case BCC_GET_TIMEOUT_DELAY:	//Get the current value of the software delay
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		

      //Clear interrupt flags before using them
      clearallflags(bccif);			
					
      result = ReadPCIWord(bccif->base_Address + BCC_SOFTWARE_DELAY_OFFSET);
			
      //Check for wb error
      ret = checkwbflag(bccif);			
			
      //End of critical section
      up(bccif->sem);	
						
      result &= BCC_SOFTWARE_DELAY_MASK;			
					
      //Put the data into the user space, assumes that the data is in the lowest possible position of the register, which it is
      if (ret == 0) ret = __put_user(result, (int*) arg);

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Software delay determined to be:%d\n", result);
	else
	  printk(KERN_INFO "BCCIF:Software delay get failed - error %d\n", ret);
      }
      break;
					
    case BCC_CLR_SOFTWARE_TIMEOUT: //Clear the enable bit for the software timeout
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break;
      }

      //Clear interrupt flags before using them
      clearallflags(bccif);			
						
      //Perform a RMW cycle
      result = ReadPCIWord(bccif->base_Address + BCC_SOFTWARE_DELAY_ENABLE_OFFSET);
      result &= (0xFFFF - BCC_SOFTWARE_DELAY_ENABLE_MASK);
      WritePCIWord(bccif->base_Address + BCC_SOFTWARE_DELAY_ENABLE_OFFSET, result);

      //Check for wb error
      ret = checkwbflag(bccif);			
						
      //End of critical section
      up(bccif->sem);			

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Software delay disabled\n");
	else
	  printk(KERN_INFO "BCCIF:Software delay clear failed - error %d\n", ret); }
      
      break;
    case BCC_SET_SOFTWARE_TIMEOUT: //Set the enable bit for the software timeout
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }

      //Clear interrupt flags before using them
      clearallflags(bccif);				
						
      //Perform a RMW cycle
      result = ReadPCIWord(bccif->base_Address + BCC_SOFTWARE_DELAY_ENABLE_OFFSET);
      result |= BCC_SOFTWARE_DELAY_ENABLE_MASK;
      WritePCIWord(bccif->base_Address + BCC_SOFTWARE_DELAY_ENABLE_OFFSET, result);

      //Check for wb error
      ret = checkwbflag(bccif);				
						
      //End of critical section
      up(bccif->sem);			
		
      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Software delay enabled\n"); 
	else
	  printk(KERN_INFO "BCCIF:Software delay set failed - error %d\n", ret); }
      
      break;
    case BCC_GET_SOFTWARE_TIMEOUT:  //Get the current state of the software timeout enable bit
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		

      //Clear interrupt flags before using them
      clearallflags(bccif);				
					
      result = ReadPCIWord(bccif->base_Address + BCC_SOFTWARE_DELAY_ENABLE_OFFSET);

      //Check for wb error
      ret = checkwbflag(bccif);						
			
      //End of critical section
      up(bccif->sem);	
						
      result &= BCC_SOFTWARE_DELAY_ENABLE_MASK;			
			
      //Convert the data to a 1 or 0 value
      if (result != 0)
	data = 1;
      else
	data = 0;
			
      //Put the data into the user space
      if (ret == 0) ret = __put_user(data, (int*) arg);
      
      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Software delay state determined to be:%d\n", data);
	else
	  printk(KERN_INFO "BCCIF:Software delay state get failed - error %d\n", ret); }
			
      break;
			
/////////////////////////////////////////////////////////////////////////////////////
//                            Serial Number commands                               //
/////////////////////////////////////////////////////////////////////////////////////						
    case BCC_GET_SERIAL_NUMBER:		//Get the serial string
      //Check that there is a serial number
      if (bccif->IdentString == NULL){
	ret = -EBADMSG;
	break;}
			
      //Copy the data to user space
      ret = copy_to_user((char *)arg, bccif->IdentString, strlen(bccif->IdentString));
      if (ret != 0) ret =  -EFAULT;
			
      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Serial number requested - %s\n", bccif->IdentString); 
	else
	  printk(KERN_INFO "BCCIF:Serial number get failed - error %d\n", ret); }
					
      break;
    case BCC_GET_SERIAL_NUMBER_LENGTH:	//Get the length of the serial string
      //Check that there is a serial number
      if (bccif->IdentString == NULL) {
	ret = -EBADMSG;
	break;}
					
      data = (strlen(bccif->IdentString) + 1);

      //Put the data into the user space
      ret = __put_user(data, (int*) arg);
      if (ret != 0) ret =  -EFAULT;
			
      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Serial string length requested, determined to be:%d\n", data); 
	else
	  printk(KERN_INFO "BCCIF:Serial string length get failed - error %d\n", ret); }
								
      break;

/////////////////////////////////////////////////////////////////////////////////////
//                          Interrupt setting and reading                          //
/////////////////////////////////////////////////////////////////////////////////////				

    case BCC_ENABLE_GLOBAL_INTERRUPTS:	//Enable interrupts for this interface
      if (bccif->irq != NO_IRQ) {	
		
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }

	//Clear interrupt flags before using them
	clearallflags(bccif);					
							
	//Perform a RMW cycle
	resultB = ReadPCIByte(bccif->base_Address + BCC_MASTER_INTERRUPT_ENABLE_OFFSET);
	resultB |= BCC_MASTER_INTERRUPT_ENABLE_MASK;
	WritePCIByte(bccif->base_Address + BCC_MASTER_INTERRUPT_ENABLE_OFFSET, resultB);

	//Check for wb error
	ret = checkwbflag(bccif);				
							
	//End of critical section
	up(bccif->sem);			
		
	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Master interrupt enabled\n"); 
	  else
	    printk(KERN_INFO "BCCIF:Master interrupt set failed - error %d\n", ret);}
      } else 
	ret = -ENOSYS;

      break;		
    case BCC_DISABLE_GLOBAL_INTERRUPTS:  //Disable interrupts for this interface 
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }

      //Clear interrupt flags before using them
      clearallflags(bccif);				
      
      //Perform a RMW cycle
      resultB = ReadPCIByte(bccif->base_Address + BCC_MASTER_INTERRUPT_ENABLE_OFFSET);
      resultB &= (0xFF - BCC_MASTER_INTERRUPT_ENABLE_MASK);
      WritePCIByte(bccif->base_Address + BCC_MASTER_INTERRUPT_ENABLE_OFFSET, resultB);
			
      //Check for wb error
      ret = checkwbflag(bccif);			
			
      //End of critical section
      up(bccif->sem);			

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Master interrupt disabled\n");
	else
	  printk(KERN_INFO "BCCIF:Master interrupt clear failed - error %d\n", ret); }
      
      break;		
    case BCC_GET_GLOBAL_INTERRUPTS_STATE:  //Get the state of the global interrupt enable bit
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }			

      //Clear interrupt flags before using them
      clearallflags(bccif);				
					
      resultB = ReadPCIByte(bccif->base_Address + BCC_MASTER_INTERRUPT_ENABLE_OFFSET);

      //Check for wb error
      ret = checkwbflag(bccif);				
						
      //End of critical section
      up(bccif->sem);	
						
      resultB &= BCC_MASTER_INTERRUPT_ENABLE_MASK;			
			
      //Convert the data to a 1 or 0 value
      if (resultB != 0)
	data = 1;
      else
	data = 0;
			
      //Put the data into the user space
      if (ret == 0) ret = __put_user(data, (int*) arg);

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Master interrupt enabled state determined to be:%d\n", data);
	else
	  printk(KERN_INFO "BCCIF:Master interrupt get failed - error %d\n", ret);}
    			
      break; 	
    		
/////////////////////////////////////////////////////////////////////////////////////    		
    			
    case BCC_ENABLE_TIMEOUT_INTERRUPT:   //Enable timeout interrupts for this device
      if (bccif->irq != NO_IRQ) {				
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }

	//Clear interrupt flags before using them
	clearallflags(bccif);						
							
	//Perform a RMW cycle
	resultB = ReadPCIByte(bccif->base_Address + BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_OFFSET);
	resultB |= BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_MASK;
	WritePCIByte(bccif->base_Address + BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_OFFSET, resultB);

	//Check for wb error
	ret = checkwbflag(bccif);					
							
	//End of critical section
	up(bccif->sem);			
		
	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Timeout interrupt enabled\n");
	  else
	    printk(KERN_INFO "BCCIF:Timeout interrupt set failed - error %d\n", ret);}
      } else 
	ret = -ENOSYS;
						
      break;			
    case BCC_DISABLE_TIMEOUT_INTERRUPT:   //Disable timeout interrupts for this interface
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }

      //Clear interrupt flags before using them
      clearallflags(bccif);					
						
      //Perform a RMW cycle
      resultB = ReadPCIByte(bccif->base_Address + BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_OFFSET);
      resultB &= (0xFF - BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_MASK);
      WritePCIByte(bccif->base_Address + BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_OFFSET, resultB);

      //Check for wb error
      ret = checkwbflag(bccif);			
						
      //End of critical section
      up(bccif->sem);			

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Timeout interrupt disabled\n");
	else
	  printk(KERN_INFO "BCCIF:Timeout interrupt clear failed - error %d\n", ret);}
    						
      break;			
    case BCC_GET_TIMEOUT_INTERRUPT_STATE:     //Get the state of the timeout interrupt enable bit
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		

      //Clear interrupt flags before using them
      clearallflags(bccif);				
					
      resultB = ReadPCIByte(bccif->base_Address + BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_OFFSET);

      //Check for wb error
      ret = checkwbflag(bccif);				
						
      //End of critical section
      up(bccif->sem);	
						
      resultB &= BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_MASK;			
			
      //Convert the data to a 1 or 0 value
      if (resultB != 0)
	data = 1;
      else
	data = 0;
			
      //Put the data into the user space
      if (ret == 0) ret = __put_user(data, (int*) arg);

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Timeout interrupt enabled state determined to be:%d\n", data);
	else
	  printk(KERN_INFO "BCCIF:Timeout interrupt enabled get failed - error %d\n", ret);}
    			
      break; 				

/////////////////////////////////////////////////////////////////////////////////////
    		
    case BCC_ENABLE_OVERFLOW_INTERRUPT:	//Enable overflow interrupts for this interface
      if (bccif->irq != NO_IRQ) {				
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }

	//Clear interrupt flags before using them
	clearallflags(bccif);				
							
	//Perform a RMW cycle
	resultB = ReadPCIByte(bccif->base_Address + BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_OFFSET);
	resultB |= BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_MASK;
	WritePCIByte(bccif->base_Address + BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_OFFSET, resultB);

	//Check for wb error
	ret = checkwbflag(bccif);				
							
	//End of critical section
	up(bccif->sem);			
		
	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Overflow interrupt enabled\n");
	  else
	    printk(KERN_INFO "BCCIF:Overflow interrupt set failed - error %d\n", ret);}
      } else 
	ret = -ENOSYS;
      
      break;			
    case BCC_DISABLE_OVERFLOW_INTERRUPT:  //Disable overflow interrupts for this device
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }

      //Clear interrupt flags before using them
      clearallflags(bccif);			
						
      //Perform a RMW cycle
      resultB = ReadPCIByte(bccif->base_Address + BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_OFFSET);
      resultB &= (0xFF - BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_MASK);
      WritePCIByte(bccif->base_Address + BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_OFFSET, resultB);
			
      //Check for wb error
      ret = checkwbflag(bccif);				
			
      //End of critical section
      up(bccif->sem);			

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Overflow interrupt disabled\n");
	else
	  printk(KERN_INFO "BCCIF:Overflow interrupt clear failed - error %d\n", ret);}
		
      break;			
    case BCC_GET_OVERFLOW_INTERRUPT_STATE: //Get the state of the overflow interrupt enable
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		

      //Clear interrupt flags before using them
      clearallflags(bccif);			
					
      resultB = ReadPCIByte(bccif->base_Address + BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_OFFSET);
      
      //Check for wb error
      ret = checkwbflag(bccif);				
						
      //End of critical section
      up(bccif->sem);	
						
      resultB &= BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_MASK;			
			
      //Convert the data to a 1 or 0 value
      if (resultB != 0)
	data = 1;
      else
	data = 0;
      
      //Put the data into the user space
      if (ret == 0) ret = __put_user(data, (int*) arg);

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Overflow interrupt enabled state determined to be:%d\n", data);
	else
	  printk(KERN_INFO "BCCIF:Overflow interrupt enabled get failed - error %d\n", ret);}
      break;	
    		
/////////////////////////////////////////////////////////////////////////////////////    		
    							
    case BCC_GET_TIMEOUT_OCCURED_STATE:  //See if a hw timeout has occured
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		

      //Clear interrupt flags before using them
      clearallflags(bccif);				
					
      resultB = ReadPCIByte(bccif->base_Address + BCC_TIMEOUT_OCCURED_OFFSET);

      //Check for wb error
      ret = checkwbflag(bccif);			
						
      //End of critical section
      up(bccif->sem);	
						
      resultB &= BCC_TIMEOUT_OCCURED_MASK;			
			
      //Convert the data to a 1 or 0 value
      if (resultB != 0)
	data = 1;
      else
	data = 0;
			
      //Put the data into the user space
      if (ret == 0) ret = __put_user(data, (int*) arg);

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Timeout occured flag state determined to be:%d\n", data);
	else
	  printk(KERN_INFO "BCCIF:Timeout occured flag get failed - error %d\n", ret);}
      
      break;		
    case BCC_CLR_TIMEOUT_OCCURED_STATE: //Clear the timeout occured bit in the status register
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }	

      //Clear interrupt flags before using them
      clearallflags(bccif);			
					
      //To clear the bit it has to be written to with a 1
      resultB = ReadPCIByte(bccif->base_Address + BCC_TIMEOUT_OCCURED_OFFSET);
      resultB |= BCC_TIMEOUT_OCCURED_MASK;
      WritePCIByte(bccif->base_Address + BCC_TIMEOUT_OCCURED_OFFSET, resultB);
      
      //Check for wb error
      ret = checkwbflag(bccif);				
									
      //End of critical section
      up(bccif->sem);	
						
      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Timeout occured flag reset\n");
	else
	  printk(KERN_INFO "BCCIF:Timeout occured flag clear failed - error %d\n", ret);}
    			
      break;
		
///////////////////////////////////////////////////////////////////////////////////// 		
				
    case BCC_GET_OVERFLOW_OCCURED_STATE:   //See if an address overflow has occured
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		

      //Clear interrupt flags before using them
      clearallflags(bccif);				
					
      result = ReadPCIWord(bccif->base_Address + BCC_OVERFLOW_OFFSET);

      //Check for wb error
      ret = checkwbflag(bccif);				
						
      //End of critical section
      up(bccif->sem);	
						
      result &= BCC_OVERFLOW_MASK;			
			
      //Convert the data to a 1 or 0 value
      if (result != 0)
	data = 1;
      else
	data = 0;
			
      //Put the data into the user space
      if (ret == 0) ret = __put_user(data, (int*) arg);

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Overflow occured flag state determined to be:%d\n", data);
	else
	  printk(KERN_INFO "BCCIF:Overflow occured flag get failed - error %d\n", ret);}
    			
      break;					
    case BCC_CLR_OVERFLOW_OCCURED_STATE:  //Clear the overflow occured bit in the address register
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		

      //Clear interrupt flags before using them
      clearallflags(bccif);					
					
      //Clear the msb of the address register
      result = ReadPCIWord(bccif->base_Address + BCC_OVERFLOW_OFFSET);
      result &= (0xFFFF - BCC_OVERFLOW_MASK);
      WritePCIWord(bccif->base_Address + BCC_OVERFLOW_OFFSET, result);

      //Check for wb error
      ret = checkwbflag(bccif);			
									
      //End of critical section
      up(bccif->sem);	
						
      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Overflow occured flag reset\n");
	else
	  printk(KERN_INFO "BCCIF:Overflow occured flag clear failed - error %d\n", ret);}

      break;
				
/////////////////////////////////////////////////////////////////////////////////////
//                            MK1 Direct Register Access                           //
/////////////////////////////////////////////////////////////////////////////////////

    case BCC_SET_ADDRESS: //Set the 15 bits of the address register	
      //Get the data from user space
      ret = __get_user(data, (int *) arg);							
      if (ret != 0) break;
		
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		

      //Clear interrupt flags before using them
      clearallflags(bccif);			
					
      //Load the Masked address into the addres register
      result = ReadPCIWord(bccif->base_Address + BCC_ADDRESS_OFFSET);
      result &= (0xFFFF - BCC_ADDRESS_MASK);
      result |= (data & BCC_ADDRESS_MASK);
      WritePCIWord(bccif->base_Address + BCC_ADDRESS_OFFSET, result);

      //Check for wb error
      ret = checkwbflag(bccif);			
									
      //End of critical section
      up(bccif->sem);	
						
      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Address register set to:%x\n", data);
	else
	  printk(KERN_INFO "BCCIF:Address register set failed - error %d\n", ret);}
      
      break;
    case BCC_GET_ADDRESS:  //Get the current address
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }			

      //Clear interrupt flags before using them
      clearallflags(bccif);			
					
      //Load the Masked address into the addres register
      result = ReadPCIWord(bccif->base_Address + BCC_ADDRESS_OFFSET);
      result &= BCC_ADDRESS_MASK;

      //Check for wb error
      ret = checkwbflag(bccif);			
			
      //End of critical section
      up(bccif->sem);				
						
      //Put the data into the user space
      if (ret == 0) ret = __put_user(result, (int*) arg);

      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Current address determined to be:%x\n", result);
	else
	  printk(KERN_INFO "BCCIF:Current address get failed - error %d\n", ret);}
			
      break;


    case BCC_READ_IF_REGISTER:	//Read register given in parameter

      ret = copy_from_user(&bccif_data, (Bccif_data *)arg, sizeof(Bccif_data));
      if (ret != 0) {
	ret = -EFAULT;
	break;
      }

      addr = bccif_data.address * 4;

      if(addr >= BCC_SPACE) {
	ret = -EFAULT;
	break;
      }


      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break;
      }	

      //Clear interrupt flags before using them
      clearallflags(bccif);
      
      //Check for wb error
      ret = checkwbflag(bccif);			
			
      //End of critical section
      up(bccif->sem);	
      
      if(ret == 0) {			

	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break;
	}			
			
	//Clear interrupt flags before using them
	clearallflags(bccif);					
					
	//Load the data out of register 0
	result = ReadPCIWord(bccif->base_Address + addr);

	bccif_data.data = result;
	// printk("<1>BCCIF:IOCTL:Read from reg addr:%x data:%x\n",addr,  bccif_data.data);

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);				
				
	//End of critical section
	up(bccif->sem);							
	
	ret = copy_to_user((Bccif_data *)arg, &bccif_data, sizeof(Bccif_data));
	if (ret != 0) {
	  ret = -EFAULT;			
	  break;
	}
	
	if(debug >= DEBUG_IF) printk(KERN_INFO "BCCIF:Register:%x Address:%x Data::%x\n",
				     bccif_data.address, addr, bccif_data.data);
      }			
			
      break;		

    case BCC_READ_REGISTER_0:	//Read from the MK1 register 0
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }	

      //Clear interrupt flags before using them
      clearallflags(bccif);				
						
      //Can only be read in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			

      //Check for wb error
      ret = checkwbflag(bccif);			
			
      //End of critical section
      up(bccif->sem);	
									
      if (result == 0 && ret == 0) {			
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }			
			
	//Clear interrupt flags before using them
	clearallflags(bccif);					
					
	//Load the data out of register 0
	result = ReadPCIWord(bccif->base_Address + BCC_REGISTER0);

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);				
				
	//End of critical section
	up(bccif->sem);							
				
	//Put the data into the user space
	if (ret == 0) ret = __put_user(result, (int*) arg);

	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 0 data determined to be:%x\n", result);
	  else
	    printk(KERN_INFO "BCCIF:Register 0 data get failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 0 data requested but in MK2 mode\n");
	ret = -EINVAL; }			
			
      break;		

    case BCC_READ_REGISTER_1:	//Read from the MK1 register 1
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }	

      //Clear interrupt flags before using them
      clearallflags(bccif);				
						
      //Can only be read in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			

      //Check for wb error
      ret = checkwbflag(bccif);			
			
      //End of critical section
      up(bccif->sem);	
									
      if (result == 0 && ret == 0) {			
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }			
			
	//Clear interrupt flags before using them
	clearallflags(bccif);					
					
	//Load the data out of register 1
	result = ReadPCIWord(bccif->base_Address + BCC_REGISTER1);

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);				
				
	//End of critical section
	up(bccif->sem);							
				
	//Put the data into the user space
	if (ret == 0) ret = __put_user(result, (int*) arg);

	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 1 data determined to be:%x\n", result);
	  else
	    printk(KERN_INFO "BCCIF:Register 1 data get failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 1 data requested but in MK2 mode\n");
	ret = -EINVAL; }			
			
      break;				
    case BCC_READ_REGISTER_2:	//Read from the MK1 register 2
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }	

      //Clear interrupt flags before using them
      clearallflags(bccif);				
						
      //Can only be read in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			

      //Check for wb error
      ret = checkwbflag(bccif);			
			
      //End of critical section
      up(bccif->sem);	
									
      if (result == 0 && ret == 0) {			
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }			
			
	//Clear interrupt flags before using them
	clearallflags(bccif);					
					
	//Load the data out of register 2
	result = ReadPCIWord(bccif->base_Address + BCC_REGISTER2);

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);				
				
	//End of critical section
	up(bccif->sem);							
				
	//Put the data into the user space
	if (ret == 0) ret = __put_user(result, (int*) arg);

	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 2 data determined to be:%x\n", result);
	  else
	    printk(KERN_INFO "BCCIF:Register 2 data get failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 2 data requested but in MK2 mode\n");
	ret = -EINVAL; }			
			
      break;					
    case BCC_READ_REGISTER_3:	//Read from the MK1 register 3
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }	

      //Clear interrupt flags before using them
      clearallflags(bccif);				
						
      //Can only be read in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			

      //Check for wb error
      ret = checkwbflag(bccif);			
			
      //End of critical section
      up(bccif->sem);	
									
      if (result == 0 && ret == 0) {			
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }			
			
	//Clear interrupt flags before using them
	clearallflags(bccif);					
					
	//Load the data out of register 3
	result = ReadPCIWord(bccif->base_Address + BCC_REGISTER3);

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);				
				
	//End of critical section
	up(bccif->sem);							
				
	//Put the data into the user space
	if (ret == 0) ret = __put_user(result, (int*) arg);

	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 3 data determined to be:%x\n", result);
	  else
	    printk(KERN_INFO "BCCIF:Register 3 data get failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 3 data requested but in MK2 mode\n");
	ret = -EINVAL; }			
			
      break;	
    case BCC_READ_REGISTER_4: //Read from the MK1 register 4
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }	

      //Clear interrupt flags before using them
      clearallflags(bccif);				
						
      //Can only be read in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			

      //Check for wb error
      ret = checkwbflag(bccif);			
			
      //End of critical section
      up(bccif->sem);	
									
      if (result == 0 && ret == 0) {			
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }			
			
	//Clear interrupt flags before using them
	clearallflags(bccif);					
					
	//Load the data out of register 4
	result = ReadPCIWord(bccif->base_Address + BCC_REGISTER4);

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);				
				
	//End of critical section
	up(bccif->sem);							
				
	//Put the data into the user space
	if (ret == 0) ret = __put_user(result, (int*) arg);

	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 4 data determined to be:%x\n", result);
	  else
	    printk(KERN_INFO "BCCIF:Register 4 data get failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 4 data requested but in MK2 mode\n");
	ret = -EINVAL; }			
			
      break;			
    case BCC_READ_REGISTER_5:	//Read from the MK1 register 5
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }	

      //Clear interrupt flags before using them
      clearallflags(bccif);				
						
      //Can only be read in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			

      //Check for wb error
      ret = checkwbflag(bccif);			
			
      //End of critical section
      up(bccif->sem);	
									
      if (result == 0 && ret == 0) {			
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }			
			
	//Clear interrupt flags before using them
	clearallflags(bccif);					
					
	//Load the data out of register 5
	result = ReadPCIWord(bccif->base_Address + BCC_REGISTER5);

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);				
				
	//End of critical section
	up(bccif->sem);							
	
	//Put the data into the user space
	if (ret == 0) ret = __put_user(result, (int*) arg);

	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 5 data determined to be:%x\n", result);
	  else
	    printk(KERN_INFO "BCCIF:Register 5 data get failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 5 data requested but in MK2 mode\n");
	ret = -EINVAL; }			
      
      break;


    case BCC_WRITE_IF_REGISTER:	//Write to register at address given in parameter

      ret = copy_from_user(&bccif_data, (Bccif_data *)arg, sizeof(Bccif_data));
      if (ret != 0) {
	ret = -EFAULT;
	break;
      }

      // printk("<1>BCCIF:IOCTL:Writing to reg addr:%x data:%x\n",bccif_data.address,  bccif_data.data);
      addr = bccif_data.address * 4;

      if(addr >= BCC_SPACE) {
	ret = -EFAULT;
	break;
      }

      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break;
      }	

      //Clear interrupt flags before using them
      clearallflags(bccif);
      
      //Check for wb error
      ret = checkwbflag(bccif);			
			
      //End of critical section
      up(bccif->sem);	
      
      if(ret == 0) {			

	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break;
	}			
			
	//Clear interrupt flags before using them
	clearallflags(bccif);					
					
	//Load the data out of register 0
	WritePCIWord(bccif->base_Address + addr, bccif_data.data);

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);				
				
	//End of critical section
	up(bccif->sem);							
	
	if(debug >= DEBUG_IF) printk(KERN_INFO "BCCIF:Write to Register:%x Address:%x Data::%x\n",
				     bccif_data.address, addr, bccif_data.data);
      }			
			
      break;		

    case BCC_WRITE_REGISTER_0:	//Write to the MK1 register 0
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		
			
      //Clear interrupt flags before using them
      clearallflags(bccif);				
				
      //Can only be written in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			

      //Check for wb error
      ret = checkwbflag(bccif);				
			
      //End of critical section
      up(bccif->sem);			

      if (result == 0 && ret == 0) {			
	ret = __get_user(data, (int *) arg);						//Get the data from user space
	if (ret != 0) break; 
	
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }		
	
	//Clear interrupt flags before using them
	clearallflags(bccif);						
					
	//Load the data into register 0
	WritePCIWord(bccif->base_Address + BCC_REGISTER0, data);				

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);					
				
	//End of critical section
	up(bccif->sem);							
				
	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 0 data written:%x\n", data);
	  else
	    printk(KERN_INFO "BCCIF:Register 0 data write failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 0 write requested but in MK2 mode\n");
	ret = -EINVAL; }						
      
      break;
    case BCC_WRITE_REGISTER_1:											//Write to the MK1 register 1
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		
			
      //Clear interrupt flags before using them
      clearallflags(bccif);				
				
      //Can only be written in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			
      
      //Check for wb error
      ret = checkwbflag(bccif);				
      
      //End of critical section
      up(bccif->sem);			
      
      if (result == 0 && ret == 0) {			
	ret = __get_user(data, (int *) arg);						//Get the data from user space
	if (ret != 0) break; 
	
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }		
		
	//Clear interrupt flags before using them
	clearallflags(bccif);						
					
	//Load the data into register 1
	WritePCIWord(bccif->base_Address + BCC_REGISTER1, data);				

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);					
				
	//End of critical section
	up(bccif->sem);							
				
	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 1 data written:%x\n", data);
	  else
	    printk(KERN_INFO "BCCIF:Register 1 data write failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 1 write requested but in MK2 mode\n");
	ret = -EINVAL; }						
					
      break;
    case BCC_WRITE_REGISTER_2:											//Write to the MK1 register 2
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		
			
      //Clear interrupt flags before using them
      clearallflags(bccif);				
				
      //Can only be written in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			

      //Check for wb error
      ret = checkwbflag(bccif);				
			
      //End of critical section
      up(bccif->sem);			

      if (result == 0 && ret == 0) {			
	ret = __get_user(data, (int *) arg);						//Get the data from user space
	if (ret != 0) break; 
	
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }		
		
	//Clear interrupt flags before using them
	clearallflags(bccif);						
					
	//Load the data into register 2
	WritePCIWord(bccif->base_Address + BCC_REGISTER2, data);				

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);					
				
	//End of critical section
	up(bccif->sem);							
				
	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 2 data written:%x\n", data);
	  else
	    printk(KERN_INFO "BCCIF:Register 2 data write failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 2 write requested but in MK2 mode\n");
	ret = -EINVAL; }						
					
      break;
    case BCC_WRITE_REGISTER_3:											//Write to the MK1 register 3
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		
			
      //Clear interrupt flags before using them
      clearallflags(bccif);				
				
      //Can only be written in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			

      //Check for wb error
      ret = checkwbflag(bccif);				
			
      //End of critical section
      up(bccif->sem);			
      
      if (result == 0 && ret == 0) {			
	ret = __get_user(data, (int *) arg);						//Get the data from user space
	if (ret != 0) break; 
		
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }		
		
	//Clear interrupt flags before using them
	clearallflags(bccif);						
					
	//Load the data into register 3
	WritePCIWord(bccif->base_Address + BCC_REGISTER3, data);				

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);					
				
	//End of critical section
	up(bccif->sem);							
				
	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 3 data written:%x\n", data);
	  else
	    printk(KERN_INFO "BCCIF:Register 3 data write failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 3 write requested but in MK2 mode\n");
	ret = -EINVAL; }						
					
      break;
    case BCC_WRITE_REGISTER_4:	//Write to the MK1 register 4
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		
			
      //Clear interrupt flags before using them
      clearallflags(bccif);				
				
      //Can only be written in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			

      //Check for wb error
      ret = checkwbflag(bccif);				
			
      //End of critical section
      up(bccif->sem);			

      if (result == 0 && ret == 0) {			
	ret = __get_user(data, (int *) arg);						//Get the data from user space
	if (ret != 0) break; 
	
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }		
		
	//Clear interrupt flags before using them
	clearallflags(bccif);						
					
	//Load the data into register 4
	WritePCIWord(bccif->base_Address + BCC_REGISTER4, data);				

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);					
				
	//End of critical section
	up(bccif->sem);							
				
	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 4 data written:%x\n", data);
	  else
	    printk(KERN_INFO "BCCIF:Register 4 data write failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 4 write requested but in MK2 mode\n");
	ret = -EINVAL; }						
					
      break;
    case BCC_WRITE_REGISTER_5:											//Write to the MK1 register 5
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		
			
      //Clear interrupt flags before using them
      clearallflags(bccif);				
				
      //Can only be written in MK1 mode
      result = ReadPCIWord(bccif->base_Address + BCC_MK2_MODE_SELECT_OFFSET);				
      result &= BCC_MK2_MODE_SELECT_MASK;			

      //Check for wb error
      ret = checkwbflag(bccif);				
			
      //End of critical section
      up(bccif->sem);			

      if (result == 0 && ret == 0) {			
	ret = __get_user(data, (int *) arg);						//Get the data from user space
	if (ret != 0) break; 
	
	//Start semaphore protection
	if(down_interruptible(bccif->sem)) {
	  ret = -ERESTARTSYS;
	  break; }		
		
	//Clear interrupt flags before using them
	clearallflags(bccif);						
	
	//Load the data into register 5
	WritePCIWord(bccif->base_Address + BCC_REGISTER5, data);				

	//Check for wb and timeout error
	ret = checkwbtimeoutflags(bccif);					
				
	//End of critical section
	up(bccif->sem);							
				
	if(debug >= DEBUG_IF){
	  if (ret == 0)
	    printk(KERN_INFO "BCCIF:Register 5 data written:%x\n", data);
	  else
	    printk(KERN_INFO "BCCIF:Register 5 data write failed - error %d\n", ret);}
      }else {
	if(debug >= DEBUG_IF)
	  printk(KERN_WARNING "BCCIF:Register 5 write requested but in MK2 mode\n");
	ret = -EINVAL; }						
					
      break;
		
/////////////////////////////////////////////////////////////////////////////////////
//                         Direct Internal Register Access                         //
/////////////////////////////////////////////////////////////////////////////////////

      //Perform a direct write to one of this interface's internal registers,
      // must have admin rights
    case BCC_WRITE_WORD_IF_REGISTER:
    case BCC_WRITE_BYTE_IF_REGISTER:
      if (! capable(CAP_SYS_ADMIN)) { 	//Make sure the user has admin privilages
	ret = -EPERM;
	break;}
      
      ret = copy_from_user(&bccif_data, (Bccif_data *)arg, sizeof(Bccif_data));
      if (ret != 0) {
	ret = -EFAULT;
	break;}
			
      //Check we don't cross a minor boundry
      if (bccif_data.address > BCCIF_REGION_SIZE) {
	ret = -EFAULT;
	break;}
				
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }		
		
      //Clear interrupt flags before using them
      clearallflags(bccif);
				
      switch (cmd) {
      case BCC_WRITE_BYTE_IF_REGISTER:
	WritePCIByte(bccif->base_Address + bccif_data.address,
		     bccif_data.data);	break;

      default:				
	//Load the data into the correct register
	WritePCIWord(bccif->base_Address + bccif_data.address,
		     bccif_data.data);				
	break;
      }

      //Check for wb and timeout error
      ret = checkwbtimeoutflags(bccif);			
			
      //End of critical section
      up(bccif->sem);						
										
      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Data:%x sucessfully written to Register Address:%x\n",
		 bccif_data.data, bccif_data.address);									
	else
	  printk(KERN_INFO "BCCIF:Register %x data write failed - error %d\n",bccif_data.address, ret);}
      
      break;

      //Perform a direct read from one of this interface's internal
      // registers, must have admin rights
    case BCC_READ_WORD_IF_REGISTER:
    case BCC_READ_BYTE_IF_REGISTER:
      if (! capable(CAP_SYS_ADMIN)) { //Make sure the user has admin privilages
	ret = -EPERM;
	break;}
			
      ret = copy_from_user(&bccif_data, (Bccif_data *)arg, sizeof(Bccif_data));
      if (ret != 0) {
	ret = -EFAULT;
	break;}			

      //Check we don't cross a minor boundry
      if (bccif_data.address > BCCIF_REGION_SIZE) {				
	ret = -EFAULT;
	break;}
							
      //Start semaphore protection
      if(down_interruptible(bccif->sem)) {
	ret = -ERESTARTSYS;
	break; }			
		
      //Clear interrupt flags before using them
      clearallflags(bccif);					
				
      //Get the data from the correct address
      switch(cmd) {
      case BCC_READ_BYTE_IF_REGISTER:
	bccif_data.data = ReadPCIByte(bccif->base_Address +
				      bccif_data.address);
	break;

      default:
	bccif_data.data = ReadPCIWord(bccif->base_Address +
				      bccif_data.address);
	break;
      }

      //Check for wb and timeout error
      ret = checkwbtimeoutflags(bccif);				
			
      //End of critical section
      up(bccif->sem);						
										
      ret = copy_to_user((Bccif_data *)arg, &bccif_data, sizeof(Bccif_data));
      if (ret != 0) {
	ret = -EFAULT;			
	break;}
			
      if(debug >= DEBUG_IF){
	if (ret == 0)
	  printk(KERN_INFO "BCCIF:Data:%x sucessfully read from Register Address:%x\n",
		 bccif_data.data, bccif_data.address);									
	else
	  printk(KERN_INFO "BCCIF:Register %x data read failed - error %d\n",bccif_data.address, ret);}
    								
      break;

  
    case BCC_WRITE_JTAG_DEVICE: //Write data from buffer pointed to by parameter
      minor = MINOR((*inode).i_rdev);
      if(minor != 4) {
	ret = -EPERM;
	break;
      }
	

      ret = copy_from_user(&btf, (unsigned long int *) arg,
			   sizeof(unsigned long int));
      if (ret != 0) {
	ret = -EFAULT;
	break;
      }

      speed = ((btf >> BCCIF_JTAG_SPEED_BIT_OFFSET) & BCCIF_JTAG_SPEED_MASK)
		    << 13;
      btf = btf & ~(BCCIF_JTAG_SPEED_MASK << BCCIF_JTAG_SPEED_BIT_OFFSET);
      nb = btf;

      //Set the Speed 
      WritePCIWord(bccif->base_Address + BCCIF_JTAG_MASTER_REG,
		   speed);
      br = 0;

      if(debug >= DEBUG_IF) {
	printk(KERN_INFO "BCCIF:Start JTAG Device Write %lu bytes\n", btf);
	printk(KERN_INFO "BCCIF:Base Addr: %lx\n", bccif->base_Address);
      }

      //Reset the Player 
      WritePCIWord(bccif->base_Address + BCCIF_JTAG_MASTER_REG,
		   BCCIF_JTAG_RESET | speed);

      /* Wait till "running" goes away */
      loops = 0;
      while(loops < BCCIF_JTAG_RESET_MAXITS) {
	int status;
	int wait_jiffies;

	status = ReadPCIWord(bccif->base_Address + BCCIF_JTAG_STATUS_REG);
	if((status & BCCIF_JTAG_RUNNING) == 0) break;
	if((status & BCCIF_JTAG_EOF) != 0) break;

	wait_jiffies = 0;
	if(loops > BCCIF_JTAG_THROTTLE_VALUE) {
	  wait_jiffies = ((loops - BCCIF_JTAG_THROTTLE_VALUE) / BCCIF_JTAG_THROTTLE_JIFFIES_DIV) + 1;
	  }

	if(wait_jiffies != 0) {
	  if(wait_jiffies > BCCIF_JTAG_THROTTLE_JIFFIES_MAX)
	    wait_jiffies = BCCIF_JTAG_THROTTLE_JIFFIES_MAX;
	  
	  if(debug >= DEBUG_IF)
	    printk(KERN_INFO "BCCIF:JTAG_RST:%d Throttling:%d Status: %x BR:%lu\n",
		   loops, wait_jiffies, status, br);
	  set_current_state(TASK_INTERRUPTIBLE);
	  schedule_timeout(wait_jiffies);
	  /* see if a signal arrived */
	  if(signal_pending(current) != 0) break;
	}
	++loops;
      }

      if(signal_pending(current) != 0) {
	ret = -ERESTARTSYS;
	break;
      }
      if(loops >= BCCIF_JTAG_RESET_MAXITS) {
	ret = -ETIMEDOUT;
	break;
      }

      bp = (char *) arg;
      bp += sizeof(unsigned long int);

      while(btf > 0) {
	unsigned char byte;
	unsigned short int status;
	int wait_jiffies;

	ret = copy_from_user(&byte, bp, sizeof(unsigned char));
	if (ret != 0) {
	  ret = -EFAULT;
	  break;
	}


	// Load the data along with the 'load' signal
	WritePCIWord(bccif->base_Address + BCCIF_JTAG_MASTER_REG,
		     (unsigned short int) (BCCIF_JTAG_LOAD | byte | speed));

	ret = 0;
	loops = 0;
	/* Wait till "running" goes away */
	while(loops < BCCIF_JTAG_LOAD_MAXITS) {
	  status = ReadPCIWord(bccif->base_Address + BCCIF_JTAG_STATUS_REG);
	  if((status & BCCIF_JTAG_RUNNING) == 0) break;
	  if((status & BCCIF_JTAG_EOF) != 0) break;

	  wait_jiffies = 0;
	  if(loops > BCCIF_JTAG_THROTTLE_VALUE) 
	    wait_jiffies = ((loops - BCCIF_JTAG_THROTTLE_VALUE) / BCCIF_JTAG_THROTTLE_JIFFIES_DIV) + 1;

	  if(wait_jiffies != 0) {
	    if(wait_jiffies > BCCIF_JTAG_THROTTLE_JIFFIES_MAX)
	      wait_jiffies = BCCIF_JTAG_THROTTLE_JIFFIES_MAX;

	    if(debug >= DEBUG_IF)
	      printk(KERN_INFO "BCCIF:JTAG_LD:%d Throttling:%d Status: %x BR:%lu\n",
		     loops, wait_jiffies, status, br);
	    set_current_state(TASK_INTERRUPTIBLE);
	    schedule_timeout(wait_jiffies);
	    /* see if a signal arrived */
	    if(signal_pending(current) != 0) break;
	  }
	  ++loops;
	}

	if(signal_pending(current) != 0) {
	  ret = -ERESTARTSYS;
	  break;
	}

	if(loops >= BCCIF_JTAG_LOAD_MAXITS) ret = -ETIMEDOUT;
	else if((status & BCCIF_JTAG_ERR) != 0) ret = -EIO;
	else if((status & BCCIF_JTAG_RDY) == 0 &&
		(status & BCCIF_JTAG_EOF) == 0) ret = -EIO;
	   
	if(ret != 0) {
	  if(debug >= DEBUG_IF) 
	    printk(KERN_INFO "BCCIF:Loops:%d Status:%x Error while JTAG Device Write: %d\n", loops, status, ret);
	  break;
	}

	++bp;
	++br;
	--btf;

	if((status & BCCIF_JTAG_EOF) != 0) {
	  if(debug >= DEBUG_IF) 
	    printk(KERN_INFO "BCCIF:BCCIF_JTAG_EOF gone hi\n");
	  break;
	}

	/*
	if((br % 100000) == 0) {
	  if(debug >= DEBUG_IF) 
	    printk(KERN_INFO "BCCIF:JTAG wait loops: %d\n", loops);
	}
	*/

      }

      if(nb != br) {
	ret = copy_to_user((unsigned long int * ) arg, &br,
			   sizeof(unsigned long ));
	if (ret != 0) {
	  ret = -EFAULT;			
	  break;
	}
      }

      if(debug >= DEBUG_IF) 
	printk(KERN_INFO "BCCIF:Completed JTAG Device Write, %lu bytes read\n", br);

			
      break;		


    default:
      if(debug >= DEBUG_INFORMATION)
	printk(KERN_INFO "BCCIF:Invalid ioctl command was detected\n");        
      ret = -ENOTTY;
    }
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////
//                             Open/Close Functions                                 //
//////////////////////////////////////////////////////////////////////////////////////

/* Open the interface specified and initialise a new device data structure */
int open(struct inode *inode, struct file *filp)
{
  int result; //Return value variable	
  int minor; //Minor number of the device being accessed
  bccif_info_struct *bccif = NULL; //Data structure of the file

	
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
    //Increment the usage count
    MOD_INC_USE_COUNT;	
#endif /* LINUX_VERSION_CODE < VERSION_CODE(2,6,0) */

    //Determine minor
    minor = MINOR(inode->i_rdev);	

    //Check the minor number is valid, assuming numbering starts from 1
    if (minor < 1 || minor > BCCIF_BLOCKS) {
      printk(KERN_INFO "BCCIF:Invalid minor number specified MINOR:%d Limit:%d\n",
	     minor, BCCIF_BLOCKS);
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
      MOD_DEC_USE_COUNT;
#endif /* LINUX_VERSION_CODE < VERSION_CODE(2,6,0) */
      return -ENODEV; }  	
  			  		    
    //Allocate enough memory to hold another strcuture of bccif
    bccif = (bccif_info_struct*)kmalloc(sizeof(bccif_info_struct), GFP_KERNEL);
	
    if(debug >= DEBUG_CRIT) 
      printk(KERN_INFO "BCCIF:Device Opened - new dev info structure created at %lx \n",
	     (unsigned long int) bccif);
	
    //Ensure that the memory was allocated  
    if (bccif == NULL) {
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
      MOD_DEC_USE_COUNT;
#endif /* LINUX_VERSION_CODE < VERSION_CODE(2,6,0) */
      return -ENOMEM; }

    //Perform a memcopy to get all revelant data from the master device structure
    memcpy(bccif, &Bccif_Info, sizeof(bccif_info_struct));

    //Check if this user has already opened this device or if the requested device is opened by somebody else	
    spin_lock(bccif->countlock[minor - 1]);									//Ensure in a multi processor system only 1 processor is accessing information at once

#if LINUX_VERSION_CODE < VERSION_CODE(2,6,31)
    if (*(bccif->open_count[minor - 1]) &&
	(*(bccif->owner[minor - 1]) != current->uid) &&
	(*(bccif->owner[minor - 1]) != current->euid) &&
	!capable(CAP_DAC_OVERRIDE)) {
#else // 2.6.31++
    if (*(bccif->open_count[minor - 1]) &&
	(*(bccif->owner[minor - 1]) != current->cred->uid) &&
	(*(bccif->owner[minor - 1]) != current->cred->euid) &&
	!capable(CAP_DAC_OVERRIDE)) {
#endif /* LINUX_VERSION_CODE < VERSION_CODE(2,6,31) */

      //If device is open but the new request comes from a user who hasn't opened it and is not the super user
      spin_unlock(bccif->countlock[minor - 1]);
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
      MOD_DEC_USE_COUNT;
#endif /* LINUX_VERSION_CODE < VERSION_CODE(2,6,0) */
      if(debug >= DEBUG_IF)
	printk(KERN_INFO "BCCIF:Another user attempted to open minor %d\n", minor);		
      kfree(bccif); 	//Free the memory becasue we are denying open
      return -EBUSY; }
		
    if (*(bccif->open_count[minor - 1]) == 0) {
      *(bccif->owner[minor - 1]) = current->cred->uid; //This minor is now assigned to this user number
      if(debug >= DEBUG_IF)
	printk(KERN_INFO "BCCIF:Minor freshly opened\n");
    } else 
      if(debug >= DEBUG_IF)
	printk(KERN_INFO "BCCIF:Minor reopened %d times\n", *(bccif->open_count[minor - 1]));    

    //Check if this interface has been opened too many times already
	if ((*(bccif->open_count[minor - 1])) > MAX_OPEN_IF) { //1 user can open it many times but not unlimited times
	  spin_unlock(bccif->countlock[minor - 1]);
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
	  MOD_DEC_USE_COUNT;
#endif /* LINUX_VERSION_CODE < VERSION_CODE(2,6,0) */
	  if(debug >= DEBUG_IF)
	    printk(KERN_INFO "BCCIF:Too many files open: %d\n", *(bccif->open_count[minor - 1]));		
	  kfree(bccif); //Free the memory
	  return -EMFILE;}	    		
    		
	(*(bccif->open_count[minor - 1]))++; //Increment the count
	spin_unlock(bccif->countlock[minor - 1]);  //Unlock the spin lock		

	//Customise the device info structure for this minor
	bccif->minor = minor;		
	bccif->base_Address = bccif->base_Address + ((minor-1) * BCC_SPACE); //Reset the base address to the new interface

	//Setup the spin lock on the variables for this file descriptor
	spin_lock_init(&bccif->flaglock);		

	//Ensure the exit flags are cleared before we start
	bccif->flags = 0;  //Spin lock not required becasue the ISR hasn't been setup yet	
						
	if(debug >= DEBUG_INFORMATION) 
	  printk(KERN_INFO "BCCIF:Opened Device MAJOR:%d MINOR:%d at address %lx\n",
		 bccif->major, bccif->minor, bccif->base_Address);

	//Store the pointer to the local data structure for later use
	filp->private_data = bccif;	
	filp->f_pos = 0; //Initialise file pointer to 0, not used for this device			
									
	//Setup the Interrupt for this opening of the device
	result = Install_ISR(bccif);
	if (result != 0) {
	  printk(KERN_WARNING "BCCIF: Couldn't allocate interrupt \n");
	  bccif->irq = NO_IRQ;}	 //Deallocate the irq						
  	
	return 0;
}


/* The close command of the interface, called when 
all copies of the file descriptor are released */
int release(struct inode *inode, struct file *filp)
{
bccif_info_struct *bccif = NULL;  //Data structure of the file
	
    bccif = filp->private_data;	
	
    if(debug >= DEBUG_CRIT)	
      printk(KERN_INFO "BCCIF:Device released MAJOR:%d MINOR:%d\n",  bccif->major, bccif->minor);	
	
    //Remove the interrupt handler
    Remove_ISR(bccif);

    //Decrement open count
    (*(bccif->open_count[bccif->minor - 1]))--; //Decrement the usage count
		
    //Destroy the data structure for this device
    kfree(filp->private_data);
	
#if LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
    //Decrement the usage count
    MOD_DEC_USE_COUNT;
#endif /* LINUX_VERSION_CODE < VERSION_CODE(2,6,0) */
    return 0;

}

//////////////////////////////////////////////////////////////////////////////////////
//                                 Private Functions                                //
//////////////////////////////////////////////////////////////////////////////////////

/* Helper function to clear the error flags */
/* Flags variable is protected by spin locks */
void clearallflags(bccif_info_struct *bccif)
{
unsigned long irqsave;  //variable used to save irq mask
		
    //Ensure the error flags are cleared before we start, and disable interrupts
    spin_lock_irqsave(&bccif->flaglock, irqsave);
    bccif->flags &= FLAGS_MASK - (WB_ERROR + TIMEOUT + OVERFLOW);	
    spin_unlock_irqrestore(&bccif->flaglock, irqsave);			
}


/* Helper function to check the wb error flag */
/* Flags variable is protected by spin locks */
int checkwbflag(bccif_info_struct *bccif)
{
unsigned long irqsave;  //variable used to save irq mask
int ret;                //return value of function
unsigned long j = jiffies + 1; //Next clock tick
	
    while (jiffies < j);	//Delay for 1 tick for interrupt to occur, if its going to
			
    //Check for wb error
    spin_lock_irqsave(&bccif->flaglock, irqsave);
    if ((bccif->flags & WB_ERROR) != 0)	  //Test for a wishbone error
      ret = -EIO;    			//Indicate an wishbone error condition		
    else
      ret = 0;
    
    spin_unlock_irqrestore(&bccif->flaglock, irqsave);				
    return ret;
}

/* Helper function to check the wb and timeout error flags */
/* Flags variable is protected by spin locks */
int checkwbtimeoutflags(bccif_info_struct *bccif)
{
unsigned long irqsave; //variable used to save irq mask
int ret = 0;          //Return value of this function
unsigned long j = jiffies + 1;   //Next clock tick;
	
    while (jiffies < j);  //Delay for 1 tick for interrupt to occur, if its going to
		
    //Check for wb error
    spin_lock_irqsave(&bccif->flaglock, irqsave);
    if ((bccif->flags & WB_ERROR) != 0) //Test for a wishbone error
      ret = -EIO;   //Indicate an wishbone error condition		
	
    //Check for timeout error
    if ((bccif->flags & TIMEOUT) != 0) //Test for a wishbone error
      ret = -ETIMEDOUT;	//Indicate an wishbone error condition				
				
    spin_unlock_irqrestore(&bccif->flaglock, irqsave);
    return ret;	
}		

