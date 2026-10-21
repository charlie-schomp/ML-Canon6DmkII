6D2 developer tools
===================

The research tools used to build the 6D Mark II port, in one module. Not
needed for shooting: leave it off unless you are debugging the port.

Debug menu > "6D2 developer tools":

- State logger: GUI state, every button event and every MPU message, written
  to ML/LOGS/STATELOG.TXT when switched off.
- Recording trace: mlv_lite logs each take's record path to ML/LOGS/RECDBG.LOG.
- Status + sensor dump, overlay alignment dump, D7 probe dump (bit depth and
  SD clock parameters, low RAM).
- Two experiments that write camera registers: the binned-readout frame timer
  and the SD clock. Research use only, with a spare card.

When loaded, the module hooks three Canon functions to log their arguments.

:Author: 6D2 port
:License: GPL
:Summary: 6D2 port research tools. Leave off for shooting.
