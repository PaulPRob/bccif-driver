// SPDX-License-Identifier: GPL-2.0
/*
 * main.c - module entry points, PCI binding and character device registration
 *          for the CSIRO ATNF Block Control Computer (BCC) PCI card.
 *
 * Original: Andrew Brown, <Andrew.J.Brown@uts.edu.au>, driver version 1.04.
 * Ported to Linux 6.8+ - see README.md for the full list of API changes.
 *
 * The 2.6 driver scanned the bus by hand from init_module() with
 * pci_find_device() (removed in 2.6.20), ioremap()'d BARs it had never
 * claimed, and registered a legacy major with register_chrdev().  This version
 * is a normal struct pci_driver: the PCI core matches the card, probe() claims
 * the regions properly and publishes /dev/bccif1..BCCIF_BLOCKS through a class,
 * and remove() unwinds it all.
 */

#include "bccif.h"
#include "regdefs.h"

/* ------------------------------------------------------------------------- */
/*                             Module parameters                             */
/* ------------------------------------------------------------------------- */

static int major = DEFAULT_MAJOR;
static int card = 1;
int debug = DEBUG_OFF;
unsigned int err_wait_us = 1000;

module_param(major, int, 0444);
MODULE_PARM_DESC(major,
		 "[major=X] If X=0 (default) a major number is allocated dynamically. If 1<=X<=255 that major is requested.");

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug,
		 "[debug=Y] 0 (default) silences all debug messages, 1..6 enable increasing levels of detail.");

module_param(card, int, 0444);
MODULE_PARM_DESC(card,
		 "[card=Z] Attach to the Zth BCC card detected on the PCI bus (default 1).");

module_param(err_wait_us, uint, 0644);
MODULE_PARM_DESC(err_wait_us,
		 "Microseconds to wait for a late error interrupt after each transaction (default 1000). The 2.6 driver busy-spun for a whole timer tick here.");

MODULE_AUTHOR("Andrew Brown, <Andrew.J.Brown@uts.edu.au>");
MODULE_DESCRIPTION("CSIRO ATNF Block Control Computer (BCC) PCI Char Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(BCCIF_VERSION);
/*
 * MODULE_SUPPORTED_DEVICE() was removed in 5.12 - it never did anything but
 * add a string to modinfo.  MODULE_DEVICE_TABLE below is the useful one: it is
 * what lets udev autoload this module when the card is present.
 */

/* ------------------------------------------------------------------------- */
/*                                  State                                    */
/* ------------------------------------------------------------------------- */

static struct class *bccif_class;

/*
 * Position of this card in bus enumeration order, counting only BCC cards.
 * This is what the "card=" parameter selects, and it is computed from the bus
 * each time rather than from a running counter so that a rebind (sysfs
 * remove + rescan) still picks the same card.
 */
static int bccif_card_index(struct pci_dev *pdev)
{
	struct pci_dev *cur = NULL;
	int n = 0;

	while ((cur = pci_get_device(VENDOR_ID, DEVICE_ID, cur)) != NULL) {
		n++;
		if (cur == pdev) {
			pci_dev_put(cur);
			return n;
		}
	}

	return 0;
}

static int bccif_count_cards(void)
{
	struct pci_dev *cur = NULL;
	int n = 0;

	while ((cur = pci_get_device(VENDOR_ID, DEVICE_ID, cur)) != NULL)
		n++;

	return n;
}

/* ------------------------------------------------------------------------- */
/*                              Initialisation                               */
/* ------------------------------------------------------------------------- */

/*
 * The card structure is refcounted so that a file descriptor left open across
 * a PCI hot-remove keeps a valid pointer.  Everything that touches the
 * hardware is fenced off separately by dev->gone.
 */
static void bccif_dev_release(struct kref *ref)
{
	struct bccif_dev *dev = container_of(ref, struct bccif_dev, ref);

	kfree(dev->ident);
	kfree(dev);
}

void bccif_dev_get(struct bccif_dev *dev)
{
	kref_get(&dev->ref);
}

void bccif_dev_put(struct bccif_dev *dev)
{
	kref_put(&dev->ref, bccif_dev_release);
}

static void bccif_init_ifaces(struct bccif_dev *dev)
{
	int i;

	for (i = 0; i < BCCIF_BLOCKS; i++) {
		struct bccif_iface *ifc = &dev->iface[i];

		ifc->dev = dev;
		ifc->minor = i + 1;
		ifc->base = dev->bar + (i * BCC_SPACE);
		ifc->owner = INVALID_UID;
		ifc->open_count = 0;
		ifc->flags = 0;
		spin_lock_init(&ifc->flaglock);
		mutex_init(&ifc->count_lock);
	}
}

static void bccif_destroy_nodes(struct bccif_dev *dev)
{
	int i;

	for (i = 0; i < BCCIF_BLOCKS; i++) {
		if (dev->ddev[i]) {
			device_destroy(bccif_class, MKDEV(MAJOR(dev->devt), i + 1));
			dev->ddev[i] = NULL;
		}
	}
}

/*
 * Register the character device and create /dev/bccif1..BCCIF_BLOCKS.
 *
 * Minor 0 is reserved (the 2.6 driver numbered the interfaces from 1 and
 * rejected minor 0, so the numbering is kept), hence BCCIF_BLOCKS + 1 minors.
 */
static int bccif_register_chrdev(struct bccif_dev *dev)
{
	int i, ret;

	if (major) {
		dev->devt = MKDEV(major, 0);
		ret = register_chrdev_region(dev->devt, BCCIF_BLOCKS + 1,
					     BCCIF_DRV_NAME);
	} else {
		ret = alloc_chrdev_region(&dev->devt, 0, BCCIF_BLOCKS + 1,
					  BCCIF_DRV_NAME);
	}
	if (ret) {
		pr_err("BCCIF: Couldn't get major %d\n", major);
		return ret;
	}

	cdev_init(&dev->cdev, &bccif_fops);
	dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&dev->cdev, dev->devt, BCCIF_BLOCKS + 1);
	if (ret) {
		pr_err("BCCIF: cdev_add failed: %d\n", ret);
		goto err_region;
	}
	dev->cdev_added = true;

	for (i = 0; i < BCCIF_BLOCKS; i++) {
		dev->ddev[i] = device_create(bccif_class, &dev->pdev->dev,
					     MKDEV(MAJOR(dev->devt), i + 1),
					     dev, "%s%d", BCCIF_DRV_NAME, i + 1);
		if (IS_ERR(dev->ddev[i])) {
			ret = PTR_ERR(dev->ddev[i]);
			dev->ddev[i] = NULL;
			pr_err("BCCIF: device_create for minor %d failed: %d\n",
			       i + 1, ret);
			goto err_nodes;
		}
	}

	pr_info("BCCIF:Major %d Assigned\n", MAJOR(dev->devt));

	return 0;

err_nodes:
	bccif_destroy_nodes(dev);
	cdev_del(&dev->cdev);
	dev->cdev_added = false;
err_region:
	unregister_chrdev_region(dev->devt, BCCIF_BLOCKS + 1);
	return ret;
}

static void bccif_unregister_chrdev(struct bccif_dev *dev)
{
	bccif_destroy_nodes(dev);

	if (dev->cdev_added) {
		cdev_del(&dev->cdev);
		dev->cdev_added = false;
	}

	unregister_chrdev_region(dev->devt, BCCIF_BLOCKS + 1);
}

/* ------------------------------------------------------------------------- */
/*                              PCI probe/remove                             */
/* ------------------------------------------------------------------------- */

static int bccif_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct bccif_dev *dev;
	int index, i, ret;

	index = bccif_card_index(pdev);

	bccif_dbg(DEBUG_INFORMATION,
		  "BCCIF Found, attempting initialisation VendorID:%x DeviceID:%x\n",
		  pdev->vendor, pdev->device);

	/*
	 * "card=" keeps the 2.6 semantics: attach to the Nth card the PCI core
	 * offers us and leave the others alone.
	 */
	if (index != card) {
		bccif_dbg(DEBUG_INFORMATION,
			  "Ignoring card %d, driver is configured for card %d\n",
			  index, card);
		return -ENODEV;
	}

	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "BCCIF: pci_enable_device failed: %d\n", ret);
		return ret;
	}

	ret = pci_request_regions(pdev, BCCIF_DRV_NAME);
	if (ret) {
		dev_err(&pdev->dev, "BCCIF: PCI regions are busy: %d\n", ret);
		goto err_disable;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		goto err_regions;
	}

	dev->pdev = pdev;
	kref_init(&dev->ref);
	mutex_init(&dev->hw_lock);

	if (debug >= DEBUG_INFORMATION) {
		pr_info("BCCIF:Possible addresses:\n");
		for (i = 0; i < 6; i++) {
			resource_size_t start = pci_resource_start(pdev, i);

			pr_info("BCCIF:     Address[%d]: %pa\n", i, &start);
		}
	}

	/* Map the PCI core config space first, then the interface space. */
	dev->cfg = pci_iomap(pdev, CONFIG_BAR, CONFIG_REGION_SIZE);
	if (!dev->cfg) {
		dev_err(&pdev->dev, "BCCIF:ioremap of config address failed\n");
		ret = -ENOMEM;
		goto err_free;
	}

	dev->bar = pci_iomap(pdev, BCCIF_BAR, BCCIF_REGION_SIZE);
	if (!dev->bar) {
		dev_err(&pdev->dev, "BCCIF:ioremap of interface address failed\n");
		ret = -ENOMEM;
		goto err_unmap_cfg;
	}

	bccif_init_ifaces(dev);

	/* Read the serial number PROM before anything else can touch the bus. */
	ret = bccif_serial_init(dev);
	if (ret) {
		pr_err("BCCIF: Couldn't start PROM interface \n");
		goto err_unmap_bar;
	}

	if (bccif_serial_read(dev) <= 0)
		strscpy(dev->ident, "NOT AVAILABLE", MAX_BCC_IDENT_LENGTH + 2);

	pr_notice("BCCIF:Serial Number: '%s' \n", dev->ident);

	ret = bccif_register_chrdev(dev);
	if (ret)
		goto err_unmap_bar;

	/*
	 * One handler for the card, installed here rather than in open().
	 * dev is the cookie, so a shared line stays unambiguous.
	 */
	dev->irq = pdev->irq;
	ret = request_irq(dev->irq, bccif_isr, IRQF_SHARED, "BCC Interface", dev);
	if (ret) {
		/*
		 * Not fatal: the card is still usable for programmed IO, only
		 * error reporting and the interrupt ioctls are unavailable.
		 */
		pr_warn("BCCIF:Can't assign IRQ = %d (%d), continuing without interrupts\n",
			dev->irq, ret);
		dev->irq_ok = false;
	} else {
		dev->irq_ok = true;
		bccif_irq_enable(dev);
		pr_info("BCCIF:IRQ line = %d\n", dev->irq);
	}

	pci_set_drvdata(pdev, dev);

	pr_notice("BCCIF:Driver version " BCCIF_VERSION ", %d interfaces on /dev/%s1..%d\n",
		  BCCIF_BLOCKS, BCCIF_DRV_NAME, BCCIF_BLOCKS);

	return 0;

err_unmap_bar:
	pci_iounmap(pdev, dev->bar);
err_unmap_cfg:
	pci_iounmap(pdev, dev->cfg);
err_free:
	bccif_dev_put(dev);		/* frees dev->ident too */
err_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

static void bccif_remove(struct pci_dev *pdev)
{
	struct bccif_dev *dev = pci_get_drvdata(pdev);

	if (!dev)
		return;

	pr_notice("BCCIF:Unregistering BCC device major no. %d\n",
		  MAJOR(dev->devt));

	/*
	 * Order matters: stop new opens, silence the card, drop the handler,
	 * then fence off the mappings before unmapping them.  Descriptors that
	 * are still open see -ENODEV from that point on and release their
	 * reference in the normal way.
	 */
	bccif_unregister_chrdev(dev);

	if (dev->irq_ok) {
		bccif_irq_disable(dev);
		free_irq(dev->irq, dev);
	}

	mutex_lock(&dev->hw_lock);
	dev->gone = true;
	mutex_unlock(&dev->hw_lock);

	pci_iounmap(pdev, dev->bar);
	pci_iounmap(pdev, dev->cfg);
	dev->bar = NULL;
	dev->cfg = NULL;

	bccif_dev_put(dev);

	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

static const struct pci_device_id bccif_ids[] = {
	{ PCI_DEVICE(VENDOR_ID, DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, bccif_ids);

static struct pci_driver bccif_pci_driver = {
	.name		= BCCIF_DRV_NAME,
	.id_table	= bccif_ids,
	.probe		= bccif_probe,
	.remove		= bccif_remove,
};

/* ------------------------------------------------------------------------- */
/*                          Module entry & exit points                       */
/* ------------------------------------------------------------------------- */

static int __init bccif_init(void)
{
	int ret;

	pr_notice("BCCIF:Debug level set to: %d\n", debug);
	pr_notice("BCCIF:Number devices supported by this driver: %d\n",
		  BCCIF_BLOCKS);

	if (card < 1)
		card = 1;

	/* class_create() lost its owner argument in 6.4. */
	bccif_class = class_create(BCCIF_DRV_NAME);
	if (IS_ERR(bccif_class))
		return PTR_ERR(bccif_class);

	ret = pci_register_driver(&bccif_pci_driver);
	if (ret) {
		class_destroy(bccif_class);
		return ret;
	}

	/*
	 * The 2.6 driver failed to load when no card was present.  A
	 * pci_driver stays registered instead and binds if a card appears
	 * later, so this is a notice rather than an error.
	 */
	if (bccif_count_cards() == 0)
		pr_notice("BCCIF:Failed to detect a PCI BCC card (%04x:%04x) - driver registered and waiting\n",
			  VENDOR_ID, DEVICE_ID);

	return 0;
}

static void __exit bccif_exit(void)
{
	pci_unregister_driver(&bccif_pci_driver);
	class_destroy(bccif_class);
}

module_init(bccif_init);
module_exit(bccif_exit);
