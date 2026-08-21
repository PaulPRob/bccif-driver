//////////////////////////////////////////////////////////////////////////////////////
//                             PCI Core Register Offsets                            //
//////////////////////////////////////////////////////////////////////////////////////

#ifndef REGDEFS_H
#define REGDEFS_H

/* PCI configuration registers */
/* Definitions for WB specific addresses */
#ifdef PCI_IMAGE0											//If PCI image 0 is defined
	#ifdef HOST												//Only defined for host implementations 
		#define P_IMG_CTRL0 0x0100							//PCI Image 0 Control Register
		#define P_AM0		0x0108							//PCI Image 0 Address Mask
		#define P_TA0		0x010C							//PCI Image 0 Address Translation Register
	#endif
	#define P_BA0 			0x0104							//PCI Image 0 Base Address Register
#endif
#ifdef PCI_IMAGE1											//If PCI image 1 is defined
	#define P_IMG_CTRL1 	0x0110							//PCI Image 1 Control Register
	#define P_BA1 			0x0114							//PCI Image 1 Base Address Register
	#define P_AM1			0x0118							//PCI Image 1 Address Mask
	#define P_TA1			0x011C							//PCI Image 1 Address Translation Register
#endif
#ifdef PCI_IMAGE2											//If PCI image 2 is defined
	#define P_IMG_CTRL2 	0x0120							//PCI Image 2 Control Register
	#define P_BA2 			0x0124							//PCI Image 2 Base Address Register
	#define P_AM2			0x0128							//PCI Image 2 Address Mask
	#define P_TA2			0x012C							//PCI Image 2 Address Translation Register
#endif
#ifdef PCI_IMAGE3											//If PCI image 3 is defined
	#define P_IMG_CTRL3 	0x0130							//PCI Image 3 Control Register
	#define P_BA3 			0x0134							//PCI Image 3 Base Address Register
	#define P_AM3			0x0138							//PCI Image 3 Address Mask
	#define P_TA3			0x013C							//PCI Image 3 Address Translation Register
#endif
#ifdef PCI_IMAGE4											//If PCI image 4 is defined
	#define P_IMG_CTRL4 	0x0140							//PCI Image 4 Control Register
	#define P_BA4 			0x0144							//PCI Image 4 Base Address Register
	#define P_AM4			0x0148							//PCI Image 4 Address Mask
	#define P_TA4			0x014C							//PCI Image 4 Address Translation Register
#endif
#ifdef PCI_IMAGE5											//If PCI image 5 is defined
	#define P_IMG_CTRL5 	0x0150							//PCI Image 5 Control Register
	#define P_BA5 			0x0154							//PCI Image 5 Base Address Register
	#define P_AM5			0x0158							//PCI Image 5 Address Mask
	#define P_TA5			0x015C							//PCI Image 5 Address Translation Register
#endif
#define P_ERR_CS			0x0160							//PCI Error Control and Status Register
#define P_ERR_ADDR			0x0164							//PCI Erroneour Address register
#define P_ERR_DATA			0x0168							//PCI Erroneous Data register

/* Internal Interface Registers */
#define WB_CONF_SPC_BAR		0x0180							//Wishbone Config Space Base Address
#ifdef WB_IMAGE1											//If Wishbone image 1 is defined
	#define W_IMG_CTRL1 	0x0184							//Wishbone Image 1 Control Register
	#define W_BA1 			0x0188							//Wishbone Image 1 Base Address Register
	#define W_AM1			0x018C							//Wishbone Image 1 Address Mask
	#define W_TA1			0x0190							//Wishbone Image 1 Address Translation Register
#endif
#ifdef WB_IMAGE2											//If Wishbone image 2 is defined
	#define W_IMG_CTRL2 	0x0194							//Wishbone Image 2 Control Register
	#define W_BA2 			0x0198							//Wishbone Image 2 Base Address Register
	#define W_AM2			0x019C							//Wishbone Image 2 Address Mask
	#define W_TA2			0x01A0							//Wishbone Image 2 Address Translation Register
#endif
#ifdef WB_IMAGE3											//If Wishbone image 3 is defined
	#define W_IMG_CTRL3 	0x01A4							//Wishbone Image 3 Control Register
	#define W_BA3 			0x01A8							//Wishbone Image 3 Base Address Register
	#define W_AM3			0x01AC							//Wishbone Image 3 Address Mask
	#define W_TA3			0x01B0							//Wishbone Image 3 Address Translation Register
#endif
#ifdef WB_IMAGE4											//If Wishbone image 4 is defined
	#define W_IMG_CTRL4 	0x01B4							//Wishbone Image 4 Control Register
	#define W_BA4 			0x01B8							//Wishbone Image 4 Base Address Register
	#define W_AM4			0x01BC							//Wishbone Image 4 Address Mask
	#define W_TA4			0x01C0							//Wishbone Image 4 Address Translation Register
#endif
#ifdef WB_IMAGE5											//If Wishbone image 5 is defined
	#define W_IMG_CTRL5 	0x01C4							//Wishbone Image 5 Control Register
	#define W_BA5 			0x01C8							//Wishbone Image 5 Base Address Register
	#define W_AM5			0x01CC							//Wishbone Image 5 Address Mask
	#define W_TA5			0x01D0							//Wishbone Image 5 Address Translation Register
#endif
#define W_ERR_CS			0x01D4							//Wishbone Error Control and Status register
#define W_ERR_ADDR			0x01D8							//Wishbone Erroneous Address regsiter
#define W_ERR_DATA			0x01DC							//Wishbone Erroneous Data register

/* Definitions for Host bridges */
#ifdef HOST
	#define CNF_ADDR		0x01E0							//Configuration Cycle Generation Address register
	#define CNF_DATA		0x01E4							//Configuration Cycle Generation Data register
	#define INT_ACK			0x01E8							//Interrupt Acknowledge register
#endif

/* Interrupt Registers */
#define	ICR					0x01EC							//Interrupt Control register
#define ISR					0x01F0							//Interrupt Status register

/* Interrupt Enables & Masks */
#define INT					0x0001							//WB interrupt mask
#define INT_PROP_EN			0x0001							//WB interrupt propagation enable
#define WB_EINT_EN			0x0002							//WB error interrupt enable
#define WB_EINT				0x0002							//WB error interrupt mask

/* Other Masks */
#define SW_RST				0x80000000						//Wishbone reset bit in ICR register							
#define ERR_EN				0x00000001						//Wishbone error reporting enable bit
#define ERR_SIG				0x00000100						//Wishone error occured

//////////////////////////////////////////////////////////////////////////////////////
//                           Internal BCC Register Offsets                          //
//////////////////////////////////////////////////////////////////////////////////////

#define BCC_ADDRESS		0x0000	     //Address Register
#define BCC_DATA		0x0004	     //Data Register
#define BCC_REGISTER0		0x0008	     //MK1 Register 0 Access
#define BCC_REGISTER1		0x000C	     //MK1 Register 1 Access
#define BCC_REGISTER2		0x0010	     //MK1 Register 2 Access
#define BCC_REGISTER3		0x0014	     //MK1 Register 3 Access
#define BCC_REGISTER4		0x0018	     //MK1 Register 4 Access
#define BCC_REGISTER5		0x001C	     //MK1 Register 5 Access
#define BCC_STATUS		0x0020	     //Status Register
#define BCC_MODE		0x0024	     //Mode Register

#define BCC_SPACE		0x0400	     //Quantity of space allocated to each bcc interface, 1K

//////////////////////////////////////////////////////////////////////////////////////
//                             BCC Masks & Addresses                                //
//////////////////////////////////////////////////////////////////////////////////////

/* Address Register Breakdown */
#define BCC_OVERFLOW_MASK								0x8000
#define BCC_OVERFLOW_OFFSET								BCC_ADDRESS
#define BCC_ADDRESS_MASK								0x7FFF
#define BCC_ADDRESS_OFFSET								BCC_ADDRESS

/* Status Register Breakdown */
#define BCC_MASTER_INTERRUPT_ENABLE_MASK				0x0001	
#define BCC_MASTER_INTERRUPT_ENABLE_OFFSET				BCC_STATUS
#define	BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_MASK			0x0004
#define BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_OFFSET			BCC_STATUS
#define BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_MASK		0x0008
#define BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_OFFSET	BCC_STATUS
#define BCC_SERIAL_PROM_RESET_MASK						0x0010
#define BCC_SERIAL_PROM_RESET_OFFSET					BCC_STATUS
#define BCC_SERIAL_PROM_CLOCK_MASK						0x0020
#define BCC_SERIAL_PROM_CLOCK_OFFSET					BCC_STATUS
#define BCC_SERIAL_PROM_DATA_MASK						0x0080
#define BCC_SERIAL_PROM_DATA_OFFSET						BCC_STATUS
#define BCC_TIMEOUT_OCCURED_MASK						0x0040
#define BCC_TIMEOUT_OCCURED_OFFSET						BCC_STATUS
#define BCC_BUS_TIMEOUT_INTERRUPT_MASK					0x0100
#define BCC_BUS_TIMEOUT_INTERRUPT_OFFSET				BCC_STATUS
#define BCC_OVERFLOW_INTERRUPT_MASK						0x0400					
#define BCC_OVERFLOW_INTERRUPT_OFFSET					BCC_STATUS


/* Mode Regsiter Breakdown */
#define BCC_SOFTWARE_DELAY_MASK							0x001F
#define BCC_SOFTWARE_DELAY_OFFSET						BCC_MODE
#define BCC_SOFTWARE_DELAY_ENABLE_MASK					0x0020
#define BCC_SOFTWARE_DELAY_ENABLE_OFFSET				BCC_MODE
#define BCC_ADDRESS_INCREMENT_ENABLE_MASK				0x0040
#define BCC_ADDRESS_INCREMENT_ENABLE_OFFSET				BCC_MODE
#define BCC_MK2_MODE_SELECT_MASK						0x0100
#define BCC_MK2_MODE_SELECT_OFFSET						BCC_MODE



/***************************************************************
 Registers used in if_special module. This hangs of device bccif4 ERD 27/3/08
***************************************************************/
#define BCCIF_JTAG_MASTER_REG    0x0040
#define     BCCIF_JTAG_RESET         0x0100
#define     BCCIF_JTAG_LOAD          0x0200
#define BCCIF_JTAG_STATUS_REG    0x0044
#define     BCCIF_JTAG_RDY           0x0400
#define     BCCIF_JTAG_EOF           0x0200
#define     BCCIF_JTAG_ERR           0x0100
#define     BCCIF_JTAG_RUNNING       0x8000


#define BCCIF_JTAG_SPEED_BIT_OFFSET        29
#define BCCIF_JTAG_SPEED_MASK             0x3
#define BCCIF_JTAG_RESET_MAXITS          1099
#define BCCIF_JTAG_LOAD_MAXITS          10999
#define BCCIF_JTAG_THROTTLE_VALUE         1000
#define BCCIF_JTAG_THROTTLE_JIFFIES_DIV   100
#define BCCIF_JTAG_THROTTLE_JIFFIES_MAX   100

#define BCCIF_DIGITISER_ID_REG    0x0048

/***************************************************************/



#endif /* #ifndef REGDEFS */

