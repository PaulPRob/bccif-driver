//////////////////////////////////////////////////////////////////////////////////////
//                       Function prototypes for interrupts.c                       //
//////////////////////////////////////////////////////////////////////////////////////

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

/* Initialisation and desstroy functions */
int Install_ISR(bccif_info_struct *);
void Remove_ISR(bccif_info_struct *);

/* Interrupt handler */
void BCC_Interrupt(int, void *, struct pt_regs *);

#endif
