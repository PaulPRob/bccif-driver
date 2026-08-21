#!/bin/bash
# hwtest.sh - hardware verification for the BCCIF 6.8 port (README.md section 6).
#
#   sudo ./hwtest.sh
#
# Runs the no-card checks first, then the with-card checks. Every step prints
# PASS / FAIL / SKIP and the script exits non-zero if anything failed, so it can
# be dropped into a commissioning checklist.

set -u

VENDOR=2321
DEVICE=0001
IRQNAME="BCC Interface"      # the name request_irq() registers (main.c:317)
KVER=$(uname -r)
NCPU=$(nproc)
PASS=0; FAIL=0; SKIP=0
LOGDIR=$(mktemp -d /tmp/bccif-hwtest-XXXXXX)

ok()   { echo "  PASS  $*"; PASS=$((PASS+1)); }
bad()  { echo "  FAIL  $*"; FAIL=$((FAIL+1)); }
skip() { echo "  SKIP  $*"; SKIP=$((SKIP+1)); }
hdr()  { echo; echo "=== $* ==="; }

# Sum only the per-CPU count columns of the driver's /proc/interrupts line.
irq_count() {
    grep -F "$IRQNAME" /proc/interrupts | \
        awk -v n="$NCPU" '{s=0; for (i=2; i<=n+1; i++) s+=$i; print s}' | \
        head -1
}

[ "$(id -u)" -eq 0 ] || { echo "must run as root (insmod/rmmod/dmesg)"; exit 1; }
cd "$(dirname "$0")" || exit 1

# Keep the existing ring buffer instead of destroying boot history.
dmesg > "$LOGDIR/dmesg.before" 2>/dev/null
# Everything after this marker belongs to the test run.
since() { dmesg | diff --unchanged-group-format='' --old-group-format='' \
                       --new-group-format='%>' "$LOGDIR/dmesg.before" - 2>/dev/null; }

# --------------------------------------------------------------------------
hdr "0. Environment"
echo "  kernel        $KVER"
echo "  distro        $(. /etc/os-release; echo "$PRETTY_NAME")"
echo "  HZ            $(grep -m1 '^CONFIG_HZ=' /boot/config-"$KVER" | cut -d= -f2)"
echo "  cpus          $NCPU"
echo "  logs          $LOGDIR"

command -v gcc-12 >/dev/null || command -v gcc >/dev/null \
    && ok "compiler present" || bad "no compiler - apt install build-essential gcc-12"
command -v make >/dev/null && ok "make present" || bad "make missing"
[ -d "/lib/modules/$KVER/build" ] && ok "headers for $KVER" \
    || bad "no headers - apt install linux-headers-$KVER"

if [ -d /sys/firmware/efi ]; then
    sb=$(mokutil --sb-state 2>/dev/null | head -1)
    case "$sb" in *enabled*) skip "Secure Boot enabled - unsigned module will be rejected (README 1)";;
                  *) ok "Secure Boot not enforcing";; esac
else
    ok "legacy BIOS boot - Secure Boot not applicable"
fi

[ "$FAIL" -eq 0 ] || { echo; echo "Toolchain incomplete - stopping."; exit 1; }

# --------------------------------------------------------------------------
hdr "1. Build"
make clean >/dev/null 2>&1
if make >"$LOGDIR/build.log" 2>&1; then
    ok "bccif.ko built"
    # The "compiler differs" banner is a cosmetic kbuild string compare.
    w=$(grep -i "warning" "$LOGDIR/build.log" | grep -vc "the compiler differs")
    [ "$w" -eq 0 ] && ok "zero build warnings" || bad "$w build warnings (see $LOGDIR/build.log)"
else
    bad "build failed - see $LOGDIR/build.log"; exit 1
fi

make test >"$LOGDIR/buildtest.log" 2>&1 \
    && ok "test programs built" || bad "test programs failed - see $LOGDIR/buildtest.log"

hdr "2. modinfo"
modinfo ./bccif.ko >"$LOGDIR/modinfo.log" 2>&1
for p in card major debug err_wait_us; do
    grep -q "^parm: *$p" "$LOGDIR/modinfo.log" && ok "param $p" || bad "param $p missing"
done
grep -qi "alias: *pci:v0000${VENDOR}d0000${DEVICE}" "$LOGDIR/modinfo.log" \
    && ok "PCI alias present (udev can autoload)" || bad "PCI alias missing"
grep -q "^vermagic: *$KVER" "$LOGDIR/modinfo.log" \
    && ok "vermagic matches running kernel" || bad "vermagic mismatch"

# --------------------------------------------------------------------------
CARD=$(lspci -d "$VENDOR:$DEVICE" -n 2>/dev/null | awk '{print $1}' | head -1)

hdr "3. Load"
lsmod | grep -q "^bccif " && rmmod bccif 2>/dev/null
if insmod ./bccif.ko debug=3 2>"$LOGDIR/insmod.err"; then
    ok "insmod debug=3"
    since >"$LOGDIR/load.dmesg"
    grep -qi "bccif" "$LOGDIR/load.dmesg" && ok "driver logged at load" || bad "nothing in dmesg"
    sed -n 's/^/    | /p' "$LOGDIR/load.dmesg" | tail -25
else
    bad "insmod failed: $(cat "$LOGDIR/insmod.err")"; exit 1
fi

# --------------------------------------------------------------------------
if [ -z "$CARD" ]; then
    hdr "4. Card checks"
    plx=$(lspci -d 10b5:9030 -n 2>/dev/null | awk '{print $1}' | head -1)
    if [ -n "$plx" ]; then
        echo "  A PLX PCI9030 is present at $plx showing factory defaults"
        echo "  (10b5:9030, subsystem 0000:0000) instead of $VENDOR:$DEVICE."
        echo "  Its serial EEPROM has not loaded - cold power cycle the chassis."
    fi
    grep -qi "registered and waiting" "$LOGDIR/load.dmesg" \
        && ok "no-card path: 'registered and waiting' (README 3.7)" \
        || bad "no-card notice missing"
    skip "no $VENDOR:$DEVICE on the bus - all card tests skipped"
else
    hdr "4. Card present at $CARD"
    lspci -s "$CARD" -vv >"$LOGDIR/lspci.log" 2>&1
    grep -q "Kernel driver in use: bccif" "$LOGDIR/lspci.log" \
        && ok "bound to bccif" \
        || bad "not bound - '$(grep -i 'driver in use' "$LOGDIR/lspci.log")'"
    grep -q "Control:.*Mem+" "$LOGDIR/lspci.log" \
        && ok "memory decoding enabled by pci_enable_device (README 2.5.2)" \
        || bad "memory decoding still off"
    irq=$(grep -o "IRQ [0-9]*" "$LOGDIR/lspci.log" | head -1 | awk '{print $2}')
    { [ -n "$irq" ] && [ "$irq" != "0" ]; } && ok "IRQ $irq assigned" || bad "no IRQ assigned"
    grep -qi "ioremap.*failed" "$LOGDIR/load.dmesg" && bad "a BAR mapping failed" \
        || ok "both BARs mapped"

    hdr "5. /dev nodes"
    for n in 1 2 3 4; do
        [ -c "/dev/bccif$n" ] \
            && ok "/dev/bccif$n  mode=$(stat -c '%a' /dev/bccif"$n") grp=$(stat -c '%G' /dev/bccif"$n") $(stat -c '%t:%T' /dev/bccif"$n")" \
            || bad "/dev/bccif$n missing"
    done

    hdr "6. Probe results"
    ser=$(grep -i "Serial Number" "$LOGDIR/load.dmesg" | tail -1 | sed "s/.*Serial Number: //")
    case "$ser" in
        ""|*"NOT AVAILABLE"*) bad "PROM serial not read (got: ${ser:-nothing})";;
        *) ok "PROM serial number read: $ser";;
    esac
    grep -qi "Major .* Assigned" "$LOGDIR/load.dmesg" \
        && ok "$(grep -i 'Major .* Assigned' "$LOGDIR/load.dmesg" | tail -1 | sed 's/.*BCCIF://')" \
        || bad "no major number logged"

    hdr "7. Module pinned while open (.owner - README 2.2.3)"
    if exec 9<>/dev/bccif1; then
        rc=$(cat /sys/module/bccif/refcnt 2>/dev/null)
        [ "${rc:-0}" -gt 0 ] && ok "refcnt=$rc with a descriptor open" \
                             || bad "refcnt=${rc:-?} - module not pinned"
        if rmmod bccif 2>/dev/null; then
            bad "rmmod SUCCEEDED with an open descriptor - .owner not effective"
            exec 9>&-
            insmod ./bccif.ko debug=3
        else
            ok "rmmod refused while descriptor open"
            exec 9>&-
        fi
    else
        bad "could not open /dev/bccif1"
    fi

    hdr "8. Interrupt line health"
    c=$(irq_count)
    if [ -n "$c" ]; then
        ok "handler installed as '$IRQNAME' on IRQ $irq, count=$c"
    else
        bad "no '$IRQNAME' line in /proc/interrupts"
    fi
    if since | grep -qiE "nobody cared|disabling IRQ"; then
        bad "IRQ line was disabled - the IRQ_NONE path is wrong (README 2.4.2)"
    else
        ok "IRQ line never disabled"
    fi
    echo
    echo "  Interactive step - not scriptable:"
    echo "    ./test/bcc_test/bcc_test     read/write, registers, serial number"
    echo "  Re-check the interrupt count afterwards with:"
    echo "    grep -F '$IRQNAME' /proc/interrupts"
fi

# --------------------------------------------------------------------------
hdr "9. Unload"
if rmmod bccif 2>"$LOGDIR/rmmod.err"; then
    ok "rmmod clean"
    since >"$LOGDIR/unload.dmesg"
    if grep -qiE "lockdep|BUG:|WARNING:|Oops|general protection|use-after-free" "$LOGDIR/unload.dmesg"; then
        bad "kernel splat during the run - see $LOGDIR/unload.dmesg"
        grep -iE "lockdep|BUG:|WARNING:|Oops" "$LOGDIR/unload.dmesg" | sed 's/^/    | /' | head
    else
        ok "no splat, warning or leak across the whole run"
    fi
    [ -e /dev/bccif1 ] && bad "/dev/bccif1 survived unload" || ok "/dev nodes removed"
    lspci -s "${CARD:-02:00.0}" -vv 2>/dev/null | grep -q "Kernel driver in use" \
        && bad "still bound after rmmod" || ok "card released"
else
    bad "rmmod failed: $(cat "$LOGDIR/rmmod.err")"
fi

hdr "Summary"
echo "  pass $PASS   fail $FAIL   skip $SKIP"
echo "  logs kept in $LOGDIR"
[ "$FAIL" -eq 0 ] || exit 1
