
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
	trs_rom.o

CR_OBJECTS = \
	compile_rom.o \
	load_hex.o

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
	trs_printer.c

CR_SOURCES = \
	compile_rom.c

HEADERS = \
	config.h \
	z80.h \
	trs.h \
	trs_iodefs.h

MISC = \
	README \
	Makefile \
	Makefile.local \
	cassette \
	xtrs.man \
	xtrs.man.text

default: 	xtrs

# Local customizations for make variables are done in Makefile.local:
include Makefile.local

CFLAGS = $(DEBUG) $(ENDIAN) $(DEFAULT_ROM) $(READLINE) $(IFLAGS) -DXTRASH
LIBS = $(XLIB) $(READLINELIBS) $(EXTRALIBS)

xtrs:		$(OBJECTS)
		$(CC) $(LDFLAGS) -o xtrs $(OBJECTS) $(LIBS)

compile_rom:	$(CR_OBJECTS)
		$(CC) -o compile_rom $(CR_OBJECTS)

trs_rom.c:	compile_rom $(BUILT_IN_ROM)
		compile_rom $(BUILT_IN_ROM) > trs_rom.c

saber_src:
		#ignore SIGIO
		#load $(LDFLAGS) $(CFLAGS) $(SOURCES) $(LIBS)

tar:		$(SOURCES) $(CR_SOURCES) $(HEADERS)
		tar cvf xtrs.tar $(SOURCES) $(CR_SOURCES) $(HEADERS) $(MISC)
		rm -f xtrs.tar.Z
		compress xtrs.tar

xtrs.man.text:	xtrs.man
		tbl xtrs.man | nroff -man > xtrs.man.text

clean:
		rm -f $(OBJECTS) *~ xtrs

link:	
		rm -f xtrs
		make xtrs

install:
		install -c -m 555 xtrs $(BINDIR)
		install -c -m 444 xtrs.man $(MANDIR)/man1/xtrs.1
