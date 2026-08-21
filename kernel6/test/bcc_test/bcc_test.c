//Author: Andrew Brown
//Version: 1.01
//Description: Test program for the bcc interface

//////////////////////////////////////////////////////////////////////////////////////
//                                  Include Files                                   //
//////////////////////////////////////////////////////////////////////////////////////

#include "bcc_test.h"                                //Header for the test program

//////////////////////////////////////////////////////////////////////////////////////
//                                   Main Program                                   //
//////////////////////////////////////////////////////////////////////////////////////

/* Program entry point */
int main(int argc, char *argv[])
{
int retval;    //Variable to hold the return value of functions
 if_data_t *dev;  //Device structure to hold device specific information
char data[2];       //Character buffer used to save data from console
int device = 0;      //Default device number
int Quit = 0;       //Variable used to signal exit from program loop
  
  //Initialise the data structure
  dev = bcc_test_init();

  //Ask user for initial parameters
  do {
    printf("Device number? [%d] : ", dev->device);                //Enter the inital interface channel to open    
    
    //Get 1 character & 1 enter from the console input
    get_input(2, data);      
    
    //Test the return value to ensure its is >0 and <max blocks
    if ((data[0] == 'q') || (data[0] == 'Q')) goto exit;            //If user entered quit then exit program
    
    if (data[0] != '\n'){                            //On enter just use the default number
      device = atoi(data);    
      if ((device > BCCIF_BLOCKS) || (device <= 0)) { 
        printf("Try again, pick a number between 1 and %d\n", BCCIF_BLOCKS);
        continue; }
      dev->device = device;}                          //Save the device number
      
    open_interface(&dev);                            //Attempt to open the handle and fill the data structure

    //If a null handle was returned then we must have errored
    if (dev->handle <= 0) printf("Open failed");            
        
  }while (dev->handle <= 0);                            //Loop until we can open a device

  //Main processing loop
  do {  
    //Display the initial menu    
    retval = displayMainMenu(&dev);
    
    //Handle menu options
    switch(retval) {
      case 0:                                  //Exit the test program
        Quit = 1;
        break;      
      case 1:                                  //Change the open interface
        do_interface_change(&dev);
        break;
      case 2:                                  //Change interrupt configuration
        do_interrupt_menu(&dev);
        break;
      case 3:                                  //Change the interface timeout configuration
        do_timeout_menu(&dev);
        break;
      case 4:                                  //Change the mode of the interface
        do_mode_menu(&dev);
        break;
      case 5:                                  //Change specific options of the interface i.e. auto increment
        do_change_options(&dev);
        break;
      case 6:                                  //Display teh full configuration of the interface in one place
        do_config_dump (&dev);
        break;
      case 7:                                  //Write data to the external bus
        do_data_write(&dev);
        break;
      case 8:                                  //Read data from the external bus
        do_data_read(&dev);
        break;
      case 9:                                  //Perform debug commands on hardware, input and output etc.
        do_interactive_debug(&dev);
        break;
      case 10:                                //Reset all inetrfaces of the hardware
        do_hw_reset(&dev);          
        break;
      default:                                //On invalid input generate an error
        printf("Selection Unhandled:%d\n", retval);}
  
  } while(Quit != 1);                                //Loop until a quit message is signaled by the user
  
exit:  
  clear_screen();                                  //On exit clear the screen
  printf("-- Exiting, goodbye --\n");  
  close_interface(dev);                              //Close the open handle
  free(dev);                                    //Free the memory space taken by the device structure
  return 0;
}

//////////////////////////////////////////////////////////////////////////////////////
//                                       Menus                                      //
//////////////////////////////////////////////////////////////////////////////////////

/* Clear the screen of data */
void clear_screen(void)
{
  printf("%c%c%c%c%c%c",27,'[','H',27,'[','J' );                  //Send a clear command to the console, will only work on ascii compliant terminals
}

/* Display the main menu of the program */
/* Control is locked in this loop until a correct choice is made which is returned to the calling function */
int displayMainMenu(if_data_t **device)
{
if_data_t *dev = (*device); //Store a local copy of the pointer to the device data structure
char recv[3];               //Local buffer for recieving input from the user
int menu;                   //The number of the menu selected by the user
int result;                //Function return variable
unsigned short int data;   //Variable used to get information from the bcc driver  
  
    //Get the operating mode    
  result = ioctl(dev->handle, BCC_GET_MK_MODE, &data);  
  if (result == -1)  
    printf("Unable to get mode - %s\n", strerror(errno));
  else
    dev->mode = (data + 1);                            //IOCTL command will return 1 for MK2 and 0 for MK1 
  
  clear_screen();
  
  //Print main menu
  printf("$==================================================================$\n");
  printf("|            Block Control Computer Memory Interface               |\n");
  printf("|                        Test Program                              |\n");
  printf("|                                                                  |\n");
  printf("|Active Interface: IF%d                                   Mode:MK%c  |\n", dev->device,(dev->mode + 48));
  printf("|                                                                  |\n");
  printf("| 1)Change active interface                                        |\n");
  printf("|                                                                  |\n");
  printf("| 2)Setup/View Interrupts                                          |\n");
  printf("| 3)Setup/View timeout                                             |\n");
  printf("| 4)Change interface mode                                          |\n");
  printf("| 5)Change other interface options                                 |\n");
  printf("| 6)Dump Interface Config                                          |\n");
  printf("| 7)Write data to external bus                                     |\n");
  printf("| 8)Read data from external bus                                    |\n");
  printf("|                                                                  |\n");  
  printf("| 9)Start HW interface debug session                               |\n");
  printf("| 10)Reset interfaces                                              |\n");
  printf("|                                                                  |\n");
  printf("| Q)Quit                                                           |\n");
  printf("|                                          Written By: Andrew Brown|\n");
  printf("$==================================================================$\n");
  printf("SN:%s\n", dev->serial_no);
PrintRequest:
  printf("Enter your selection[Q]: ");
  
  //Get max 2 characters & 1 terminating character from the console input
  get_input(3, recv);  
  
  if ((recv[0] == 'q') || (recv[0] == 'Q') || (recv[0] == '\n')) return 0;    //If q is pressed then signal a desire to leave the program
  menu = atoi(recv);
  
  if (menu == 0 || menu > 10) {                          //Check validity of user input
    printf("Please pick a choice from above\n");
    goto PrintRequest; }
  return menu;                                  //Retrun the user's request  
}

/* Change the active interface */
void do_interface_change(if_data_t **device)
{
  if_data_t *dev = (*device);                            //Local copy of the device structure
  char recv[2];                                  //Character buffer used to store user input

  clear_screen();

  //Print sub menu  
  printf("$==================================================================$\n");
  printf("|            Block Control Computer Memory Interface               |\n");
  printf("|                   Change Active Interface                        |\n");  
  printf("|                                                                  |\n");
  printf("|Active Interface: IF%d                                             |\n", dev->device);
  printf("|                                                                  |\n");          
  printf("| n)Open interface n                                               |\n");          
  printf("|                                                                  |\n");          
  printf("| E)Quit to main menu                                              |\n");
  printf("|                                                                  |\n");
  printf("$==================================================================$\n");  
  
  do {                                      //Loop until the user enters a correct interface or quits
    printf("Enter a new interface to use[E]:>");    
    
    //Get 1 character from the console input
    get_input(2, recv);      
    
    //Test the return value to ensure it is >0 and <max blocks
    if ((recv[0] == 'e') || (recv[0] == 'E') || (recv[0] == '\n')) break;    //exit to main menu
    dev->device = atoi(recv);
    
    if ((dev->device > BCCIF_BLOCKS) || (dev->device <= 0)) { 
      printf("Try again, pick a number between 1 and %d\n", BCCIF_BLOCKS);
      continue; }
    
    //If there was a previous device open then close it
    if (dev->handle > 0) close_interface(dev);
      
    //Attempt to open the handle and fill the data structure again    
    open_interface(&dev);
    
    //If a null handle is returned then we couldn't open the interface
    if (dev->handle <= 0) printf("Open failed\n");
      
  }while (dev->handle <= 0);    
}

/* Handle viewing and changing the status of the interrupts */
void do_interrupt_menu(if_data_t **device)
{
  if_data_t *dev = (*device);                            //Local copy of the device info structure
  char *buffer;                                  //Pointer to character status buffer
  char recv[2];                                  //Data recieved from stdin
  int result;                                    //Return value from functions
  unsigned short int data;                            //Data varaible passed to the bcc driver

  //Allocate a buffer space for the status messages
  buffer = (char*) malloc(MAX_INFO_LENGTH);  
    
  do {                                      //Loop until user requests exit
  
    clear_screen();
  
    //Display sub menu    
    printf("$==================================================================$\n");
    printf("|            Block Control Computer Memory Interface               |\n");
    printf("|                    Setup/View Interrupts                         |\n");
    printf("|                                                                  |\n");
    printf("|Active Interface: IF%d                                             |\n", dev->device);
    printf("|                                                                  |\n");
    printf("|-MASK STATUS                                                      |\n");
  
    //Load the status of the global interrupt enable bit
    result = ioctl(dev->handle, BCC_GET_GLOBAL_INTERRUPTS_STATE, &data);  
    if (result < 0) {  
      printf("Unable to get global interrupt state - %s\n", strerror(errno));
      strcpy(buffer, "UNKNOWN");}
    else {
      if (data == 0)       
        strcpy(buffer, "DISABLED");
      else
        strcpy(buffer, "ENABLED ");}      
  
    printf("|  Master Interrupt:%s                                       |\n", buffer);
          
    //Load the status of the overflow interrupt enable bit
    result = ioctl(dev->handle, BCC_GET_OVERFLOW_INTERRUPT_STATE, &data);  
    if (result < 0) {  
      printf("Unable to get overflow interrupt state - %s\n", strerror(errno));
      strcpy(buffer, "UNKNOWN");}
    else {
      if (data == 0)       
        strcpy(buffer, "DISABLED");
      else
        strcpy(buffer, "ENABLED ");}  
  
    printf("|  Overflow Interrupt:%s                                     |\n", buffer);
  
    //Load the status of the timeout interrupt enable bit
    result = ioctl(dev->handle, BCC_GET_TIMEOUT_INTERRUPT_STATE, &data);  
    if (result < 0) {  
      printf("Unable to get timeout interrupt state - %s\n", strerror(errno));
      strcpy(buffer, "UNKNOWN");}
    else {
      if (data == 0) 
        strcpy(buffer, "DISABLED"); 
      else
        strcpy(buffer, "ENABLED ");}  
  
    printf("|  Timeout Interrupt:%s                                      |\n", buffer);
    printf("|                                                                  |\n");
    printf("|-INTERRUPT STATUS                                                 |\n");
  
    //Get the status of the overflow occured bit      
    result = ioctl(dev->handle, BCC_GET_OVERFLOW_OCCURED_STATE, &data);
    if (result < 0) {  
      printf("Unable to get overflow occured state - %s\n", strerror(errno));
      strcpy(buffer, "???");}
    else {
      if (data == 0)       
        strcpy(buffer, "NO ");
      else
        strcpy(buffer, "YES");}  
  
    printf("|  Overflow Occured:%s                                            |\n", buffer);
  
    //Get the status of the timeout occured bit      
    result = ioctl(dev->handle, BCC_GET_TIMEOUT_OCCURED_STATE, &data);  
    if (result < 0) {  
      printf("Unable to get timeout occured state - %s\n", strerror(errno));
      strcpy(buffer, "???");}
    else {
      if (data == 0)       
        strcpy(buffer, "NO ");
      else
        strcpy(buffer, "YES");}  
  
    printf("|  Timeout Occured:%s                                             |\n", buffer);    
    printf("|                                                                  |\n");
    printf("| 1)Clear occured status                                           |\n");
    printf("| 2)Toggle global interrupt mask                                   |\n");
    printf("| 3)Toggle overflow interrupt mask                                 |\n");
    printf("| 4)Toggle timeout interrupt mask                                  |\n");
    printf("|                                                                  |\n");
    printf("| E)Quit back to main menu                                         |\n");
    printf("|                                                                  |\n");
    printf("$==================================================================$\n");
    printf("Enter your selection[E]:>");    
    
    //Get 1 character & 1 new line character from the console input
    get_input(2, recv);      
    
    //Test the return value for the exit command
    if ((recv[0] == 'e') || (recv[0] == 'E') || (recv[0] == '\n')) break;  
    
    //Perform the command otherwise
    switch (recv[0]) {
      case '1':                                //Clear the overflow and timeout occured flags
        //Clear the overflow occured flag
        result = ioctl(dev->handle, BCC_CLR_OVERFLOW_OCCURED_STATE, NULL);  
        if (result < 0)
          printf("Unable to clear overflow occured state - %s\n", strerror(errno));
          
        //Clear the timeout occured flag    
        result = ioctl(dev->handle, BCC_CLR_TIMEOUT_OCCURED_STATE, NULL);  
        if (result < 0)
          printf("Unable to clear timeout occured state - %s\n", strerror(errno));
    
        break;
      case '2':                                //Toggle the state of the global interrupt enable for this IF
        //First get the current state
        result = ioctl(dev->handle, BCC_GET_GLOBAL_INTERRUPTS_STATE, &data);  
        if (result < 0)
          printf("Unable to get global interrupt state - %s\n", strerror(errno));
        if (data == 0) {                            //Is the master interrupt enable off
          result = ioctl(dev->handle, BCC_ENABLE_GLOBAL_INTERRUPTS, NULL);  //Then set it
          if (result < 0)
            printf("Unable to enable global interrupts - %s\n", strerror(errno)); }
        else {
          result = ioctl(dev->handle, BCC_DISABLE_GLOBAL_INTERRUPTS, NULL);  //Then clear it
          if (result < 0)
            printf("Unable to disable global interrupts - %s\n", strerror(errno)); }          
      
        break;      
      case '3':                                //Toggle the state of the overflow interrupt
        //First get the current state
        result = ioctl(dev->handle, BCC_GET_OVERFLOW_INTERRUPT_STATE, &data);  
        if (result < 0)
          printf("Unable to get overflow interrupt state - %s\n", strerror(errno));
        if (data == 0) {                            //Is the overflow interrupt enable off
          result = ioctl(dev->handle, BCC_ENABLE_OVERFLOW_INTERRUPT, NULL);  //Then set it
          if (result < 0)
            printf("Unable to enable overflow interrupts - %s\n", strerror(errno)); }
        else {
          result = ioctl(dev->handle, BCC_DISABLE_OVERFLOW_INTERRUPT, NULL);  //Then clear it
          if (result < 0)
            printf("Unable to disable overflow interrupts - %s\n", strerror(errno)); }          
      
        break;      
      case '4':                                //Toggle the state of the timeput interrupt
        //First get the current state
        result = ioctl(dev->handle, BCC_GET_TIMEOUT_INTERRUPT_STATE, &data);  
        if (result < 0)
          printf("Unable to get timeout interrupt state - %s\n", strerror(errno));
        if (data == 0) {                            //Is the timeout interrupt enable off
          result = ioctl(dev->handle, BCC_ENABLE_TIMEOUT_INTERRUPT, NULL);  //Then set it
          if (result < 0)
            printf("Unable to enable timeout interrupts - %s\n", strerror(errno)); }
        else {
          result = ioctl(dev->handle, BCC_DISABLE_TIMEOUT_INTERRUPT, NULL);  //Then clear it
          if (result < 0)
            printf("Unable to disable timeout interrupts - %s\n", strerror(errno)); }          
      
        break;      
      default:
        printf("Try again, pick a number between 1 and 4 or E\n");        
    }    
  }while (1);  
  
  free(buffer);                                  //Free the memory taken by the status buffer  
}

/* Display the timeout menu screen */
void do_timeout_menu(if_data_t **device)
{
  if_data_t *dev = (*device);                            //Local copy of the device data structure
  char *buffer;                                  //Buffer for status messages 
  char recv[5];                                  //Buffer for data recieved from stdin
  int result, quit;                                //Return variable and signal variable
  unsigned int delay_clk;                              //Return value of the number of PCI clocks in the software delay
  float delay_usec;                                //Approximation of the software delay in micro seconds
  unsigned short int data;                            //Data variable used to pass info back/forward to the driver

  //Allocate a buffer space for the status variable  
  buffer = (char*) malloc(MAX_INFO_LENGTH);  

  do {                                      //Loop until user requests exit
  
    //Load the status of the software timeout enable 
    result = ioctl(dev->handle, BCC_GET_SOFTWARE_TIMEOUT, &data);  
    if (result < 0) {  
      printf("Unable to get software timeout enabled state - %s\n", strerror(errno));
      strcpy(buffer, "UNKNOWN");}
    else {
      if (data == 0)       
        strcpy(buffer, "DISABLED");
      else
        strcpy(buffer, "ENABLED ");}      

    //Load the software timeout delay 
    result = ioctl(dev->handle, BCC_GET_TIMEOUT_DELAY, &data);  
    if (result < 0) {  
      printf("Unable to get software timeout enabled state - %s\n", strerror(errno));
      delay_clk = 0;
      delay_usec = 0;}
    else {
      delay_clk = (data << 3) + 7;                      //Conversion equation between register value and actual PCI clocks for delay
      delay_usec = delay_clk * 0.03; }                    //Assuming pCi bus at 33MHz the number of clocks * 30nS will be equal to the delay                      

    clear_screen();

    //Display sub menu
    printf("$==================================================================$\n");
    printf("|            Block Control Computer Memory Interface               |\n");
    printf("|                     Setup/View timeout                           |\n");
    printf("|                                                                  |\n");
    printf("|Active Interface: IF%d                                             |\n", dev->device);
    printf("|                                                                  |\n");
    printf("|-Software Timeout:%s                                        |\n", buffer);
    printf("|                                                                  |\n");
    printf("|-Hardware Delay:%3d PCI clocks, approx %3.2f uSec                  |\n", HW_DELAY, HW_DELAY*0.03);
    printf("|-Software Delay:%3d PCI clocks, approx %3.2f uSec                  |\n", delay_clk, delay_usec);
    printf("|                                                                  |\n");
    printf("| 1) Toggle Software timeout enabled                               |\n");
    printf("|                                                                  |\n");
    printf("| 2) Modify the delay in PCI clocks (value will be approximated)   |\n");    
    printf("| 3) Modify the delay in mSecs (value will be approximated)        |\n");        
    printf("|                                                                  |\n");
    printf("| E)Quit back to main menu                                         |\n");
    printf("|                                                                  |\n");
    printf("$==================================================================$\n");
    printf("Enter your selection[E]:>");  
    
    //Get 1 character from the console input
    get_input(2, recv);      
    
    //Test the return value for the exit command
    if ((recv[0] == 'e') || (recv[0] == 'E') || (recv[0] == '\n')) break;
        
    //Perform the command otherwise
    switch (recv[0]) {
      case '1':                                //Toggle the state of the software timeout enable bit                                
        //First get the current state
        result = ioctl(dev->handle, BCC_GET_SOFTWARE_TIMEOUT, &data);  
        if (result < 0)
          printf("Unable to get software timeout enabled state - %s\n", strerror(errno));
        if (data == 0) {                          //Is the software timeout enable off
          result = ioctl(dev->handle, BCC_SET_SOFTWARE_TIMEOUT, NULL);  //Then set it
          if (result < 0)
            printf("Unable to enable software timeout - %s\n", strerror(errno)); }
        else {
          result = ioctl(dev->handle, BCC_CLR_SOFTWARE_TIMEOUT, NULL);  //Then clear it
          if (result < 0)
            printf("Unable to disable software timeout - %s\n", strerror(errno)); }          
      
        break;  
      case '2':                                //Change the value of the softwrae timeout, enter the value in PCI clocks
        printf("Enter a new timeout value for external \ninterface transactions in PCI clock cycles\n");
        printf("Timeout value must be between 7 and 255:>");
        
        //Get max of 4 characters from the console input
        get_input(4, recv);        
        
        //Convert the result
        data = atoi(recv);              
        data >>= 3;        
        data &= 31;                              //Convert the value to somthing that can be set in the resgister, divide it by 8 and then mask it
        
        quit = 0;
        do {
          printf("Timeout will be set to %d PCI clocks, Y or N:>", ((data << 3) + 7));
          //Get max of 2 characters from the console input
          get_input(2, recv);                        //Because the value the user entered may not be the same as the final value check before setting it
            switch (recv[0]) {
              case 'y':
              case 'Y':
                //Update the software timeout value
                result = ioctl(dev->handle, BCC_SET_TIMEOUT_DELAY, &data);  
                if (result < 0)
                  printf("Unable to set software timeout delay - %s\n", strerror(errno));                
                quit = 1;
                break;
              case 'n':
              case 'N':                
                quit = 1;
                break;              
              default:
                printf("Not a valid response, try again\n");
            }
        } while(quit == 0);                          //Loop until the user enters a valid response
        break;
      case '3':                                //Enter a new timeout value in micro seconds
        printf("Enter a new timeout value for external \ninterface transactions in micro seconds\n");
        printf("Timeout value must be between 0.21 and 7.65 to 2 decimal places:>");
        
        //Get max of 5 characters from the console input
        get_input(5, recv);  
        sscanf(recv, "%f", &delay_usec);                  //convert the recieved value into a floating point number
        
        //Determine the number of PCI clocks and the new delay value
        delay_clk = floor(delay_usec / 0.03);                //Uses the reverse of the equation specified above        
        delay_clk >>= 3;
        delay_clk &= 31;
        
        quit = 0;
        do {
          printf("Timeout will be set to %3.2f uSec, Y or N:>", ((delay_clk << 3) + 7) * 0.03);
          //Get max of 2 characters from the console input
          get_input(2, recv);                        //As before check that the calculated value is ok by the user      
            switch (recv[0]) {
              case 'y':
              case 'Y':
                //Update the value
                data = delay_clk;
                result = ioctl(dev->handle, BCC_SET_TIMEOUT_DELAY, &data);  
                if (result < 0)
                  printf("Unable to set software timeout delay - %s\n", strerror(errno));                
                quit = 1;
                break;
              case 'n':
              case 'N':                
                quit = 1;
                break;              
              default:
                printf("Not a valid response, try again\n");
            }
        } while(quit == 0);
        break;                  
      default:
        printf("Try again, pick a number between 1 and 3 or E\n");        
    }    
  } while(1);                                    //Loop forever
  
  free(buffer);                                  //Free the status display buffer  
}

/* Display the change interface mode menu */
void do_mode_menu(if_data_t **device)
{
  if_data_t *dev = (*device);                            //Local copy of the device info structure
  int result, quit;                                //Function return variable and indicator variable
  unsigned short int data;                            //Passing variable for information passing to/from bcc driver
  char recv[2];                                  //Character buffer for console input
    
  do {

      //Get the operating mode    
    result = ioctl(dev->handle, BCC_GET_MK_MODE, &data);            //Has to be done every cycle just in case it changes, becasue we are changing the mode
    if (result == -1)  
      printf("Unable to get mode - %s\n", strerror(errno));
    else
      dev->mode = (data + 1);    
      
    clear_screen();
  
    //Display screen    
    printf("$==================================================================$\n");
    printf("|            Block Control Computer Memory Interface               |\n");
    printf("|                    Change interface mode                         |\n");
    printf("|                                                                  |\n");
    printf("|Active Interface: IF%d                                             |\n", dev->device);
    printf("|                                                                  |\n");
    printf("|-Current Mode:MK%d                                                 |\n", dev->mode);
    printf("|                                                                  |\n");
    printf("| 1) Change the current interface mode                             |\n");
    printf("|                                                                  |\n");
    printf("| NOTE: The mode selected in the inetrnal registers can be         |\n");
    printf("|       overridden by the jumpers on the board, for any changes    |\n");
    printf("|       to take effect the jumpers (J1 & J2) must be removed from  |\n");
    printf("|       the board for the particular interface under test          |\n");            
    printf("|                                                                  |\n");
    printf("| E)Quit back to main menu                                         |\n");
    printf("|                                                                  |\n");
    printf("$==================================================================$\n");    
    printf("Enter your selection[E]:>");  
    
    //Get 1 character from the console input
    get_input(2, recv);      
    
    //Test the return value for the exit command
    if ((recv[0] == 'e') || (recv[0] == 'E') || (recv[0] == '\n')) break;    //Exit the infinite loop when e is pressed    
    
    if (recv[0] == '1') {
      quit = 0;
      do {
        printf("Enter a new mode of interface %d , Mode 1 or 2:>", dev->device);
        
        //Get 1 character from the console input
        get_input(2, recv);      
      
        if (recv[0] == '1') {                        //Change the operating mode to MK1
            //Update the operating mode    
          result = ioctl(dev->handle, BCC_SET_MK1_MODE, NULL);  
          if (result == -1)  
            printf("Unable to set MK1 mode - %s\n", strerror(errno));
          quit = 1;  
        }else if (recv[0] == '2') {                      //Chnage the operating mode to MK2
            //Update the operating mode    
          result = ioctl(dev->handle, BCC_SET_MK2_MODE, NULL);  
          if (result == -1)  
            printf("Unable to set MK2 mode - %s\n", strerror(errno));          
          quit = 1;
        } else
          printf("Not a valid response, enter 1 or 2\n");
      } while(quit == 0);      
    } else
      printf("Try again, pick number 1 or E\n");
    
  }while(1);        
}

/* Chnage the misc options of the interface */
void do_change_options(if_data_t **device)
{
  if_data_t *dev = (*device);                            //Local copy of the device info structure
  char recv[2];                                  //Character buffer for console input
  char *buffer;                                  //Pointer to status buffer
  int result;                                    //Return variable from functions
  unsigned short int data;                            //Data variable passed to/from driver

  //Allocate a buffer space for the status information  
  buffer = (char*) malloc(MAX_INFO_LENGTH);  
    
  do {
    clear_screen();
  
    //Print the submenu
    printf("$==================================================================$\n");
    printf("|            Block Control Computer Memory Interface               |\n");
    printf("|                Change other interface options                    |\n");  
    printf("|                                                                  |\n");
    printf("|Active Interface: IF%d                                             |\n", dev->device);
    printf("|                                                                  |\n");
    printf("|-OPTIONS                                                          |\n");
  
    //Load the status of the global interrupt enable bit
    result = ioctl(dev->handle, BCC_GET_INCREMENT_ENABLE, &data);  
    if (result < 0) {  
      printf("Unable to get address increment enable state - %s\n", strerror(errno));
      strcpy(buffer, "UNKNOWN");}
    else {
      if (data == 0)       
        strcpy(buffer, "DISABLED");
      else
        strcpy(buffer, "ENABLED ");}      
  
    printf("|  Address Auto Increment:%s                                 |\n", buffer);
    printf("|                                                                  |\n");          
    printf("| 1)Toggle address auto increment                                  |\n");          
    printf("|                                                                  |\n");  
    printf("|                                                                  |\n");  
    printf("|                                                                  |\n");  
    printf("| E)Quit to main menu                                              |\n");
    printf("|                                                                  |\n");
    printf("$==================================================================$\n");      
    printf("Enter your selection[E]:>");    

    //Get 1 character from the console input
    get_input(2, recv);      
    
    //Test the return value for the exit command
    if ((recv[0] == 'e') || (recv[0] == 'E') || (recv[0] == '\n')) break;    
    
    //Perform the command otherwise
    switch (recv[0]) {
      case '1':
        //First get the current state
        result = ioctl(dev->handle, BCC_GET_INCREMENT_ENABLE, &data);    //Toggle the auto increment bit  
        if (result < 0)
          printf("Unable to get address increment enabled state - %s\n", strerror(errno));
        if (data == 0) {                                //Is the address increment enable off
          result = ioctl(dev->handle, BCC_SET_INCREMENT_ENABLE, NULL);        //Then set it
          if (result < 0)
            printf("Unable to enable auto address incrementing - %s\n", strerror(errno)); }
        else {
          result = ioctl(dev->handle, BCC_CLR_INCREMENT_ENABLE, NULL);        //Then clear it
          if (result < 0)
            printf("Unable to disable auto address incrementing - %s\n", strerror(errno)); }          
      
        break;                  
      default:
        printf("Try again, pick number 1 or E\n");        
    }    
  } while(1);      
  
  free(buffer);                                  //Free the status buffer
}

/* Prints out all the config info from the active device on one screen, can't change anything but */
void do_config_dump (if_data_t **device)
{
  if_data_t *dev = (*device);                            //Local pointer to the device structure
  char recv[2], master_int[9], timeout_int[9], overflow_int[9], sw_timeout_enabled[9], timeout_occured[4]; //Character buffers for status storage
  char overflow_occured[4], interface_mode[4], address_auto_increment[9];      //More character buffers
  unsigned short int address_reg, mode_reg, status_reg, data;            //16 bit data buffers where if regsiter data is stored
  int sw_timeout, result;                              //Intermediate software timeout storage and function return variable
  Bccif_data data_struct;                              //Custom data structure for the register reda/write functions  
  
  clear_screen();

  //Load the register states, status, mode & address using the if reg read commands
  data_struct.address = BCC_STATUS;                        //Set the request address to the status register    
  result = ioctl(dev->handle, BCC_READ_IF_REGISTER, &data_struct);  
  if (result < 0)
    printf("Unable to get status reg - %s\n", strerror(errno));  
  status_reg = data_struct.data;

  data_struct.address = BCC_MODE;                          //Set the request address to the mode register  
  result = ioctl(dev->handle, BCC_READ_IF_REGISTER, &data_struct);  
  if (result < 0)
    printf("Unable to get mode reg - %s\n", strerror(errno));  
  mode_reg = data_struct.data;    
  
  data_struct.address = BCC_ADDRESS;                        //Set the request address to the address register    
  result = ioctl(dev->handle, BCC_READ_IF_REGISTER, &data_struct);  
  if (result < 0)
    printf("Unable to get address reg - %s\n", strerror(errno));  
  address_reg = data_struct.data;  
  
  //Load the interrupt enables, master, timeout & overflow
  result = ioctl(dev->handle, BCC_GET_GLOBAL_INTERRUPTS_STATE, &data);  
  if (result < 0) {  
    printf("Unable to get global interrupt state - %s\n", strerror(errno));
    strcpy(master_int, "UNKNOWN");}
  else {
    if (data == 0)       
      strcpy(master_int, "DISABLED");
    else
      strcpy(master_int, "ENABLED ");}  
  
  result = ioctl(dev->handle, BCC_GET_TIMEOUT_INTERRUPT_STATE, &data);  
  if (result < 0) {  
    printf("Unable to get timeout interrupt state - %s\n", strerror(errno));
    strcpy(timeout_int, "UNKNOWN");}
  else {
    if (data == 0)       
      strcpy(timeout_int, "DISABLED");
    else
      strcpy(timeout_int, "ENABLED ");}  
      
  result = ioctl(dev->handle, BCC_GET_OVERFLOW_INTERRUPT_STATE, &data);  
  if (result < 0) {  
    printf("Unable to get overflow interrupt state - %s\n", strerror(errno));
    strcpy(overflow_int, "UNKNOWN");}
  else {
    if (data == 0)       
      strcpy(overflow_int, "DISABLED");
    else
      strcpy(overflow_int, "ENABLED ");}      
      
  //Get interrupt occured bits          
  result = ioctl(dev->handle, BCC_GET_OVERFLOW_OCCURED_STATE, &data);
  if (result < 0) {  
    printf("Unable to get overflow occured state - %s\n", strerror(errno));
    strcpy(overflow_occured, "???");}
  else {
    if (data == 0)       
      strcpy(overflow_occured, "NO ");
    else
      strcpy(overflow_occured, "YES");}  
  
  //Get the timeout occured bit      
  result = ioctl(dev->handle, BCC_GET_TIMEOUT_OCCURED_STATE, &data);  
  if (result < 0) {  
    printf("Unable to get timeout occured state - %s\n", strerror(errno));
    strcpy(timeout_occured, "???");}
  else {
    if (data == 0)       
      strcpy(timeout_occured, "NO ");
    else
      strcpy(timeout_occured, "YES");}      
      
  //Get the software delay value      
  result = ioctl(dev->handle, BCC_GET_TIMEOUT_DELAY, &data);  
  if (result < 0) {  
    printf("Unable to get software timeout enabled state - %s\n", strerror(errno));
    sw_timeout = 0;}
  else 
    sw_timeout = (data << 3) + 7;                      //Conversion equation between register value and actual PCI clocks for delay      
  
  //Get the software delay enabled bit
  result = ioctl(dev->handle, BCC_GET_SOFTWARE_TIMEOUT, &data);  
  if (result < 0) {  
    printf("Unable to get software timeout enabled state - %s\n", strerror(errno));
    strcpy(sw_timeout_enabled, "UNKNOWN");}
  else {
    if (data == 0)       
      strcpy(sw_timeout_enabled, "DISABLED");
    else
      strcpy(sw_timeout_enabled, "ENABLED ");}      
  
  //Get the auto increment enabled bit
  result = ioctl(dev->handle, BCC_GET_INCREMENT_ENABLE, &data);  
  if (result < 0) {  
    printf("Unable to get address increment enable state - %s\n", strerror(errno));
    strcpy(address_auto_increment, "UNKNOWN");}
  else {
    if (data == 0)       
      strcpy(address_auto_increment, "DISABLED");
    else
      strcpy(address_auto_increment, "ENABLED ");}  

  //Get the interface operating mode
  result = ioctl(dev->handle, BCC_GET_MK_MODE, &data);  
  if (result == -1)  
    printf("Unable to get mode - %s\n", strerror(errno));
  else {
    if (data == 0)       
      strcpy(interface_mode, "MK1");
    else
      strcpy(interface_mode, "MK2");}      
      
  //Display all the above data      
  printf("$==================================================================$\n");
  printf("|            Block Control Computer Memory Interface               |\n");
  printf("|                    Dump Interface Config                         |\n");  
  printf("|                                                                  |\n");
  printf("|-Status Register:0x%04x                                           |\n", status_reg);          
  printf("|  Master Interrupt:%s                                       |\n", master_int);          
  printf("|  Timeout Interrupt:%s                                      |\n", timeout_int);  
  printf("|  Overflow Interrupt:%s                                     |\n", overflow_int);          
  printf("|  Timeout Occured:%s                                             |\n", timeout_occured);  
  printf("|  Overflow Occured:%s                                            |\n", overflow_occured);
  printf("|                                                                  |\n");          
  printf("|-Mode Register:0x%04x                                             |\n", mode_reg);
  printf("|  SW Timeout Delay (PCI Clocks):%4d                              |\n", sw_timeout);          
  printf("|  SW Timeout:%s                                             |\n", sw_timeout_enabled);
  printf("|  Address Auto Increment:%s                                 |\n", address_auto_increment);          
  printf("|  Interface Mode:%s                                              |\n", interface_mode);  
  printf("|                                                                  |\n");          
  printf("|-Address Reg:0x%04x                                               |\n", address_reg);
  printf("|                                                                  |\n");          
  printf("|                                                                  |\n");    
  printf("|                                                                  |\n");  
  printf("|                                                                  |\n");
  printf("$==================================================================$\n");      
  printf("Press enter to exit");    
    
  //Get 1 character from the console input and exit
  get_input(1, recv);                                //Get 1 character and exit  
}

/* Reset all the interfaces */
void do_hw_reset(if_data_t **device)
{
  if_data_t *dev = (*device);                            //Local copy of the device data structure
  char recv[2];                                  //Character buffer for console input
  int result;                                    //Function return variable
  
  do {                                      //Loop until the user enters the correct information
    clear_screen();
  
    //Print the sub menu
    printf("$==================================================================$\n");
    printf("|            Block Control Computer Memory Interface               |\n");
    printf("|                       Reset interfaces                           |\n");  
    printf("|                                                                  |\n");
    printf("|                                                                  |\n");          
    printf("| 1)Do HW reset                                                    |\n");          
    printf("|                                                                  |\n");  
    printf("| NOTE: By pressing 1 all of the interfaces on this card will be   |\n");
    printf("|       reset, this means that the address registers will all be   |\n");
    printf("|       cleared and all current options will revert back to        |\n");
    printf("|       defaults                                                   |\n");
    printf("|                                                                  |\n");  
    printf("| E)Quit to main menu                                              |\n");
    printf("|                                                                  |\n");
    printf("$==================================================================$\n");      
    printf("Enter your selection[E]:>");    
    
    //Get 1 character from the console input
    get_input(2, recv);      
    
    //Did we request an exit
    if ((recv[0] == 'e') || (recv[0] == 'E') || (recv[0] == '\n')) break;    //exit to main menu

    if (recv[0] == '1') {
      //Do the reset  
      result = ioctl(dev->handle, BCC_RESET_INTERFACES, NULL);        //For this to work we need admin privilages  
      if (result == -1)  
        printf("Unable to reset the interfaces - %s\n", strerror(errno));
      break;      
    } else 
      printf("Try again, pick number 1 or E\n");
      
  }while (1);        
}

/* Write a quantity of data to the external bus */
/* Will not increment the address counter unless auto increment is on */
void do_data_write(if_data_t **device)
{
  if_data_t *dev = (*device);                            //Standard device information structure
  char *buffer, *curbuffer, *found;                        //Pointers to different sections of the input string
  char *data;                                    //The current string we are working with
  int result;                                    //return variable
  int length;                                    //Length of the current section we are working with
  int count, i;                                  //Count of number of data segments to write
  int writes;                                    //Number of sucessful writes
  unsigned short int *data_buf, address, number;                  //Variables used to communicate with the interface, data buf is a pointer to an array

  buffer = (char*)malloc(MAX_LINE_LENGTH*sizeof(char));              //Allocate space for the console input buffer
  data = (char*)malloc(MAX_LINE_LENGTH*sizeof(char));                //Allocat space for the working buffer
  
  clear_screen();
  
  //Print the sub menu
  printf("$==================================================================$\n");
  printf("|            Block Control Computer Memory Interface               |\n");
  printf("|                 Write data to external bus                       |\n");  
  printf("|                                                                  |\n");
  printf("$==================================================================$\n");    
    
  do {    
    printf("Enter the address on the external bus to write to(HEX):>");  
    get_input(MAX_LINE_LENGTH, buffer);                      //Get the user input
    if ((strstr(buffer, "q") != NULL) || (strstr(buffer, "Q") != NULL)) break;  //Is there a q anywhere? if so quit
    if (sscanf(buffer, "%hx", &address) != 1) {                  //Convert the enter data to a short int
      printf("Not a valid number\n");
      continue; }  
    
    printf("Enter the data in hexadecimal\n");
    printf("Press space to write more data with the same address,\n");
    printf("press enter to change the address, and press q to quit\n");

    printf(":>");
    get_input(MAX_LINE_LENGTH, buffer);                      //Get the string of data words, each word should be seperated by a space
      
    if ((strstr(buffer, "q") != NULL) || (strstr(buffer, "Q") != NULL)) break;
    
    if (buffer[(strlen(buffer) - 1)] == '\n') buffer[(strlen(buffer) - 1)]= '\0';  //When the dat is read the last character could be a newline, if it is remove it
    
    //Find out how many writes there are to this address by counting spca characters
    count = 1;                                  //Always at least one
    for (i = 0; i < (int)strlen(buffer); i++) {
      if (buffer[i] == ' ') count++;}    

    data_buf = (unsigned short int*) malloc((count + 1)*sizeof(unsigned short int));  //Allocate a buffer the correct size
    
    data_buf[0] = address;                            //For a write the first address in the buffer is the staring address of the if write
            
    //Process the data words 1 by 1 and add them to the array
    curbuffer = buffer;                              //Start at the beginning of the character buffer
    writes = 0;                                  //Count DATA words only; write() adds the address word itself
    for (i=0; i<count; i++) {
      //Function works by assuming curbuffer points at the beginning of the 
      //current string, and all we need to do is find the next space which 
      //marks the end of the current string. The pointer found is the end of 
      //the current string
      
      //Get a pointer to the beginning of the next space character
      found = strstr(curbuffer, " ");
      
      if (found == NULL)                            //If there is no next space character then we are working with the last one, so length is the remaineder of the string
        length = strlen(curbuffer);
      else
        length = strlen(curbuffer) - strlen(found);
      memcpy(data, curbuffer, length);                    //Copy out the length of the processing string from the main buffer to the data buffer
      
      //skip the space character
      curbuffer = found + 1;                          //Update the pointer for the next illiteration
      
      data[length] = '\0';                          //To make it a propper string put a null at the end
      writes += sscanf(data, "%hx", &data_buf[i+1]);              //Increment the writes variable if we sucessfully convert the value
    }

    //The write routiene will detect any problems with overflows or timeout and inform the user
    
    //Prior to the write we have to clear the overflow and timeout occured flags
    result = ioctl(dev->handle, BCC_CLR_OVERFLOW_OCCURED_STATE, NULL);  
    if (result < 0)
      printf("Unable to clear overflow occured state - %s\n", strerror(errno));
            
    result = ioctl(dev->handle, BCC_CLR_TIMEOUT_OCCURED_STATE, NULL);  
    if (result < 0)
      printf("Unable to clear timeout occured state - %s\n", strerror(errno));    
    
    //Actually write the inputted data
    result = write(dev->handle, data_buf, writes);
    
    //Find if there was  an error
    result = ioctl(dev->handle, BCC_GET_OVERFLOW_OCCURED_STATE, &number);
    if (result < 0)  
      printf("Unable to get overflow occured state - %s\n", strerror(errno));
    else if (number == 1)
      printf("Address counter overflowed while writing data\n");
    else {
      result = ioctl(dev->handle, BCC_GET_TIMEOUT_OCCURED_STATE, &number);  
      if (result < 0)  
        printf("Unable to get timeout occured state - %s\n", strerror(errno));
      else if (number == 1)
        printf("Bus timeout error occured writing data\n");    
      else
        printf("%d Words written sucessfully\n", writes);}          //If no errors occured then it was all a sucess  
          
    free(data_buf);                                //Dump the allocated data buffer  
  } while(1);
  free(buffer);                                  //Free the main input buffer
  free(data);                                    //Free the current data buffer
}

/* Read a quantity of data from the bus */
/* Will not increment the address counter unless auto increment is on */
void do_data_read(if_data_t **device)
{
  if_data_t *dev = (*device);                            //Standard device information structure
  char *buffer;                                  //Pointer to input string
  int result, i, reads;                              //return variables & loop count variables  
  unsigned short int *data_buf, address, number, ret;                //Variables used to communicate with the interface
  
  buffer = (char*)malloc(MAX_LINE_LENGTH*sizeof(char));              //Allocate a buffer for console input
  
  clear_screen();
  
  //Print the sub menu
  printf("$==================================================================$\n");
  printf("|            Block Control Computer Memory Interface               |\n");
  printf("|                 Read data from external bus                      |\n");  
  printf("|                                                                  |\n");
  printf("$==================================================================$\n");    
    
  do {  
    printf("Enter the address on the external bus to read from(HEX):>");  
    get_input(MAX_LINE_LENGTH, buffer);
    if ((strstr(buffer, "q") != NULL) || (strstr(buffer, "Q") != NULL)) break;  //Was there a q in the input anywhere?
    if (sscanf(buffer, "%hx", &address) != 1) {                  //Convert the enter data to a short int
      printf("Not a valid number\n");
      continue; }  
      
    //Set the address register
    result = ioctl(dev->handle, BCC_SET_ADDRESS, &address);            //Update the address reg with the data just entered  
    if (result < 0)
      printf("Unable to set address register - %s\n", strerror(errno));

    printf("Enter how many words you would like to read, in decimal,\n");
    printf("or press q to quit\n");
    printf(":>");
    get_input(MAX_LINE_LENGTH, buffer);                      //Get the number of words the user wants to read
    
    if ((strstr(buffer, "q") != NULL) || (strstr(buffer, "Q") != NULL)) break;  //If there was a q anywhere then quit to the main menu
    
    number = atoi(buffer);                            //Convert the number       
    if (number == 0) { 
      printf("Not a valid quantity of words, try again\n");
      continue; }
    
    data_buf = (unsigned short int*)malloc((number + 1) * sizeof(unsigned short int));  //Allocate enough buffer space for all the reads
      
    //Read 1 word at a time, in order to catch errors while interrupts are off, use interactive debug to read/write without error checking
    unsigned short int rdbuf[2];                    //One address word + one data word
    reads = 0;
    for (i=0; i <number; i++) {                          //Loop through all requests
      //Make sure error flags are clear before we start
      result = ioctl(dev->handle, BCC_CLR_OVERFLOW_OCCURED_STATE, NULL);  
      if (result < 0)
        printf("Unable to clear overflow occured state - %s\n", strerror(errno));
            
      result = ioctl(dev->handle, BCC_CLR_TIMEOUT_OCCURED_STATE, NULL);  
      if (result < 0)
        printf("Unable to clear timeout occured state - %s\n", strerror(errno));    
    
      //Actually read the data, 1 word at a time
      //read() takes buf[0] as the address and returns the data in buf[1]
      rdbuf[0] = address + i;
      result = read(dev->handle, rdbuf, 1);
      data_buf[i] = rdbuf[1];
    
      //Find if there was  an error
      result = ioctl(dev->handle, BCC_GET_OVERFLOW_OCCURED_STATE, &ret);
      if (result < 0)  {
        printf("Unable to get overflow occured state - %s\n", strerror(errno));
        break; }
      else if (ret == 1) {
        printf("Address counter overflowed while writing data\n");
        break; }
      else {
        result = ioctl(dev->handle, BCC_GET_TIMEOUT_OCCURED_STATE, &ret);  
        if (result < 0)  {
          printf("Unable to get timeout occured state - %s\n", strerror(errno));
          break; }
        else if (ret == 1) {
          printf("Bus timeout error occured writing data\n");  
          break; }  
        else
          ++reads;}}                            //Increment the read sucess indicator    
    
    //Display the data
    if (reads > 0) {
      printf("------------------------------------------------\n");
      printf("                     DATA                       \n\t");
      for (i=0; i < reads; i++ ) {
        if ((i % 8) == 0) printf("\n\t");
        printf("%04hx   ",data_buf[i]);} 
      printf("\n                   END DATA                     \n");
      printf("------------------------------------------------\n");}
      printf("%d out of %d reads suceeded", reads, number);
  
    free(data_buf);                                //Free the data buffer  
  } while(1);
  free(buffer);                                  //Free the console input buffer
}

/* Provides a full interactive session to execute io commands on registers and the if bus*/
/* No error checking is done on the sucess or failure of read/write commands */
void do_interactive_debug(if_data_t **device) 
{
if_data_t *dev = (*device); //Standard device information structure
char *buffer, *processing, *i, *End_ptr, *data, *curbuffer, *found; //Pointers to different sections of the input string
char *last_cmd; //Buffer containing what the last command was                                          
clock_t start = 0,total = 0; //Time variables used for the bench marking  
Bccif_data data_struct;  //Data structure used for register reads/writes  
int result, errors, j, k, length, count; //Return variable, look count variables, and error coun variables
unsigned short int x, y, s, a, b, d, l; //input variables from function commands
unsigned short int databuf[2], *dataset; //Arrays used for reading and writing the external interface
  

    buffer = (char*)malloc(MAX_LINE_LENGTH*sizeof(char));              //Allocate a buffer to recieve console input
    last_cmd = (char*)malloc(MAX_LINE_LENGTH*sizeof(char));              //Allocate a buffer to store the previous command
    processing = buffer;                              //Set the current processing pointer to the beginning of the string
    
    strcpy(last_cmd, "");                              //Set the previous command to nothing
  
    clear_screen();
  
    //Print heading
    printf("$==================================================================$\n");
    printf("|            Block Control Computer Memory Interface               |\n");
    printf("|                  HW interface debug session                      |\n");  
    printf("|                                                                  |\n");
    printf("$==================================================================$\n");
  
    do {                                      //Loop until a quit command is recieved
      printf("[IF:%d]:> ", dev->device);  
      get_input(MAX_LINE_LENGTH, buffer);                      //Get input from console
      if ((strstr(buffer, "q") != NULL) || (strstr(buffer, "Q") != NULL)) break;    
      
      if (buffer[0] != '\n') {                          //If the command wasn't a return then copy the just entered command into the last command buffer
  if (buffer[strlen(buffer)-1] == '\n') buffer[strlen(buffer)-1] = '\0';  //If the last character recieved was a new line then remove it
  strcpy(last_cmd, buffer); }
      
      End_ptr = buffer + strlen(buffer) * sizeof(char);              //Set the memory addres of the end of the string
      
      processing = buffer;                            //Set the currently processing pointer to the beginning
      do {    
  
  //Loop through the string and find a colon
  //Logic works by first assuming that the processing pointer 
  //is set for the beginning of the current string, all we have to is 
  //find the end of the string and replace the colon with a null
  for (i = processing; i < End_ptr; i+= sizeof(char))            //Loop from the current position to the end of the string and find the first colon and replace it with a null
    {
      if (i[0] == ';') {
        i[0] = '\0';
        break; }
    }
  
  //Handle the command truncated command      
  switch(processing[0]) {
  case 'h':
  case 'H':                              //Show the help screen
  case '?':
    printf("Interactive Debug Help\n\n");
    printf("? \tDisplay this screen\n");
    printf("i a b\tGet b words starting at address a on the external bus\n");
    printf("o a d ... Write data d to address a on the external bus\n");
    printf("f a l b\tFill memory start at a with data b for l words on the external bus\n");
    printf("r a\tRead 1 word from an internal register address a\n");
    printf("w a d\tWrite word d to an internal register address a\n");
    printf("t s x y\tTest y words at x locations starting at s on the external bus\n");
    printf("ret\tRepeat the last command\n"); 
    printf("q \tExit back to main menu\n\n");
    printf("All numeric values are entered in HEX, commands are case insensitive\nand multiple commands can be seperated by colons on a single line\n");      
    break;
  case 'f':
  case 'F':                              //Fill a section of memory with a specified word
    if (processing[1] != ' '){                    //Make sure the second character is a space
      printf("Unrecognised command\n");
      break; }        
    
    //Drop the command character off the string
    processing += sizeof(char);                    //Increment the pointer by 1 position
    result = sscanf(processing, " %hx %hx %hx", &a, &l, &b);    //Input 3 characters from the string
    if (result != 3) {
      printf("Invalid data, specify 3 16 bit numbers\n");
      break; }
    
    //Execute the command
    for (j=a;j <(l+a); j++) {                    //loop from the beginning addres specified for the length specified
      databuf[0] = j;                        //set the address
      databuf[1] = b;                        //The word that the user specified
      result = write(dev->handle, &databuf, 1);          //1 data word after the address -> returns 2      
      if(result == -1 || result != 2)
        printf("Unable to write to device with error - %s\n", strerror(errno));}                    
    break;  
  case 'i':  
  case 'I':                              //Input data from interface, incremnts the address pointer regardless of auto increment 
    if (processing[1] != ' ') { //Make sure the second character is a space
      printf("Unrecognised command\n");
      break; }      
    
    //Drop the command character off the string
    processing += sizeof(char);
    result = sscanf(processing, " %hx %hx", &a, &b);  //Decode 2 words from the parameters
    if (result != 2) {
      printf("Invalid data, specify 2 16 bit numbers\n");
      break; }            
    
    //Execute the command
    for (j = a; j < (a + b); j++) { //Loop from the starting address for the requested length
      //write the address
      data_struct.address = BCC_ADDRESS;
      data_struct.data = j;
      // printf("Setting up address reg (%x) with:%x\n", data_struct.address, data_struct.data);
      result = ioctl(dev->handle, BCC_WRITE_IF_REGISTER, &data_struct);

      //      result = write(dev->handle, &j, 0);   //Update the address pointer
      //      if(result == -1 || result != 1)
      if(result < 0)
        printf("Unable to write to device with error - %s\n", strerror(errno));

      // data_struct.data = 0;
      // result = ioctl(dev->handle, BCC_READ_IF_REGISTER, &data_struct);
      // printf("Read check of BCC_ADDRESS:%x\n", data_struct.data);


      //read a word
      data_struct.address = BCC_DATA;
      data_struct.data = 0;
      // printf("Reading data reg (%x)\n", data_struct.address);
      result = ioctl(dev->handle, BCC_READ_IF_REGISTER, &data_struct);

      // result = read(dev->handle, &d, 1);    
      //      if(result == -1)
      if(result < 0)
        printf("Unable to read from device with error - %s\n", strerror(errno));
      d = data_struct.data;

      //Display the data as its read
      if ((j % 8) == 0) printf("\nA:%04hx: ", j);          //Display 8 words accross with the address printed at the beginning of each line, the e symbolises that it represents an external address
      printf("%04hx  ", d);} 
    
    printf("\n");                          //At the end print a newline
    
    break;
  case 'o':
  case 'O':                              //Output data to the interface
    if (processing[1] != ' '){ //Make sure the second character is a space
      printf("Unrecognised command\n");
      break; }        
    
    //Drop the command character off the string
    processing += sizeof(char);
    
    //Get the starting address
    result = sscanf(processing, " %hx %hx", &a, &d); //Decode at least 2 parameters of the command            
    if (result != 2) {
      printf("Invalid data, specify 2 or more 16 bit numbers\n");
      break; }            
    
    //Set the processing pointer at the beginning of the first data element
    processing += sizeof(char); //Drop the first space
    processing = strstr(processing, " "); //Set the processing pointer at the next space found
    processing += sizeof(char); //Then drop that space
    
    //Find out how many writes there are to this address
    count = 1; //Always at least 1 write
    for (j = 0; j < (int)strlen(processing); j++) {
      if (processing[j] == ' ') count++;}  //Loop through the string and count spaces
    
    data = (char*)malloc(MAX_LINE_LENGTH*sizeof(char)); //Make a space
    
    //Process the writes one by one
    curbuffer = processing; //Set the curbuffer (pointer to the beginning of the current string) to the begining of the data segment
    for (j=0; j<count; j++) {
      //Get a pointer to the next space character
      found = strstr(curbuffer, " "); //Works as in read and wirte commands, finds the next space and then copies out the string between the beginning and end pointers 
      
      if (found == NULL)
        length = strlen(curbuffer);
      else
        length = strlen(curbuffer) - strlen(found);
      
      //Copy the string accross
      memcpy(data, curbuffer, length);
      
      //skip the space character
      curbuffer = found + sizeof(char); //Update the the begining of the next string            
      
      data[length] = '\0'; //Make it a propper string by adding a null at the end
      if (sscanf(data, "%hx", &databuf[1]) != 1)   //Convert the hex formatted data to a number
        printf("Data format error\n");

      //Actually write the data, 1 word at a time
      databuf[0] = a;

      // printf("Addr:%x Data:%x\n", databuf[0], databuf[1]);

      //write the address
      data_struct.address = BCC_ADDRESS;
      data_struct.data = a;
      // printf("Setting up address reg (%x) with:%x\n", data_struct.address, data_struct.data);
      result = ioctl(dev->handle, BCC_WRITE_IF_REGISTER, &data_struct);

      //      result = write(dev->handle, &j, 0);   //Update the address pointer
      //      if(result == -1 || result != 1)
      if(result < 0)
        printf("Unable to write to BCCIF addr reg: error:%s\n", strerror(errno));                 
      
      //write a word
      data_struct.address = BCC_DATA;
      data_struct.data = databuf[1];
      // printf("Writing to data reg (%x) with:%x\n", data_struct.address, data_struct.data);
      result = ioctl(dev->handle, BCC_WRITE_IF_REGISTER, &data_struct);

      
      //      result = write(dev->handle, databuf, 1);

      if (result < 0)
        printf("Unable to write data - %s\n", strerror(errno));
      a++;}
    free(data);                            //When all writing is finished free the buffer
    break;
  case 'r':
  case 'R':                              //Read 1 word from an interface register, uses interfaces internal address offsets
    if (processing[1] != ' '){                    //Check that the character after the command is a space
      printf("Unrecognised command\n");
      break; }
    
    //Drop the command character off the string
    processing += sizeof(char);
    result = sscanf(processing, " %hx", &a);            //Convert the address parameter into a number
    if (result != 1) {
      printf("Invalid data, specify 1 16 bit number\n");
      break; }  
    
    data_struct.address = a;    
    result = ioctl(dev->handle, BCC_READ_IF_REGISTER, &data_struct);  //Use the interface read/write io command  
    if (result < 0)
      printf("Unable to get register data - %s\n", strerror(errno));  
    
    printf("%04hx:%04hx\n", a, data_struct.data);          //Print the returned data
    break;
  case 'w':
  case 'W':                              //Complement of above, write 1 word to an interface register
    if (processing[1] != ' '){                    //Check that the character after the command is a space
      printf("Unrecognised command\n");
      break; }
    
    //Drop the command character off the string
    processing += sizeof(char);
    result = sscanf(processing, " %hx %hx", &a, &d);        //Convert the two parameters to numbers            
    if (result != 2) {
      printf("Invalid data, specify 2 16 bit numbers\n");
      break; }            
    
    data_struct.address = a;
    data_struct.data = d;                      //Setup the data structure
    
    result = ioctl(dev->handle, BCC_WRITE_IF_REGISTER, &data_struct);  //Write the data using the ioctl write command  
    if (result < 0)
      printf("Unable to write register data - %s\n", strerror(errno));            
    
    break;
  case 't':
  case 'T':                              //run a test sequence on the bus, i.e writes a series of random values to the bus and then reads them back and compares, alos useful for benchmark testing the speed of the interfaces
    if (processing[1] != ' '){                    //Check that the character after the command is a space
      printf("Unrecognised command\n");
      break; }
    
    //Drop the command character off the string
    ++processing;
    result = sscanf(processing, " %hx %hx %hx", &s, &x, &y);    //Convert the 3 required parameters
    if (result != 3) {
      printf("Invalid data, specify 3 16 bit numbers\n");
      break; }            
    
    //Allocate enough space for the randomly generated data set
    dataset = (unsigned short int*) malloc(y * sizeof(unsigned short int));
    
    //Generate the random data between 0 and 0xffff
    for (j = 0; j < y; j++) {
      //Generate a random number for each write 
      dataset[j] = (int) (65535.0 * rand()/(RAND_MAX + 1.0)); }
    
    //Initially there are no errors
    errors = 0;
    
    //The locations are chosen sequentially starting from s
    start=(float)clock();                      //Save the start time for benchmarking
    for (k = s; k < (x + s); k++) {                  //Loop through the addresses
      for (j = 0; j < y; j++) {                  //Loop through the test values
        databuf[0] = k;                      //The address
        databuf[1] = dataset[j];                //The data
        result = write(dev->handle, &databuf, 1);        //1 data word after the address -> returns 2
        if(result == -1 || result != 2)
    printf("Unable to write to device with error - %s\n", strerror(errno));    
        
        //Incase of auto increment rewrite the address
        result = write(dev->handle, &k, 0);            //0 data words = set address only; //Ensure that the read occurs on the right address
        if(result == -1 || result != 1)
    printf("Unable to write to device with error - %s\n", strerror(errno));                 
        
        databuf[0] = k;                    //address for the read-back
        result = read(dev->handle, &databuf, 1);        //data returns in databuf[1]
        if(result == -1)
    printf("Unable to read from device with error - %s\n", strerror(errno));
        else {
    if (databuf[1] != dataset[j]) {
      printf("Error occured data not equal - Sent:%hx, Recieved:%hx\n", dataset[j], databuf[1]);
      errors++;}}}}
    
    total=((clock() - start)/1000);                //Get the final finishing time
    printf("Out of %d words %d errored\n", x*y, errors);    //Display errors
    printf("Average speed was %7.3f Words/mSec\n", (float) (x*y*3)/total);  //Display the words/mSec
    free(dataset);                        //Free the data set memory
    break;
  case '\n':                              //Repeat the last command
    memcpy(buffer, last_cmd, strlen(last_cmd));            //Copy the buffer back again
    processing = buffer;                      //Reset the counting pointer
    End_ptr = buffer + strlen(buffer) * sizeof(char);        //Reset the end pointer
    continue;      
  default:
    printf("Unrecognised command\n");
  }
  
  processing += ((strlen(processing)) + 1);                //Multiple commands can be seperated on the 1 line by a colon, update the counting pointer and get the next command
      } while (processing < End_ptr);                        //Loop until we run out of commands
    } while(1);    
    free(buffer);                                  //Free the console buffer
}

/* Gets a specified number of characters from the input stream */
/* Blocks program until an enter is pressed */
/*
 * The original read fgets(data, ++number, stdin).  Every caller declares its
 * buffer as exactly "number" bytes, so that wrote one byte past the end of the
 * buffer on every single call - harmless-looking in 2004, a _FORTIFY_SOURCE
 * abort on Ubuntu 24.04.  This version consumes the whole line (so the next
 * prompt is not fed the leftover newline), still reports an empty line as "\n"
 * because the menus test for that, and writes at most "number" bytes.
 */
char * get_input(int number, char* data)
{
  char line[MAX_LINE_LENGTH * 2];
  size_t len;

  if (number < 1) number = 1;

  if (fgets(line, sizeof(line), stdin) == NULL) {
    data[0] = '\0';
    return data; }

  if (line[0] == '\n' || line[0] == '\0') {          //Bare enter
    if (number > 1) {
      data[0] = '\n';
      data[1] = '\0';
    } else
      data[0] = '\0';
    return data; }

  len = strcspn(line, "\n");                         //Drop the trailing newline
  if (len > (size_t)(number - 1)) len = (size_t)(number - 1);

  memcpy(data, line, len);
  data[len] = '\0';

  return data;
}

//////////////////////////////////////////////////////////////////////////////////////
//                                   Initialisation                                 //
//////////////////////////////////////////////////////////////////////////////////////

/* Initilaise the device data structure */
if_data_t * bcc_test_init(void)
{
  if_data_t *dev;                                  //Device structure  

  //Allocate the memory
  dev = (if_data_t *) malloc(sizeof(if_data_t));  

  //Clear the structure
  memset(dev, 0, sizeof(if_data_t));
  
  dev->device = DEFAULT_DEVICE;                          //Set the device to default  
  return dev;
}


//////////////////////////////////////////////////////////////////////////////////////
//                                     Interface                                    //
//////////////////////////////////////////////////////////////////////////////////////

/* Open an external interface */
void open_interface(if_data_t **device)
{
  if_data_t *dev = (*device);                            //Local copy of device data structure
  char *buffer;                                  //Pointer to character buffer
  unsigned short int data;                            //Variable passed to driver
  int serial_len = 0;                                 //GET_SERIAL_NUMBER_LENGTH is declared int*, not short*
  int result;                                    //Return variable from function calls
  
  //Allocate a buffer space  
  buffer = (char*) malloc(MAX_LINE_LENGTH);
  
  //Get device name
  sprintf(buffer, "/dev/%s%d", BCCIF_NAME, dev->device);
  
  //Open the device
  dev->handle = open(buffer, O_RDWR);
  if (dev->handle < 0) {
    printf("Unable to open device %s%d with error - %s\n", BCCIF_NAME, dev->device, strerror(errno));
    return; }

    printf("Success....%s opened\n", buffer);

    //Get the operating mode    
  result = ioctl(dev->handle, BCC_GET_MK_MODE, &data);  
  if (result == -1)  
    printf("Unable to get mode - %s\n", strerror(errno));
  else
    dev->mode = (data + 1);
    
   //Get the serial number string, first the length and then the number itself
  result = ioctl(dev->handle, BCC_GET_SERIAL_NUMBER_LENGTH, &serial_len);  
  if (result == -1) {
    printf("Unable to get serial number length - %s\n", strerror(errno));
    dev->serial_no = NULL; }
  else {    
    dev->serial_no = (char*) calloc(serial_len + 1, sizeof(char));            //With the length found we can allocate a buffer of the correct size
    result = ioctl(dev->handle, BCC_GET_SERIAL_NUMBER, dev->serial_no);      //Get the serial number
    if (result == -1)
      printf("Unable to get serial number - %s\n", strerror(errno));}
  
    free(buffer);                                  //Free the temp buffer
}

/* Close one of the external interfaces */
void close_interface(if_data_t *dev)
{
  //Close the handle
  close(dev->handle);  
  
  //Free the serial number code buffer
  free(dev->serial_no);
}
