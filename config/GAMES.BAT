@ECHO OFF
REM =====================================================================
REM  GAMES.BAT  -  launcher handoff wrapper for CASTALIA DOS
REM  Installed to C:\CASTALIA\BIN\GAMES.BAT (on PATH)
REM
REM  Runs the game launcher, then - if the launcher prepared a game -
REM  runs the one-shot _RUNGAME.BAT it wrote.  Because LAUNCH.EXE has
REM  already exited by then, the game gets all free conventional memory.
REM
REM     LAUNCH.EXE exits 77, then _RUNGAME.BAT runs, then the game.
REM
REM  NOTE: never write the greater-than, less-than or pipe characters in
REM  a REM.  FreeCOM parses redirection before it notices the line is a
REM  comment, so an arrow drawn in a comment makes the shell try to
REM  redirect into a file named after the next word.
REM =====================================================================

C:\CASTALIA\BIN\LAUNCH.EXE
IF ERRORLEVEL 78 GOTO done
IF ERRORLEVEL 77 GOTO run
GOTO done

:run
IF EXIST C:\CASTALIA\CFG\_RUNGAME.BAT CALL C:\CASTALIA\CFG\_RUNGAME.BAT
IF EXIST C:\CASTALIA\CFG\_RUNGAME.BAT DEL C:\CASTALIA\CFG\_RUNGAME.BAT >NUL

:done
