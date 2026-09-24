D7 probe (6D2)
==============

Read-only research probe for the 6D Mark II: logs Canon's LiveView
pre-processing path parameters (PACK32 bit width, raw DMAC geometry) and
SetSDClkFrequency calls, dumps the raw EDMAC shadow registers, the SD clock
tables and low RAM.  Debug menu > "D7 probe dump" writes
ML/LOGS/D7PROBE.LOG and ML/LOGS/LOWRAM.BIN.  Nothing is written to hardware.

:Author: 6D2 port
:License: GPL
:Summary: 6D2 research probe: bit depth + SD clock (read-only)
