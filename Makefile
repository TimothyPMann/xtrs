
#
# Makefile for xtrs, the TRS-80 emulator.
#

OBJECTS = \
	z80.o \
	main.o \
	load_cmd.o \
	load_hex.o \
	trs_memory.o \
	trs_keyboard.o \
	error.o \
	debug.o \
	dis.o \
	trs_io.o \
	trs_cassette.o \
	trs_xinterface.o \
	trs_chars.o \
	trs_printer.o \
	trs_rom1.o \
	trs_rom3.o \
	trs_rom4p.o \
	trs_disk.o \
	trs_interrupt.o \
	trs_imp_exp.o \
	trs_hard.o \
	trs_uart.o

CR_OBJECTS = \
	compile_rom.o \
	error.o \
	load_cmd.o \
	load_hex.o

MD_OBJECTS = \
	mkdisk.o

HC_OBJECTS = \
	cmd.o \
	error.o \
	load_hex.o \
	hex2cmd.o

CD_OBJECTS = \
	cmddump.o \
	load_cmd.o

SOURCES = \
	cmd.c \
	cmddump.c \
	compile_rom.c \
	debug.c \
	dis.c \
	error.c \
	hex2cmd.c \
	load_cmd.c \
	load_hex.c \
	main.c \
	mkdisk.c \
	trs_cassette.c \
	trs_chars.c \
	trs_disk.c \
	trs_hard.c \
	trs_imp_exp.c \
	trs_interrupt.c \
	trs_io.c \
	trs_keyboard.c \
	trs_memory.c \
	trs_printer.c \
	trs_uart.c \
	trs_xinterface.c \
	z80.c

HEADERS = \
	cmd.h \
	config.h \
	reed.h \
	trs.h \
	trs_disk.h \
	trs_hard.h \
	trs_imp_exp.h \
	trs_iodefs.h \
	trs_uart.h \
	z80.h

MISC = \
	ChangeLog \
	Makefile \
	Makefile.local \
	README \
	README-1.0 \
	cassette \
	cassette.txt \
	cmddump.txt \
	export.bas \
	export.cmd \
	export.lst \
	export.z \
	hardfmt.txt \
	hex2cmd.txt \
	import.bas \
	import.cmd \
	import.lst \
	import.z \
	m1format.fix \
	mkdisk.txt \
	settime.ccc \
	settime.cmd \
	settime.lst \
	settime.z \
	utility.dsk \
	utility.jcl \
	cpmutil.dsk \
	xtrs.txt \
	xtrsemt.ccc \
	xtrsemt.h \
	xtrshard.dct \
	xtrshard.lst \
	xtrshard.z \
	xtrsmous.cmd \
	xtrsmous.lst \
	xtrsmous.z

Z80CODE = export.cmd import.cmd settime.cmd xtrsmous.cmd \
	xtrs8.dct xtrshard.dct \
	fakerom.hex xtrsrom4p.hex

MANSOURCES = cassette.man \
	cmddump.man \
	hex2cmd.man \
	mkdisk.man \
	xtrs.man

MANPAGES = xtrs.txt mkdisk.txt cassette.txt cmddump.txt hex2cmd.txt 

PROGS = xtrs mkdisk hex2cmd cmddump

default: xtrs mkdisk hex2cmd cmddump manpages

manpages: $(MANPAGES)

z80code: $(Z80CODE)

# Local customizations for make variables are done in Makefile.local:
include Makefile.local

CFLAGS = $(DEBUG) $(ENDIAN) $(DEFAULT_ROM) $(READLINE) $(DISKDIR) $(IFLAGS) \
       $(APPDEFAULTS) -DKBWAIT -DHAVE_SIGIO
LIBS = $(XLIB) $(READLINELIBS) $(EXTRALIBS)

ZMACFLAGS = -h
.SUFFIXES:	.z .cmd .dct .man .txt .hex
.z.cmd:
	zmac $(ZMACFLAGS) $<
	hex2cmd $*.hex > $*.cmd
	rm -f $*.hex
.z.dct:
	zmac $(ZMACFLAGS) $<
	hex2cmd $*.hex > $*.dct
	rm -f $*.hex
.z.hex:
	zmac $(ZMACFLAGS) $<
.man.txt:
	nroff -man -c -Tascii $< | colcrt - | cat -s > $*.txt

xtrs:		$(OBJECTS)
		$(CC) $(LDFLAGS) -o xtrs $(OBJECTS) $(LIBS)

compile_rom:	$(CR_OBJECTS)
		$(CC) -o compile_rom $(CR_OBJECTS)

trs_rom1.c:	compile_rom $(BUILT_IN_ROM)
		./compile_rom 1 $(BUILT_IN_ROM) > trs_rom1.c

trs_rom3.c:	compile_rom $(BUILT_IN_ROM3)
		./compile_rom 3 $(BUILT_IN_ROM3) > trs_rom3.c

trs_rom4p.c:	compile_rom $(BUILT_IN_ROM4P)
		./compile_rom 4p $(BUILT_IN_ROM4P) > trs_rom4p.c

mkdisk:		$(MD_OBJECTS)
		$(CC) -o mkdisk $(MD_OBJECTS)

hex2cmd:	$(HC_OBJECTS)
		$(CC) -o hex2cmd $(HC_OBJECTS)

cmddump:	$(CD_OBJECTS)
		$(CC) -o cmddump $(CD_OBJECTS)

saber_src:
		#ignore SIGIO
		#load $(LDFLAGS) $(CFLAGS) $(SOURCES) $(LIBS)

tar:		$(SOURCES) $(HEADERS)
		tar cvf xtrs.tar $(SOURCES) $(HEADERS)  $(MANSOURCES) $(MISC)
		rm -f xtrs.tar.Z
		compress xtrs.tar

clean:
		rm -f $(OBJECTS) $(MD_OBJECTS) $(CR_OBJECTS) $(HC_OBJECTS) \
			$(CD_OBJECTS) trs_rom*.c *~ \
			$(PROGS) compile_rom

veryclean: clean
		rm -f $(Z80CODE) $(MANPAGES) *.lst

link:	
		rm -f xtrs
		make xtrs

install: install-progs install-man

install-progs: $(PROGS)
	$(INSTALL) -c -m 755 $(PROGS) $(BINDIR)

install-man: $(MANPAGES)
	$(INSTALL) -c -m 644 xtrs.man $(MANDIR)/man1/xtrs.1
	$(INSTALL) -c -m 644 cassette.man $(MANDIR)/man1/cassette.1
	$(INSTALL) -c -m 644 mkdisk.man $(MANDIR)/man1/mkdisk.1
	$(INSTALL) -c -m 644 cmddump.man $(MANDIR)/man1/cmddump.1
	$(INSTALL) -c -m 644 hex2cmd.man $(MANDIR)/man1/hex2cmd.1

depend:
	makedepend -- $(CFLAGS) -- $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
