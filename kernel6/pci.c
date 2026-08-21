// SPDX-License-Identifier: GPL-2.0
/*
 * pci.c - memory mapped register accessors.
 *
 * The 2.6 version stored the ioremap() cookie in an "unsigned long int" and
 * called readl((int *) addr).  On 6.8 readl()/writel() take
 * "const volatile void __iomem *": the casts are a build error and they throw
 * away every __iomem guarantee sparse could otherwise check.  The accessors
 * now take the mapping plus a register offset, which also keeps the debug
 * output useful (an offset, not a hashed kernel pointer).
 */

#include "bccif.h"

u32 bccif_readl(void __iomem *base, unsigned int off)
{
	u32 p = readl(base + off);

	bccif_dbg(DEBUG_PCI, "Read 32 bits %x from +%03x\n", p, off);

	return p;
}

void bccif_writel(void __iomem *base, unsigned int off, u32 data)
{
	writel(data, base + off);

	bccif_dbg(DEBUG_PCI, "Wrote 32 bits %x to +%03x\n", data, off);
}

u16 bccif_readw(void __iomem *base, unsigned int off)
{
	u16 p = readw(base + off);

	bccif_dbg(DEBUG_PCI, "Read 16 bits %x from +%03x\n", p, off);

	return p;
}

void bccif_writew(void __iomem *base, unsigned int off, u16 data)
{
	writew(data, base + off);

	bccif_dbg(DEBUG_PCI, "Wrote 16 bits %x to +%03x\n", data, off);
}

u8 bccif_readb(void __iomem *base, unsigned int off)
{
	u8 p = readb(base + off);

	bccif_dbg(DEBUG_PCI, "Read 8 bits %x from +%03x\n", p, off);

	return p;
}

void bccif_writeb(void __iomem *base, unsigned int off, u8 data)
{
	writeb(data, base + off);

	bccif_dbg(DEBUG_PCI, "Wrote 8 bits %x to +%03x\n", data, off);
}
