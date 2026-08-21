/* SPDX-License-Identifier: GPL-2.0 */
/*
 * bccif_dev.h - driver state.  Replaces bcc_struct.h from the 2.6 driver.
 *
 * The 2.6 driver kept one "master" bccif_info_struct and memcpy()'d the whole
 * thing into a fresh allocation on every open(), which meant every file
 * descriptor owned a *private* copy of the spinlock and of the error flags the
 * ISR is supposed to publish.  Here the shared state lives exactly once per
 * card (struct bccif_dev), the per-interface state lives once per interface
 * (struct bccif_iface), and an open file holds nothing but a pointer pair
 * (struct bccif_file).
 */

#ifndef BCCIF_DEV_H
#define BCCIF_DEV_H

/*
 * Error flags, set by the ISR and consumed by the file operations.
 * Values kept identical to the 2.6 driver's bcc_struct.h.
 */
#define TIMEOUT			0x10	/* Interface bus timeout occurred */
#define OVERFLOW		0x20	/* Address overflow occurred */
#define WB_ERROR		0x40	/* Wishbone bus error occurred */
#define BCCIF_ERROR_FLAGS	(TIMEOUT | OVERFLOW | WB_ERROR)

/* One per external interface (minor 1..BCCIF_BLOCKS). */
struct bccif_iface {
	void __iomem		*base;		/* dev->bar + idx * BCC_SPACE */
	unsigned int		minor;		/* 1-based, as exposed in /dev */
	struct bccif_dev	*dev;		/* owning card */

	spinlock_t		flaglock;	/* protects flags; taken by ISR */
	unsigned long		flags;		/* BCCIF_ERROR_FLAGS */

	struct mutex		count_lock;	/* protects owner + open_count */
	kuid_t			owner;		/* uid that currently holds it */
	int			open_count;
};

/* One per card. */
struct bccif_dev {
	struct pci_dev		*pdev;
	struct device		*ddev[BCCIF_BLOCKS];	/* /dev/bccif1..4 */

	/*
	 * An open file descriptor can outlive remove() (sysfs "remove", or a
	 * surprise hot-unplug), so the allocation is refcounted and "gone"
	 * fences off the hardware once the mappings have been torn down.  The
	 * 2.6 driver simply freed everything and left dangling pointers.
	 */
	struct kref		ref;
	bool			gone;

	void __iomem		*cfg;		/* BAR0: PCI core config space */
	void __iomem		*bar;		/* BAR1: interface registers */
	int			irq;
	bool			irq_ok;

	/*
	 * Serialises every PCI transaction on the card, and is held across the
	 * post-transaction error-flag sample so an error can never be
	 * attributed to the wrong requester.  Replaces the 2.6 "struct
	 * semaphore *sem" used as a mutex.
	 */
	struct mutex		hw_lock;

	dev_t			devt;
	struct cdev		cdev;
	bool			cdev_added;

	char			*ident;		/* serial number string */

	struct bccif_iface	iface[BCCIF_BLOCKS];
};

/* Attached to filp->private_data by open(). */
struct bccif_file {
	struct bccif_dev	*dev;
	struct bccif_iface	*ifc;
};

#endif /* BCCIF_DEV_H */
