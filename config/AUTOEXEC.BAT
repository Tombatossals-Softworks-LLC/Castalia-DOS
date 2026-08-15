@ECHO OFF
REM =====================================================================
REM  CASTALIA DOS 386SX Edition  -  AUTOEXEC.BAT
REM  Finishes each boot profile chosen in the CONFIG.SYS menu.
REM
REM  The FreeDOS kernel exports the chosen menu entry as %CONFIG%, and it
REM  is the DIGIT "1".."8", not a block name - this kernel has no MS-DOS
REM  style [BLOCK] sections.  The first thing we do is turn that digit
REM  into the readable profile name the rest of the file branches on,
REM  and which every Castalia tool reads back as %CASTPROFILE%.
REM
REM  EDITING RULE: never write the characters greater-than, less-than or
REM  pipe inside a REM.  FreeCOM parses redirection before it notices the
REM  line is a comment, so a REM mentioning "C:\" and a redirect arrow
REM  makes the shell try to redirect into a file named "]".
REM
REM  Original Castalia work, no Microsoft content.
REM =====================================================================

REM ---- Menu digit to profile name -------------------------------------
SET CASTPROFILE=
IF "%CONFIG%"=="1" SET CASTPROFILE=CLEAN
IF "%CONFIG%"=="2" SET CASTPROFILE=XMS
IF "%CONFIG%"=="3" SET CASTPROFILE=EMS
IF "%CONFIG%"=="4" SET CASTPROFILE=CDROM
IF "%CONFIG%"=="5" SET CASTPROFILE=WIN3X
IF "%CONFIG%"=="6" SET CASTPROFILE=DIAG
IF "%CONFIG%"=="7" SET CASTPROFILE=SAFE
IF "%CONFIG%"=="8" SET CASTPROFILE=PROMPT
REM  Menu skipped with F5, or an edited CONFIG.SYS with no menu at all.
IF "%CASTPROFILE%"=="" SET CASTPROFILE=CLEAN

REM ---- Shared environment (all profiles) ------------------------------
SET CASTALIA=C:\CASTALIA
SET PATH=C:\CASTALIA\BIN;C:\DOS;C:\
SET TEMP=C:\TEMP
SET TMP=C:\TEMP
SET DIRCMD=/O:GN /P
SET HELPPATH=C:\CASTALIA\HELP
IF NOT EXIST C:\TEMP\NUL MKDIR C:\TEMP

REM ---- Castalia prompt -------------------------------------------------
REM  Plain ASCII on purpose: no ANSI driver is loaded in any profile, so
REM  colour escapes would print as raw garbage instead of colouring.
PROMPT [CASTALIA $P]$G

REM ---- Safe Mode: minimal, stop here ---------------------------------
IF "%CASTPROFILE%"=="SAFE" GOTO SAFE

REM ---- Keyboard layout (optional; edit to your locale) ---------------
IF EXIST C:\DOS\KEYB.EXE C:\DOS\KEYB.EXE US,,C:\DOS\KEYBOARD.SYS >NUL

REM ---- Sound environment (SET BLASTER/SOUND/MIDI) --------------------
REM  SETSOUND.EXE writes C:\CASTALIA\CFG\SOUND.BAT.  If it is absent we
REM  fall back to a sane Sound Blaster Pro default so audio still works.
IF EXIST C:\CASTALIA\CFG\SOUND.BAT CALL C:\CASTALIA\CFG\SOUND.BAT
IF NOT EXIST C:\CASTALIA\CFG\SOUND.BAT SET BLASTER=A220 I5 D1 T4

REM ---- Branch to the profile-specific finisher ------------------------
GOTO %CASTPROFILE%

REM =====================================================================
:CLEAN
REM  No TSRs.  Maximum free conventional memory.
GOTO BANNER

REM =====================================================================
:XMS
REM  Load the mouse high (UMBs exist in this profile).
IF EXIST C:\CASTALIA\DRV\CTMOUSE.EXE LH C:\CASTALIA\DRV\CTMOUSE.EXE >NUL
GOTO BANNER

REM =====================================================================
:EMS
IF EXIST C:\CASTALIA\DRV\CTMOUSE.EXE LH C:\CASTALIA\DRV\CTMOUSE.EXE >NUL
GOTO BANNER

REM =====================================================================
:CDROM
REM  Bind the CD-ROM (UIDE named it CASTLCD1) to a drive letter with the
REM  SHSUCDX MSCDEX replacement, loaded high.  /L:D requests drive D:.
IF EXIST C:\CASTALIA\DRV\CTMOUSE.EXE LH C:\CASTALIA\DRV\CTMOUSE.EXE >NUL
IF EXIST C:\DOS\SHSUCDX.COM LH C:\DOS\SHSUCDX.COM /D:CASTLCD1 /L:D
IF NOT EXIST C:\DOS\SHSUCDX.COM ECHO   (SHSUCDX not found - CD-ROM letter not assigned)
GOTO BANNER

REM =====================================================================
:WIN3X
REM  Mouse + disk cache help Windows.  A SmartDrive-style cache loaded low
REM  is fine here; 1 MB cache, 0 write-behind for safety on real hardware.
IF EXIST C:\CASTALIA\DRV\CTMOUSE.EXE LH C:\CASTALIA\DRV\CTMOUSE.EXE >NUL
IF EXIST C:\DOS\SMARTDRV.EXE LH C:\DOS\SMARTDRV.EXE 1024 0
ECHO.
ECHO   Windows 3.x profile ready.  Type WIN to start Windows, or use the
ECHO   Castalia menu.  (Enhanced mode needs a 386DX or better.)
GOTO BANNER

REM =====================================================================
:DIAG
REM  Boot, then show the hardware report.
IF EXIST C:\CASTALIA\BIN\HWINFO.EXE C:\CASTALIA\BIN\HWINFO.EXE
IF NOT EXIST C:\CASTALIA\BIN\HWINFO.EXE ECHO   HWINFO.EXE not installed.
ECHO.
ECHO   Diagnostics profile.  You are at the command prompt.
GOTO END

REM =====================================================================
:PROMPT
REM  Power-user mode: no menu, just a prompt.
IF EXIST C:\CASTALIA\BIN\BANNER.EXE C:\CASTALIA\BIN\BANNER.EXE
ECHO.
ECHO   Command-prompt profile.  Type CASTALIA to open the menu, or
ECHO   LAUNCH to open the game launcher.
GOTO END

REM =====================================================================
:SAFE
SET PATH=C:\DOS;C:\
CLS
ECHO ===================================================================
ECHO   CASTALIA DOS - SAFE MODE
ECHO   Bare kernel, no drivers loaded.  Use this to repair the system.
ECHO   Helpful tools:  SAFEBOOT   CFGEDIT   HWINFO   MEM /C
ECHO ===================================================================
GOTO END

REM =====================================================================
REM  BANNER: the animated castle (BANNER.EXE) when installed, otherwise a
REM  plain text banner.  BANNER.EXE skips on any key and never blocks.
REM  Every profile that shows the banner then wants the menu, so this
REM  block simply falls through into CASTMENU below.
REM =====================================================================
:BANNER
IF EXIST C:\CASTALIA\BIN\BANNER.EXE GOTO BANEXE
ECHO.
ECHO   CASTALIA DOS 386SX Edition   -   profile: %CASTPROFILE%
ECHO   -----------------------------------------------------------------
GOTO CASTMENU
:BANEXE
C:\CASTALIA\BIN\BANNER.EXE

REM =====================================================================
REM  CASTMENU  -  main Castalia menu loop.
REM  CASTALIA.EXE returns an errorlevel telling this loop what to do:
REM     10 = open the game launcher (LAUNCH.EXE, via GAMES.BAT)
REM      0 = the user chose "Exit to Command Line"
REM  Routing game launches through batch files keeps CASTALIA.EXE and
REM  LAUNCH.EXE OUT of memory while the game runs, so the game gets all
REM  available conventional memory.
REM =====================================================================
:CASTMENU
IF NOT EXIST C:\CASTALIA\BIN\CASTALIA.EXE GOTO END
:CASTLOOP
C:\CASTALIA\BIN\CASTALIA.EXE
IF ERRORLEVEL 10 IF NOT ERRORLEVEL 11 GOTO CASTGAMES
GOTO END
:CASTGAMES
IF EXIST C:\CASTALIA\BIN\GAMES.BAT CALL C:\CASTALIA\BIN\GAMES.BAT
GOTO CASTLOOP

REM =====================================================================
:END
REM  Fall through to the command prompt.
