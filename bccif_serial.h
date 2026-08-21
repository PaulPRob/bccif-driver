//////////////////////////////////////////////////////////////////////////////////////
//                     Function prototypes for bccif_serial.c                       //
//////////////////////////////////////////////////////////////////////////////////////

#ifndef BCCIF_SERIAL_H
#define BCCIF_SERIAL_H

/* Exported functions */
int Get_BCC_Serial(bccif_info_struct *);
int Initialise_BCC_Serial(bccif_info_struct *);

/* Internal helper functions */
int read_prom_bit(bccif_info_struct *);
unsigned char read_prom_byte(bccif_info_struct *);
void clock_prom(bccif_info_struct *);

#endif /* #ifndef BCCIF_SERIAL_H */
