//Author: Andrew Brown
//Version: 1.00
//Description: The test application for the BCC driver

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <asm/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>

#include "../../bcc_ioctrl.h"

#define WORDTEST		0x7fff
#define MK2WORDS		0xffff

int main(int argc, char *argv[])
{
	int Handle, Handle2;											//File handle to the BCC	
	int result, i;
	unsigned short int buffer[WORDTEST] = {0};
	unsigned short int data;
	int serial_len = 0;						//BCC_GET_SERIAL_NUMBER_LENGTH is declared int*, not short*
	char *Serial = NULL;
	int recv;
	int errors;	
	errors = 0;	

	/*
	 * Bounds, so this can run unattended.  Defaults reproduce the original
	 * test; pass smaller values for a quick functional pass.
	 *   argv[1] = words per read/write loop   (default 0x7fff)
	 *   argv[2] = MK2 soak passes, 0 = until first error (original behaviour)
	 */
	int nwords = 0x7fff;
	long maxloops = 1;
	if (argc > 1) nwords = atoi(argv[1]);
	if (argc > 2) maxloops = atol(argv[2]);
	if (nwords < 2 || nwords > WORDTEST) nwords = WORDTEST;
	printf("test_app: nwords=%d maxloops=%ld\n", nwords, maxloops);
	
	
	
		
	Handle = open("/dev/bccif1", O_RDWR);
	if(Handle == -1) {
		printf("Unable to open device bccif1 with error - %s\n", strerror(errno));
		return -1; }
/*	else
		printf("Handle opened - %x\n", Handle); */

	result = ioctl(Handle, BCC_RESET_INTERFACES, NULL);	
	if (result == -1)	
		printf("Unable to do interface reset - %s\n", strerror(errno));
/*	else
		printf("Interface reset suceeded\n"); 		*/

	//Set to MK2 mode
/*	result = ioctl(Handle, BCC_SET_MK2_MODE, NULL);	
	if (result == -1)	
		printf("Unable to set interface mode - %s\n", strerror(errno));
	else
		printf("Interface mode set to MK2\n"); 		*/
		
	//Get the mode
	result = ioctl(Handle, BCC_GET_MK_MODE, &data);	
	if (result == -1)	
		printf("Unable to get mode - %s\n", strerror(errno));
	else{
		if (data == 1)	
			printf("MK2 mode is selected\n"); 
		else
			printf("MK1 mode is selected\n"); }

	//Test second open			
	Handle2 = open("/dev/bccif1", O_RDWR);
	if(Handle == -1) {
		printf("Unable to open device bccif1 with error - %s\n", strerror(errno));
		return -1; }				
			
	//Get the length of the serial number
	result = ioctl(Handle, BCC_GET_SERIAL_NUMBER_LENGTH, &serial_len);	
	if (result == -1) {
		printf("Unable to get serial number - %s\n", strerror(errno));
		serial_len = 0; }
/*	else 
		printf("Length of serial number is %d\n", serial_len); */

	if (serial_len > 0) {
		Serial = (char*) calloc(serial_len + 1, sizeof(char)); 				
		
		result = ioctl(Handle2, BCC_GET_SERIAL_NUMBER, Serial);		
		if (result == -1)
			printf("Unable to get serial number - %s\n", strerror(errno));
		else 
			printf("Serial number determined to be:'%s'\n", Serial); }									
								
	result = close(Handle2);
	if(result == -1)
		printf("Unable to close device bccif1 with error - %s\n", strerror(errno));
	else
		printf("Handle Closed\n");				
			
	//Get the auto increment status
/*	result = ioctl(Handle, BCC_GET_INCREMENT_ENABLE, &data);	
	if (result == -1)	
		printf("Unable to get auto increment status - %s\n", strerror(errno));
	else
		printf("Increment status is:%d\n", data); 		*/

	//Zero Memory on MK1
	for (i = 0; i < nwords; i++) {
	
		buffer[0] = i; 
		buffer[1] = 0;				
		result = write(Handle, &buffer, 1);		//buf[0]=address, buf[1]=data -> returns 2
		if(result == -1 || result != 2)
			printf("Unable to write to device bccif1 with error - %s\n", strerror(errno)); }		

	//Check zero
	for (i = 0; i < nwords; i++) {
	
		buffer[0] = i; 			
		//read() writes buf[0] to the address register itself - no separate write
		result = read(Handle, &buffer, 1);
		if(result == -1)
			printf("Unable to read from device bccif1 with error - %s\n", strerror(errno));
		else {
			if (buffer[1] != 0) {
				printf("MK1:Error occured data not equal - Recieved:%d\n", buffer[1]);
				errors++;}}}

		printf("MK1:Memory Cleared out of %d words %d errored\n", i, errors);				
			
			
			
			
					
	//Test write speed on MK1
	for (i = 0; i < nwords; i++) {
	
		buffer[0] = 0x7ffe; 
		buffer[1] = i;				
		result = write(Handle, &buffer, 1);		//one data word after the address
		if(result == -1 || result != 2)
			printf("Unable to write to device bccif1 with error - %s\n", strerror(errno)); 	 

		buffer[0] = 0x7ffe;				//address for the read-back
		result = read(Handle, &buffer, 1);
		if(result == -1)
			printf("Unable to read from device bccif1 with error - %s\n", strerror(errno));
		else {
			if (buffer[1] != i) {
				printf("MK1:Error occured data not equal - Sent:%d, Recieved:%d\n", i, buffer[1]);
				errors++;}}}
							
	printf("MK1:Out of %d words %d errored\n", i, errors);		
		
		
	result = ioctl(Handle, BCC_SET_INCREMENT_ENABLE, NULL);	
	if (result == -1)	
		printf("Unable to set auto increment status - %s\n", strerror(errno));

	for (i=1 ;i <nwords; i++) {
		buffer[i] = (i - 1); }		
		buffer[0] = 0;	
										
	result = write(Handle, &buffer, nwords - 1);		//nwords-1 data words follow the address
	if(result == -1)
		printf("Unable to write to device bccif1 with error - %s\n", strerror(errno));
	else
		printf("Write completed OK, wrote %d words\n", result);
	
	buffer[0] = 0;
									//read() sets the address from buf[0] itself
	result = read(Handle, &buffer, nwords - 1);		
	if(result == -1)
		printf("Unable to read from device bccif1 with error - %s\n", strerror(errno));
	else {
		for(i=0; i < result - 1; i++){
			if (buffer[i + 1] != i) {
				printf("Error data did not match %d != %d\n", buffer[i + 1], i);
				break;}}
	}
	if (i == nwords - 1) printf("Read completed OK, read %d words correctly out of %d \n", i, nwords - 1);

	
		
	//Test the interrupts
	result = ioctl(Handle, BCC_ENABLE_GLOBAL_INTERRUPTS, &data);	
	if (result == -1)	
		printf("TEST1(ERR):Unable to set global interrupt state - %s\n", strerror(errno));
	else
		printf("TEST1(OK):Global interrupts on\n"); 			
	
	result = ioctl(Handle, BCC_ENABLE_TIMEOUT_INTERRUPT, &data);	
	if (result == -1)	
		printf("TEST1(ERR):Unable to set timeout interrupt state - %s\n", strerror(errno));
	else
		printf("TEST1(OK):Timeout interrupts on\n"); 

	result = ioctl(Handle, BCC_ENABLE_OVERFLOW_INTERRUPT, &data);	
	if (result == -1)	
		printf("TEST1(ERR):Unable to set overflow interrupt state - %s\n", strerror(errno));
	else
		printf("TEST1(OK):Overflow interrupts on\n"); 			
	
	result = ioctl(Handle, BCC_CLR_OVERFLOW_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("TEST1(ERR):Unable to clear overflow occured status - %s\n", strerror(errno));
	else
		printf("TEST1(OK):Overflow status cleared\n"); 		
		
	//Test Overflow	
	buffer[0] = 0x7ffd;
	buffer[1] = 0x1111;
	buffer[2] = 0x2222;
	buffer[3] = 0x3333;
	buffer[4] = 0x4444;
	buffer[5] = 0x5555;
	buffer[6] = 0x6666;
	buffer[7] = 0x7777;
	buffer[8] = 0x8888;
	buffer[9] = 0x9999;
	buffer[10] = 0xaaaa;
	result = write(Handle, &buffer[0], 51);								//write data	
	if (result == -1)	
		printf("TEST1(ERR):Error occured during write - %s\n", strerror(errno));
	else {
		if (result == 0)
			printf("TEST1(OK):Write sucecceded, wrote %d words\n", result);
		else
			printf("TEST1(ERR):Write sucecceded, wrote %d words(0)\n", result);}
		
	result = ioctl(Handle, BCC_GET_OVERFLOW_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("TEST1(ERR):Unable to get overflow occured status - %s\n", strerror(errno));
	else {
		if (data == 1)
			printf("TEST1(OK):Overflow occured\n"); 
		else
			printf("TEST1(ERR):Overflow did not occur\n"); 	}
	
	result = ioctl(Handle, BCC_GET_TIMEOUT_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("TEST2(ERR):Unable to get timeout occured status - %s\n", strerror(errno));
	else {
		if (data == 1)
			printf("TEST1(ERR):Timeout occured\n"); 
		else
			printf("TEST1(OK):Timeout did not occur\n"); 	}	
			
	
			
	//Test timeout	
	result = ioctl(Handle, BCC_CLR_OVERFLOW_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("TEST1(ERR):Unable to clear overflow occured status - %s\n", strerror(errno));
	else
		printf("TEST1(OK):Overflow status cleared\n"); 	
		
	result = ioctl(Handle, BCC_CLR_TIMEOUT_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("TEST2(ERR):Unable to clear timeout occured status - %s\n", strerror(errno));
	else
		printf("TEST2(OK):Timeout status cleared\n"); 		

	buffer[0] = 0x1111;
	result = ioctl(Handle,BCC_WRITE_REGISTER_0, &buffer);								//write data to reg 0
	if (result == -1)	
		printf("TEST2(OK):Error occured during write (timeout) - %s\n", strerror(errno));
	else
		printf("TEST2(ERR):Write suceeded\n"); 			
		
	result = ioctl(Handle, BCC_GET_TIMEOUT_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("TEST2(ERR):Unable to get timeout occured status - %s\n", strerror(errno));
	else {
		if (data == 1)
			printf("TEST2(OK):Timeout occured\n"); 
		else
			printf("TEST2(ERR):Timeout did not occur\n"); 	}			
	
	result = ioctl(Handle, BCC_GET_OVERFLOW_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("TEST1(ERR):Unable to get overflow occured status - %s\n", strerror(errno));
	else {
		if (data == 1)
			printf("TEST2(ERR):Overflow occured\n"); 
		else
			printf("TEST2(OK):Overflow did not occur\n"); 	}			
			
			
			
			
					
	//Clear status		
/*	result = ioctl(Handle, BCC_CLR_INCREMENT_ENABLE, NULL);	
	if (result == -1)	
		printf("Unable to clear auto increment status - %s\n", strerror(errno));
	else
		printf("Set increment status suceeded\n"); 	
		
	result = ioctl(Handle, BCC_GET_INCREMENT_ENABLE, &data);	
	if (result == -1)	
		printf("Unable to get auto increment status - %s\n", strerror(errno));
	else
		printf("Increment status is:%d\n", data); 	*/
		

	//set read/set/read of software delay
/*	result = ioctl(Handle, BCC_GET_TIMEOUT_DELAY, &data);
	if (result == -1)	
		printf("Unable to get software delay length - %s\n", strerror(errno));
	else
		printf("Software delay is:%d\n", data); 			
	
	data = 25;
	result = ioctl(Handle, BCC_SET_TIMEOUT_DELAY, &data);
	if (result == -1)	
		printf("Unable to set software delay length - %s\n", strerror(errno));
	else
		printf("Software delay set\n"); 		

	result = ioctl(Handle, BCC_GET_TIMEOUT_DELAY, &data);
	if (result == -1)	
		printf("Unable to get software delay length - %s\n", strerror(errno));
	else
		printf("Software delay is:%d\n", data); 

	//Read, set, read, clear, read software delay enable
	result = ioctl(Handle, BCC_GET_SOFTWARE_TIMEOUT, &data);	
	if (result == -1)	
		printf("Unable to get software timeout status - %s\n", strerror(errno));
	else
		printf("Software timeout status is:%d\n", data); 		
		
	result = ioctl(Handle, BCC_SET_SOFTWARE_TIMEOUT, NULL);	
	if (result == -1)	
		printf("Unable to set software timeout - %s\n", strerror(errno));
	else
		printf("Software timeout set\n"); 			

	result = ioctl(Handle, BCC_GET_SOFTWARE_TIMEOUT, &data);	
	if (result == -1)	
		printf("Unable to get software timeout status - %s\n", strerror(errno));
	else
		printf("Software timeout status is:%d\n", data); 	
	
	result = ioctl(Handle, BCC_CLR_SOFTWARE_TIMEOUT, NULL);	
	if (result == -1)	
		printf("Unable to clear software timeout - %s\n", strerror(errno));
	else
		printf("Software timeout cleared\n"); 		
		
	result = ioctl(Handle, BCC_GET_SOFTWARE_TIMEOUT, &data);	
	if (result == -1)	
		printf("Unable to get software timeout status - %s\n", strerror(errno));
	else
		printf("Software timeout status is:%d\n", data); */


		
		
		
		
////////////////////////////////////////////////////////////////////////////////////////////////////				
		
	//Test Overflow flag
	//Check that it is 0 now
/*	result = ioctl(Handle, BCC_GET_OVERFLOW_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("Unable to get overflow status - %s\n", strerror(errno));
	else
		printf("Overflow occured is:%d\n", data);	
	
	//Enable auto increment
	result = ioctl(Handle, BCC_SET_INCREMENT_ENABLE, NULL);	
	if (result == -1)	
		printf("Unable to set auto increment status - %s\n", strerror(errno));
	else
		printf("Set increment status suceeded\n");
	
	buffer[0] = 0x7FFE; 
	buffer[1] = 0x0001;
	buffer[2] = 0x0002; 
	buffer[3] = 0x0003; 
	result = write(Handle, &buffer, 4);
	if(result == -1)
		printf("Unable to write to device bccif1 with error - %s\n", strerror(errno));
	else
		printf("Write completed OK, wrote %d words\n", result); 
		
	//Make sure it's set
	result = ioctl(Handle, BCC_GET_OVERFLOW_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("Unable to get overflow status - %s\n", strerror(errno));
	else
		printf("Overflow occured is:%d\n", data);
		
	//Clear it
	result = ioctl(Handle, BCC_CLR_OVERFLOW_OCCURED_STATE, NULL);	
	if (result == -1)	
		printf("Unable to clear overflow status - %s\n", strerror(errno));
	else
		printf("Overflow cleared\n");						
	
	//Make sure it cleared
	result = ioctl(Handle, BCC_GET_OVERFLOW_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("Unable to get overflow status - %s\n", strerror(errno));
	else
		printf("Overflow occured is:%d\n", data);		*/		
		
////////////////////////////////////////////////////////////////////////////////////////////////////		
		
		
	result = close(Handle);
	if(result == -1)
		printf("Unable to close device bccif1 with error - %s\n", strerror(errno));
	else
		printf("Handle Closed\n");		
	
	free(Serial);

////////////////////////////////////////////////////////////////////////////////////////////////////	
	//Test timeout indicator

	Handle = open("/dev/bccif2", O_RDWR);
	if(Handle == -1) {
		printf("Unable to open device bccif2 with error - %s\n", strerror(errno));
		return -1; } 
	else
		printf("Handle opened - %x\n", Handle);		
	
	//Get the mode
	result = ioctl(Handle, BCC_GET_MK_MODE, &data);	
	if (result == -1)	
		printf("Unable to get mode - %s\n", strerror(errno));
	else{
		if (data == 1)	
			printf("MK2 mode is selected\n"); 
		else
			printf("MK1 mode is selected\n"); }

/*	data = 25;
	result = ioctl(Handle, BCC_SET_TIMEOUT_DELAY, &data);
	if (result == -1)	
		printf("Unable to set software delay length - %s\n", strerror(errno));
	else
		printf("Software delay set\n"); 		

	result = ioctl(Handle, BCC_GET_TIMEOUT_DELAY, &data);
	if (result == -1)	
		printf("Unable to get software delay length - %s\n", strerror(errno));
	else
		printf("Software delay is:%d\n", data); 

	//Read, set, read, clear, read software delay enable
	result = ioctl(Handle, BCC_GET_SOFTWARE_TIMEOUT, &data);	
	if (result == -1)	
		printf("Unable to get software timeout status - %s\n", strerror(errno));
	else
		printf("Software timeout status is:%d\n", data); 		
		
	result = ioctl(Handle, BCC_SET_SOFTWARE_TIMEOUT, NULL);	
	if (result == -1)	
		printf("Unable to set software timeout - %s\n", strerror(errno));
	else
		printf("Software timeout set\n");			
			
	result = ioctl(Handle, BCC_GET_SOFTWARE_TIMEOUT, &data);	
	if (result == -1)	
		printf("Unable to get software timeout status - %s\n", strerror(errno));
	else
		printf("Software timeout status is:%d\n", data); 			

	//Set the interrupt for timeout
	result = ioctl(Handle, BCC_ENABLE_GLOBAL_INTERRUPTS, NULL);	
	if (result == -1)	
		printf("Unable to enable global interrupts - %s\n", strerror(errno));
	else
		printf("Global interrupts enabled"); 
	result = ioctl(Handle, BCC_ENABLE_TIMEOUT_INTERRUPT, NULL);	
	if (result == -1)	
		printf("Unable to enable timeout interrupt - %s\n", strerror(errno));
	else
		printf("Timeout interrupt enabled"); 	
							
	//Check that it is 0 now
	result = ioctl(Handle, BCC_GET_TIMEOUT_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("Unable to get timeout status - %s\n", strerror(errno));
	else
		printf("Timeout occured is:%d\n", data);		

	buffer[0] = 0x0001; 
	buffer[1] = 0x0001;				
	result = write(Handle, &buffer, 2);		
	if(result == -1)
		printf("Unable to write to device bccif2 with error - %s\n", strerror(errno));
	else
		printf("Write completed OK, wrote %d words\n", result); 	
	
	//Check that it is 1 now
	result = ioctl(Handle, BCC_GET_TIMEOUT_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("Unable to get timeout status - %s\n", strerror(errno));
	else
		printf("Timeout occured is:%d\n", data);
		
	//Clear it
	result = ioctl(Handle, BCC_CLR_TIMEOUT_OCCURED_STATE, NULL);	
	if (result == -1)	
		printf("Unable to clear timeout status - %s\n", strerror(errno));
	else
		printf("Timeout cleared\n");						
	
	//Make sure it cleared
	result = ioctl(Handle, BCC_GET_TIMEOUT_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("Unable to get timeout status - %s\n", strerror(errno));
	else
		printf("Timeout occured is:%d\n", data);		*/		
	

		
	if (isatty(STDIN_FILENO)) {
		printf("Press any Key\n");
		if (scanf("%d", &recv) != 1) recv = 0; }
		
	clock_t start = 0,total = 0;
	errors = 0;
	i = 0;
	long k = 0;
	//Test writes on MK2 interface	
	do {

		if (i==0) start=(float)clock();
			
		buffer[0] = 0x1001; 
		buffer[1] = i;				
		result = write(Handle, &buffer, 1);		//one data word after the address
		if(result == -1 || result != 2)
			printf("Unable to write to device bccif2 with error - %s\n", strerror(errno)); 
//		else
//			printf("Write completed OK, wrote %d words\n", result); 		 
	
		buffer[0] = 0x1001;
		result = read(Handle, &buffer, 1);		//value -> buffer[1]
		if(result == -1)
			printf("Unable to read from device bccif2 with error - %s\n", strerror(errno));		
		buffer[2] = 0x1001;
		result = read(Handle, &buffer[2], 1);		//value -> buffer[3]
		if(result == -1)
			printf("Unable to read from device bccif2 with error - %s\n", strerror(errno));		
		
		if (buffer[1] != buffer[3] || buffer[1] != i || buffer[3] != i){
			printf("Error Data not equal - Sent:%d, Recieved (1):%d, Recieved (2):%d\n", i, buffer[1], buffer[3]);
			errors++;}
	
		i++;
		
		if (i > nwords) {
			i = 0;
			k++;			
			total=((clock() - start)/1000);		
			if (total > 0)
				printf("Bitrate is words/mSec %f or words/sec %f\n", (float)nwords*3/total, (float)nwords*3000/total);}
				
	}while(errors < 1 && (maxloops == 0 || k < maxloops) ); 
							
	printf("MK2:Out of %d words in %ld loops %d errored\n", i, k, errors); 
/*		
	//Make sure it cleared
	result = ioctl(Handle, BCC_GET_TIMEOUT_OCCURED_STATE, &data);	
	if (result == -1)	
		printf("Unable to get timeout status - %s\n", strerror(errno));
	else
		printf("Timeout occured is:%d\n", data);	*/	

	
/*		buffer[0] = 0x1001; 
		buffer[1] = 0x2222;				
		result = write(Handle, &buffer, 2);		
		if(result == -1 || result != 2)
			printf("Unable to write to device bccif1 with error - %s\n", strerror(errno)); 
		
	
	recv = 0;		
	do {		
					 
		buffer[0] = 0x1001;
		buffer[1] = recv;
		result = write(Handle, &buffer, 2);			
		
		result = read(Handle, &buffer, 1);		
		if(result == -1)
			printf("Unable to read from device bccif2 with error - %s\n", strerror(errno));
		else {
			if (buffer[0] != recv )printf("Data returned - %d, %d\n", buffer[0], recv); }		
		recv++;				
	} while (recv < 1000);		*/
		
		
				
	result = close(Handle);
	if(result == -1)
		printf("Unable to close device bccif2 with error - %s\n", strerror(errno));
	else
		printf("Handle Closed\n");			

	printf("test_app: %d errors\n", errors);
	return errors ? 1 : 0;
}
