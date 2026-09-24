Magic Lantern for the EOS 6D Mark II, firmware 1.2.0
====================================================

Status: boots in QEMU (qemu-eos).  NOT tested on hardware.  See
ml-6d2-handoff/handoff/FINDINGS_120.md in the port project for the full account.

This port was derived from platform/6D2.111.  1.2.0 is the same firmware with a few
code insertions, so every ROM0 stub address shifts by a per-region constant; the
region table is documented at the top of stubs.S.  DryOS kernel space (0xdf00xxxx)
and the low-RAM globals are unchanged.  RESTARTSTART also moved (0xe0f90 -> 0xe0fd0)
because user_mem_start moved.

THERE IS DELIBERATELY NO ML-SETUP.FIR IN THIS DIRECTORY.
-------------------------------------------------------
platform/6D2.111/ML-SETUP.FIR is a signed, encrypted Canon firmware update file whose
header declares version "1.1.1".  Its payload cannot be inspected or rebuilt without
DIGIC 7 signing keys, which are private to Canon and the ML team.  It necessarily
contains code compiled against 1.1.1 addresses, and this port exists precisely because
those addresses are wrong on 1.2.0.  Running it on a 1.2.0 body would execute
mis-targeted code while writing the boot flag to flash.  Do not copy it here.

Enabling the boot flag on a 1.2.0 body requires an ML-SETUP.FIR signed for 1.2.0,
which only the ML team can produce.  Until that exists, this platform can be built and
run in emulation only.
