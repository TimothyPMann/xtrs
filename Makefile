
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

MF_OBJECTS = \
	mkfloppy.o

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

MF_SOURCES = \
	mkfloppy.c

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
	xtrs.man.text

default:	xtrs mkfloppy hex2cmd xtrs.man.txt

# Local customizations for make variables are done in Makefile.local:
include Makefile.local

CFLAGS = $(DEBUG) $(ENDIAN) $(DEFAULT_ROM) $(READLINE) $(IFLAGS) \
	-DKBWAIT -DHAVE_SIGIO
LIBS = $(XLIB) $(READLINELIBS) $(EXTRALIBS)

.SUFFIXES:	.z .cmd
.z.cmd:
	zmac $<
	hex2cmd $*.hex > $*.cmd

xtrs:		$(OBJECTS)
		$(CC) $(LDFLAGS) -o xtrs $(OBJECTS) $(LIBS)

compile_rom:	$(CR_OBJECTS)
		$(CC) -o compile_rom $(CR_OBJECTS)

trs_rom1.c:	compile_rom $(BUILT_IN_ROM)
		./compile_rom 1 $(BUILT_IN_ROM) > trs_rom1.c

trs_rom3.c:	compile_rom $(BUILT_IN_ROM3)
		./compile_rom 3 $(BUILT_IN_ROM3) > trs_rom3.c

mkfloppy:	$(MF_OBJECTS)
		$(CC) -o mkfloppy $(MF_OBJECTS)

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
		tbl xtrs.man | nroff -man > xtrs.man.txt

clean:
		rm -f $(OBJECTS) $(MF_OBJECTS) $(CR_OBJECTS) $(HC_OBJECTS) \
			*~ xtrs mkfloppy compile_rom hex2cmd trs_rom*.c

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

z80.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
z80.o: /usr/include/ctype.h trs.h
main.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
main.o: /usr/include/ctype.h trs.h trs_disk.h
load_hex.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
load_hex.o: /usr/include/ctype.h
trs_memory.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
trs_memory.o: /usr/include/ctype.h trs.h /usr/include/malloc.h
trs_memory.o: /usr/include/stdlib.h trs_disk.h
trs_keyboard.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
trs_keyboard.o: /usr/include/ctype.h trs.h
error.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
error.o: /usr/include/ctype.h
debug.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
debug.o: /usr/include/ctype.h /usr/include/malloc.h /usr/include/stdlib.h
debug.o: /usr/include/signal.h /usr/include/machine/signal.h
dis.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
dis.o: /usr/include/ctype.h
trs_io.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
trs_io.o: /usr/include/ctype.h trs.h trs_imp_exp.h trs_disk.h
trs_cassette.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
trs_cassette.o: /usr/include/ctype.h /usr/include/string.h
trs_cassette.o: /usr/include/sys/types.h
trs_xinterface.o: /usr/include/stdio.h /usr/include/standards.h
trs_xinterface.o: /usr/include/fcntl.h /usr/include/sys/fcntl.h
trs_xinterface.o: /usr/include/sys/types.h /usr/include/signal.h
trs_xinterface.o: /usr/include/machine/signal.h /usr/include/sys/time.h
trs_xinterface.o: /usr/include/sys/limits.h /usr/include/sys/signal.h
trs_xinterface.o: /usr/include/sys/time.h /usr/include/malloc.h
trs_xinterface.o: /usr/include/string.h /usr/include/stdlib.h
trs_xinterface.o: /usr/include/X11/Xlib.h /usr/include/X11/X.h
trs_xinterface.o: /usr/include/X11/Xfuncproto.h /usr/include/X11/Xosdefs.h
trs_xinterface.o: /usr/include/stddef.h /usr/include/X11/Xatom.h
trs_xinterface.o: /usr/include/X11/Xutil.h /usr/include/X11/keysym.h
trs_xinterface.o: /usr/include/X11/keysymdef.h /usr/include/X11/Xresource.h
trs_xinterface.o: trs_iodefs.h trs.h z80.h config.h /usr/include/ctype.h
trs_chars.o: trs_iodefs.h
trs_printer.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
trs_printer.o: /usr/include/ctype.h trs.h
trs_disk.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
trs_disk.o: /usr/include/ctype.h trs.h trs_disk.h /usr/include/sys/time.h
trs_disk.o: /usr/include/sys/limits.h /usr/include/sys/types.h
trs_disk.o: /usr/include/sys/signal.h /usr/include/sys/time.h
trs_disk.o: /usr/include/sys/stat.h /usr/include/sys/mode.h
trs_interrupt.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
trs_interrupt.o: /usr/include/ctype.h trs.h /usr/include/sys/time.h
trs_interrupt.o: /usr/include/sys/limits.h /usr/include/sys/types.h
trs_interrupt.o: /usr/include/sys/signal.h /usr/include/sys/time.h
trs_interrupt.o: /usr/include/signal.h /usr/include/machine/signal.h
trs_imp_exp.o: /usr/include/stdio.h /usr/include/standards.h
trs_imp_exp.o: /usr/include/errno.h /usr/include/signal.h
trs_imp_exp.o: /usr/include/machine/signal.h trs_imp_exp.h
compile_rom.o: z80.h config.h /usr/include/stdio.h /usr/include/standards.h
compile_rom.o: /usr/include/ctype.h
mkfloppy.o: /usr/include/stdio.h /usr/include/standards.h
cmd.o: /usr/include/stdio.h /usr/include/standards.h
hex2cmd.o: /usr/include/stdio.h /usr/include/standards.h cmd.h
