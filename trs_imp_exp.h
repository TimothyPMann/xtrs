/* 
 * Copyright (c) 1996-2018, Timothy P. Mann
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * trs_imp_exp.h
 *
 * Features to make transferring files into and out of the emulator
 * (etc.) easier, using a set of emulator traps (instructions that
 * don't exist in a real Z80).
 *
 * ED20-ED21 reserved; used by Jeff Vavasour Model III/4 emulator
 *
 * ED28 emt_system
 *         Before, HL => shell command, null terminated
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 BC =  command exit status, normally 0 if OK
 *
 * ED29 emt_mouse
 *         Implements Goben/Reed-style mouse driver in one instruction.
 *         Documentation adapted from original Misosys Quarterly V.iii article.
 *         (Notes: 1. The emt does not affect registers that do not have
 *         documented return values. 2. Different versions of the native
 *         drivers varied in whether coordinate values ranged from 0 to
 *         XSIZE or 0 to XSIZE-1. This driver will return values up to
 *         XSIZE if XSIZE is odd, up to XSIZE-1 if XSIZE is even, thus
 *         finessing the issue. 3. The sensitivity value is ignored. 4. The 
 *         mouse is assumed to have 3 buttons.  5. The set operations always
 *         return success.)
 *  
 *      GET MOUSE CURSOR POSITION
 *         Before, B  = 1
 *         After,  HL = mouse cursor X value (0 to XSIZE-1)
 *                 DE = mouse cursor Y value (0 to YSIZE-1)
 *                 A  = button status
 *                      Bit 0: Reset if right button pressed
 *                      Bit 1: Reset if middle button pressed (both buttons)
 *                      Bit 2: Reset if left button pressed
 *  
 *      SET MOUSE CURSOR POSITION
 *         Before, B  = 2
 *                 HL = X value (0 - XSIZE-1)
 *                 DE = Y value (0 - YSIZE-1)
 *         After,  AF = Z flag set if operation was successful
 *  
 *      GET SENSITIVITY AND MAXIMUM VALUES
 *         Before, B  = 3
 *         After,  HL = current X maximum (XSIZE)
 *                 DE = current Y maximum (YSIZE)
 *                 A  = current sensitivity (0 - 3)
 *  
 *      SET SENSITIVITY AND MAXIMUM VALUES
 *         Before, B =  4
 *                 HL = new X maximum (XSIZE)
 *                 DE = new Y maximum (YSIZE)
 *                 C  = sensitivity (0 - 3); 3 is most sensitive
 *         After,  AF = Z flag set if operation was successful
 *  
 *      GET MOUSE TYPE
 *         Before, B =  5
 *         After,  A =  0 if 2-button, 1 if 3-button
 *
 * ED2A emt_getddir
 *   Get the value of the -diskdir command-line parameter.
 *         Before, BC =  nbytes
 *                 HL => buffer
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 HL => same buffer, containing NUL-terminated pathname
 *                 BC =  strlen(buffer), 0xFFFF if error
 *
 * ED2B emt_setddir
 *   Change the value of the -diskdir command-line parameter.  Does not
 *   force a disk change; use emt_misc afterwards if you want one.
 *         Before, HL => path, null terminated
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *
 * ED2C-ED2E reserved
 *
 * ED2F emt_debug
 *   Enter zbx, the xtrs debugger.
 *
 * ED30 emt_open
 *         Before, HL => path, null terminated
 *                 BC =  oflag (use EO_ values defined below)
 *                 DE =  mode
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 DE =  fd, 0xFFFF if error
 *
 * ED31 emt_close
 *         Before, DE =  fd
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *
 * ED32 emt_read
 *         Before, BC =  nbytes
 *                 DE =  fd
 *                 HL => buffer
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 BC =  nbytes read, 0xFFFF if error
 *
 * ED33 emt_write
 *         Before, BC =  nbytes
 *                 DE =  fd
 *                 HL => buffer
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 BC =  nbytes written, 0xFFFF if error
 *
 * ED34 emt_lseek
 *         Before, BC =  whence
 *                 DE =  fd
 *                 HL => offset (8-byte little-endian integer)
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 HL => location in file, 0xFFFFFFFF if error
 *
 * ED35 emt_strerror
 *         Before, A  =  error number
 *                 HL => buffer for error string
 *                 BC =  buffer size
 *         After,  AF =  0 if OK, new error number if not (Z flag affected)
 *                 HL => same buffer, containing \r\0-terminated error string
 *                 BC =  strlen(buffer), 0xFFFF if error
 *
 * ED36 emt_time
 *         Before, A  =  0 for UTC (GMT), 1 for local time
 *         After,  BCDE= time in seconds since Jan 1, 1970 00:00:00
 *
 * ED37 emt_opendir
 *         Before, HL => path, null terminated
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 DE =  dirfd, 0xFFFF if error
 *
 * ED38 emt_closedir
 *         Before, DE =  dirfd
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *
 * ED39 emt_readdir
 *         Before, BC =  nbytes
 *                 DE =  dirfd
 *                 HL => buffer
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 HL => same buffer, containing NUL-terminated filename
 *                 BC =  strlen(buffer), 0xFFFF if error
 *
 * ED3A emt_chdir
 *   Warning: Changing the working directory will change where the disk*-*
 *   files are found on the next disk-change command, unless -diskdir has
 *   been specified as an absolute pathname.
 *         Before, HL => path, null terminated
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *
 * ED3B emt_getcwd
 *         Before, BC =  nbytes
 *                 HL => buffer
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 HL => same buffer, containing NUL-terminated pathname
 *                 BC =  strlen(buffer), 0xFFFF if error
 *
 * ED3C emt_misc
 *         Before, A  = function code, other registers as listed
 *         After, other registers as listed
 *
 *   Function codes:
 *     0 = disk change
 *         After, HL = disk change count (F7 presses + emt_misc 0 calls)
 *     1 = exit emulator
 *     2 = enter debugger (if active)
 *     3 = press reset button
 *     4 = query disk change count
 *         After, HL = disk change count (F7 presses + emt_misc 0 calls)
 *     5 = query model
 *         After, HL = model: 1, 3, 4, or 5 (=4P)
 *     6 = query disk size
 *         Before, BC = unit number, 0-7
 *         After,  HL = size, 5 or 8
 *     7 = set disk size
 *         Before, BC = unit number, 0-7
 *                 HL = new size, 5 or 8
 *     8 = query disk single/double step
 *         Before, BC = unit number, 0-7
 *         After,  HL = step, 1 or 2
 *     9 = set disk single/double step
 *         Before, BC = unit number, 0-7
 *                 HL = new step, 1 or 2
 *    10 = query graphics type
 *         Before, HL = 0 Radio Shack, 1 Micro Labs
 *    11 = set graphics type
 *         Before, HL = 0 Radio Shack, 1 Micro Labs
 *    12 = query delay
 *         After,  HL = delay
 *                 BC = autodelay flag (0 or 1)
 *    13 = set delay
 *         Before, HL = new delay
 *                 BC = autodelay flag (0 or 1)
 *    14 = query keystretch
 *         After,  HL = amount (in T-states)
 *    15 = set keystretch
 *         Before, HL = amount (in T-states)
 *    16 = query doubler
 *         After,  HL = 0 none, 1 Percom, 2 Tandy, 3 both
 *    17 = set doubler
 *         Before, HL = 0 none, 1 Percom, 2 Tandy, 3 both
 *    18 = reserved (formerly query SoundBlaster volume)
 *    19 = reserved (formerly set SoundBlaster volume)
 *    20 = query truedam flag
 *         After,  HL = 0 or 1
 *    21 = set truedam flag
 *         Before, HL = 0 or 1
 *
 * ED3D emt_ftruncate
 *         Before, DE =  fd
 *                 HL => offset (8-byte little-endian integer)
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *
 * ED3E emt_opendisk
 *   Similar to emt_open, except that (1) the path, if not absolute, is
 *   interpreted relative to -diskdir, (2) emt_closedisk must be
 *   used in place of emt_close.
 *         Before, HL => path, null terminated
 *                 BC =  oflag (use EO_ values defined below)
 *                 DE =  mode
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 DE =  fd, 0xFFFF if error
 *
 * ED3F emt_closedisk
 *   Similar to emt_close, but pairs with emt_opendisk.
 *         Before, DE =  fd, or -1 to close all fds opened with emt_opendisk
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *
 */

/* Minimal subset of standard O_ flags.  We have to define our own for
   portability; the numeric values differ amongst Unix
   implementations. */

#define EO_ACCMODE   03
#define EO_RDONLY    00
#define EO_WRONLY    01
#define EO_RDWR      02
#define EO_CREAT   0100
#define EO_EXCL    0200
#define EO_TRUNC  01000
#define EO_APPEND 02000

void do_emt_system();
void do_emt_getddir();
void do_emt_setddir();
void do_emt_mouse();
void do_emt_open();
void do_emt_close();
void do_emt_read();
void do_emt_write();
void do_emt_lseek();
void do_emt_strerror();
void do_emt_time();
void do_emt_opendir();
void do_emt_closedir();
void do_emt_readdir();
void do_emt_chdir();
void do_emt_getcwd();
void do_emt_misc();
void do_emt_ftruncate();
void do_emt_opendisk();
void do_emt_closedisk();
