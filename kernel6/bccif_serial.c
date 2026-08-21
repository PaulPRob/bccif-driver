// SPDX-License-Identifier: GPL-2.0
/*
 * bccif_serial.c - read the version/serial number PROM.
 *
 * The bit-banging protocol is unchanged from the 2.6 driver.  What changed:
 *  - "struct semaphore *sem" -> the card's hw_lock mutex,
 *  - the accessors take a void __iomem * mapping,
 *  - the buffer is checked for NULL *before* it is memset() (the 2.6 code
 *    memset()'d first, so an allocation failure was a NULL dereference).
 */

#include "bccif.h"
#include "regdefs.h"

/*
 * Format of the data read from the serial PROM.
 *
 *	11111111111111111111111 ... <= Preamble bits.
 *	XXXXXXX0 <= Start byte.
 *	DDDDDDDD <= First data byte.
 *	DDDDDDDD <= Second data byte.
 *	 ...     <= More data bytes.
 *	00000000 <= Stop byte.
 *
 * If there is no PROM there will be an infinite number of preamble bits (the
 * hardware simply keeps returning '1's), so the absence of a PROM cannot be
 * detected with certainty - MAX_PREAMBLE_BITS is the cut-off.
 *
 * Bytes are read one bit at a time, least significant bit first.
 */

/* Clock the PROM: read/modify/write the status register. */
static void clock_prom(struct bccif_dev *dev)
{
	u8 result = bccif_readb(dev->bar, BCC_SERIAL_PROM_CLOCK_OFFSET);

	bccif_writeb(dev->bar, BCC_SERIAL_PROM_CLOCK_OFFSET,
		     result | BCC_SERIAL_PROM_CLOCK_MASK);
}

static int read_prom_bit(struct bccif_dev *dev)
{
	u8 result = bccif_readb(dev->bar, BCC_SERIAL_PROM_DATA_OFFSET);

	return (result & BCC_SERIAL_PROM_DATA_MASK) != 0 ? 1 : 0;
}

static u8 read_prom_byte(struct bccif_dev *dev)
{
	u8 byte = 0x00;
	int i;

	for (i = 0; i < 8; i++) {
		clock_prom(dev);
		byte |= read_prom_bit(dev) << i;
	}

	if (byte >= ' ')
		bccif_dbg(DEBUG_INFORMATION, "Serial Byte %c \n", byte);

	return byte;
}

/* Allocate the buffer that holds the serial number string. */
int bccif_serial_init(struct bccif_dev *dev)
{
	dev->ident = kzalloc(MAX_BCC_IDENT_LENGTH + 2, GFP_KERNEL);
	if (!dev->ident)
		return -ENOMEM;

	bccif_dbg(DEBUG_CRIT, "Created Serial string space\n");

	return 0;
}

/*
 * Read the serial number string out of the PROM.
 * Returns the number of characters read, or a negative errno.
 */
int bccif_serial_read(struct bccif_dev *dev)
{
	u8 result;
	int ret = 0;
	int i;

	if (mutex_lock_interruptible(&dev->hw_lock))
		return -ERESTARTSYS;

	/* Reset version/serial number PROM */
	result = bccif_readb(dev->bar, BCC_SERIAL_PROM_RESET_OFFSET);
	bccif_writeb(dev->bar, BCC_SERIAL_PROM_RESET_OFFSET,
		     result | BCC_SERIAL_PROM_RESET_MASK);

	/* Discard bits until the start bit is found. */
	for (i = 0; ; i++) {
		if (i >= MAX_PREAMBLE_BITS) {
			ret = -ENXIO;
			pr_warn("BCCIF:Serial Number - no start bit detected\n");
			goto out;
		} else if (read_prom_bit(dev) == 0) {
			break;
		}
		clock_prom(dev);
	}

	bccif_dbg(DEBUG_INFORMATION, "End of preamble detected at %d bits\n", i);

	/* The next 7 bits are don't cares. */
	for (i = 0; i < 7; i++) {
		clock_prom(dev);
		read_prom_bit(dev);
	}

	/* Store bytes in the buffer until the stop byte is found. */
	for (i = 0; ; i++) {
		result = read_prom_byte(dev);
		dev->ident[i] = (char)result;
		if (i == MAX_BCC_IDENT_LENGTH) {
			dev->ident[i] = '\0';
			pr_warn("BCCIF:Serial Number - no stop byte detected after %d bytes\n",
				i);
			break;
		} else if (result == 0x00) {
			break;
		}
	}
	ret = i;

out:
	mutex_unlock(&dev->hw_lock);
	return ret;
}
