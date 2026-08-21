# BCCIF — Linux driver for the CSIRO ATNF Block Control Computer PCI card

Character-device driver for the ATNF Block Control Computer (BCC) PCI
interface card, exposing `/dev/bccif1..4` to the BCC control software.

The repository holds two trees:

| Directory | Kernel | Status |
|---|---|---|
| `kernel6/` | Linux 6.8+ (Ubuntu 24.04, x86-64) | **Current.** Build and install from here. |
| `./` (root) | Linux 2.2 – 2.6, last built April 2010 | Historical reference. Does not compile on a modern kernel. |

Each tree carries its own copy of the two userspace test programs — `bcc_test`
(menu-driven hardware debugger) and `test_app` (IF1/IF2 read/write soak test) —
under `test/` and `kernel6/test/` respectively. Build the one matching the
driver you are running.

The userspace ABI is identical between the two: same device nodes, same minors,
same ioctl numbers and argument layouts, same word-oriented `read()`/`write()`
semantics. Existing BCC control software does not need rebuilding.

Original driver by Andrew Brown (version 1.04). The 6.8 port keeps the frozen
ioctl ABI and documents every API change it had to make.

## Quick start (Linux 6.8+)

```bash
sudo apt install build-essential linux-headers-$(uname -r)
cd kernel6
make                  # builds bccif.ko against the running kernel
sudo make install     # /lib/modules/<rel>/extra + udev rule + depmod
sudo modprobe bccif
```

`kernel6/README.md` is the real documentation: build options, DKMS setup,
Secure Boot signing, module parameters, the udev rule, the hardware test
script, and a detailed account of why the 2.6 driver cannot run on 6.8.

Building the userspace test programs:

```bash
cd kernel6 && make test     # or: make -C kernel6/test/bcc_test
```

## Layout

```
kernel6/          Linux 6.8+ driver — build here
  Makefile        kbuild + install/load targets
  dkms.conf       rebuild automatically across kernel updates
  99-bccif.rules  udev rule for /dev node ownership
  hwtest.sh       on-hardware smoke test
  README.md       full documentation
  test/           userspace test programs for this driver

*.c *.h Makefile  original 2.2–2.6 driver (reference only)
test/             userspace test programs for the 2.6 driver
  bcc_test/       menu-driven hardware debugger
  test_app/       IF1/IF2 read/write soak test
RCS/              original RCS revision history of the 2.6 tree
Manual.doc        original hardware/driver manual
readme            original notes
```

## Building the old 2.6 tree

Kept for reference only — it will not build against a modern kernel. See
`kernel6/README.md` §2 for the full list of reasons.

## Licence

The 6.8 sources carry `SPDX-License-Identifier: GPL-2.0`, as a Linux kernel
module must. The original 2.6 tree predates that convention and carries no
explicit notice.
