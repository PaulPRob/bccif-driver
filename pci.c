//Author: Andrew Brown
//Version: 1.01
//Description: The pci based read & write routines

#include "includes.h"	//Generate include file

extern int debug;																	//Debug variable defined in main.c

//////////////////////////////////////////////////////////////////////////////////////
//                               Read/Write Functions                               //
//////////////////////////////////////////////////////////////////////////////////////

/* Read a 32 bit word from the PCI bus */
unsigned int ReadPCIDWord(unsigned long int addr)
{
	unsigned int p;

	p = readl((int *) addr);
    
	if(debug >= DEBUG_PCI) 
		printk(KERN_INFO "BCCIF:Read 32 bits %x from %lx\n", p, addr);    
    
    return(p);
}

/* Write a 32 bit word to the PCI bus */
unsigned int WritePCIDWord(unsigned long int addr, unsigned int data)
{
	writel(data, (int *) addr);
	
	if(debug >= DEBUG_PCI) 
		printk(KERN_INFO "BCCIF:Wrote 32 bits %x to %lx\n", data, addr);


    return(data);
}

/* Read a 16 bit word from the PCI bus */
unsigned short int ReadPCIWord(unsigned long int addr)
{
	unsigned short int p;

	p = readw((short int *) addr);
       
	if(debug >= DEBUG_PCI) 
		printk(KERN_INFO "BCCIF:Read 16 bits %x from %lx\n", p, addr);
        
    return(p);
}

/* Write a 16 bit word to the PCI bus */
unsigned short int WritePCIWord(unsigned long int addr, unsigned short int data)
{
	writew(data, (short int *) addr);

    if(debug >= DEBUG_PCI) 
		printk(KERN_INFO "BCCIF:Wrote 16 bits %x to %lx\n", data, addr);
      
    return(data);
}

/* Read an 8 bit byte from the PCI bus */
unsigned char ReadPCIByte(unsigned long int addr)
{  
	unsigned char p;
	    
	p = readb((char *) addr);
	
	if(debug >= DEBUG_PCI) 
		printk(KERN_INFO "BCCIF:Read 8 bits %x from %lx\n", p, addr);
	   
    return(p);
}

/* Write an 8 bit byte to the PCI bus */
unsigned char WritePCIByte(unsigned long int addr, unsigned char data)
{
	writeb(data, (char *) addr);   

	if(debug >= DEBUG_PCI) 
		printk(KERN_INFO "BCCIF:Wrote 8 bits %x to %lx\n", data, addr);	
	
	return(data);
}

