// SPDX-License-Identifier: GPL-2.0
/*
 * file_ops.c - character device operations for the BCC interface card.
 *
 * The ioctl command set, the /dev names and minors, and the word-oriented
 * read()/write() semantics are unchanged from the 2.6 driver: existing BCC
 * control software keeps working without a rebuild.
 *
 * What had to change for 6.8:
 *
 *  - .ioctl (which used to be called under the BKL) was removed in 2.6.36.
 *    The handler is now .unlocked_ioctl, which returns long and gets no
 *    "struct inode *"; .compat_ioctl = compat_ptr_ioctl lets 32-bit userspace
 *    keep working on a 64-bit kernel.
 *  - access_ok() lost its VERIFY_READ/VERIFY_WRITE argument in 5.0.  The
 *    preamble that used it is gone entirely and every __get_user()/__put_user()
 *    became the checked get_user()/put_user(), which validate on their own.
 *  - current->uid / current->cred->uid were replaced by the credentials API in
 *    2.6.29; uids are opaque kuid_t values now, compared with uid_eq().
 *  - "struct semaphore" used as a mutex became a real struct mutex, which
 *    lockdep can actually see.
 *  - The one-timer-tick busy-wait (while (jiffies < j);) that ran after every
 *    register access is now a sleeping wait of err_wait_us microseconds.  On a
 *    multicore preemptible kernel the old loop pinned a core for up to a tick
 *    (4 ms at Ubuntu's CONFIG_HZ=250) per access, and its unqualified
 *    comparison was jiffies-wraparound unsafe.
 *  - All the handlers are static and prefixed; the 2.6 driver exported
 *    functions literally called read(), write(), open() and ioctl().
 */

#include "bccif.h"
#include "regdefs.h"
#include "bcc_ioctrl.h"

/* ------------------------------------------------------------------------- */
/*                        Error flag / locking helpers                       */
/* ------------------------------------------------------------------------- */

/* Clear the error flags before a transaction that is about to use them. */
static void bccif_clear_flags(struct bccif_iface *ifc)
{
	unsigned long irqsave;

	spin_lock_irqsave(&ifc->flaglock, irqsave);
	ifc->flags &= ~BCCIF_ERROR_FLAGS;
	spin_unlock_irqrestore(&ifc->flaglock, irqsave);
}

/*
 * Give a late error interrupt time to land, then sample the flags.
 *
 * Called with hw_lock held, and deliberately so: if the lock were dropped
 * first, another requester could start a transaction and have its error
 * attributed to us.
 */
static int bccif_wait_flags(struct bccif_iface *ifc, bool include_timeout)
{
	unsigned long irqsave;
	int ret = 0;

	if (err_wait_us)
		usleep_range(err_wait_us, err_wait_us * 2);

	spin_lock_irqsave(&ifc->flaglock, irqsave);

	if ((ifc->flags & WB_ERROR) != 0)
		ret = -EIO;

	if (include_timeout && (ifc->flags & TIMEOUT) != 0)
		ret = -ETIMEDOUT;

	spin_unlock_irqrestore(&ifc->flaglock, irqsave);

	return ret;
}

#define checkwbflag(ifc)		bccif_wait_flags((ifc), false)
#define checkwbtimeoutflags(ifc)	bccif_wait_flags((ifc), true)

/*
 * Take the card lock and confirm the hardware is still there.  remove() sets
 * dev->gone under this same lock before unmapping, so anything that reaches
 * the registers below this point is guaranteed a live mapping.
 */
static int bccif_lock(struct bccif_iface *ifc)
{
	if (mutex_lock_interruptible(&ifc->dev->hw_lock))
		return -ERESTARTSYS;

	if (ifc->dev->gone) {
		mutex_unlock(&ifc->dev->hw_lock);
		return -ENODEV;
	}

	bccif_clear_flags(ifc);
	return 0;
}

static void bccif_unlock(struct bccif_iface *ifc)
{
	mutex_unlock(&ifc->dev->hw_lock);
}

/* ------------------------------------------------------------------------- */
/*                          Register access helpers                          */
/*                                                                           */
/* Each of these reproduces one of the transaction shapes the 2.6 ioctl()     */
/* open-coded ~45 times: take the lock, clear the flags, do the access, wait  */
/* for a possible error interrupt, drop the lock.                            */
/* ------------------------------------------------------------------------- */

/* Read/modify/write a 16 bit register: clear "clr" bits, then set "set". */
static int if_rmw_w(struct bccif_iface *ifc, unsigned int off, u16 set, u16 clr)
{
	u16 val;
	int ret;

	ret = bccif_lock(ifc);
	if (ret)
		return ret;

	val = bccif_readw(ifc->base, off);
	val &= (u16)~clr;
	val |= set;
	bccif_writew(ifc->base, off, val);

	ret = checkwbflag(ifc);
	bccif_unlock(ifc);

	return ret;
}

/* Read a 16 bit register and mask it. */
static int if_get_w(struct bccif_iface *ifc, unsigned int off, u16 mask, u16 *out)
{
	u16 val;
	int ret;

	ret = bccif_lock(ifc);
	if (ret)
		return ret;

	val = bccif_readw(ifc->base, off);

	ret = checkwbflag(ifc);
	bccif_unlock(ifc);

	*out = val & mask;
	return ret;
}

/* Read/modify/write an 8 bit register. */
static int if_rmw_b(struct bccif_iface *ifc, unsigned int off, u8 set, u8 clr)
{
	u8 val;
	int ret;

	ret = bccif_lock(ifc);
	if (ret)
		return ret;

	val = bccif_readb(ifc->base, off);
	val &= (u8)~clr;
	val |= set;
	bccif_writeb(ifc->base, off, val);

	ret = checkwbflag(ifc);
	bccif_unlock(ifc);

	return ret;
}

/* Read an 8 bit register and mask it. */
static int if_get_b(struct bccif_iface *ifc, unsigned int off, u8 mask, u8 *out)
{
	u8 val;
	int ret;

	ret = bccif_lock(ifc);
	if (ret)
		return ret;

	val = bccif_readb(ifc->base, off);

	ret = checkwbflag(ifc);
	bccif_unlock(ifc);

	*out = val & mask;
	return ret;
}

/* Plain 16 bit write, checked for both wishbone and timeout errors. */
static int if_write_w_tmo(struct bccif_iface *ifc, unsigned int off, u16 val)
{
	int ret = bccif_lock(ifc);

	if (ret)
		return ret;

	bccif_writew(ifc->base, off, val);

	ret = checkwbtimeoutflags(ifc);
	bccif_unlock(ifc);

	return ret;
}

/* Plain 16 bit read, checked for both wishbone and timeout errors. */
static int if_read_w_tmo(struct bccif_iface *ifc, unsigned int off, u16 *out)
{
	int ret = bccif_lock(ifc);

	if (ret)
		return ret;

	*out = bccif_readw(ifc->base, off);

	ret = checkwbtimeoutflags(ifc);
	bccif_unlock(ifc);

	return ret;
}

/* Plain 8 bit write / read, checked for both error kinds. */
static int if_write_b_tmo(struct bccif_iface *ifc, unsigned int off, u8 val)
{
	int ret = bccif_lock(ifc);

	if (ret)
		return ret;

	bccif_writeb(ifc->base, off, val);

	ret = checkwbtimeoutflags(ifc);
	bccif_unlock(ifc);

	return ret;
}

static int if_read_b_tmo(struct bccif_iface *ifc, unsigned int off, u8 *out)
{
	int ret = bccif_lock(ifc);

	if (ret)
		return ret;

	*out = bccif_readb(ifc->base, off);

	ret = checkwbtimeoutflags(ifc);
	bccif_unlock(ifc);

	return ret;
}

/*
 * "Is the bus healthy?" probe - a transaction-free lock/clear/check cycle that
 * BCC_READ_IF_REGISTER and BCC_WRITE_IF_REGISTER perform before the real
 * access.  Kept because it is observable: it reports a wishbone error left over
 * from a previous requester before this one touches the bus.
 */
static int if_bus_probe(struct bccif_iface *ifc)
{
	int ret = bccif_lock(ifc);

	if (ret)
		return ret;

	ret = checkwbflag(ifc);
	bccif_unlock(ifc);

	return ret;
}

/*
 * The MK1-only registers may only be touched when the interface is in MK1
 * mode.  Returns 0 when the access may proceed.
 *
 * Note the -EINVAL: the 2.6 driver reported a failed mode read as -EINVAL too
 * ("if (result == 0 && ret == 0) ... else ret = -EINVAL"), and that return
 * value is part of the ABI, so it is reproduced here rather than "fixed".
 */
static int if_mk1_gate(struct bccif_iface *ifc)
{
	u16 mode;
	int ret;

	ret = if_get_w(ifc, BCC_MK2_MODE_SELECT_OFFSET,
		       BCC_MK2_MODE_SELECT_MASK, &mode);

	if (ret == 0 && mode == 0)
		return 0;

	return -EINVAL;
}

/*
 * Bounds check for the "raw offset" register commands.
 *
 * The 2.6 driver tested "address > BCCIF_REGION_SIZE" against the size of the
 * whole BAR while indexing from the *interface* base, so a CAP_SYS_ADMIN caller
 * could drive an access up to three interfaces past the end of the 4 KiB
 * mapping.  Everything the old check allowed inside the mapping still works;
 * only accesses that would have run off the end are rejected.
 */
static bool if_addr_ok(struct bccif_iface *ifc, unsigned int addr,
		       unsigned int width)
{
	unsigned int base = (ifc->minor - 1) * BCC_SPACE;

	return (base + addr + width) <= BCCIF_REGION_SIZE;
}

/*
 * Parameter width follows bcc_ioctrl.h, which is the actual contract.
 *
 * The 2.6 driver read and wrote every ioctl parameter as int - four bytes -
 * via __put_user(x, (int *) arg) / __get_user(x, (int *) arg), regardless of
 * what the header declared.  All but one of these commands are declared
 * "unsigned short int*" and documented as "a pointer to a short int", and the
 * BCC control software (and both test programs) duly pass the address of an
 * unsigned short.  The 4-byte access therefore ran two bytes past the caller's
 * object on every one of those commands.  That corrupted whatever the compiler
 * had placed next on the caller's stack - harmless-looking padding for fifteen
 * years, and an immediate "*** stack smashing detected ***" abort once
 * userspace is built with -fstack-protector-strong (the Ubuntu default since
 * 22.04), because the overwrite lands on the canary.
 *
 * The _short helpers below match the declared width, so a conforming caller is
 * no longer corrupted and needs no rebuild.  put_user_int() stays for the one
 * command genuinely declared int*: BCC_GET_SERIAL_NUMBER_LENGTH.
 */
static int put_user_short(u16 val, unsigned long arg)
{
	if (put_user(val, (u16 __user *)arg))
		return -EFAULT;

	return 0;
}

static int get_user_short(int *val, unsigned long arg)
{
	u16 tmp;

	if (get_user(tmp, (u16 __user *)arg))
		return -EFAULT;

	*val = tmp;

	return 0;
}

static int put_user_int(int val, unsigned long arg)
{
	if (put_user(val, (int __user *)arg))
		return -EFAULT;

	return 0;
}

/* Log helper reproducing the 2.6 "did it work" debug lines. */
static void log_result(int ret, const char *ok, const char *fail)
{
	if (debug < DEBUG_IF)
		return;

	if (ret == 0)
		pr_info("BCCIF:%s\n", ok);
	else
		pr_info("BCCIF:%s - error %d\n", fail, ret);
}

/* ------------------------------------------------------------------------- */
/*                              File read/write                              */
/* ------------------------------------------------------------------------- */

/*
 * Both directions take an array of 16 bit words:
 *	arg[0] - address of the far end
 *	arg[1] - data[0]
 *	arg[2] - data[1]
 *	...
 * "count" is the number of data words, so the buffer holds count + 1 words and
 * the return value on success is count + 1.
 *
 * Errors detected through the ISR: -EBADMSG for a wishbone error, 0 for a
 * timeout or address overflow (i.e. "nothing was transferred").
 */

/* Largest transfer we will allocate for, in words. */
#define BCCIF_MAX_WORDS		(INT_MAX / (int)sizeof(u16) - 1)

/* Sample the error flags mid-burst.  Returns 0 to continue, else the result. */
static int burst_error(struct bccif_iface *ifc, bool *stop)
{
	unsigned long irqsave;
	int ret = 0;

	spin_lock_irqsave(&ifc->flaglock, irqsave);

	if ((ifc->flags & BCCIF_ERROR_FLAGS) != 0) {
		ret = (ifc->flags & WB_ERROR) ? -EBADMSG : 0;
		*stop = true;
	}

	spin_unlock_irqrestore(&ifc->flaglock, irqsave);

	return ret;
}

static ssize_t bccif_read(struct file *filp, char __user *arg, size_t count,
			  loff_t *off)
{
	struct bccif_file *f = filp->private_data;
	struct bccif_iface *ifc = f->ifc;
	bool stop = false;
	ssize_t ret;
	u16 *data;
	size_t i;
	int err;

	if (count > BCCIF_MAX_WORDS)
		return -EINVAL;

	data = kvmalloc_array(count + 1, sizeof(u16), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* Only the address word is supplied by the caller. */
	if (copy_from_user(data, arg, sizeof(u16))) {
		ret = -EFAULT;
		goto exit;
	}

	ret = bccif_lock(ifc);
	if (ret)
		goto exit;

	err = 0;
	for (i = 0; i < count + 1; i++) {
		if (i == 0)
			bccif_writew(ifc->base, BCC_ADDRESS_OFFSET, data[0]);
		else
			data[i] = bccif_readw(ifc->base, BCC_DATA);

		/*
		 * At full speed the interrupt lags the failing access by about
		 * five words, so this is a best-effort mid-burst check.
		 */
		err = burst_error(ifc, &stop);
		if (stop) {
			bccif_dbg(DEBUG_INFORMATION, "Dropped out of read loop early\n");
			break;
		}
	}

	bccif_unlock(ifc);

	if (stop) {
		/*
		 * The 2.6 code assigned the error code into the "transferred"
		 * counter and then handed it to copy_to_user() as a length,
		 * so a wishbone error surfaced as -EFAULT (and, on a hardened
		 * kernel, a usercopy warning).  Report the intended code.
		 */
		ret = err;
		goto exit;
	}

	if (copy_to_user(arg, data, (count + 1) * sizeof(u16))) {
		ret = -EFAULT;
		goto exit;
	}

	ret = count + 1;

	bccif_dbg(DEBUG_IFRW, "Read completed on MAJOR:%d MINOR:%u, read %zd WORDS\n",
		  MAJOR(ifc->dev->devt), ifc->minor, ret);

exit:
	kvfree(data);
	return ret;
}

static ssize_t bccif_write(struct file *filp, const char __user *arg,
			   size_t count, loff_t *off)
{
	struct bccif_file *f = filp->private_data;
	struct bccif_iface *ifc = f->ifc;
	bool stop = false;
	ssize_t ret;
	u16 *data;
	size_t i;
	int err;

	if (count > BCCIF_MAX_WORDS)
		return -EINVAL;

	data = kvmalloc_array(count + 1, sizeof(u16), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (copy_from_user(data, arg, (count + 1) * sizeof(u16))) {
		ret = -EFAULT;
		goto exit;
	}

	ret = bccif_lock(ifc);
	if (ret)
		goto exit;

	err = 0;
	for (i = 0; i < count + 1; i++) {
		if (i == 0)		/* first word is the address */
			bccif_writew(ifc->base, BCC_ADDRESS_OFFSET, data[0]);
		else			/* everything else is data */
			bccif_writew(ifc->base, BCC_DATA, data[i]);

		err = burst_error(ifc, &stop);
		if (stop) {
			bccif_dbg(DEBUG_INFORMATION, "Dropped out of write loop early\n");
			break;
		}
	}

	bccif_unlock(ifc);

	ret = stop ? err : (ssize_t)(count + 1);

	bccif_dbg(DEBUG_IFRW, "Write completed on MAJOR:%d MINOR:%u, wrote %zd WORDS\n",
		  MAJOR(ifc->dev->devt), ifc->minor, ret);

exit:
	kvfree(data);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                              JTAG bulk write                              */
/* ------------------------------------------------------------------------- */

/*
 * BCC_WRITE_JTAG_DEVICE, only valid on the special 4th interface.
 *
 * arg points at an unsigned long byte count (with the player speed encoded in
 * its top bits) followed by that many data bytes.  On a short write the number
 * of bytes actually consumed is written back over the count.
 *
 * Like the 2.6 version this deliberately runs without hw_lock: an FPGA image
 * takes minutes to shift in and the other three interfaces must stay usable.
 */
static long bccif_jtag_write(struct bccif_iface *ifc, unsigned long arg)
{
	unsigned long btf, br = 0, nb;
	const u8 __user *bp;
	u16 status = 0;
	int loops, speed;
	long ret;

	if (ifc->dev->gone)
		return -ENODEV;

	if (copy_from_user(&btf, (unsigned long __user *)arg, sizeof(btf)))
		return -EFAULT;

	speed = ((btf >> BCCIF_JTAG_SPEED_BIT_OFFSET) & BCCIF_JTAG_SPEED_MASK) << 13;
	btf = btf & ~((unsigned long)BCCIF_JTAG_SPEED_MASK << BCCIF_JTAG_SPEED_BIT_OFFSET);
	nb = btf;

	/* Set the speed */
	bccif_writew(ifc->base, BCCIF_JTAG_MASTER_REG, speed);

	bccif_dbg(DEBUG_IF, "Start JTAG Device Write %lu bytes\n", btf);

	/* Reset the player */
	bccif_writew(ifc->base, BCCIF_JTAG_MASTER_REG, BCCIF_JTAG_RESET | speed);

	/* Wait till "running" goes away */
	for (loops = 0; loops < BCCIF_JTAG_RESET_MAXITS; loops++) {
		int wait_jiffies = 0;

		status = bccif_readw(ifc->base, BCCIF_JTAG_STATUS_REG);
		if ((status & BCCIF_JTAG_RUNNING) == 0)
			break;
		if ((status & BCCIF_JTAG_EOF) != 0)
			break;

		if (loops > BCCIF_JTAG_THROTTLE_VALUE)
			wait_jiffies = ((loops - BCCIF_JTAG_THROTTLE_VALUE) /
					BCCIF_JTAG_THROTTLE_JIFFIES_DIV) + 1;

		if (wait_jiffies != 0) {
			if (wait_jiffies > BCCIF_JTAG_THROTTLE_JIFFIES_MAX)
				wait_jiffies = BCCIF_JTAG_THROTTLE_JIFFIES_MAX;

			bccif_dbg(DEBUG_IF, "JTAG_RST:%d Throttling:%d Status: %x BR:%lu\n",
				  loops, wait_jiffies, status, br);

			schedule_timeout_interruptible(wait_jiffies);
			if (signal_pending(current))
				break;
		}
	}

	if (signal_pending(current))
		return -ERESTARTSYS;
	if (loops >= BCCIF_JTAG_RESET_MAXITS)
		return -ETIMEDOUT;

	bp = (const u8 __user *)arg + sizeof(unsigned long);

	ret = 0;
	while (btf > 0) {
		u8 byte;

		/*
		 * This loop deliberately runs without hw_lock (an FPGA image
		 * takes minutes to shift in and the other interfaces must stay
		 * usable), so it re-checks for a card that has been removed
		 * underneath it rather than relying on the lock to fence it.
		 */
		if (ifc->dev->gone) {
			ret = -ENODEV;
			break;
		}

		if (copy_from_user(&byte, bp, sizeof(byte))) {
			ret = -EFAULT;
			break;
		}

		/* Load the data along with the 'load' signal */
		bccif_writew(ifc->base, BCCIF_JTAG_MASTER_REG,
			     (u16)(BCCIF_JTAG_LOAD | byte | speed));

		/* Wait till "running" goes away */
		for (loops = 0; loops < BCCIF_JTAG_LOAD_MAXITS; loops++) {
			int wait_jiffies = 0;

			status = bccif_readw(ifc->base, BCCIF_JTAG_STATUS_REG);
			if ((status & BCCIF_JTAG_RUNNING) == 0)
				break;
			if ((status & BCCIF_JTAG_EOF) != 0)
				break;

			if (loops > BCCIF_JTAG_THROTTLE_VALUE)
				wait_jiffies = ((loops - BCCIF_JTAG_THROTTLE_VALUE) /
						BCCIF_JTAG_THROTTLE_JIFFIES_DIV) + 1;

			if (wait_jiffies != 0) {
				if (wait_jiffies > BCCIF_JTAG_THROTTLE_JIFFIES_MAX)
					wait_jiffies = BCCIF_JTAG_THROTTLE_JIFFIES_MAX;

				bccif_dbg(DEBUG_IF, "JTAG_LD:%d Throttling:%d Status: %x BR:%lu\n",
					  loops, wait_jiffies, status, br);

				schedule_timeout_interruptible(wait_jiffies);
				if (signal_pending(current))
					break;
			}
		}

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		if (loops >= BCCIF_JTAG_LOAD_MAXITS)
			ret = -ETIMEDOUT;
		else if ((status & BCCIF_JTAG_ERR) != 0)
			ret = -EIO;
		else if ((status & BCCIF_JTAG_RDY) == 0 &&
			 (status & BCCIF_JTAG_EOF) == 0)
			ret = -EIO;

		if (ret != 0) {
			bccif_dbg(DEBUG_IF, "Loops:%d Status:%x Error while JTAG Device Write: %ld\n",
				  loops, status, ret);
			break;
		}

		++bp;
		++br;
		--btf;

		if ((status & BCCIF_JTAG_EOF) != 0) {
			bccif_dbg(DEBUG_IF, "BCCIF_JTAG_EOF gone hi\n");
			break;
		}
	}

	if (nb != br) {
		if (copy_to_user((unsigned long __user *)arg, &br, sizeof(br)))
			return -EFAULT;
	}

	bccif_dbg(DEBUG_IF, "Completed JTAG Device Write, %lu bytes read\n", br);

	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                   ioctl                                   */
/* ------------------------------------------------------------------------- */

static long bccif_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct bccif_file *f = filp->private_data;
	struct bccif_iface *ifc = f->ifc;
	struct bccif_dev *dev = f->dev;
	Bccif_data bccif_data;
	unsigned int addr;
	long ret = 0;
	u16 result;
	u8 resultB;
	int data;

	/* Only respond to commands directed at this driver. */
	if (_IOC_TYPE(cmd) != BCC_IOC_MAGIC)
		return -ENOTTY;

	bccif_dbg(DEBUG_IF, "Ioctl being called: type = %x, op = %x\n",
		  _IOC_TYPE(cmd), _IOC_NR(cmd));

	/*
	 * No access_ok() preamble any more: it lost its VERIFY_* argument in
	 * 5.0, and every copy_*_user()/get_user()/put_user() below performs the
	 * check itself.
	 */

	switch (cmd) {

	/* ----------------------------------------------------------------- */
	/* General commands: mode select, reset                              */
	/* ----------------------------------------------------------------- */

	case BCC_RESET_INTERFACES: {	/* Signal a reset to the wishbone bus */
		u32 resultD;

		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		ret = bccif_lock(ifc);
		if (ret)
			break;

		resultD = bccif_readl(dev->cfg, ICR);
		resultD |= SW_RST;
		bccif_writel(dev->cfg, ICR, resultD);

		udelay(1);		/* reset pulse width */

		resultD = bccif_readl(dev->cfg, ICR);
		resultD &= ~(u32)SW_RST;
		bccif_writel(dev->cfg, ICR, resultD);

		ret = checkwbflag(ifc);
		bccif_unlock(ifc);

		log_result(ret, "Wishbone bus reset", "Wishbone bus reset failed");
		break;
	}

	case BCC_SET_MK2_MODE:		/* Enable MK2 mode on this interface */
		ret = if_rmw_w(ifc, BCC_MK2_MODE_SELECT_OFFSET,
			       BCC_MK2_MODE_SELECT_MASK, 0);
		log_result(ret, "MK2 mode enabled", "MK2 mode set failed");
		break;

	case BCC_SET_MK1_MODE:		/* Enable MK1 mode on this interface */
		ret = if_rmw_w(ifc, BCC_MK2_MODE_SELECT_OFFSET,
			       0, BCC_MK2_MODE_SELECT_MASK);
		log_result(ret, "MK1 mode enabled", "MK1 mode set failed");
		break;

	case BCC_GET_MK_MODE:		/* 1 = MK2, 0 = MK1 (as actually driven) */
		ret = if_get_w(ifc, BCC_MK2_MODE_SELECT_OFFSET,
			       BCC_MK2_MODE_SELECT_MASK, &result);
		data = result ? 1 : 0;

		if (ret == 0)
			ret = put_user_short(data, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "MK2 determined to be:%d\n", data);
		else
			bccif_dbg(DEBUG_IF, "MK2 mode get failed - error %ld\n", ret);
		break;

	/* ----------------------------------------------------------------- */
	/* Address auto-increment                                            */
	/* ----------------------------------------------------------------- */

	case BCC_SET_INCREMENT_ENABLE:
		ret = if_rmw_w(ifc, BCC_ADDRESS_INCREMENT_ENABLE_OFFSET,
			       BCC_ADDRESS_INCREMENT_ENABLE_MASK, 0);
		log_result(ret, "Address increment enabled",
			   "Address increment set failed");
		break;

	case BCC_CLR_INCREMENT_ENABLE:
		ret = if_rmw_w(ifc, BCC_ADDRESS_INCREMENT_ENABLE_OFFSET,
			       0, BCC_ADDRESS_INCREMENT_ENABLE_MASK);
		log_result(ret, "Address increment disabled",
			   "Address increment clear failed");
		break;

	case BCC_GET_INCREMENT_ENABLE:
		ret = if_get_w(ifc, BCC_ADDRESS_INCREMENT_ENABLE_OFFSET,
			       BCC_ADDRESS_INCREMENT_ENABLE_MASK, &result);
		data = result ? 1 : 0;

		if (ret == 0)
			ret = put_user_short(data, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Address increment determined to be:%d\n", data);
		else
			bccif_dbg(DEBUG_IF, "Address increment get failed - error %ld\n", ret);
		break;

	/* ----------------------------------------------------------------- */
	/* Software timeout delay                                            */
	/* ----------------------------------------------------------------- */

	case BCC_SET_TIMEOUT_DELAY:
		ret = get_user_short(&data, arg);
		if (ret)
			break;

		ret = if_rmw_w(ifc, BCC_SOFTWARE_DELAY_OFFSET,
			       (u16)data & BCC_SOFTWARE_DELAY_MASK,
			       BCC_SOFTWARE_DELAY_MASK);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Software delay set to:%d\n", data);
		else
			bccif_dbg(DEBUG_IF, "Software delay set failed - error %ld\n", ret);
		break;

	case BCC_GET_TIMEOUT_DELAY:
		ret = if_get_w(ifc, BCC_SOFTWARE_DELAY_OFFSET,
			       BCC_SOFTWARE_DELAY_MASK, &result);

		if (ret == 0)
			ret = put_user_short(result, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Software delay determined to be:%d\n", result);
		else
			bccif_dbg(DEBUG_IF, "Software delay get failed - error %ld\n", ret);
		break;

	case BCC_CLR_SOFTWARE_TIMEOUT:
		ret = if_rmw_w(ifc, BCC_SOFTWARE_DELAY_ENABLE_OFFSET,
			       0, BCC_SOFTWARE_DELAY_ENABLE_MASK);
		log_result(ret, "Software delay disabled",
			   "Software delay clear failed");
		break;

	case BCC_SET_SOFTWARE_TIMEOUT:
		ret = if_rmw_w(ifc, BCC_SOFTWARE_DELAY_ENABLE_OFFSET,
			       BCC_SOFTWARE_DELAY_ENABLE_MASK, 0);
		log_result(ret, "Software delay enabled",
			   "Software delay set failed");
		break;

	case BCC_GET_SOFTWARE_TIMEOUT:
		ret = if_get_w(ifc, BCC_SOFTWARE_DELAY_ENABLE_OFFSET,
			       BCC_SOFTWARE_DELAY_ENABLE_MASK, &result);
		data = result ? 1 : 0;

		if (ret == 0)
			ret = put_user_short(data, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Software delay state determined to be:%d\n", data);
		else
			bccif_dbg(DEBUG_IF, "Software delay state get failed - error %ld\n", ret);
		break;

	/* ----------------------------------------------------------------- */
	/* Serial number                                                     */
	/* ----------------------------------------------------------------- */

	case BCC_GET_SERIAL_NUMBER:
		if (!dev->ident) {
			ret = -EBADMSG;
			break;
		}

		if (copy_to_user((char __user *)arg, dev->ident,
				 strlen(dev->ident)))
			ret = -EFAULT;

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Serial number requested - %s\n", dev->ident);
		else
			bccif_dbg(DEBUG_IF, "Serial number get failed - error %ld\n", ret);
		break;

	case BCC_GET_SERIAL_NUMBER_LENGTH:
		if (!dev->ident) {
			ret = -EBADMSG;
			break;
		}

		data = strlen(dev->ident) + 1;
		ret = put_user_int(data, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Serial string length requested, determined to be:%d\n",
				  data);
		else
			bccif_dbg(DEBUG_IF, "Serial string length get failed - error %ld\n", ret);
		break;

	/* ----------------------------------------------------------------- */
	/* Interrupt enables and status                                      */
	/* ----------------------------------------------------------------- */

	case BCC_ENABLE_GLOBAL_INTERRUPTS:
		if (!dev->irq_ok) {
			ret = -ENOSYS;
			break;
		}

		ret = if_rmw_b(ifc, BCC_MASTER_INTERRUPT_ENABLE_OFFSET,
			       BCC_MASTER_INTERRUPT_ENABLE_MASK, 0);
		log_result(ret, "Master interrupt enabled",
			   "Master interrupt set failed");
		break;

	case BCC_DISABLE_GLOBAL_INTERRUPTS:
		ret = if_rmw_b(ifc, BCC_MASTER_INTERRUPT_ENABLE_OFFSET,
			       0, BCC_MASTER_INTERRUPT_ENABLE_MASK);
		log_result(ret, "Master interrupt disabled",
			   "Master interrupt clear failed");
		break;

	case BCC_GET_GLOBAL_INTERRUPTS_STATE:
		ret = if_get_b(ifc, BCC_MASTER_INTERRUPT_ENABLE_OFFSET,
			       BCC_MASTER_INTERRUPT_ENABLE_MASK, &resultB);
		data = resultB ? 1 : 0;

		if (ret == 0)
			ret = put_user_short(data, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Master interrupt enabled state determined to be:%d\n",
				  data);
		else
			bccif_dbg(DEBUG_IF, "Master interrupt get failed - error %ld\n", ret);
		break;

	case BCC_ENABLE_TIMEOUT_INTERRUPT:
		if (!dev->irq_ok) {
			ret = -ENOSYS;
			break;
		}

		ret = if_rmw_b(ifc, BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_OFFSET,
			       BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_MASK, 0);
		log_result(ret, "Timeout interrupt enabled",
			   "Timeout interrupt set failed");
		break;

	case BCC_DISABLE_TIMEOUT_INTERRUPT:
		ret = if_rmw_b(ifc, BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_OFFSET,
			       0, BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_MASK);
		log_result(ret, "Timeout interrupt disabled",
			   "Timeout interrupt clear failed");
		break;

	case BCC_GET_TIMEOUT_INTERRUPT_STATE:
		ret = if_get_b(ifc, BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_OFFSET,
			       BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_MASK, &resultB);
		data = resultB ? 1 : 0;

		if (ret == 0)
			ret = put_user_short(data, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Timeout interrupt enabled state determined to be:%d\n",
				  data);
		else
			bccif_dbg(DEBUG_IF, "Timeout interrupt enabled get failed - error %ld\n",
				  ret);
		break;

	case BCC_ENABLE_OVERFLOW_INTERRUPT:
		if (!dev->irq_ok) {
			ret = -ENOSYS;
			break;
		}

		ret = if_rmw_b(ifc, BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_OFFSET,
			       BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_MASK, 0);
		log_result(ret, "Overflow interrupt enabled",
			   "Overflow interrupt set failed");
		break;

	case BCC_DISABLE_OVERFLOW_INTERRUPT:
		ret = if_rmw_b(ifc, BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_OFFSET,
			       0, BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_MASK);
		log_result(ret, "Overflow interrupt disabled",
			   "Overflow interrupt clear failed");
		break;

	case BCC_GET_OVERFLOW_INTERRUPT_STATE:
		ret = if_get_b(ifc, BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_OFFSET,
			       BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_MASK, &resultB);
		data = resultB ? 1 : 0;

		if (ret == 0)
			ret = put_user_short(data, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Overflow interrupt enabled state determined to be:%d\n",
				  data);
		else
			bccif_dbg(DEBUG_IF, "Overflow interrupt enabled get failed - error %ld\n",
				  ret);
		break;

	/* ----------------------------------------------------------------- */
	/* Latched timeout / overflow state                                  */
	/* ----------------------------------------------------------------- */

	case BCC_GET_TIMEOUT_OCCURED_STATE:
		ret = if_get_b(ifc, BCC_TIMEOUT_OCCURED_OFFSET,
			       BCC_TIMEOUT_OCCURED_MASK, &resultB);
		data = resultB ? 1 : 0;

		if (ret == 0)
			ret = put_user_short(data, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Timeout occured flag state determined to be:%d\n",
				  data);
		else
			bccif_dbg(DEBUG_IF, "Timeout occured flag get failed - error %ld\n", ret);
		break;

	case BCC_CLR_TIMEOUT_OCCURED_STATE:
		/* The bit is cleared by writing a 1 to it. */
		ret = if_rmw_b(ifc, BCC_TIMEOUT_OCCURED_OFFSET,
			       BCC_TIMEOUT_OCCURED_MASK, 0);
		log_result(ret, "Timeout occured flag reset",
			   "Timeout occured flag clear failed");
		break;

	case BCC_GET_OVERFLOW_OCCURED_STATE:
		ret = if_get_w(ifc, BCC_OVERFLOW_OFFSET, BCC_OVERFLOW_MASK,
			       &result);
		data = result ? 1 : 0;

		if (ret == 0)
			ret = put_user_short(data, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Overflow occured flag state determined to be:%d\n",
				  data);
		else
			bccif_dbg(DEBUG_IF, "Overflow occured flag get failed - error %ld\n", ret);
		break;

	case BCC_CLR_OVERFLOW_OCCURED_STATE:
		/* Clear the msb of the address register. */
		ret = if_rmw_w(ifc, BCC_OVERFLOW_OFFSET, 0, BCC_OVERFLOW_MASK);
		log_result(ret, "Overflow occured flag reset",
			   "Overflow occured flag clear failed");
		break;

	/* ----------------------------------------------------------------- */
	/* Address register                                                  */
	/* ----------------------------------------------------------------- */

	case BCC_SET_ADDRESS:
		ret = get_user_short(&data, arg);
		if (ret)
			break;

		ret = if_rmw_w(ifc, BCC_ADDRESS_OFFSET,
			       (u16)data & BCC_ADDRESS_MASK, BCC_ADDRESS_MASK);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Address register set to:%x\n", data);
		else
			bccif_dbg(DEBUG_IF, "Address register set failed - error %ld\n", ret);
		break;

	case BCC_GET_ADDRESS:
		ret = if_get_w(ifc, BCC_ADDRESS_OFFSET, BCC_ADDRESS_MASK,
			       &result);

		if (ret == 0)
			ret = put_user_short(result, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Current address determined to be:%x\n", result);
		else
			bccif_dbg(DEBUG_IF, "Current address get failed - error %ld\n", ret);
		break;

	/* ----------------------------------------------------------------- */
	/* Register access by index (address is a register number)           */
	/* ----------------------------------------------------------------- */

	case BCC_READ_IF_REGISTER:
		if (copy_from_user(&bccif_data, (Bccif_data __user *)arg,
				   sizeof(bccif_data))) {
			ret = -EFAULT;
			break;
		}

		addr = bccif_data.address * 4;
		if (addr >= BCC_SPACE) {
			ret = -EFAULT;
			break;
		}

		ret = if_bus_probe(ifc);
		if (ret)
			break;

		ret = if_read_w_tmo(ifc, addr, &result);
		bccif_data.data = result;

		if (copy_to_user((Bccif_data __user *)arg, &bccif_data,
				 sizeof(bccif_data))) {
			ret = -EFAULT;
			break;
		}

		bccif_dbg(DEBUG_IF, "Register:%x Address:%x Data::%x\n",
			  bccif_data.address, addr, bccif_data.data);
		break;

	case BCC_WRITE_IF_REGISTER:
		if (copy_from_user(&bccif_data, (Bccif_data __user *)arg,
				   sizeof(bccif_data))) {
			ret = -EFAULT;
			break;
		}

		addr = bccif_data.address * 4;
		if (addr >= BCC_SPACE) {
			ret = -EFAULT;
			break;
		}

		ret = if_bus_probe(ifc);
		if (ret)
			break;

		ret = if_write_w_tmo(ifc, addr, bccif_data.data);

		bccif_dbg(DEBUG_IF, "Write to Register:%x Address:%x Data::%x\n",
			  bccif_data.address, addr, bccif_data.data);
		break;

	/* ----------------------------------------------------------------- */
	/* MK1 direct register access (registers 0..5)                       */
	/* ----------------------------------------------------------------- */

	case BCC_READ_REGISTER_0:
	case BCC_READ_REGISTER_1:
	case BCC_READ_REGISTER_2:
	case BCC_READ_REGISTER_3:
	case BCC_READ_REGISTER_4:
	case BCC_READ_REGISTER_5: {
		unsigned int reg = BCC_REGISTER0 +
				   (cmd - BCC_READ_REGISTER_0) * 4;
		unsigned int nr = cmd - BCC_READ_REGISTER_0;

		ret = if_mk1_gate(ifc);
		if (ret) {
			bccif_dbg(DEBUG_IF, "Register %u data requested but in MK2 mode\n", nr);
			break;
		}

		ret = if_read_w_tmo(ifc, reg, &result);

		if (ret == 0)
			ret = put_user_short(result, arg);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Register %u data determined to be:%x\n",
				  nr, result);
		else
			bccif_dbg(DEBUG_IF, "Register %u data get failed - error %ld\n",
				  nr, ret);
		break;
	}

	case BCC_WRITE_REGISTER_0:
	case BCC_WRITE_REGISTER_1:
	case BCC_WRITE_REGISTER_2:
	case BCC_WRITE_REGISTER_3:
	case BCC_WRITE_REGISTER_4:
	case BCC_WRITE_REGISTER_5: {
		unsigned int reg = BCC_REGISTER0 +
				   (cmd - BCC_WRITE_REGISTER_0) * 4;
		unsigned int nr = cmd - BCC_WRITE_REGISTER_0;

		ret = if_mk1_gate(ifc);
		if (ret) {
			bccif_dbg(DEBUG_IF, "Register %u write requested but in MK2 mode\n", nr);
			break;
		}

		ret = get_user_short(&data, arg);
		if (ret)
			break;

		ret = if_write_w_tmo(ifc, reg, (u16)data);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Register %u data written:%x\n", nr, data);
		else
			bccif_dbg(DEBUG_IF, "Register %u data write failed - error %ld\n",
				  nr, ret);
		break;
	}

	/* ----------------------------------------------------------------- */
	/* Direct internal register access (address is a byte offset)        */
	/* ----------------------------------------------------------------- */

	case BCC_WRITE_WORD_IF_REGISTER:
	case BCC_WRITE_BYTE_IF_REGISTER:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		if (copy_from_user(&bccif_data, (Bccif_data __user *)arg,
				   sizeof(bccif_data))) {
			ret = -EFAULT;
			break;
		}

		/* Don't run off the end of the mapping. */
		if (!if_addr_ok(ifc, bccif_data.address,
				cmd == BCC_WRITE_BYTE_IF_REGISTER ? 1 : 2)) {
			ret = -EFAULT;
			break;
		}

		if (cmd == BCC_WRITE_BYTE_IF_REGISTER)
			ret = if_write_b_tmo(ifc, bccif_data.address,
					     (u8)bccif_data.data);
		else
			ret = if_write_w_tmo(ifc, bccif_data.address,
					     bccif_data.data);

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Data:%x sucessfully written to Register Address:%x\n",
				  bccif_data.data, bccif_data.address);
		else
			bccif_dbg(DEBUG_IF, "Register %x data write failed - error %ld\n",
				  bccif_data.address, ret);
		break;

	case BCC_READ_WORD_IF_REGISTER:
	case BCC_READ_BYTE_IF_REGISTER:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		if (copy_from_user(&bccif_data, (Bccif_data __user *)arg,
				   sizeof(bccif_data))) {
			ret = -EFAULT;
			break;
		}

		if (!if_addr_ok(ifc, bccif_data.address,
				cmd == BCC_READ_BYTE_IF_REGISTER ? 1 : 2)) {
			ret = -EFAULT;
			break;
		}

		if (cmd == BCC_READ_BYTE_IF_REGISTER) {
			ret = if_read_b_tmo(ifc, bccif_data.address, &resultB);
			bccif_data.data = resultB;
		} else {
			ret = if_read_w_tmo(ifc, bccif_data.address, &result);
			bccif_data.data = result;
		}

		if (copy_to_user((Bccif_data __user *)arg, &bccif_data,
				 sizeof(bccif_data))) {
			ret = -EFAULT;
			break;
		}

		if (ret == 0)
			bccif_dbg(DEBUG_IF, "Data:%x sucessfully read from Register Address:%x\n",
				  bccif_data.data, bccif_data.address);
		else
			bccif_dbg(DEBUG_IF, "Register %x data read failed - error %ld\n",
				  bccif_data.address, ret);
		break;

	/* ----------------------------------------------------------------- */
	/* JTAG download (special 4th interface only)                        */
	/* ----------------------------------------------------------------- */

	case BCC_WRITE_JTAG_DEVICE:
		if (ifc->minor != 4)
			return -EPERM;

		ret = bccif_jtag_write(ifc, arg);
		break;

	default:
		bccif_dbg(DEBUG_INFORMATION, "Invalid ioctl command was detected\n");
		ret = -ENOTTY;
		break;
	}

	return ret;
}

/* ------------------------------------------------------------------------- */
/*                              Open and release                             */
/* ------------------------------------------------------------------------- */

/*
 * An interface belongs to one user at a time, who may open it up to
 * MAX_OPEN_IF times.  CAP_DAC_OVERRIDE bypasses the ownership test.
 *
 * The 2.6 driver kept those counters in kmalloc()'d ints reached through the
 * per-open copy of the device structure, incremented them under a spinlock and
 * decremented them under no lock at all, and never released the owner - so the
 * first uid to open an interface owned it until the module was unloaded.
 */
static int bccif_open(struct inode *inode, struct file *filp)
{
	struct bccif_dev *dev = container_of(inode->i_cdev, struct bccif_dev, cdev);
	unsigned int minor = iminor(inode);
	struct bccif_iface *ifc;
	struct bccif_file *f;
	int ret = 0;

	/* Interfaces are numbered from 1; minor 0 is not an interface. */
	if (minor < 1 || minor > BCCIF_BLOCKS) {
		pr_info("BCCIF:Invalid minor number specified MINOR:%u Limit:%d\n",
			minor, BCCIF_BLOCKS);
		return -ENODEV;
	}

	ifc = &dev->iface[minor - 1];

	f = kzalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;

	f->dev = dev;
	f->ifc = ifc;

	if (dev->gone) {
		kfree(f);
		return -ENODEV;
	}

	bccif_dev_get(dev);

	mutex_lock(&ifc->count_lock);

	if (ifc->open_count &&
	    !uid_eq(ifc->owner, current_uid()) &&
	    !uid_eq(ifc->owner, current_euid()) &&
	    !capable(CAP_DAC_OVERRIDE)) {
		bccif_dbg(DEBUG_IF, "Another user attempted to open minor %u\n", minor);
		ret = -EBUSY;
		goto out;
	}

	if (ifc->open_count == 0) {
		ifc->owner = current_uid();
		bccif_dbg(DEBUG_IF, "Minor freshly opened\n");
	} else {
		bccif_dbg(DEBUG_IF, "Minor reopened %d times\n", ifc->open_count);
	}

	/* One user may open an interface many times, but not without limit. */
	if (ifc->open_count > MAX_OPEN_IF) {
		bccif_dbg(DEBUG_IF, "Too many files open: %d\n", ifc->open_count);
		ret = -EMFILE;
		goto out;
	}

	ifc->open_count++;

out:
	mutex_unlock(&ifc->count_lock);

	if (ret) {
		bccif_dev_put(dev);
		kfree(f);
		return ret;
	}

	filp->private_data = f;

	bccif_dbg(DEBUG_INFORMATION, "Opened Device MAJOR:%d MINOR:%u\n",
		  MAJOR(dev->devt), minor);

	return 0;
}

static int bccif_release(struct inode *inode, struct file *filp)
{
	struct bccif_file *f = filp->private_data;
	struct bccif_iface *ifc = f->ifc;

	bccif_dbg(DEBUG_CRIT, "Device released MAJOR:%d MINOR:%u\n",
		  MAJOR(f->dev->devt), ifc->minor);

	mutex_lock(&ifc->count_lock);

	if (ifc->open_count > 0)
		ifc->open_count--;

	/* Hand the interface back once the last descriptor goes away. */
	if (ifc->open_count == 0)
		ifc->owner = INVALID_UID;

	mutex_unlock(&ifc->count_lock);

	filp->private_data = NULL;
	bccif_dev_put(f->dev);
	kfree(f);

	return 0;
}

const struct file_operations bccif_fops = {
	.owner		= THIS_MODULE,
	.read		= bccif_read,
	.write		= bccif_write,
	.unlocked_ioctl	= bccif_ioctl,
	.compat_ioctl	= compat_ptr_ioctl,
	.open		= bccif_open,
	.release	= bccif_release,
	.llseek		= noop_llseek,
};
