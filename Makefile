
#
# Makefile for xtrs, the TRS-80 emulator.
#

OBJECTS = \
	z80.o \
	main.o \
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
	trs_disk.o \
	trs_interrupt.o \
	trs_imp_exp.o

CR_OBJECTS = \
	compile_rom.o \
	load_hex.o

MD_OBJECTS = \
	mkdisk.o

HC_OBJECTS = \
	cmd.o \
	load_hex.o \
	hex2cmd.o

SOURCES = \
	z80.c \
	main.c \
	load_hex.c \
	trs_memory.c \
	trs_keyboard.c \
	error.c \
	debug.c \
	dis.c \
	trs_io.c \
	trs_cassette.c \
	trs_xinterface.c \
	trs_chars.c \
	trs_printer.c \
	trs_disk.c \
	trs_interrupt.c \
	trs_imp_exp.c

CR_SOURCES = \
	compile_rom.c

MD_SOURCES = \
	mkdisk.c

HC_SOURCES = \
	cmd.c \
	hex2cmd.c

HEADERS = \
	config.h \
	z80.h \
	trs.h \
	trs_iodefs.h \
	trs_disk.h \
	trs_imp_exp.h \
	cmd.h

MISC = \
	README \
	Makefile \
	Makefile.local \
	cassette \
	xtrs.man \
	xtrs.man.txt

default:	xtrs mkdisk hex2cmd xtrs.man.txt mkdisk.man.txt

# Local customizations for make variables are done in Makefile.local:
include Makefile.local

CFLAGS = $(DEBUG) $(ENDIAN) $(DEFAULT_ROM) $(READLINE) $(DISKDIR) $(IFLAGS) \
	-DKBWAIT -DHAVE_SIGIO
LIBS = $(XLIB) $(READLINELIBS) $(EXTRALIBS)

.SUFFIXES:	.z .cmd .dct
.z.cmd:
	zmac $<
	hex2cmd $*.hex > $*.cmd
.z.dct:
	zmac $<
	hex2cmd $*.hex > $*.dct

xtrs:		$(OBJECTS)
		$(CC) $(LDFLAGS) -o xtrs $(OBJECTS) $(LIBS)

compile_rom:	$(CR_OBJECTS)
		$(CC) -o compile_rom $(CR_OBJECTS)

trs_rom1.c:	compile_rom $(BUILT_IN_ROM)
		./compile_rom 1 $(BUILT_IN_ROM) > trs_rom1.c

trs_rom3.c:	compile_rom $(BUILT_IN_ROM3)
		./compile_rom 3 $(BUILT_IN_ROM3) > trs_rom3.c

mkdisk:		$(MD_OBJECTS)
		$(CC) -o mkdisk $(MD_OBJECTS)

hex2cmd:	$(HC_OBJECTS)
		$(CC) -o hex2cmd $(HC_OBJECTS)

saber_src:
		#ignore SIGIO
		#load $(LDFLAGS) $(CFLAGS) $(SOURCES) $(LIBS)

tar:		$(SOURCES) $(CR_SOURCES) $(MF_SOURCES) $(HEADERS)
		tar cvf xtrs.tar $(SOURCES) $(CR_SOURCES) $(MF_SOURCES) \
			$(HEADERS) $(MISC)
		rm -f xtrs.tar.Z
		compress xtrs.tar

xtrs.man.txt:	xtrs.man
		nroff -man xtrs.man > xtrs.man.txt

mkdisk.man.txt:	mkdisk.man
		nroff -man mkdisk.man > mkdisk.man.txt

clean:
		rm -f $(OBJECTS) $(MF_OBJECTS) $(CR_OBJECTS) $(HC_OBJECTS) \
			*~ xtrs mkdisk compile_rom hex2cmd trs_rom*.c

link:	
		rm -f xtrs
		make xtrs

install:
		install -c -m 555 xtrs $(BINDIR)
		install -c -m 444 xtrs.man $(MANDIR)/man1/xtrs.1

depend:
	makedepend -- $(CFLAGS) -- \
		$(SOURCES) $(CR_SOURCES) $(MF_SOURCES) $(HC_SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
