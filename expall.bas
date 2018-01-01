20 CLEAR 1000
40 REM-- List an LDOS directory by opening and reading DIR/SYS
50 REM-- and use the xtrs export program to export all files
60 LINE INPUT "Drive? "; DR$
80 OPEN "ri", 1, "dir/sys.rs0lt0ff:" + DR$, 32
100 FIELD 1, 1 AS AT$, 1 AS FL$, 1 AS DT$, 1 AS EF$, 1 AS RL$, 8 AS NM$, 3 AS NX$, 2 AS OP$, 2 AS UP$, 2 AS ER$, 10 AS XT$
120 GET 1, 16
140 N = 0
160 IF EOF(1) THEN END
180 N = N + 1
200 GET 1
220 IF (ASC(AT$) AND 16) = 0 THEN GOTO 160
240 I=INSTR(NM$, " ")
260 IF I THEN NH$ = LEFT$(NM$, I-1) ELSE NH$ = NM$
280 I = INSTR(NX$, " ")
300 IF I THEN NT$ = LEFT$(NX$, I-1) ELSE NT$ = NX$
320 REM print n; " ";
340 NF$ = NH$ + "/" + NT$
360 PRINT NF$
380 UF$ = NH$ + "." + NT$
400 CMD "export -l " + NF$ + ".rs0lt0ff:" + DR$ + " " + UF$
420 GOTO 160
