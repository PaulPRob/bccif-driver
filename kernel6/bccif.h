/* SPDX-License-Identifier: GPL-2.0 */
/*
 * bccif.h - constants and shared prototypes for the CSIRO ATNF BCC PCI driver.
 *
 * Original driver: Andrew Brown, <Andrew.J.Brown@uts.edu.au>, 2004-2010 (kernel 2.2-2.6)
 * Port to Linux 6.8+: see README.md for the list of API changes this replaces.
 *
 * This header intentionally carries no LINUX_VERSION_CODE conditionals: the
 * kernel6 tree targets 6.8 and newer only.  The 2.6 sources in the parent
 * directory are kept for reference.
 */

#ifndef BCCIF_H
#define BCCIF_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/capability.h>
#include <linux/cdev.h>
#include <linux/cred.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kref.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>

/*
 * ---------------------------------------------------------------------------
 * Device constants (unchanged from includes.h)
 * ---------------------------------------------------------------------------
 */

/* PCI core implementation specifics */
#define BCCIF_BLOCKS		4	/* External interfaces on the PCI card.
					 * The 4th unit is special and is not a
					 * normal IF.  ERD 10/11/07
					 */
#define PCI_IMAGE0			/* Config access to bridge, for regdefs.h */
#define PCI_IMAGE1			/* External BCC IF access, for regdefs.h */

/* Driver specific options */
#define MAX_OPEN_IF		5	/* Times one user may open an interface */
#define MAX_DEVICES		5	/* Maximum cards supported in one host */
#define DEFAULT_MAJOR		0	/* 0 = allocate a major dynamically */

/* Serial PROM */
#define MAX_PREAMBLE_BITS	256	/* Assume no PROM after this many bits */
#define MAX_BCC_IDENT_LENGTH	0x100	/* Max 256 characters of serial number */

/* PCI card identification and BAR layout */
#define VENDOR_ID		0x2321	/* Generic vendor id */
#define DEVICE_ID		0x0001	/* BCC device id */
#define CONFIG_BAR		0	/* BAR holding the PCI core config regs */
#define CONFIG_REGION_SIZE	0x1000
#define BCCIF_BAR		1	/* BAR holding the interface registers */
#define BCCIF_REGION_SIZE	0x1000

/* Debug levels (module parameter "debug") */
#define DEBUG_ALL		0x6	/* Everything - will flood the console */
#define DEBUG_PCI		0x5	/* Every PCI read/write - floods logs */
#define DEBUG_IFRW		0x4	/* Every interface read/write */
#define DEBUG_IF		0x3	/* Interface transactions except rd/wr */
#define DEBUG_INFORMATION	0x2	/* Informational messages */
#define DEBUG_CRIT		0x1	/* Critical information only */
#define DEBUG_OFF		0x0	/* Silent */

#define BCCIF_DRV_NAME		"bccif"
#define BCCIF_VERSION		"2.0"

/* Module parameters, defined in main.c */
extern int debug;
extern unsigned int err_wait_us;

#define bccif_dbg(level, fmt, ...)					\
	do {								\
		if (debug >= (level))					\
			pr_info("BCCIF:" fmt, ##__VA_ARGS__);		\
	} while (0)

#include "bccif_dev.h"

/*
 * ---------------------------------------------------------------------------
 * Shared prototypes
 * ---------------------------------------------------------------------------
 */

/* pci.c - memory mapped register accessors */
u32 bccif_readl(void __iomem *base, unsigned int off);
void bccif_writel(void __iomem *base, unsigned int off, u32 data);
u16 bccif_readw(void __iomem *base, unsigned int off);
void bccif_writew(void __iomem *base, unsigned int off, u16 data);
u8 bccif_readb(void __iomem *base, unsigned int off);
void bccif_writeb(void __iomem *base, unsigned int off, u8 data);

/* interrupts.c */
irqreturn_t bccif_isr(int irq, void *data);
void bccif_irq_enable(struct bccif_dev *dev);
void bccif_irq_disable(struct bccif_dev *dev);

/* bccif_serial.c */
int bccif_serial_init(struct bccif_dev *dev);
int bccif_serial_read(struct bccif_dev *dev);

/* main.c - device lifetime */
void bccif_dev_get(struct bccif_dev *dev);
void bccif_dev_put(struct bccif_dev *dev);

/* file_ops.c */
extern const struct file_operations bccif_fops;

#endif /* BCCIF_H */
