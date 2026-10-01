# =====================================================================
#  CASTALIA DOS  -  Makefile for Open Watcom wmake
# ---------------------------------------------------------------------
#  Builds the 16-bit real-mode DOS tools into ./build.
#
#  Usage (with the Open Watcom environment loaded):
#      wmake            build all tools
#      wmake smoke      build the CI DOS smoke test
#      wmake vidtest    build the video-adapter probe
#      wmake clean      remove build products
#      wmake launch     build only LAUNCH.EXE (any tool name works)
#
#  Paths use FORWARD slashes: the Open Watcom tools accept them on
#  every host, and the Linux CI runner requires them (backslash paths
#  do not resolve there).
#
#  Compiler flags:
#      -0     8086 code (runs on every 386SX and up; max portability)
#      -ml    large memory model (room for menu + game data)
#      -os    optimize for size (small tools, fast load on a 386SX)
#      -bt=dos target DOS
#      -zq    quiet operation
#      -wx    strict warnings
#  CPUDET.C's probes are byte-encoded machine code inside #pragma aux
#  (the 16-bit compiler rejects CPUID/FNSTSW mnemonics and 32-bit
#  register lists at any -N level), so every unit builds at plain -0.
# =====================================================================

CC      = wcc
BUILD   = build
COMMON  = src/common
# -i=$(%WATCOM)/h pins the DOS-target headers explicitly: wcc searches
# -i paths BEFORE the INCLUDE env var, so a host whose INCLUDE points at
# the Linux headers (as the setup-watcom CI action does) still compiles
# against the right ones for -bt=dos.
INCDIRS = -i=$(COMMON) -i=$(%WATCOM)/h
CFLAGS  = -0 -ml -os -bt=dos -zq -wx $(INCDIRS)
LINK    = wlink

OBJS_COMMON = $(BUILD)/ini.obj $(BUILD)/ui.obj

all: hello $(BUILD)/launch.exe $(BUILD)/castalia.exe $(BUILD)/hwinfo.exe &
     $(BUILD)/setsound.exe $(BUILD)/memprof.exe $(BUILD)/setup.exe &
     $(BUILD)/safeboot.exe $(BUILD)/cfgedit.exe $(BUILD)/gamecfg.exe &
     $(BUILD)/castfm.exe $(BUILD)/castmark.exe $(BUILD)/castcopy.exe &
     $(BUILD)/castdoc.exe $(BUILD)/snake.exe $(BUILD)/puzzle.exe &
     $(BUILD)/almena.exe $(BUILD)/minas.exe $(BUILD)/banner.exe &
     $(BUILD)/help.exe $(BUILD)/castedit.exe $(BUILD)/castid.exe &
     $(BUILD)/siege.exe $(BUILD)/reversi.exe $(BUILD)/barrels.exe &
     $(BUILD)/solitare.exe $(BUILD)/cdplayer.exe $(BUILD)/saver.exe &
     $(BUILD)/casttour.exe $(BUILD)/castlink.exe &
     $(BUILD)/undel.exe .SYMBOLIC
	@echo Build complete.  Tools are in the ./build directory.

hello: .SYMBOLIC
	@echo Building CASTALIA DOS tools with Open Watcom...

# --- Short names ------------------------------------------------------
launch:   $(BUILD)/launch.exe   .SYMBOLIC
castalia: $(BUILD)/castalia.exe .SYMBOLIC
hwinfo:   $(BUILD)/hwinfo.exe   .SYMBOLIC
setsound: $(BUILD)/setsound.exe .SYMBOLIC
memprof:  $(BUILD)/memprof.exe  .SYMBOLIC
setup:    $(BUILD)/setup.exe    .SYMBOLIC
safeboot: $(BUILD)/safeboot.exe .SYMBOLIC
cfgedit:  $(BUILD)/cfgedit.exe  .SYMBOLIC
gamecfg:  $(BUILD)/gamecfg.exe  .SYMBOLIC
castfm:   $(BUILD)/castfm.exe   .SYMBOLIC
castmark: $(BUILD)/castmark.exe .SYMBOLIC
castcopy: $(BUILD)/castcopy.exe .SYMBOLIC
castdoc:  $(BUILD)/castdoc.exe  .SYMBOLIC
snake:    $(BUILD)/snake.exe    .SYMBOLIC
puzzle:   $(BUILD)/puzzle.exe   .SYMBOLIC
almena:   $(BUILD)/almena.exe   .SYMBOLIC
minas:    $(BUILD)/minas.exe    .SYMBOLIC
banner:   $(BUILD)/banner.exe   .SYMBOLIC
help:     $(BUILD)/help.exe     .SYMBOLIC
castedit: $(BUILD)/castedit.exe .SYMBOLIC
castid:   $(BUILD)/castid.exe   .SYMBOLIC
siege:    $(BUILD)/siege.exe    .SYMBOLIC
reversi:  $(BUILD)/reversi.exe  .SYMBOLIC
barrels:  $(BUILD)/barrels.exe  .SYMBOLIC
solitare: $(BUILD)/solitare.exe .SYMBOLIC
cdplayer: $(BUILD)/cdplayer.exe .SYMBOLIC
saver:    $(BUILD)/saver.exe    .SYMBOLIC
casttour: $(BUILD)/casttour.exe .SYMBOLIC
castlink: $(BUILD)/castlink.exe .SYMBOLIC
undel:    $(BUILD)/undel.exe    .SYMBOLIC
smoke:    $(BUILD)/smoke.exe    .SYMBOLIC
ktest:    $(BUILD)/ktest.exe    .SYMBOLIC
vidtest:  $(BUILD)/vidtest.exe  .SYMBOLIC
spktest:  $(BUILD)/spktest.exe  .SYMBOLIC
cdtest:   $(BUILD)/cdtest.exe   .SYMBOLIC

# --- Common objects ---------------------------------------------------
$(BUILD)/ini.obj: $(COMMON)/INI.C $(COMMON)/INI.H
	$(CC) $(CFLAGS) -fo=$@ $(COMMON)/INI.C

$(BUILD)/ui.obj: $(COMMON)/UI.C $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ $(COMMON)/UI.C

$(BUILD)/dirw.obj: $(COMMON)/DIRW.C $(COMMON)/DIRW.H
	$(CC) $(CFLAGS) -fo=$@ $(COMMON)/DIRW.C

$(BUILD)/cpudet.obj: $(COMMON)/CPUDET.C $(COMMON)/CPUDET.H
	$(CC) $(CFLAGS) -fo=$@ $(COMMON)/CPUDET.C

$(BUILD)/logo.obj: $(COMMON)/LOGO.C $(COMMON)/LOGO.H $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ $(COMMON)/LOGO.C

$(BUILD)/spk.obj: $(COMMON)/SPK.C $(COMMON)/SPK.H $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ $(COMMON)/SPK.C

$(BUILD)/xmsinfo.obj: $(COMMON)/XMSINFO.C $(COMMON)/XMSINFO.H
	$(CC) $(CFLAGS) -fo=$@ $(COMMON)/XMSINFO.C

$(BUILD)/viddet.obj: $(COMMON)/VIDDET.C $(COMMON)/VIDDET.H
	$(CC) $(CFLAGS) -fo=$@ $(COMMON)/VIDDET.C

$(BUILD)/safeio.obj: $(COMMON)/SAFEIO.C $(COMMON)/SAFEIO.H
	$(CC) $(CFLAGS) -fo=$@ $(COMMON)/SAFEIO.C

# --- LAUNCH.EXE -------------------------------------------------------
$(BUILD)/launch.obj: src/launch/LAUNCH.C $(COMMON)/INI.H $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ src/launch/LAUNCH.C

$(BUILD)/launch.exe: $(BUILD)/launch.obj $(OBJS_COMMON)
	$(LINK) system dos name $@ file $(BUILD)/launch.obj,$(BUILD)/ini.obj,$(BUILD)/ui.obj

# --- CASTALIA.EXE -----------------------------------------------------
$(BUILD)/castalia.obj: src/castalia/CASTALIA.C $(COMMON)/INI.H $(COMMON)/UI.H $(COMMON)/LOGO.H
	$(CC) $(CFLAGS) -fo=$@ src/castalia/CASTALIA.C

$(BUILD)/castalia.exe: $(BUILD)/castalia.obj $(OBJS_COMMON) $(BUILD)/logo.obj
	$(LINK) system dos name $@ file $(BUILD)/castalia.obj,$(BUILD)/ini.obj,$(BUILD)/ui.obj,$(BUILD)/logo.obj

# --- HWINFO.EXE  (UI + CPUDET) ----------------------------------------
$(BUILD)/hwinfo.obj: src/hwinfo/HWINFO.C $(COMMON)/UI.H $(COMMON)/CPUDET.H $(COMMON)/XMSINFO.H $(COMMON)/VIDDET.H
	$(CC) $(CFLAGS) -fo=$@ src/hwinfo/HWINFO.C

$(BUILD)/hwinfo.exe: $(BUILD)/viddet.obj $(BUILD)/hwinfo.obj $(BUILD)/ui.obj $(BUILD)/cpudet.obj $(BUILD)/xmsinfo.obj
	$(LINK) system dos name $@ file $(BUILD)/hwinfo.obj,$(BUILD)/viddet.obj,$(BUILD)/ui.obj,$(BUILD)/cpudet.obj,$(BUILD)/xmsinfo.obj

# ---  -----------------------------------------------------------------
$(BUILD)/setsound.obj: src/setsound/SETSOUND.C $(COMMON)/UI.H $(COMMON)/SPK.H $(COMMON)/SAFEIO.H
	$(CC) $(CFLAGS) -fo=$@ src/setsound/SETSOUND.C

$(BUILD)/setsound.exe: $(BUILD)/setsound.obj $(BUILD)/ui.obj $(BUILD)/spk.obj $(BUILD)/safeio.obj
	$(LINK) system dos name $@ file $(BUILD)/setsound.obj,$(BUILD)/ui.obj,$(BUILD)/spk.obj,$(BUILD)/safeio.obj

# --- MEMPROF.EXE  (INI + UI) ------------------------------------------
$(BUILD)/memprof.obj: src/memprof/MEMPROF.C $(COMMON)/INI.H $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ src/memprof/MEMPROF.C

$(BUILD)/memprof.exe: $(BUILD)/memprof.obj $(OBJS_COMMON)
	$(LINK) system dos name $@ file $(BUILD)/memprof.obj,$(BUILD)/ini.obj,$(BUILD)/ui.obj

# ---  -----------------------------------------------------------------
$(BUILD)/setup.obj: src/setup/SETUP.C $(COMMON)/UI.H $(COMMON)/LOGO.H $(COMMON)/DIRW.H $(COMMON)/SAFEIO.H
	$(CC) $(CFLAGS) -fo=$@ src/setup/SETUP.C

$(BUILD)/setup.exe: $(BUILD)/setup.obj $(BUILD)/ui.obj $(BUILD)/logo.obj $(BUILD)/dirw.obj $(BUILD)/safeio.obj
	$(LINK) system dos name $@ file $(BUILD)/setup.obj,$(BUILD)/ui.obj,$(BUILD)/logo.obj,$(BUILD)/dirw.obj,$(BUILD)/safeio.obj

# ---  -----------------------------------------------------------------
$(BUILD)/safeboot.obj: src/safeboot/SAFEBOOT.C $(COMMON)/UI.H $(COMMON)/SAFEIO.H
	$(CC) $(CFLAGS) -fo=$@ src/safeboot/SAFEBOOT.C

$(BUILD)/safeboot.exe: $(BUILD)/safeboot.obj $(BUILD)/ui.obj $(BUILD)/safeio.obj
	$(LINK) system dos name $@ file $(BUILD)/safeboot.obj,$(BUILD)/ui.obj,$(BUILD)/safeio.obj

# ---  -----------------------------------------------------------------
$(BUILD)/cfgedit.obj: src/cfgedit/CFGEDIT.C $(COMMON)/UI.H $(COMMON)/DIRW.H $(COMMON)/SAFEIO.H
	$(CC) $(CFLAGS) -fo=$@ src/cfgedit/CFGEDIT.C

$(BUILD)/cfgedit.exe: $(BUILD)/cfgedit.obj $(BUILD)/ui.obj $(BUILD)/dirw.obj $(BUILD)/safeio.obj
	$(LINK) system dos name $@ file $(BUILD)/cfgedit.obj,$(BUILD)/ui.obj,$(BUILD)/dirw.obj,$(BUILD)/safeio.obj

# ---  -----------------------------------------------------------------
$(BUILD)/gamecfg.obj: src/gamecfg/GAMECFG.C $(COMMON)/INI.H $(COMMON)/UI.H $(COMMON)/SAFEIO.H
	$(CC) $(CFLAGS) -fo=$@ src/gamecfg/GAMECFG.C

$(BUILD)/gamecfg.exe: $(BUILD)/gamecfg.obj $(OBJS_COMMON) $(BUILD)/safeio.obj
	$(LINK) system dos name $@ file $(BUILD)/gamecfg.obj,$(BUILD)/ini.obj,$(BUILD)/ui.obj,$(BUILD)/safeio.obj

# ---  -----------------------------------------------------------------
$(BUILD)/castfm.obj: src/castfm/CASTFM.C $(COMMON)/UI.H $(COMMON)/DIRW.H $(COMMON)/SAFEIO.H
	$(CC) $(CFLAGS) -fo=$@ src/castfm/CASTFM.C

$(BUILD)/castfm.exe: $(BUILD)/castfm.obj $(BUILD)/ui.obj $(BUILD)/dirw.obj $(BUILD)/safeio.obj
	$(LINK) system dos name $@ file $(BUILD)/castfm.obj,$(BUILD)/ui.obj,$(BUILD)/dirw.obj,$(BUILD)/safeio.obj

# --- CASTMARK.EXE  (UI + INI + CPUDET) --------------------------------
$(BUILD)/castmark.obj: src/castmark/CASTMARK.C $(COMMON)/UI.H $(COMMON)/INI.H $(COMMON)/CPUDET.H
	$(CC) $(CFLAGS) -fo=$@ src/castmark/CASTMARK.C

$(BUILD)/castmark.exe: $(BUILD)/viddet.obj $(BUILD)/castmark.obj $(OBJS_COMMON) $(BUILD)/cpudet.obj $(BUILD)/xmsinfo.obj
	$(LINK) system dos name $@ file $(BUILD)/castmark.obj,$(BUILD)/viddet.obj,$(BUILD)/ini.obj,$(BUILD)/ui.obj,$(BUILD)/cpudet.obj,$(BUILD)/xmsinfo.obj

# ---  -----------------------------------------------------------------
$(BUILD)/castcopy.obj: src/castcopy/CASTCOPY.C $(COMMON)/UI.H $(COMMON)/DIRW.H $(COMMON)/SAFEIO.H
	$(CC) $(CFLAGS) -fo=$@ src/castcopy/CASTCOPY.C

$(BUILD)/castcopy.exe: $(BUILD)/castcopy.obj $(BUILD)/ui.obj $(BUILD)/dirw.obj $(BUILD)/safeio.obj
	$(LINK) system dos name $@ file $(BUILD)/castcopy.obj,$(BUILD)/ui.obj,$(BUILD)/dirw.obj,$(BUILD)/safeio.obj

# --- CASTDOC.EXE  (UI only) -------------------------------------------
$(BUILD)/castdoc.obj: src/castdoc/CASTDOC.C $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ src/castdoc/CASTDOC.C

$(BUILD)/castdoc.exe: $(BUILD)/castdoc.obj $(BUILD)/ui.obj
	$(LINK) system dos name $@ file $(BUILD)/castdoc.obj,$(BUILD)/ui.obj

# --- Minigames  (UI only) ---------------------------------------------
$(BUILD)/snake.obj: src/games/SNAKE.C $(COMMON)/UI.H $(COMMON)/SPK.H
	$(CC) $(CFLAGS) -fo=$@ src/games/SNAKE.C

$(BUILD)/snake.exe: $(BUILD)/snake.obj $(BUILD)/ui.obj $(BUILD)/spk.obj
	$(LINK) system dos name $@ file $(BUILD)/snake.obj,$(BUILD)/ui.obj,$(BUILD)/spk.obj

$(BUILD)/puzzle.obj: src/games/PUZZLE.C $(COMMON)/UI.H $(COMMON)/SPK.H
	$(CC) $(CFLAGS) -fo=$@ src/games/PUZZLE.C

$(BUILD)/puzzle.exe: $(BUILD)/puzzle.obj $(BUILD)/ui.obj $(BUILD)/spk.obj
	$(LINK) system dos name $@ file $(BUILD)/puzzle.obj,$(BUILD)/ui.obj,$(BUILD)/spk.obj

$(BUILD)/almena.obj: src/games/ALMENA.C $(COMMON)/UI.H $(COMMON)/SPK.H
	$(CC) $(CFLAGS) -fo=$@ src/games/ALMENA.C

$(BUILD)/almena.exe: $(BUILD)/almena.obj $(BUILD)/ui.obj $(BUILD)/spk.obj
	$(LINK) system dos name $@ file $(BUILD)/almena.obj,$(BUILD)/ui.obj,$(BUILD)/spk.obj

$(BUILD)/minas.obj: src/games/MINAS.C $(COMMON)/UI.H $(COMMON)/SPK.H
	$(CC) $(CFLAGS) -fo=$@ src/games/MINAS.C

$(BUILD)/minas.exe: $(BUILD)/minas.obj $(BUILD)/ui.obj $(BUILD)/spk.obj
	$(LINK) system dos name $@ file $(BUILD)/minas.obj,$(BUILD)/ui.obj,$(BUILD)/spk.obj

# --- BANNER.EXE  (UI + LOGO) ------------------------------------------
$(BUILD)/banner.obj: src/banner/BANNER.C $(COMMON)/UI.H $(COMMON)/LOGO.H
	$(CC) $(CFLAGS) -fo=$@ src/banner/BANNER.C

$(BUILD)/banner.exe: $(BUILD)/banner.obj $(BUILD)/ui.obj $(BUILD)/logo.obj
	$(LINK) system dos name $@ file $(BUILD)/banner.obj,$(BUILD)/ui.obj,$(BUILD)/logo.obj

# --- HELP.EXE  (UI + INI) ---------------------------------------------
$(BUILD)/help.obj: src/help/HELP.C $(COMMON)/UI.H $(COMMON)/INI.H
	$(CC) $(CFLAGS) -fo=$@ src/help/HELP.C

$(BUILD)/help.exe: $(BUILD)/help.obj $(OBJS_COMMON)
	$(LINK) system dos name $@ file $(BUILD)/help.obj,$(BUILD)/ini.obj,$(BUILD)/ui.obj

# ---  -----------------------------------------------------------------
$(BUILD)/castedit.obj: src/castedit/CASTEDIT.C $(COMMON)/UI.H $(COMMON)/SAFEIO.H
	$(CC) $(CFLAGS) -fo=$@ src/castedit/CASTEDIT.C

$(BUILD)/castedit.exe: $(BUILD)/castedit.obj $(BUILD)/ui.obj $(BUILD)/safeio.obj
	$(LINK) system dos name $@ file $(BUILD)/castedit.obj,$(BUILD)/ui.obj,$(BUILD)/safeio.obj

# --- CASTID.EXE  (UI + CPUDET) ----------------------------------------
$(BUILD)/castid.obj: src/castid/CASTID.C $(COMMON)/UI.H $(COMMON)/CPUDET.H $(COMMON)/XMSINFO.H
	$(CC) $(CFLAGS) -fo=$@ src/castid/CASTID.C

$(BUILD)/castid.exe: $(BUILD)/castid.obj $(BUILD)/ui.obj $(BUILD)/cpudet.obj $(BUILD)/xmsinfo.obj
	$(LINK) system dos name $@ file $(BUILD)/castid.obj,$(BUILD)/ui.obj,$(BUILD)/cpudet.obj,$(BUILD)/xmsinfo.obj

# --- SIEGE.EXE / REVERSI.EXE / BARRELS.EXE  (games, UI only) ----------
$(BUILD)/siege.obj: src/games/SIEGE.C $(COMMON)/UI.H $(COMMON)/SPK.H
	$(CC) $(CFLAGS) -fo=$@ src/games/SIEGE.C

$(BUILD)/siege.exe: $(BUILD)/siege.obj $(BUILD)/ui.obj $(BUILD)/spk.obj
	$(LINK) system dos name $@ file $(BUILD)/siege.obj,$(BUILD)/ui.obj,$(BUILD)/spk.obj

$(BUILD)/reversi.obj: src/games/REVERSI.C $(COMMON)/UI.H $(COMMON)/SPK.H
	$(CC) $(CFLAGS) -fo=$@ src/games/REVERSI.C

$(BUILD)/reversi.exe: $(BUILD)/reversi.obj $(BUILD)/ui.obj $(BUILD)/spk.obj
	$(LINK) system dos name $@ file $(BUILD)/reversi.obj,$(BUILD)/ui.obj,$(BUILD)/spk.obj

$(BUILD)/barrels.obj: src/games/BARRELS.C $(COMMON)/UI.H $(COMMON)/SPK.H
	$(CC) $(CFLAGS) -fo=$@ src/games/BARRELS.C

$(BUILD)/barrels.exe: $(BUILD)/barrels.obj $(BUILD)/ui.obj $(BUILD)/spk.obj
	$(LINK) system dos name $@ file $(BUILD)/barrels.obj,$(BUILD)/ui.obj,$(BUILD)/spk.obj

# --- SOLITARE.EXE  (game, UI only) ------------------------------------
$(BUILD)/solitare.obj: src/games/SOLITARE.C $(COMMON)/UI.H $(COMMON)/SPK.H
	$(CC) $(CFLAGS) -fo=$@ src/games/SOLITARE.C

$(BUILD)/solitare.exe: $(BUILD)/solitare.obj $(BUILD)/ui.obj $(BUILD)/spk.obj
	$(LINK) system dos name $@ file $(BUILD)/solitare.obj,$(BUILD)/ui.obj,$(BUILD)/spk.obj

# --- CDPLAYER.EXE  (MSCDEX audio, UI only) ----------------------------
$(BUILD)/cdplayer.obj: src/cdplayer/CDPLAYER.C $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ src/cdplayer/CDPLAYER.C

$(BUILD)/cdplayer.exe: $(BUILD)/cdplayer.obj $(BUILD)/ui.obj
	$(LINK) system dos name $@ file $(BUILD)/cdplayer.obj,$(BUILD)/ui.obj

# --- SAVER.EXE  (screensaver, UI only) --------------------------------
$(BUILD)/saver.obj: src/saver/SAVER.C $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ src/saver/SAVER.C

$(BUILD)/saver.exe: $(BUILD)/saver.obj $(BUILD)/ui.obj
	$(LINK) system dos name $@ file $(BUILD)/saver.obj,$(BUILD)/ui.obj

# --- CASTTOUR.EXE  (guided tour / attract mode, UI only) --------------
$(BUILD)/casttour.obj: src/casttour/CASTTOUR.C $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ src/casttour/CASTTOUR.C

$(BUILD)/casttour.exe: $(BUILD)/casttour.obj $(BUILD)/ui.obj
	$(LINK) system dos name $@ file $(BUILD)/casttour.obj,$(BUILD)/ui.obj

# --- CASTLINK.EXE  (serial transfer, UI + DIRW) -----------------------
$(BUILD)/castlink.obj: src/castlink/CASTLINK.C $(COMMON)/UI.H $(COMMON)/DIRW.H
	$(CC) $(CFLAGS) -fo=$@ src/castlink/CASTLINK.C

$(BUILD)/castlink.exe: $(BUILD)/castlink.obj $(BUILD)/ui.obj $(BUILD)/dirw.obj
	$(LINK) system dos name $@ file $(BUILD)/castlink.obj,$(BUILD)/ui.obj,$(BUILD)/dirw.obj

# --- UNDEL.EXE  (deleted-file recovery, read-only source; UI only) ----
$(BUILD)/undel.obj: src/undel/UNDEL.C $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ src/undel/UNDEL.C

$(BUILD)/undel.exe: $(BUILD)/undel.obj $(BUILD)/ui.obj
	$(LINK) system dos name $@ file $(BUILD)/undel.obj,$(BUILD)/ui.obj

# --- KTEST.EXE  (CI kernel-API probe; not part of 'all') --------------
$(BUILD)/ktest.obj: tests/dos/KTEST.C
	$(CC) $(CFLAGS) -fo=$@ tests/dos/KTEST.C

$(BUILD)/ktest.exe: $(BUILD)/ktest.obj
	$(LINK) system dos name $@ file $(BUILD)/ktest.obj

$(BUILD)/vidtest.obj: tests/dos/VIDTEST.C $(COMMON)/VIDDET.H
	$(CC) $(CFLAGS) -fo=$@ tests/dos/VIDTEST.C

$(BUILD)/vidtest.exe: $(BUILD)/vidtest.obj $(BUILD)/viddet.obj
	$(LINK) system dos name $@ file $(BUILD)/vidtest.obj,$(BUILD)/viddet.obj

$(BUILD)/spktest.obj: tests/dos/SPKTEST.C $(COMMON)/SPK.H $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ tests/dos/SPKTEST.C

$(BUILD)/spktest.exe: $(BUILD)/spktest.obj $(BUILD)/spk.obj $(BUILD)/ui.obj
	$(LINK) system dos name $@ file $(BUILD)/spktest.obj,$(BUILD)/spk.obj,$(BUILD)/ui.obj

# CDTEST #includes CDPLAYER.C behind -DCDPLAYER_TEST, so the object it
# builds already contains the player's code; only UI is linked beside it.
$(BUILD)/cdtest.obj: tests/dos/CDTEST.C src/cdplayer/CDPLAYER.C $(COMMON)/UI.H
	$(CC) $(CFLAGS) -fo=$@ tests/dos/CDTEST.C

$(BUILD)/cdtest.exe: $(BUILD)/cdtest.obj $(BUILD)/ui.obj
	$(LINK) system dos name $@ file $(BUILD)/cdtest.obj,$(BUILD)/ui.obj

# --- SMOKE.EXE  (CI test program; not part of 'all') ------------------
$(BUILD)/smoke.obj: tests/dos/SMOKE.C $(COMMON)/INI.H
	$(CC) $(CFLAGS) -fo=$@ tests/dos/SMOKE.C

$(BUILD)/smoke.exe: $(BUILD)/smoke.obj $(BUILD)/ini.obj
	$(LINK) system dos name $@ file $(BUILD)/smoke.obj,$(BUILD)/ini.obj

# --- Housekeeping ------------------------------------------------------
clean: .SYMBOLIC
!ifdef __UNIX__
	rm -f $(BUILD)/*.obj $(BUILD)/*.exe $(BUILD)/*.map $(BUILD)/*.lst
!else
	@if exist $(BUILD)\*.obj del $(BUILD)\*.obj
	@if exist $(BUILD)\*.exe del $(BUILD)\*.exe
	@if exist $(BUILD)\*.map del $(BUILD)\*.map
!endif
	@echo Clean done.
