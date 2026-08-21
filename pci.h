//////////////////////////////////////////////////////////////////////////////////////
//                         Function prototypes for pci.c                            //
//////////////////////////////////////////////////////////////////////////////////////

#ifndef PCI_H
#define PCI_H

/* 32 bit reads & writes */
unsigned int ReadPCIDWord(unsigned long int);
unsigned int WritePCIDWord(unsigned long int, unsigned int);

/* 16 bit reads & writes */
unsigned short int ReadPCIWord(unsigned long int);
unsigned short int WritePCIWord(unsigned long int, unsigned short int);

/* 8 bit reads & writes */
unsigned char ReadPCIByte(unsigned long int);
unsigned char WritePCIByte(unsigned long int, unsigned char);

#endif /* #ifndef PCI_H */
