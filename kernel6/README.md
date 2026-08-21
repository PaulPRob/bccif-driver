# BCCIF driver for Linux 6.8+ (Ubuntu 24.04)

Port of the CSIRO ATNF Block Control Computer (BCC) PCI character driver
(`../`, written for kernel 2.2–2.6, last built April 2010) to Linux 6.8 and
newer on x86-64 multicore hardware.

The original tree is untouched and remains the reference. Everything here is
self-contained: no `#include "../..."`, no `LINUX_VERSION_CODE` conditionals.

**The userspace ABI is unchanged.** Same `/dev/bccif1..4`, same minors, same
ioctl numbers and argument layouts, same word-oriented `read()`/`write()`
semantics and return values. Existing BCC control software does not need to be
rebuilt — and with the width fix in §4.5 it no longer has its stack corrupted by
the read-back commands, which is why that one deliberate departure from 2.6
behaviour is here.

```
Makefile        kbuild + install/load/test targets
dkms.conf       rebuild automatically across kernel updates
bccif.h         constants and prototypes            (was includes.h)
bccif_dev.h     driver state                        (was bcc_struct.h)
bcc_ioctrl.h    ioctl command set                   (copied verbatim - frozen ABI)
regdefs.h       hardware register map               (copied verbatim)
main.c          pci_driver, chrdev/class, module params
pci.c           MMIO accessors
interrupts.c    shared IRQ handler
file_ops.c      read/write/ioctl/open/release
bccif_serial.c  serial-number PROM reader
99-bccif.rules  udev rule for /dev node ownership
test/           bcc_test (menu driven) and test_app (soak test)
```

---

## 1. Build and install

```bash
sudo apt install build-essential linux-headers-$(uname -r)
make                       # builds bccif.ko against the running kernel
make test                  # builds the two userspace test programs
sudo make install          # /lib/modules/<rel>/extra + udev rule + depmod
sudo modprobe bccif        # or: sudo insmod ./bccif.ko debug=3
```

Build against a kernel other than the running one:

```bash
make KVER=6.8.0-136-generic
make KERNELDIR=/path/to/linux-headers-6.8.0-136-generic
```

### DKMS (recommended on a machine that takes kernel updates)

```bash
sudo cp -r . /usr/src/bccif-2.0
sudo dkms add -m bccif -v 2.0
sudo dkms build -m bccif -v 2.0
sudo dkms install -m bccif -v 2.0
```

### Secure Boot

Ubuntu 24.04 enables Secure Boot by default, and an unsigned out-of-tree module
is rejected with `insmod: ERROR: could not insert module: Key was rejected by
service` (`-EKEYREJECTED`). Either:

* let DKMS sign it — `sudo mokutil --import /var/lib/shim-signed/mok/MOK.der`,
  set a one-time password, reboot and enrol the key in the MOK manager; or
* sign manually with `/usr/src/linux-headers-$(uname -r)/scripts/sign-file`; or
* `sudo mokutil --disable-validation` (turns Secure Boot validation off — a
  policy decision, not a technical one).

### Loading

Module parameters:

| Parameter | Default | Meaning |
|---|---|---|
| `card=N` | 1 | Attach to the Nth BCC card found on the bus (2.6 semantics). |
| `major=N` | 0 | 0 allocates a major dynamically; non-zero requests that major. |
| `debug=N` | 0 | 0 silent … 6 everything. 5 and 6 log every PCI access and will flood the journal. |
| `err_wait_us=N` | 1000 | Microseconds to wait for a late error interrupt after each register access (see §3.7). |

`/dev/bccif1..4` are created automatically by the driver — no `mknod`. The
shipped udev rule sets `MODE=0660 GROUP=dialout`; edit it for whatever account
the control software runs as.

---

## 2. Why the 2.6 driver does not work on 6.8

Grouped by cause. Line references are to the original sources in `../`.

### 2.1 Headers and module boilerplate — the build stops immediately

| 2.6 code | Status on 6.8 |
|---|---|
| `#include <linux/config.h>` (`includes.h:18`) | Removed in 2.6.19 |
| `#include <linux/autoconf.h>` (`includes.h:28`) | Moved to `generated/autoconf.h` in 2.6.33, and never included by hand |
| `#include <linux/modversions.h>` | Removed; modversions are a kbuild concern |
| `#include <asm/system.h>` (`includes.h:57`) | Removed in 3.4 (split into `barrier.h`, `switch_to.h`, …) |
| `#include <asm/semaphore.h>` | Moved to `<linux/semaphore.h>` in 2.6.26 |
| `#include <linux/netdevice.h>` for `SET_MODULE_OWNER` | `SET_MODULE_OWNER` deleted in 2.6.18 |
| `MODULE_SUPPORTED_DEVICE(...)` (`main.c:69`) | Removed in 5.12 — hard build error |
| `EXPORT_NO_SYMBOLS`, `MODULE_PARM()`, `MOD_INC_USE_COUNT` | Long gone |
| `sysdep.h` (828-line LDD 2.x shim) | Obsolete; deleted |
| no `#include <linux/uaccess.h>` | `copy_*_user` no longer arrives implicitly, and `<asm/uaccess.h>` is not the spelling to use |

The old `Makefile` also fed `-I$(KERNELDIR)` and `-O3` into the kbuild
invocation and hand-copied `bccif-2_6.ko` to `bccif.ko`; modern kbuild owns the
compiler flags and produces the final name itself.

### 2.2 `struct file_operations` changed shape

1. **GNU colon designators.** `main.c:32-38` writes `read: read, write: write,
   ioctl: ioctl,`. GCC removed that extension in GCC 8 — must be `.read = …`.
2. **`.ioctl` no longer exists.** The BKL-holding member was deleted in 2.6.36.
   The replacement is `.unlocked_ioctl`, which returns `long` and receives no
   `struct inode *`. The old handler used `inode` for the JTAG minor check
   (`file_ops.c:2030`), which now comes from the per-open state.
3. **No `.owner`.** With `SET_MODULE_OWNER` gone, nothing pinned the module
   while a descriptor was open — `rmmod` during use would have oopsed.
4. **No `.compat_ioctl`**, so a 32-bit binary on a 64-bit kernel got `-ENOTTY`.
   Now `compat_ptr_ioctl` (every command's argument is a pointer).
5. **`.llseek`** was unset, meaning `default_llseek`. Now `noop_llseek` —
   `no_llseek` was itself removed in 6.12, so this stays valid *above* 6.8 too.
6. **Symbol names.** The handlers were non-static globals literally named
   `read`, `write`, `open`, `release` and `ioctl`. All static and `bccif_`
   prefixed now.
7. **No `__user` annotations** anywhere, so nothing was ever checked.

### 2.3 User-copy and credentials APIs

1. `access_ok(VERIFY_WRITE, ptr, size)` (`file_ops.c:246,248`) — the type
   argument and the `VERIFY_*` constants were removed in 5.0. The whole
   preamble is gone: `copy_*_user`, `get_user` and `put_user` validate on their
   own.
2. `__put_user` / `__get_user` are the *unchecked* variants, legal only
   directly after a matching `access_ok`. With that gone, all ~40 sites became
   checked `put_user` / `get_user`.
3. `current->uid` / `current->euid` (`file_ops.c:2258`) were removed in 2.6.29
   by the credentials rework. The "2.6.31+" branch at `file_ops.c:2263`
   (`current->cred->uid`) is equally dead: `cred` is `__rcu`-annotated and a uid
   is now an opaque, namespace-aware `kuid_t`, not an `int`. The driver now
   uses `current_uid()` / `current_euid()` with `uid_eq()`, and the ownership
   fields are `kuid_t`.

### 2.4 Interrupt handling

1. `void BCC_Interrupt(int, void *, struct pt_regs *)` (`interrupts.c:18`) — the
   `pt_regs` argument was removed in 2.6.19 and handlers must return
   `irqreturn_t`. `interrupts.c:84` hid the mismatch with
   `request_irq(irq, (void *) BCC_Interrupt, …)`; that no longer compiles, and
   would be undefined behaviour if it did.
2. **The handler never returned `IRQ_NONE`** — its "not our interrupt" test is
   commented out at `interrupts.c:33-34`. On a shared legacy PCI line the
   kernel counts unhandled returns and eventually kills the line with
   `irq N: nobody cared`.
3. **One handler was requested per `open()`** and freed at `release()`
   (`file_ops.c:2319`), so N descriptors meant N handlers on one line — and
   because the PCI core kept interrupts enabled after the last close, the card
   could raise an interrupt with no handler attached at all. There is now a
   single handler per card, installed in `probe()`.
4. `bccif->irq = dev->irq` was read before any `pci_enable_device()`, so it was
   not yet the routed Linux IRQ number.

### 2.5 PCI discovery, enabling and MMIO

1. `pci_find_device()` (`main.c:218`) was removed in 2.6.20 — it returned an
   unpinned `pci_dev`. Replaced by a proper `struct pci_driver` with an ID
   table and `MODULE_DEVICE_TABLE`, so the module can also be autoloaded.
2. **No `pci_enable_device()` and no `pci_request_regions()`.** The driver
   `ioremap`'d BARs it had never claimed and whose memory decoding may not even
   have been enabled — all-`0xffff` reads at best, and no protection against
   another driver claiming the same window.
3. **`ioremap()` cast to `unsigned long int`** (`main.c:265,273`,
   `bcc_struct.h:10-11`, all of `pci.c`) and then `readl((int *) addr)`.
   `readl`/`writel` take `const volatile void __iomem *`; the casts are a build
   error under the modern prototypes and throw away every `__iomem` guarantee.
4. `pci_resource_start()` into an `unsigned long` printed with `%lx` — it is a
   `resource_size_t` and prints with `%pa`.
5. `iounmap()` in `cleanup_module` ran unconditionally, even if probing failed.

### 2.6 Character device registration and `/dev`

`register_chrdev(major, "bccif", &fops)` (`main.c:325`) still exists but
reserves all 256 minors, and Ubuntu 24.04 has no static `/dev` — nothing
appeared without a manual `mknod`. Replaced with `alloc_chrdev_region` +
`cdev_add` + `class_create`/`device_create`. Note that **`class_create()` lost
its `owner` argument in 6.4**, so any example older than that will not build on
6.8 either.

### 2.7 Multicore and preemption correctness

These are the parts that "worked" on a 2007 single-socket box and do not on a
modern preemptible multicore kernel.

1. **A whole timer tick of busy-waiting under a lock.** `checkwbflag()` and
   `checkwbtimeoutflags()` (`file_ops.c:2379-2381`, `2400-2402`) spin on
   `while (jiffies < j);` after *every* register access, while holding the
   device semaphore, so that a late error interrupt has time to land. On
   Ubuntu's `CONFIG_HZ=250` kernel that is up to 4 ms of a core burned per
   access, with every other interface blocked behind it, and the bare `<`
   comparison is jiffies-wraparound unsafe (`time_before()` exists for this).
   It is now a sleeping `usleep_range()` of `err_wait_us` (default 1000 µs),
   still inside the lock so that an error cannot be attributed to the wrong
   requester.
2. **`struct semaphore` used as a mutex.** Discouraged since 2.6.16: no owner
   tracking, so lockdep, RT priority inheritance and deadlock detection are all
   blind to it. Now `struct mutex`.
3. **Per-open `memcpy` of the entire device structure** (`file_ops.c:2251`)
   copied the spinlock and the error-flag byte, then re-initialised the lock —
   so every descriptor had a *private* copy of the state the ISR publishes.
   State now lives once per card and once per interface; an open file holds
   only a pointer pair.
4. **Unlocked counter updates.** `release()` decremented `open_count`
   (`file_ops.c:2343`) with no lock at all, racing the locked increment in
   `open()`.
5. **The interface owner was never released**, so the first uid to open an
   interface owned it until the module was unloaded.
6. **`flags` was a plain `char`** shared between hardirq and process context.

### 2.8 Latent bugs fixed in passing

* `Initialise_BCC_Serial()` (`bccif_serial.c:17-27`) `memset`s the buffer before
  checking `kmalloc` for `NULL` — a NULL dereference on allocation failure.
* `read()` (`file_ops.c:83,97`) assigned `-EBADMSG` into the *transferred
  words* counter and then passed it to `copy_to_user()` as a length: a negative
  value widening to a huge `size_t`. A wishbone error therefore surfaced to
  userspace as `-EFAULT`, and on a hardened kernel would produce a usercopy
  warning. It now returns `-EBADMSG` as intended.
* `read()`/`write()` sized their `kmalloc` from a fully user-controlled `count`
  with no overflow check. Now `kvmalloc_array()`, which is overflow-checked and
  falls back to vmalloc for large transfers, with an explicit cap.
* `write()` assigned a `short int *` allocation to an `unsigned short int *`.
* `main.c` leaked every allocation from `InitSysInfo()` if probing later failed.
* An open descriptor could outlive the card (sysfs `remove`, hot-unplug) and
  keep using freed memory and unmapped iomem. The card structure is now
  refcounted and fenced with a `gone` flag checked under the same lock that
  guards the registers.

---

## 3. Deliberate decisions

1. **The ioctl ABI is frozen.** `bcc_ioctrl.h` is copied byte for byte. Several
   commands have provably wrong `_IOC` direction encodings — `BCC_WRITE_IF_REGISTER`
   and `BCC_WRITE_JTAG_DEVICE` are declared `_IOR`, for instance — but the
   driver switches on the full 32-bit command value, so renumbering them would
   break every existing binary. They are left wrong on purpose.
2. **Return codes preserved even where questionable.** A failed mode-register
   read on the MK1-only register commands still surfaces as `-EINVAL` rather
   than the underlying bus error, as in 2.6. The "no interrupt available"
   commands still return `-ENOSYS` (checkpatch objects; it is the historical
   value).
3. **`put_user`/`get_user` widths now follow the header, not 2.6.** This is the
   one place the port deliberately departs from the 2.6 byte-for-byte
   behaviour, because the 2.6 behaviour corrupted its callers — see §4.5.
4. **Wishbone error reporting now actually works** — see §4.
5. **The card counter (`card=`) is honoured in `probe()`**, so only the selected
   card is bound, exactly as before. Multiple cards in one host are supported
   by loading with different `card=` values only in the sense the 2.6 driver
   supported them (one bound card per loaded module).
6. **The JTAG download deliberately runs without the card lock**, as it did in
   2.6: an FPGA image takes minutes to shift in and the other three interfaces
   must stay usable meanwhile.
7. **Failing to load when no card is present is gone.** A `pci_driver` stays
   registered and binds if a card appears; the log says so at load time.

---

## 4. Behavioural changes to be aware of

Two changes are visible to userspace. Both are fixes, but the observatory
should know about them before this goes into production.

**4.1 Wishbone errors are now reported.** The 2.6 ISR tested
`if ((data & ERR_SIG) != 0)` where `data` was `ISR & ICR` — but `ERR_SIG`
(`0x100`) is a bit of the `W_ERR_CS` register, not of `ISR`. That test could
never be true, so the driver never cleared the error latch and never set its
`WB_ERROR` flag: wishbone errors were silently discarded. The handler now reads
`W_ERR_CS`, clears `ERR_SIG`, and flags the error, so ioctls that hit a bus
fault return `-EIO` where they previously returned success. If a broken far-end
address has been quietly tolerated in normal operation, this will start showing
up as errors.

**4.2 Register reads no longer swallow bus errors.** In the 2.6 code the
`BCC_READ_IF_REGISTER`, `BCC_READ_WORD_IF_REGISTER` and
`BCC_READ_BYTE_IF_REGISTER` cases finished with `ret = copy_to_user(...)`, which
overwrote the error just returned by the flag check with 0 on a successful copy.
A wishbone error or bus timeout during those reads was therefore reported as
success (with whatever the read returned). The error code now survives; the copy
to userspace only overrides it on an actual `-EFAULT`.

**4.3 Raw register offsets are bounded by the mapping.** The `*_IF_REGISTER`
byte/word commands compared their offset against the size of the whole BAR while
indexing from the interface base, so a `CAP_SYS_ADMIN` caller on interface 4
could push an access up to 3 KiB past the end of the 4 KiB mapping — a write to
whatever the kernel had mapped next. The check now accounts for the interface's
own base offset and the access width. Nothing that landed inside the mapping
before is rejected now.

**4.4 Error detection latency is now `err_wait_us` (1 ms), not one timer tick.**
If a late error interrupt takes longer than that to arrive, it will be picked up
by the *next* access rather than the current one (and cleared by that access's
flag reset). Raise `err_wait_us` if you see errors going unreported; it can be
changed at runtime via `/sys/module/bccif/parameters/err_wait_us`.

**4.5 ioctl parameters are now accessed at the width the header declares.**
Found on the bench: `bcc_test` aborted with `*** stack smashing detected ***`
immediately after opening `/dev/bccif1`.

All but one of these commands are declared `unsigned short int*` in
`bcc_ioctrl.h` and documented as "a pointer to a short int", and every known
caller — the BCC control software and both test programs — passes the address of
an `unsigned short`. The 2.6 driver nevertheless read and wrote them as `int`
(`__put_user(x, (int *) arg)`), so **every one of those commands wrote two bytes
past the end of the caller's object.** On the 2007 toolchain that quietly
clobbered adjacent stack padding and went unnoticed for fifteen years. Userspace
built with `-fstack-protector-strong` — the Ubuntu default since 22.04 — places
the canary exactly there, so the same overwrite is now a hard abort on the first
such ioctl.

The driver now uses `put_user_short()`/`get_user_short()` (2 bytes) for the 14
sites whose commands are declared `unsigned short int*`, and keeps the 4-byte
`put_user_int()` for `BCC_GET_SERIAL_NUMBER_LENGTH`, which really is declared
`int*`. Conforming callers are fixed without a rebuild.

*If any local software declares these arguments as `int` instead*, it will now
have its upper two bytes left untouched rather than zeroed. Every value carried
by these commands is 16-bit or smaller, so an initialised variable is unaffected;
an uninitialised one would see stale high bytes. Worth grepping for before
deployment.

---

## 5. Known limitations

* **32-bit userspace and the JTAG command.** `BCC_WRITE_JTAG_DEVICE` puts an
  `unsigned long` byte-count at the head of its buffer, which is 4 bytes for a
  32-bit caller and 8 for a 64-bit kernel. `compat_ptr_ioctl` translates the
  pointer, not the layout, so a 32-bit JTAG loader would need its own compat
  path. Every other command is fine.
* **No MSI.** The card is a legacy PCI device on a level-triggered line;
  `IRQF_SHARED` with a proper `IRQ_NONE` path is the correct handling.
* **The card is not made a bus master** (`pci_set_master()` is not called), as
  in 2.6 — the driver only does programmed IO.

---

## 6. Verification status

Done here:

* Builds against real Ubuntu 6.8 headers (`linux-headers-6.8.0-136-generic`,
  gcc-12) with `-Wall -Wextra` **and** `make W=1`: zero warnings.
* `modinfo` reports the expected parameters and the alias
  `pci:v00002321d00000001sv*sd*bc*sc*i*`, so udev can autoload it.
* `checkpatch.pl -f`: 0 errors. The remaining warnings are the preserved
  historical log strings and the `-ENOSYS` return values noted in §3.2.
* Both test programs build clean with modern GCC (`-Wall -Wextra`). `get_input()`
  in `bcc_test` was writing one byte past every caller's buffer via
  `fgets(data, ++number, stdin)`; on 24.04 `_FORTIFY_SOURCE` turns that into an
  abort, so it was fixed.

Still to do on the target machine (no BCC card is attached here):

```bash
# 1. no card required
sudo insmod ./bccif.ko debug=3
dmesg | tail -20            # expect the "registered and waiting" notice
sudo rmmod bccif            # expect no leak or lockdep splat

# 2. with the card
lspci -d 2321:0001 -vv      # memory decoding enabled, IRQ assigned, "bccif" in use
sudo modprobe bccif debug=2
ls -l /dev/bccif[1-4]
dmesg | grep BCCIF          # both BAR mappings, PROM serial number, major number
make -C test/bcc_test && ./test/bcc_test/bcc_test    # read/write, registers, serial
grep -i bcc /proc/interrupts                          # count rises, line never disabled
# with a descriptor open, rmmod must fail with -EBUSY (proves .owner)
```

A lockdep-enabled kernel is worth one run if you have one: the new mutexes are
visible to it, the old semaphores were not.
