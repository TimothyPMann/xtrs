
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
	trs_rom.o \
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

default: 	xtrs mkfloppy hex2cmd

# Local customizations for make variables are done in Makefile.local:
include Makefile.local

CFLAGS = $(DEBUG) $(ENDIAN) $(DEFAULT_ROM) $(READLINE) $(IFLAGS) \
	-DXTRASH -DKBWAIT -DKBQUEUE
#CFLAGS = $(DEBUG) $(ENDIAN) $(DEFAULT_ROM) $(READLINE) $(IFLAGS) \
#	-DXTRASH
LIBS = $(XLIB) $(READLINELIBS) $(EXTRALIBS)

xtrs:		$(OBJECTS)
		$(CC) $(LDFLAGS) -o xtrs $(OBJECTS) $(LIBS)

compile_rom:	$(CR_OBJECTS)
		$(CC) -o compile_rom $(CR_OBJECTS)

trs_rom.c:	compile_rom $(BUILT_IN_ROM)
		./compile_rom $(BUILT_IN_ROM) > trs_rom.c

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

xtrs.man.text:	xtrs.man
		tbl xtrs.man | nroff -man > xtrs.man.text

clean:
		rm -f $(OBJECTS) $(MF_OBJECTS) $(CR_OBJECTS) $(HC_OBJECTS) \
			*~ xtrs mkfloppy compile_rom hex2cmd

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

z80.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
z80.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h /usr/include/ctype.h
z80.o: /usr/include/features.h /usr/include/endian.h /usr/include/bytesex.h
z80.o: trs.h
main.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
main.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
main.o: /usr/include/ctype.h /usr/include/features.h /usr/include/endian.h
main.o: /usr/include/bytesex.h trs.h trs_disk.h
load_hex.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
load_hex.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
load_hex.o: /usr/include/ctype.h /usr/include/features.h
load_hex.o: /usr/include/endian.h /usr/include/bytesex.h
trs_memory.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
trs_memory.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
trs_memory.o: /usr/include/ctype.h /usr/include/features.h
trs_memory.o: /usr/include/endian.h /usr/include/bytesex.h trs.h
trs_memory.o: /usr/include/malloc.h
trs_memory.o: /usr/lib/gcc-lib/i386-linux/2.7.2.1/include/stddef.h
trs_memory.o: /usr/include/stdlib.h /usr/include/errno.h
trs_memory.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
trs_memory.o: /usr/include/alloca.h trs_disk.h
trs_keyboard.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
trs_keyboard.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
trs_keyboard.o: /usr/include/ctype.h /usr/include/features.h
trs_keyboard.o: /usr/include/endian.h /usr/include/bytesex.h trs.h
error.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
error.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
error.o: /usr/include/ctype.h /usr/include/features.h /usr/include/endian.h
error.o: /usr/include/bytesex.h
debug.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
debug.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
debug.o: /usr/include/ctype.h /usr/include/features.h /usr/include/endian.h
debug.o: /usr/include/bytesex.h /usr/include/malloc.h
debug.o: /usr/lib/gcc-lib/i386-linux/2.7.2.1/include/stddef.h
debug.o: /usr/include/stdlib.h /usr/include/errno.h
debug.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
debug.o: /usr/include/alloca.h /usr/include/signal.h /usr/include/sys/types.h
debug.o: /usr/include/linux/types.h /usr/include/linux/posix_types.h
debug.o: /usr/include/asm/posix_types.h /usr/include/asm/types.h
debug.o: /usr/include/sys/bitypes.h /usr/include/linux/signal.h
debug.o: /usr/include/asm/signal.h
dis.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
dis.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h /usr/include/ctype.h
dis.o: /usr/include/features.h /usr/include/endian.h /usr/include/bytesex.h
trs_io.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
trs_io.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
trs_io.o: /usr/include/ctype.h /usr/include/features.h /usr/include/endian.h
trs_io.o: /usr/include/bytesex.h trs.h trs_imp_exp.h trs_disk.h
trs_cassette.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
trs_cassette.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
trs_cassette.o: /usr/include/ctype.h /usr/include/features.h
trs_cassette.o: /usr/include/endian.h /usr/include/bytesex.h
trs_cassette.o: /usr/include/string.h
trs_cassette.o: /usr/lib/gcc-lib/i386-linux/2.7.2.1/include/stddef.h
trs_xinterface.o: /usr/include/stdio.h /usr/include/libio.h
trs_xinterface.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
trs_xinterface.o: /usr/include/fcntl.h /usr/include/features.h
trs_xinterface.o: /usr/include/sys/types.h /usr/include/linux/types.h
trs_xinterface.o: /usr/include/linux/posix_types.h
trs_xinterface.o: /usr/include/asm/posix_types.h /usr/include/asm/types.h
trs_xinterface.o: /usr/include/sys/bitypes.h /usr/include/gnu/types.h
trs_xinterface.o: /usr/include/linux/fcntl.h /usr/include/asm/fcntl.h
trs_xinterface.o: /usr/include/signal.h /usr/include/linux/signal.h
trs_xinterface.o: /usr/include/asm/signal.h /usr/include/sys/time.h
trs_xinterface.o: /usr/include/linux/time.h /usr/include/time.h
trs_xinterface.o: /usr/include/malloc.h
trs_xinterface.o: /usr/lib/gcc-lib/i386-linux/2.7.2.1/include/stddef.h
trs_xinterface.o: /usr/include/string.h /usr/include/stdlib.h
trs_xinterface.o: /usr/include/errno.h /usr/include/linux/errno.h
trs_xinterface.o: /usr/include/asm/errno.h /usr/include/alloca.h
trs_xinterface.o: /usr/X11/include/X11/Xlib.h /usr/X11/include/X11/X.h
trs_xinterface.o: /usr/X11/include/X11/Xfuncproto.h
trs_xinterface.o: /usr/X11/include/X11/Xosdefs.h /usr/X11/include/X11/Xatom.h
trs_xinterface.o: /usr/X11/include/X11/Xutil.h /usr/X11/include/X11/keysym.h
trs_xinterface.o: /usr/X11/include/X11/keysymdef.h
trs_xinterface.o: /usr/X11/include/X11/Xresource.h trs_iodefs.h trs.h z80.h
trs_xinterface.o: config.h /usr/include/ctype.h /usr/include/endian.h
trs_xinterface.o: /usr/include/bytesex.h
trs_chars.o: trs_iodefs.h
trs_printer.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
trs_printer.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
trs_printer.o: /usr/include/ctype.h /usr/include/features.h
trs_printer.o: /usr/include/endian.h /usr/include/bytesex.h trs.h
trs_disk.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
trs_disk.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
trs_disk.o: /usr/include/ctype.h /usr/include/features.h
trs_disk.o: /usr/include/endian.h /usr/include/bytesex.h trs.h trs_disk.h
trs_disk.o: /usr/include/sys/time.h /usr/include/linux/types.h
trs_disk.o: /usr/include/linux/posix_types.h /usr/include/asm/posix_types.h
trs_disk.o: /usr/include/asm/types.h /usr/include/linux/time.h
trs_disk.o: /usr/include/time.h /usr/include/sys/types.h
trs_disk.o: /usr/include/sys/bitypes.h /usr/include/sys/stat.h
trs_disk.o: /usr/include/linux/stat.h
trs_interrupt.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
trs_interrupt.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
trs_interrupt.o: /usr/include/ctype.h /usr/include/features.h
trs_interrupt.o: /usr/include/endian.h /usr/include/bytesex.h trs.h
trs_interrupt.o: /usr/include/sys/time.h /usr/include/linux/types.h
trs_interrupt.o: /usr/include/linux/posix_types.h
trs_interrupt.o: /usr/include/asm/posix_types.h /usr/include/asm/types.h
trs_interrupt.o: /usr/include/linux/time.h /usr/include/time.h
trs_interrupt.o: /usr/include/sys/types.h /usr/include/sys/bitypes.h
trs_interrupt.o: /usr/include/signal.h /usr/include/linux/signal.h
trs_interrupt.o: /usr/include/asm/signal.h
trs_imp_exp.o: /usr/include/stdio.h /usr/include/libio.h
trs_imp_exp.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
trs_imp_exp.o: /usr/include/errno.h /usr/include/features.h
trs_imp_exp.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
trs_imp_exp.o: /usr/include/signal.h /usr/include/sys/types.h
trs_imp_exp.o: /usr/include/linux/types.h /usr/include/linux/posix_types.h
trs_imp_exp.o: /usr/include/asm/posix_types.h /usr/include/asm/types.h
trs_imp_exp.o: /usr/include/sys/bitypes.h /usr/include/linux/signal.h
trs_imp_exp.o: /usr/include/asm/signal.h trs_imp_exp.h
compile_rom.o: z80.h config.h /usr/include/stdio.h /usr/include/libio.h
compile_rom.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
compile_rom.o: /usr/include/ctype.h /usr/include/features.h
compile_rom.o: /usr/include/endian.h /usr/include/bytesex.h
mkfloppy.o: /usr/include/stdio.h /usr/include/libio.h
mkfloppy.o: /usr/include/_G_config.h /usr/include/sys/cdefs.h
cmd.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
cmd.o: /usr/include/sys/cdefs.h
hex2cmd.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
hex2cmd.o: /usr/include/sys/cdefs.h cmd.h
