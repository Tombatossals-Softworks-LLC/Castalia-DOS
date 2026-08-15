@ECHO OFF
REM =====================================================================
REM  SOUND.BAT  -  sound environment for CASTALIA DOS
REM  Installed to C:\CASTALIA\CFG\SOUND.BAT
REM  CALLed from AUTOEXEC.BAT.  SETSOUND.EXE regenerates this file when
REM  you change the sound profile.  This default assumes a Sound Blaster
REM  Pro at the classic address; edit or re-run SETSOUND to change it.
REM
REM  SET BLASTER fields:
REM    A = I/O base port    I = IRQ    D = 8-bit DMA
REM    H = 16-bit DMA       T = card type (4 = SB Pro, 6 = SB16)
REM =====================================================================

SET BLASTER=A220 I5 D1 H5 T4
SET SOUND=C:\CASTALIA
SET MIDI=SYNTH:1 MAP:E
