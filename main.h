//////////////////////////////////////////////////////////////////////////////////////
//                        Function prototypes for bccif.c                           //
//////////////////////////////////////////////////////////////////////////////////////

#ifndef BCCIF_H
#define BCCIF_H

/* Loading and Unloading of the driver */
int init_module(void);
void cleanup_module(void);

/* Initialisation functions */
bccif_info_struct *InitSysInfo(bccif_info_struct *);
int Find_BCC(int*, bccif_info_struct *);
int Initialise_BCC_Card(bccif_info_struct *);

#endif /* #ifndef BCCIF_H */

