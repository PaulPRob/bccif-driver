// SPDX-License-Identifier: GPL-2.0
/*
 * interrupts.c - shared interrupt handling for the BCC card.
 *
 * Changes from the 2.6 driver:
 *
 *  - The handler prototype lost its "struct pt_regs *" argument in 2.6.19 and
 *    must return irqreturn_t.  The old code hid the mismatch by casting the
 *    function pointer in request_irq(), which no longer builds.
 *
 *  - The old handler never returned IRQ_NONE (its "not our interrupt" test was
 *    commented out).  On a shared legacy PCI line the kernel counts unhandled
 *    returns and eventually kills the line with "irq N: nobody cared", so a
 *    real IRQ_NONE path matters.
 *
 *  - One handler is now installed per card at probe() time instead of one per
 *    open().  The old scheme registered N handlers for N opens and, because the
 *    PCI core kept interrupts enabled after the last close, left the card able
 *    to raise an interrupt with no handler attached at all.
 */

#include "bccif.h"
#include "regdefs.h"

/* Record an error flag for one interface. */
static void bccif_set_flag(struct bccif_iface *ifc, unsigned long flag)
{
	spin_lock(&ifc->flaglock);
	ifc->flags |= flag;
	spin_unlock(&ifc->flaglock);
}

/*
 * Read one interface's status register and latch whatever it reports.
 * Reading BCC_STATUS clears the interrupt bits in the hardware, exactly as in
 * the 2.6 driver.  Returns true if this interface had an interrupt pending.
 */
static bool bccif_service_iface(struct bccif_iface *ifc)
{
	u16 intr;
	bool served = false;

	intr = bccif_readw(ifc->base, BCC_STATUS);

	if (intr & (BCC_OVERFLOW_INTERRUPT_MASK | BCC_BUS_TIMEOUT_INTERRUPT_MASK))
		bccif_dbg(DEBUG_CRIT, "General interrupt occured %04x on MINOR:%u\n",
			  intr, ifc->minor);

	/* Address overflow */
	if ((intr & BCC_OVERFLOW_INTERRUPT_MASK) != 0) {
		served = true;
		if ((intr & BCC_ADDRESS_OVERFLOW_INTERRUPT_ENABLE_MASK) != 0) {
			bccif_dbg(DEBUG_INFORMATION, "Overflow interrupt occured\n");
			bccif_set_flag(ifc, OVERFLOW);
		}
	}

	/* Bus timeout */
	if ((intr & BCC_BUS_TIMEOUT_INTERRUPT_MASK) != 0) {
		served = true;
		if ((intr & BCC_BUS_TIMEOUT_INTERRUPT_ENABLE_MASK) != 0) {
			bccif_dbg(DEBUG_INFORMATION, "Timeout interrupt occured\n");
			bccif_set_flag(ifc, TIMEOUT);
		}
	}

	return served;
}

irqreturn_t bccif_isr(int irq, void *data)
{
	struct bccif_dev *dev = data;
	bool served = false;
	u32 status, mask;
	int i;

	/* Did the PCI core raise this?  ICR masks out what we did not enable. */
	status = bccif_readl(dev->cfg, ISR);
	mask = bccif_readl(dev->cfg, ICR);
	status &= mask;

	if ((status & WB_EINT) != 0) {
		served = true;

		/*
		 * A wishbone error was signalled.  ERR_SIG lives in W_ERR_CS,
		 * not in ISR - the 2.6 code tested "data & ERR_SIG" against the
		 * masked ISR value, a bit that register does not have, so it
		 * never cleared the latch and never reported the error.  See
		 * README.md ("Behavioural fixes") if you need the old silence.
		 */
		if ((bccif_readl(dev->cfg, W_ERR_CS) & ERR_SIG) != 0) {
			bccif_writel(dev->cfg, W_ERR_CS, ERR_SIG);

			/*
			 * The 2.6 driver could only mark the one interface the
			 * firing file descriptor belonged to.  A wishbone error
			 * is a card-wide condition, so flag every interface and
			 * let each requester notice it.
			 */
			for (i = 0; i < BCCIF_BLOCKS; i++)
				bccif_set_flag(&dev->iface[i], WB_ERROR);

			bccif_dbg(DEBUG_CRIT, "Wishbone error interrupt occured\n");
		}
	}

	/*
	 * Scan the interfaces.  This is done even when the PCI core reported
	 * nothing, so that an interface-generated interrupt that does not make
	 * it into ISR still gets claimed rather than left to scream on a shared
	 * line - but if nothing at all is pending we return IRQ_NONE and let
	 * the kernel offer the interrupt to the next handler.
	 */
	for (i = 0; i < BCCIF_BLOCKS; i++) {
		if (bccif_service_iface(&dev->iface[i]))
			served = true;
	}

	return served ? IRQ_HANDLED : IRQ_NONE;
}

/* Enable wishbone + error interrupt propagation in the PCI core. */
void bccif_irq_enable(struct bccif_dev *dev)
{
	int i;

	mutex_lock(&dev->hw_lock);

	/* Zero the status register of every interface */
	for (i = 0; i < BCCIF_BLOCKS; i++)
		bccif_writew(dev->iface[i].base, BCC_STATUS, 0);

	bccif_writel(dev->cfg, ICR, INT_PROP_EN | WB_EINT_EN);
	bccif_writel(dev->cfg, W_ERR_CS, ERR_EN);

	mutex_unlock(&dev->hw_lock);

	bccif_dbg(DEBUG_CRIT, "Interrupts enabled, IRQ line = %d\n", dev->irq);
}

/*
 * Silence the card.  Called before free_irq() in remove(), which the 2.6
 * driver never did - it left INT_PROP_EN set after the last close.
 */
void bccif_irq_disable(struct bccif_dev *dev)
{
	int i;

	mutex_lock(&dev->hw_lock);

	bccif_writel(dev->cfg, ICR, 0);
	bccif_writel(dev->cfg, W_ERR_CS, 0);

	for (i = 0; i < BCCIF_BLOCKS; i++)
		bccif_writew(dev->iface[i].base, BCC_STATUS, 0);

	mutex_unlock(&dev->hw_lock);

	bccif_dbg(DEBUG_CRIT, "Interrupts disabled\n");
}
