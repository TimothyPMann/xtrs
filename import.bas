20 REM-- IMPORT/BAS
40 REM-- Uses xtrs 1.3 import feature to read a Unix file
50 REM--   and display or save the results.
60 REM-- Timothy Mann, 10/30/96
70 REM-- Last modified on Fri Feb 13 12:18:22 PST 1998 by mann
80 CLEAR 5000
100 LINE INPUT "Unix input file? ";C$
120 LINE INPUT "TRS-80 output file? (*DO to display) ";F$
140 OPEN "O",1,F$
160 INPUT "Convert newlines (y/n)";CN$
180 CN%=LEFT$(CN$,1)="y" OR LEFT$(CN$,1)="Y"
200 OUT &HD0,0
220 OUT &HD0,1
240 FOR I%=1 TO LEN(C$)
260 OUT &HD1, ASC(MID$(C$, I%))
280 NEXT I%
300 OUT &HD1,0
320 S%=INP(&HD0)
340 IF S% <> 0 THEN 480
360 B%=INP(&HD1)
380 IF CN% THEN IF B% = &H0A THEN B% =&H0D
400 IF B% <> 0 THEN PRINT#1,CHR$(B%);:GOTO 360
420 S%=INP(&HD0)
440 IF S% = &HFF THEN PRINT#1,CHR$(B%);:GOTO 360
460 IF S%=0 THEN 500
480 PRINT:PRINT "Error ";S%
500 CLOSE
