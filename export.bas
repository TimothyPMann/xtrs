20 REM-- EXPORT/BAS
40 REM-- Uses xtrs 1.3 export feature to copy a TRS-80 
60 REM--   file to a Unix file.
70 REM-- Timothy Mann, 10/30/96
75 REM-- Last modified on Wed Oct  1 17:37:25 PDT 1997 by mann
80 CLEAR 5000
120 LINE INPUT "TRS-80 input file? ";F$
130 LINE INPUT "Unix output file? ";C$
140 OPEN"RO",1,F$,1
160 FIELD #1,1 AS B$
180 INPUT "Convert newlines (y/n)";CN$
200 CN% = LEFT$(CN$,1) = "y" OR LEFT$(CN$,1) = "Y"
205 OUT &HD0,0
210 OUT &HD0,2
220 FOR I% = 1 TO LEN(C$)
240 OUT &HD1, ASC(MID$(C$, I%))
260 NEXT I%
270 OUT &HD1,0
300 S% = INP(&HD0)
320 IF S% <> 0 THEN 520
340 IF LOC(1) >= LOF(1) THEN 440
360 GET#1
380 B% = ASC(B$)
400 IF CN% THEN IF B% = &H0D THEN B% = &H0A
420 OUT &HD1, B%:GOTO 340
440 CLOSE #1
445 S% = INP(&HD0)
450 IF S% <> 0 THEN 520
460 OUT &HD0,3
480 S% = INP(&HD0)
500 IF S% = 0 THEN 540
520 PRINT:PRINT "Error ";S%
540 CLOSE
