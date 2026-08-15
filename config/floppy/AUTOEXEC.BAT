@ECHO OFF
REM =====================================================================
REM  CASTALIA DOS  -  AUTOEXEC.BAT for the BOOT/RESCUE DISKETTE
REM  Everything lives on A:.  The installed system's AUTOEXEC template
REM  rides along in A:\INSTALL\ and is written to C:\ by SETUP.
REM =====================================================================

SET CASTALIA=A:\CASTALIA
SET PATH=A:\CASTALIA\BIN;A:\DOS;A:\
PROMPT $P$G

REM ---- CI boot-test marker: only written when the test flag exists ----
REM  Written BEFORE the splash so the boot test never depends on it.
IF EXIST A:\CITEST.FLG ECHO CASTALIA-BOOT-OK > A:\BOOTOK.TXT

REM ---- Load splash -----------------------------------------------------
REM  BANNER /R draws the keep AND the rescue command list in one screen and
REM  leaves it up, dropping the prompt just below it.  So we print NOTHING
REM  after it: on a real floppy, echoing a text menu once BANNER had cleared
REM  the screen produced a long black gap while the drive read each line.
REM  The plain-text menu is a fallback, reached only when BANNER is missing.
IF EXIST A:\CASTALIA\BIN\BANNER.EXE GOTO SPLASH

ECHO.
ECHO   ================================================================
ECHO      CASTALIA DOS 386SX Edition   -   Rescue and Install Disk
ECHO   ================================================================
ECHO      SETUP      install CASTALIA DOS to the hard disk
ECHO      SAFEBOOT   repair an installed system's boot files
ECHO      HWINFO     inspect this machine
ECHO      FDISK / FORMAT / SYS / CHKDSK   live in A:\DOS
ECHO   ----------------------------------------------------------------
ECHO      Type a command and press Enter.  The fortress stands ready.
GOTO END

:SPLASH
A:\CASTALIA\BIN\BANNER.EXE /R

:END
