/*
 * Copyright (C) 1992 Clarendon Hill Software.
 *
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software, provided this copyright notice is retained. 
 *
 * This software is provided "as is" without any expressed or implied
 * warranty.  If this software brings on any sort of damage -- physical,
 * monetary, emotional, or brain -- too bad.  You've got no one to blame
 * but yourself. 
 *
 * The software may be modified for your own purposes, but modified versions
 * must retain this notice.
 */

/*
   Modified by Timothy Mann, 1996
   Last modified on Sat May  6 14:53:21 PDT 2000 by mann
*/

/*
 * This module implements cassette I/O, game sound, and Orchestra
 *  85/90 sound.  "Game sound" is defined as output to the cassette
 *  port when the cassette motor is off, or output to the Model III/4
 *  sound option card (a 1-bit DAC).
 * 
 * Compile time options:
 *
 * HAVE_OSS  You have the Open Sound System.  If this is set, cassettes
 *   can be read/written directly from/to /dev/dsp.  This should work on
 *   Linux and other systems with OSS.
 *
 * OSS_SOUND  Game sound is emulated using the Open Sound System.
 *   HAVE_OSS must also be set.
 *
 * SB_SOUND  Game sound is emulated by read/writing SoundBlaster
 *   hardware registers directly.  This probably works only on Linux,
 *   and of course only with true SoundBlaster-compatible hardware.
 *   Requires root privileges and the -sb command line option.  If you
 *   define both SB_SOUND and OSS_SOUND, then SB_SOUND will be used if
 *   the -sb command line option is given, OSS_SOUND otherwise.
 *   OSS_SOUND seems to work much better than SB_SOUND now, so
 *   SB_SOUND is off by default and -sb has been removed from the
 *   man page.  In case you turn it on, here is how it works:
 *  
 * -sb portbase,vol
 *   Enable sound support using direct I/O to a SoundBlaster with I/O
 *   port base at portbase, playing sounds at vol percent of
 *   maximum volume.  A typical setting would be -sb 0x220,30.
 *   Fabio Ferrari contributed the SB_SOUND implementation.  
 */

#if __linux
#define HAVE_OSS 1
#define OSS_SOUND 1
#endif

/*#define CASSDEBUG 1*/
/*#define CASSDEBUG2 1*/
/*#define CASSDEBUG3 1*/
/*#define CASSDEBUG4 1*/

#include "trs.h"
#include "z80.h"
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#if SB_SOUND
/*#include <sys/io.h>  delete this line if it gives you a compile error */
#include <asm/io.h>
#endif

#if HAVE_OSS
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#endif

#define CLOSE		0
#define READ		1
#define WRITE		2
#define SOUND           3  /* used for OSS_SOUND only */
#define ORCH90          4  /* used for OSS_SOUND only */
#define FAILED          5

#define CAS_FORMAT         1  /* recovered bit/byte stream */
#define CPT_FORMAT         2  /* cassette pulse train w/ exact timing */
#define WAV_FORMAT         3  /* wave file */
#define DIRECT_FORMAT      4  /* direct to sound card */
#define DEBUG_FORMAT       5  /* like cpt but in ASCII */
static char *format_name[] = {
  NULL, "cas", "cpt", "wav", "direct", "debug" };
#define DEFAULT_SAMPLE_RATE 44100  /* samples/sec to use for .wav files */
#define NOISE_FLOOR 64

#define CONTROL_FILENAME	".cassette.ctl"
#define DEFAULT_FILENAME	"cassette.cas"
#define DSP_FILENAME            "/dev/dsp"  /* for sound output */
#define DEFAULT_FORMAT		CAS_FORMAT

#define FLUSH -500  /* special fake signal value used when turning off motor */

static char cassette_filename[256];
static int cassette_position;
static int cassette_format;
static int cassette_state = CLOSE;
static int cassette_motor = 0;
static FILE *cassette_file;
static float cassette_avg;
static float cassette_env;
static int cassette_noisefloor;
static int cassette_sample_rate;
int cassette_default_sample_rate = DEFAULT_SAMPLE_RATE;
static int cassette_stereo = 0;
#if HAVE_OSS
static int cassette_afmt = AFMT_U8;
#endif

/* For bit-level emulation */
static tstate_t cassette_transition;
static tstate_t last_sound;
static tstate_t cassette_firstoutread;
static int cassette_value, cassette_next, cassette_flipflop;
static int cassette_lastnonzero;
static int cassette_transitionsout;
static unsigned long cassette_delta;
static float cassette_roundoff_error;

/* For bit/byte conversion (.cas file i/o) */
static int cassette_byte;
static int cassette_bitnumber;
static int cassette_pulsestate;
#define SPEED_500     0
#define SPEED_1500    1
#define SPEED_250     2
int cassette_speed = SPEED_500;

/* Pulse shapes for conversion from .cas on input */
#define CAS_MAXSTATES 8
struct {
  int delta_us;
  int next;
} pulse_shape[3][2][CAS_MAXSTATES] = {
  {{
    /* Low-speed zero: clock 1 data 0 */
    { 0,    1 },
    { 128,  2 },
    { 128,  0 },
    { 1871, 0 },  /* normally 1757; 1871 after 8th bit */
    { -1,  -1 }
  }, {
    /* Low-speed one: clock 1 data 1 */
    { 0,    1 },
    { 128,  2 },
    { 128,  0 },
    { 748,  1 },
    { 128,  2 },
    { 128,  0 },
    { 860, 0 },  /* normally 748; 860 after 8th bit; 1894 after a5 sync */
    { -1,  -1 }
  }}, {{
    /* High-speed zero: wide pulse */
    { 0,    1 },
    { 376,  2 },
    { 376,  1 },
    { -1,  -1 }
  }, {
    /* High-speed one: narrow pulse */
    { 0,    1 },
    { 188,  2 },
    { 188,  1 },
    { -1,  -1 }
  }}, {{
    /* Level I zero: clock 1 data 0 */
    { 0,    1 },
    { 125,  2 },
    { 125,  0 },
    { 3568, 0 },
    { -1,  -1 }
  }, {
    /* Level I one: clock 1 data 1 */
    { 0,    1 },
    { 128,  2 },
    { 128,  0 },
    { 1673, 1 },
    { 128,  2 },
    { 128,  0 },
    { 1673, 0 },
    { -1,  -1 }
  }}    
};

/* States and thresholds for conversion to .cas on output */
#define ST_INITIAL   0
#define ST_500GOTCLK 1
#define ST_500GOTDAT 2
#define ST_1500      3
#define ST_250       4
#define ST_250GOTCLK 5
#define ST_250GOTDAT 6
#define ST_500THRESH  1250.0 /* us threshold between 0 and 1 */
#define ST_1500THRESH  282.0 /* us threshold between 1 and 0 */
#define ST_250THRESH  2500.0 /* us threshold between 0 and 1 */

#define DETECT_250    1200.0 /* detect level 1 input routine */

/* Values for conversion to .wav on output */
/* Values in comments are from Model I technical manual.  Model III/4 are
   close though not quite the same, as one resistor in the network was
   changed; we ignore the difference.  Actually, we ignore more than
   that; we convert the values as if 0 were really halfway between
   high and low.  */
Uchar value_to_sample[] = { 127, /* 0.46 V */
			    254, /* 0.85 V */
			    0,   /* 0.00 V */
			    127, /* unused, but close to 0.46 V */
};

/* .wav file definitions */
#define WAVE_FORMAT_PCM (0x0001)
#define WAVE_FORMAT_MONO 1
#define WAVE_FORMAT_STEREO 2
#define WAVE_FORMAT_8BIT 8
#define WAVE_FORMAT_16BIT 16
#define WAVE_RIFFSIZE_OFFSET 0x04
#define WAVE_RIFF_OFFSET 0x08
#define WAVE_DATAID_OFFSET 0x24
#define WAVE_DATASIZE_OFFSET 0x28
#define WAVE_DATA_OFFSET 0x2c
static long wave_dataid_offset = WAVE_DATAID_OFFSET;
static long wave_datasize_offset = WAVE_DATASIZE_OFFSET;
static long wave_data_offset = WAVE_DATA_OFFSET;

/* Orchestra 80/85/90 stuff */
static int orch90_left = 128, orch90_right = 128;

#if SB_SOUND
/* ioport of the SoundBlaster command register. 0 means none */
static unsigned char sb_cassette_volume[4];
static unsigned char sb_sound_volume[2];
#endif /*SB_SOUND*/
static unsigned int sb_address=0;
static int sb_volume = 0;

/* Put a 2-byte quantity to a file in little-endian order */
/* Return -1 on error, 0 otherwise */
static int
put_twobyte(Ushort n, FILE* f)
{
  int c;
  struct twobyte *p = (struct twobyte *) &n;
  c = putc(p->low, f);
  if (c == -1) return c;
  c = putc(p->high, f);
  if (c == -1) return c;
  return 0;
}

/* Put a 4-byte quantity to a file in little-endian order */
/* Return -1 on error, 0 otherwise */
static int
put_fourbyte(Uint n, FILE* f)
{
  int c;
  struct fourbyte *p = (struct fourbyte *) &n;
  c = putc(p->byte0, f);
  if (c == -1) return c;
  c = putc(p->byte1, f);
  if (c == -1) return c;
  c = putc(p->byte2, f);
  if (c == -1) return c;
  c = putc(p->byte3, f);
  if (c == -1) return c;
  return 0;
}

/* Get a 2-byte quantity from a file in little-endian order */
/* Return -1 on error, 0 otherwise */
static int
get_twobyte(Ushort *pp, FILE* f)
{
  int c;
  struct twobyte *p = (struct twobyte *) pp;
  c = getc(f);
  if (c == -1) return c;
  p->low = c;
  c = getc(f);
  if (c == -1) return c;
  p->high = c;
  return 0;
}

/* Get a 4-byte quantity from a file in little-endian order */
/* Return -1 on error, 0 otherwise */
static int
get_fourbyte(Uint *pp, FILE* f)
{
  int c;
  struct fourbyte *p = (struct fourbyte *) pp;
  c = getc(f);
  if (c == -1) return c;
  p->byte0 = c;
  c = getc(f);
  if (c == -1) return c;
  p->byte1 = c;
  c = getc(f);
  if (c == -1) return c;
  p->byte2 = c;
  c = getc(f);
  if (c == -1) return c;
  p->byte3 = c;
  return 0;
}

/* Output an 8-byte unsigned sample, if necessary converting to a
 * different sample format.  */
static void
put_sample(Uchar sample, int convert, FILE* f)
{
  if (convert) {
#if HAVE_OSS
    switch (cassette_afmt) {
    case AFMT_U8:
      putc(sample, f);
      break;
    case AFMT_S16_LE:
      put_twobyte((sample << 8) - 0x8000, f);
      break;
    default:
      error("sample format 0x%x not supported", cassette_afmt);
      break;
    }
    return;
#endif
  }
  putc(sample, f);
}    

/* Get an 8-byte unsigned sample, if necessary converting from a
 * different sample format and/or reducing stereo to mono.  */
static int
get_sample(int convert, FILE* f)
{
#if HAVE_OSS
  if (convert) {
    int ret = 0;
    short s = 0;
    switch (cassette_afmt) {
    case AFMT_U8:
      ret = getc(f);
      break;
    case AFMT_S16_LE:
      ret = get_twobyte(&s, cassette_file);      
      if (ret == EOF) break;
      ret = ((s + 0x8000) >> 8) & 0xff;
      break;
    default:
      error("sample format 0x%x not supported", cassette_afmt);
      break;
    }
    return ret;
  }
#endif
  return getc(f);
}

/* Write a new .wav file header to a file.  Return -1 on error. */
static int
create_wav_header(FILE *f)
{
  Uint field;
  /* Chunk sizes don't count the 4-byte chunk type name nor the 4-byte
     size field itself.  The RIFF chunk is the whole file, so its size
     is the actual length of the file minus WAVE_RIFF_OFFSET (=8).
     The data chunk is the actual sample data, so its size is the size
     of the file minus wave_data_offset. */

  wave_dataid_offset = WAVE_DATAID_OFFSET;
  wave_datasize_offset = WAVE_DATASIZE_OFFSET;
  wave_data_offset = WAVE_DATA_OFFSET;
  if (cassette_position < wave_data_offset) {
    cassette_position = wave_data_offset;
  }

  if (fputs("RIFF", f) < 0) return -1;
  if (put_fourbyte(0, f) < 0) return -1; /* RIFF chunk size */
  if (fputs("WAVEfmt ", f) < 0) return -1;
  if (put_fourbyte(16, f) < 0) return -1; /* fmt chunk size */
  if (put_twobyte(WAVE_FORMAT_PCM, f) < 0) return -1;
  if (put_twobyte(WAVE_FORMAT_MONO, f) < 0) return -1;
  if (put_fourbyte(cassette_sample_rate, f) < 0) return -1;
  field = (WAVE_FORMAT_MONO * cassette_sample_rate * WAVE_FORMAT_8BIT/8);
  if (put_fourbyte(field, f) < 0) return -1;
  field = (WAVE_FORMAT_MONO * WAVE_FORMAT_8BIT/8);
  if (put_twobyte(field, f) < 0) return -1;
  if (put_twobyte(WAVE_FORMAT_8BIT, f) < 0) return -1; /* end of fmt chunk */
  if (fputs("data", f) < 0) return -1;
  if (put_fourbyte(0, f) < 0) return -1; /* size of data chunk payload */
  /* payload starts here */
  return 0;
}

/* Error message generator */
static int
check_chunk_id(char *expected, FILE* f)
{
  char c4[5];
  c4[4] = '\0';
  if (fread(c4, 4, 1, f) != 1) return -1;
  if (strcmp(c4, expected) != 0) {
    error("unusable wav file: expected chunk id '%s', got '%s'", expected, c4);
    return -1;
  }
  return 0;
}

/* Parse a .wav file's RIFF header.  We don't understand much about
   the RIFF format, so we might fail on valid .WAV files.  For now,
   that's just tough.  Try running the file through sox to convert it
   to something more vanilla. */
static int
parse_wav_header(FILE *f)
{
  Uint n4;
  Uint fmt_size;
  Ushort n2, expect2;

  if (check_chunk_id("RIFF", f) < 0) return -1;
  if (get_fourbyte(&n4, f) < 0) return -1; /* ignore this field */
  if (check_chunk_id("WAVE", f) < 0) return -1;
  if (check_chunk_id("fmt ", f) < 0) return -1;
  if (get_fourbyte(&fmt_size, f) < 0) return -1;
  if (get_twobyte(&n2, f) < 0) return -1;
  if (n2 != WAVE_FORMAT_PCM) {
    error("unusable wav file: must be pcm");
    return -1;
  }
  if (get_twobyte(&n2, f) < 0) return -1;
  if (n2 != WAVE_FORMAT_MONO) {
    error("unusable wav file: must be mono");
    return -1;
  }
  if (get_fourbyte(&n4, f) < 0) return -1;
  cassette_sample_rate = n4;
  if (get_fourbyte(&n4, f) < 0) return -1; /* ignore this field */
  expect2 = WAVE_FORMAT_MONO * WAVE_FORMAT_8BIT/8;
  if (get_twobyte(&n2, f) < 0) return -1;
  if (n2 != expect2) {
    error("unusable wav file: must be %d bytes/sample", expect2);
    return -1;
  }
  expect2 = WAVE_FORMAT_8BIT;
  if (get_twobyte(&n2, f) < 0) return -1;
  if (n2 != expect2) {
    error("unusable wav file: must be %d bits/sample", expect2);
    return -1;
  }
  fmt_size -= 16;  /* size read so far */
  while (fmt_size-- > 0) getc(f); /* ignore additional */
  wave_dataid_offset = ftell(f);
  if (check_chunk_id("data", f) < 0) return -1;
  wave_datasize_offset = ftell(f);
  if (get_fourbyte(&n4, f) < 0) return -1; /* ignore this field */
  wave_data_offset = ftell(f);
  if (cassette_position < wave_data_offset) {
    cassette_position = wave_data_offset;
  }
  return 0;
}  

#if !USESOX
static int
set_audio_format(FILE *f, int state)
{
#if HAVE_OSS
  int audio_fd = fileno(f);
  int format, stereo, speed, req;
  req = format = AFMT_U8;  /* unsigned 8-bit */
  if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format)==-1) return -1;
  if (format != req) {
    req = format = AFMT_S16_LE; /* signed 16-bit little-endian */
    if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format)==-1) return -1;
    if (format != req) {
      error("requested audio format 0x%x, got 0x%x", req, format);
      errno = EINVAL;
      return -1;
    }
  }
  cassette_afmt = format;
  req = stereo = (state == ORCH90);
  if (ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo)==-1) return -1;
  if (req && !stereo) {
    error("requested stereo, got mono");
    errno = EINVAL;
    return -1;
  }
  cassette_stereo = stereo;
  req = speed = cassette_sample_rate;
  if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &speed)==-1) return -1;
  if (abs(speed - req) > req/20) {
    error("requested sample rate %d Hz, got %d Hz", req, speed);
    errno = EINVAL;
    return -1;
  }
#else
  /* Hope for the best.  Might work on Suns with /dev/audio (mu-law). */
#endif
  return 0;
}
#endif

static void get_control()
{
  FILE *f;

  f = fopen(CONTROL_FILENAME, "r");
  cassette_format = DEFAULT_FORMAT;
  if ((!f) ||
      (fscanf(f, "%s %d %d", cassette_filename,
	      &cassette_position, &cassette_format) < 2)) {
    error("can't read %s (%s);\n  cassette file will be: %s, format %s",
	  CONTROL_FILENAME, strerror(errno),
	  DEFAULT_FILENAME, format_name[DEFAULT_FORMAT]);
    strcpy(cassette_filename, DEFAULT_FILENAME);
    cassette_position = 0;
  }
  if (f) {
    fclose(f);
  }
}

static void put_control()
{
  FILE *f;

  f = fopen(CONTROL_FILENAME, "w");

  if (f) {
    trs_paused = 1;  /* disable speed measurement for this round */
    fprintf(f, "%s %d %d\n", cassette_filename, cassette_position,
	    cassette_format);
    fclose(f);
  }
}

/* Return value: 1 = already that state; 0 = state changed; -1 = failed */
static int assert_state(int state)
{
  if (cassette_state == state) {
    return 1;
  }
  if (cassette_state == FAILED && state != CLOSE) {
    return -1;
  }

#if CASSDEBUG
  debug("state %d -> %d\n", cassette_state, state);
#endif

  if (cassette_state == ORCH90) {
    trs_orch90_out(0, FLUSH);
  }

  if (cassette_state != CLOSE && cassette_state != FAILED) {
    if (cassette_format == DIRECT_FORMAT) {
#if USESOX
      pclose(cassette_file);
#else
      sigset_t set, oldset;
      sigemptyset(&set);
      sigaddset(&set, SIGALRM);
      sigaddset(&set, SIGIO);
      sigprocmask(SIG_BLOCK, &set, &oldset);
      trs_paused = 1;  /* disable speed measurement for this round */
      fclose(cassette_file);
      sigprocmask(SIG_SETMASK, &oldset, NULL);
#endif
      cassette_position = 0;
    } else {
      cassette_position = ftell(cassette_file);
      if (cassette_format == WAV_FORMAT && cassette_state == WRITE) {
	fseek(cassette_file, WAVE_RIFFSIZE_OFFSET, 0);
	put_fourbyte(cassette_position - WAVE_RIFF_OFFSET, cassette_file);
	fseek(cassette_file, wave_datasize_offset, 0);
	put_fourbyte(cassette_position - wave_data_offset, cassette_file);
      }
      fclose(cassette_file);
    }
    if (cassette_state != SOUND && cassette_state != ORCH90) {
      put_control();
    }
#if HAVE_OSS
    cassette_stereo = 0;
    cassette_afmt = AFMT_U8;
#endif
  }

  switch (state) {
  case READ:
    get_control();
    if (cassette_format == DIRECT_FORMAT) {
#if USESOX
      char command[256];
      cassette_sample_rate = cassette_default_sample_rate;
      sprintf(command,
	      "sox -t ossdsp -r %d -u -b /dev/dsp -t raw -r %d -u -b -",
	      cassette_sample_rate, cassette_sample_rate);
      cassette_file = popen(command, "r");
      if (cassette_file == NULL) {
	error("couldn't read from sound card: %s", strerror(errno));
	cassette_state = FAILED;
	return -1;
      }
#else
      cassette_file = fopen(cassette_filename, "r");
      if (cassette_file == NULL) {
	error("couldn't read %s: %s", cassette_filename, strerror(errno));
	cassette_state = FAILED;
	return -1;
      }
      /*setbuf(cassette_file, NULL);*/ /* seems no need for this */
      cassette_sample_rate = cassette_default_sample_rate;
      if (set_audio_format(cassette_file, state) < 0) {
	error("couldn't set audio format on %s: %s",
	      cassette_filename, strerror(errno));
	cassette_file = NULL;
	cassette_state = FAILED;
	return -1;
      }
#endif
    } else {
      cassette_file = fopen(cassette_filename, "r");
      if (cassette_format == WAV_FORMAT &&
	  cassette_file != NULL && parse_wav_header(cassette_file) < 0) {
	cassette_file = NULL;
      }
      if (cassette_file == NULL) {
	error("couldn't read %s: %s", cassette_filename, strerror(errno));
	cassette_state = FAILED;
	return -1;
      }
      fseek(cassette_file, cassette_position, 0);
    }
    break;

  case SOUND:
  case ORCH90:
  case WRITE:
    if (state == SOUND || state == ORCH90) {
      cassette_format = DIRECT_FORMAT;
      strcpy(cassette_filename, DSP_FILENAME);
    } else {
      get_control(state);
    }
    if (cassette_format == DIRECT_FORMAT) {
#if USESOX
      char command[256];
      cassette_sample_rate = cassette_default_sample_rate;
      sprintf(command, "sox -t raw -r %d -u -b -c %d - -t ossdsp /dev/dsp",
	      cassette_sample_rate, (state == ORCH90) ? 2 : 1);
      cassette_file = popen(command, "w");
#else
      cassette_sample_rate = cassette_default_sample_rate;
      cassette_file = fopen(cassette_filename, "w");
      if (cassette_file == NULL) {
	error("couldn't write %s: %s", cassette_filename, strerror(errno));
	cassette_state = FAILED;
	return -1;
      }
      setbuf(cassette_file, NULL); /* ??hangs on some OSS drivers */
#if OSS_SOUND && HAVE_OSS
      if (state == SOUND || state == ORCH90) {
	/*int arg = 0x7fff0008;*/ /* unlimited fragments of size (1 << 8) */
	int arg = 0x00200008; /* 32 fragments of size (1 << 8) */
	if (ioctl(fileno(cassette_file), SNDCTL_DSP_SETFRAGMENT, &arg) < 0) {
	  error("warning: couldn't set sound fragment size: %s",
		strerror(errno));
	}
      }
#endif
      if (set_audio_format(cassette_file, state) < 0) {
	error("couldn't set audio format on %s: %s",
	      cassette_filename, strerror(errno));
	cassette_file = NULL;
	cassette_state = FAILED;
	return -1;
      }	
#endif
    } else if (cassette_format == WAV_FORMAT) {
      cassette_file = fopen(cassette_filename, "r+");
      if (cassette_file == NULL) {
	cassette_sample_rate = cassette_default_sample_rate;
	cassette_file = fopen(cassette_filename, "w");
	if (cassette_file && create_wav_header(cassette_file) < 0) {
	  cassette_file = NULL;
	}
      } else {
	if (parse_wav_header(cassette_file) < 0) {
	  fclose(cassette_file);
	  cassette_file = NULL;
	}
      }
      if (cassette_file != NULL) {
	fseek(cassette_file, cassette_position, 0);
      }
    } else {
      cassette_file = fopen(cassette_filename, "r+");
      if (cassette_file == NULL) {
	cassette_file = fopen(cassette_filename, "w");
      }
      if (cassette_file != NULL) {
	fseek(cassette_file, cassette_position, 0);
      }
    }
    if (cassette_file == NULL) {
      error("couldn't write %s: %s", cassette_filename, strerror(errno));
      cassette_state = FAILED;
      return -1;
    }
    break;
  }
    
  cassette_state = state;
  return 0;
}


/* Record an output transition.
   value is either the new port value or FLUSH.
*/
static void
transition_out(int value)
{
  Uchar sample;
  long nsamples, delta_us;
  Ushort code;
  float ddelta_us;
  sigset_t set, oldset;

  cassette_transitionsout++;
  if (value != FLUSH && value == cassette_value) return;

  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);

  ddelta_us = (z80_state.t_count - cassette_transition) / z80_state.clockMHz
    - cassette_roundoff_error;

  switch (cassette_format) {
  case DEBUG_FORMAT:
    /* Print value and delta_us in ASCII for easier examination */
    if (value == FLUSH) value = cassette_value;
    delta_us = (unsigned long) (ddelta_us + 0.5);
    cassette_roundoff_error = delta_us - ddelta_us;
    fprintf(cassette_file, "%d %lu\n", value, delta_us);
    break;
    
  case CPT_FORMAT:
    /* Encode value and delta_us in two bytes if delta_us is small enough.
       Pack bits as ddddddddddddddvv and store this value in little-
       endian order. */
    if (value == FLUSH) value = cassette_value;
    delta_us = (unsigned long) (ddelta_us + 0.5);
    cassette_roundoff_error = delta_us - ddelta_us;
    if (delta_us < 0x3fff) {
      code = value | (delta_us << 2);
      put_twobyte(code, cassette_file);
    } else {
      /* Else write 0xffff escape code and encode in five bytes:
	 1-byte value, then 4-byte delta_us in little-endian order */
      put_twobyte(0xffff, cassette_file);
      putc(value, cassette_file);
      put_fourbyte(delta_us, cassette_file);
    }
    break;

  case WAV_FORMAT:
  case DIRECT_FORMAT:
#if OSS_SOUND && HAVE_OSS
    if (cassette_state == SOUND) {
      if (ddelta_us > 20000.0) {
	/* Truncate silent periods */
	ddelta_us = 20000.0;
	cassette_roundoff_error = 0.0;
      }
      if (trs_event_scheduled() == transition_out ||
	  trs_event_scheduled() == (trs_event_func) assert_state) {
	trs_cancel_event();
      }
      if (value == FLUSH) {
	trs_schedule_event((trs_event_func)assert_state, CLOSE, 5000000);
      } else {
	trs_schedule_event(transition_out, FLUSH,
			   (int)(25000 * z80_state.clockMHz));
      }
    }
#endif
    sample = value_to_sample[cassette_value];
    nsamples = (unsigned long)
      (ddelta_us / (1000000.0/cassette_sample_rate) + 0.5);
    if (nsamples == 0) nsamples = 1; /* always at least one sample */
    cassette_roundoff_error =
      nsamples * (1000000.0/cassette_sample_rate) - ddelta_us;
#if CASSDEBUG
    debug("%d %4lu %d -> %3lu\n", cassette_value,
	  z80_state.t_count - cassette_transition, value, nsamples);
#endif
    if (cassette_format == DIRECT_FORMAT && cassette_stereo) nsamples *= 2;
    while (nsamples-- > 0) {
      put_sample(sample, cassette_format == DIRECT_FORMAT, cassette_file);
    }
    if (value == FLUSH) {
      value = cassette_value;
#if OSS_SOUND && HAVE_OSS
      if (cassette_format == DIRECT_FORMAT) {
	ioctl(fileno(cassette_file), SNDCTL_DSP_POST, 0);
      }
      trs_restore_delay();
#endif
    }
    break;

  case CAS_FORMAT:
    if (value == FLUSH && cassette_bitnumber != 0) {
      putc(cassette_byte, cassette_file);
      cassette_byte = 0;
      break;
    }
    sample = 2; /* i.e., no bit */
    switch (cassette_pulsestate) {
    case ST_INITIAL:
      if (cassette_value == 2 && value == 0) {
	/* Low speed, end of first pulse.  Assume clock */
	cassette_pulsestate = ST_500GOTCLK;
      } else if (cassette_value == 2 && value == 1) {
	/* High speed, nothing interesting yet. */
	cassette_pulsestate = ST_1500;
      }
      break;

    case ST_500GOTCLK:
      if (cassette_value == 0 && value == 1) {
	/* Low speed, start of next pulse. */
	if (ddelta_us > ST_250THRESH) {
	  /* Oops, really ultra-low speed */
	  /* It's the next clock; bit was 0 */
	  sample = 0;
	  /* Watch for end of this clock */
	  cassette_pulsestate = ST_250;
	} else if (ddelta_us > ST_500THRESH) {
	  /* It's the next clock; bit was 0 */
	  sample = 0;
	  /* Watch for end of this clock */
	  cassette_pulsestate = ST_INITIAL;
	} else {
	  /* It's a data pulse; bit was 1 */
	  sample = 1;
	  /* Ignore the data pulse falling edge */
	  cassette_pulsestate = ST_500GOTDAT;
	}
      }
      break;
      
    case ST_500GOTDAT:
      if (cassette_value == 2 && value == 0) {
	/* End of data pulse; watch for end of next clock */
	cassette_pulsestate = ST_INITIAL;
      }
      break;

    case ST_1500:
      if (cassette_value == 1 && value == 2) {
	sample = (ddelta_us < ST_1500THRESH);
      }
      break;

    case ST_250:
      if (cassette_value == 2 && value == 0) {
	/* Ultra-low speed, end of first pulse.  Assume clock */
	cassette_pulsestate = ST_250GOTCLK;
      }
      break;
      
    case ST_250GOTCLK:
      if (cassette_value == 0 && value == 1) {
	/* Low speed, start of next pulse. */
	if (ddelta_us > ST_250THRESH) {
	  /* It's the next clock; bit was 0 */
	  sample = 0;
	  /* Watch for end of this clock */
	  cassette_pulsestate = ST_250;
	} else {
	  /* It's a data pulse; bit was 1 */
	  sample = 1;
	  /* Ignore the data pulse falling edge */
	  cassette_pulsestate = ST_250GOTDAT;
	}
      }
      break;
      
    case ST_250GOTDAT:
      if (cassette_value == 2 && value == 0) {
	/* End of data pulse; watch for end of next clock */
	cassette_pulsestate = ST_250;
      }
      break;
    }
    if (sample == 2) break;

    cassette_bitnumber--;
    if (cassette_bitnumber < 0) cassette_bitnumber = 7;
    cassette_byte |= (sample << cassette_bitnumber);
    if (cassette_bitnumber == 0) {
      putc(cassette_byte, cassette_file);
      cassette_byte = 0;
    }
    break;


  default:
    error("output format %s not implemented",
	  cassette_format < (sizeof(format_name)/sizeof(char *)) ?
	  format_name[cassette_format] : "out of range;");
    break;
  }

  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (cassette_value != value) last_sound = z80_state.t_count;
  cassette_transition = z80_state.t_count;
  cassette_value = value;
}

/* Read a new transition, updating cassette_next and cassette_delta.
   If file read fails (perhaps due to eof), return 0, else 1.
   Set cassette_delta to (unsigned long) -1 on failure. */
static int
transition_in()
{
  unsigned long delta_us, nsamples, maxsamples;
  Ushort code;
  Uint d;
  int next, ret = 0;
  int c, cabs;
  float delta_ts;
  sigset_t set, oldset;

  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);

  switch (cassette_format) {
  case DEBUG_FORMAT:
    if (fscanf(cassette_file, "%d %lu\n", &next, &delta_us) == 2) {
      delta_ts = delta_us * z80_state.clockMHz - cassette_roundoff_error;
      cassette_delta = (unsigned long)(delta_ts + 0.5);
      cassette_roundoff_error = cassette_delta - delta_ts;
      cassette_next = next;
#if CASSDEBUG
      debug("%d %4lu %d\n", cassette_value, cassette_delta, cassette_next);
#endif
      ret = 1;
    }
    break;
    
  case CPT_FORMAT:
    c = get_twobyte(&code, cassette_file);
    if (c == -1) break;
    if (code == 0xffff) {
      c = getc(cassette_file);
      if (c == EOF) break;
      cassette_next = c;
      c = get_fourbyte(&d, cassette_file);
      if (c == -1) break;
      delta_us = d;
    } else {
      cassette_next = code & 3;
      delta_us = code >> 2;
    }
    delta_ts = delta_us * z80_state.clockMHz - cassette_roundoff_error;
    cassette_delta = (unsigned long)(delta_ts + 0.5);
    cassette_roundoff_error = cassette_delta - delta_ts;
#if CASSDEBUG
    debug("%d %4lu %d\n", cassette_value, cassette_delta, cassette_next);
#endif
    ret = 1;
    break;

  case DIRECT_FORMAT:
  case WAV_FORMAT:
    nsamples = 0;
    maxsamples = cassette_sample_rate / 100;
    do {
      int direct = (cassette_format == DIRECT_FORMAT);
      c = get_sample(direct, cassette_file);
      if (direct && cassette_stereo) {
	/* Discard right channel */
	(void) get_sample(direct, cassette_file);
      }
      if (c == EOF) goto fail;
      if (c > 127 + cassette_noisefloor) {
	next = 1;
      } else if (c <= 127 - cassette_noisefloor) {
	next = 2;
      } else {
	next = 0;
      }
      if (cassette_speed == SPEED_1500) {
	cassette_noisefloor = 2;
      } else {
	/* Attempt to learn the correct noise cutoff adaptively.
	 * This code is just a hack; it would be nice to know a
	 * real signal-processing algorithm for this application
	 */
	cabs = abs(c - 127);
#if CASSDEBUG2
	debug("%f %f %d %d -> %d\n", cassette_avg, cassette_env,
	       cassette_noisefloor, cabs, next);
#endif
	if (cabs > 1) {
	  cassette_avg = (99*cassette_avg + cabs)/100;
	}
	if (cabs > cassette_env) {
	  cassette_env = (cassette_env + 9*cabs)/10;
	} else if (cabs > 10) {
	  cassette_env = (99*cassette_env + cabs)/100;
	}
	cassette_noisefloor = (cassette_avg + cassette_env)/2;
      }
      nsamples++;
      /* Allow reset button */
      trs_get_event(0);
      if (z80_state.nmi) break;
    } while (next == cassette_value && maxsamples-- > 0);
    cassette_next = next;
    delta_ts = nsamples * (1000000.0/cassette_sample_rate)
      * z80_state.clockMHz - cassette_roundoff_error;
    cassette_delta = (unsigned long) delta_ts + 0.5;
    cassette_roundoff_error = cassette_delta - delta_ts;
#if CASSDEBUG
    debug("%3lu -> %d %4lu %d\n",
	  nsamples, cassette_value, cassette_delta, cassette_next);
#endif
    ret = 1;
    break;

  case CAS_FORMAT:
    if (cassette_pulsestate == 0) {
      cassette_bitnumber--;
    }
    if (cassette_bitnumber < 0) {
      c = getc(cassette_file);
      if (c == EOF) {
	/* Add one extra zero byte to work around an apparent bug
	   in the Vavasour Model I emulator's .CAS files */
	if (cassette_byte == 0x100) goto fail;
	c = 0x100;
      }
      cassette_byte = c;
      cassette_bitnumber = 7;
    }
    c = (cassette_byte >> cassette_bitnumber) & 1;
    delta_us =
      pulse_shape[cassette_speed][c][cassette_pulsestate].delta_us;
    cassette_next =
      pulse_shape[cassette_speed][c][cassette_pulsestate].next;
    cassette_pulsestate++;
    if (pulse_shape[cassette_speed][c][cassette_pulsestate].next == -1) {
      cassette_pulsestate = 0;
      /* Kludge to emulate extra delay that's needed after the initial
	 0xA5 sync byte to let Basic execute the CLEAR routine.
      */
      if (cassette_byte == 0xa5 && cassette_speed == SPEED_500) {
	delta_us += 1034;
      }
    }
    delta_ts = delta_us * z80_state.clockMHz - cassette_roundoff_error;
    cassette_delta = (unsigned long)(delta_ts + 0.5);
    cassette_roundoff_error = cassette_delta - delta_ts;
#if CASSDEBUG
    debug("%d %4lu %d\n",
	  cassette_value, cassette_delta, cassette_next);
#endif
    ret = 1;
    break;

  default:
    error("input format %s not implemented",
	  cassette_format < (sizeof(format_name)/sizeof(char *)) ?
	  format_name[cassette_format] : "out of range;");
    break;
  }
  fail:
  if (ret == 0) {
    cassette_delta = (unsigned long) -1;
  }
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  return ret;
}

/* If the motor has been on for 1 second (emulated time), the i/o port
   has been neither read nor written, and the Z-80 program has 1500
   bps rise or fall interrupts enabled, then give it one of each just
   to get things going. */
void
trs_cassette_kickoff(int dummy)
{
  if (cassette_motor && cassette_state == CLOSE &&
      trs_cassette_interrupts_enabled()) {
    cassette_speed = SPEED_1500;
    cassette_transition = z80_state.t_count;
    trs_cassette_fall_interrupt(1);
    trs_cassette_rise_interrupt(1);
  }
}

/* Z-80 program is turning motor on or off */
void trs_cassette_motor(int value)
{
  if (value) {
    /* motor on */
    if (!cassette_motor) {
#if CASSDEBUG3
      debug("motor on %ld\n", z80_state.t_count);
#endif
      cassette_motor = 1;
      cassette_transition = z80_state.t_count;
      cassette_value = 0;
      cassette_next = 0;
      cassette_delta = 0;
      cassette_flipflop = 0;
      cassette_byte = 0;
      cassette_bitnumber = 0;
      cassette_pulsestate = 0;
      cassette_speed = SPEED_500;
      cassette_roundoff_error = 0.0;
      cassette_avg = NOISE_FLOOR;
      cassette_env = 127;
      cassette_noisefloor = NOISE_FLOOR;
      cassette_firstoutread = 0;
      cassette_transitionsout = 0;
      if (trs_model > 1) {
	/* Get 1500bps reading started after 1 second */
	trs_schedule_event(trs_cassette_kickoff, 0,
			   (tstate_t) (1000000 * z80_state.clockMHz));
      }
    }
  } else {
    /* motor off */
    if (cassette_motor) {
      if (cassette_state == WRITE) {
	transition_out(FLUSH);
      }
      assert_state(CLOSE);
      cassette_motor = 0;
    }
  }
}

void trs_cassette_out(int value)
{
#if CASSDEBUG3
  debug("out %ld\n", z80_state.t_count);
#endif
  if (cassette_motor) {
    if (cassette_state == READ) {
      trs_cassette_update(0);
      cassette_flipflop = 0;
      if (cassette_firstoutread == 0) {
	cassette_firstoutread = z80_state.t_count;
      }
    }
    if (cassette_state != READ && value != cassette_value) {
      if (assert_state(WRITE) < 0) return;
      transition_out(value);
    }
  }

#if SB_SOUND
  /* Do sound emulation by wiggling SoundBlaster output in real time */
  if ((cassette_motor == 0) && sb_address) {
    while (inb(sb_address + 0xC) & 0x80) /*poll*/ ;
    outb(0x10, sb_address + 0xC);
    while (inb(sb_address + 0xC) & 0x80) /*poll*/ ;
    outb(sb_cassette_volume[value], sb_address + 0xC);
  }
#endif
#if OSS_SOUND && HAVE_OSS
  /* Do sound emulation by sending samples to /dev/dsp */
  if (cassette_motor == 0 && !sb_address) {
    if (cassette_state != SOUND && value == 0) return;
    if (assert_state(SOUND) < 0) return;
    trs_suspend_delay();
    transition_out(value);
  }
#endif
}


/* Model 4 sound port */
void
trs_sound_out(int value)
{
#if SB_SOUND
  /* Do sound emulation */
  if (sb_address) {
    while (inb(sb_address + 0xC) & 0x80) /*poll*/ ;
    outb(0x10, sb_address + 0xC);
    while (inb(sb_address + 0xC) & 0x80) /*poll*/ ;
    outb(sb_sound_volume[value], sb_address + 0xC);
  }
#endif
#if HAVE_OSS
  if (cassette_motor == 0 && !sb_address) {
    if (assert_state(SOUND) < 0) return;
    trs_suspend_delay();
    transition_out(value ? 1 : 2);
  }
#endif
}

static void
orch90_flush(int dummy)
{
  trs_orch90_out(0, FLUSH);
}

/* Orchestra 85/90 */
/* Not supported in obsolescent SB_SOUND mode */
/* Implementation shares some global state with cassette and game
   sound implementations. */
void
trs_orch90_out(int channels, int value)
{
#if HAVE_OSS
  long nsamples;
  float ddelta_us;
  sigset_t set, oldset;
  int new_left, new_right;
  int v;

  /* Convert 8-bit signed to 8-bit unsigned */
  v = (value & 0xff) ^ 0x80;

  if (cassette_motor != 0) return;
  if (assert_state(ORCH90) < 0) return;
  trs_suspend_delay();
  if (channels & 1) {
    new_left = v;
  } else {
    new_left = orch90_left;
  }
  if (channels & 2) {
    new_right = v;
  } else {
    new_right = orch90_right;
  }
  if (value != FLUSH &&
      new_left == orch90_left && new_right == orch90_right) return;
  
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);

  ddelta_us = (z80_state.t_count - cassette_transition) / z80_state.clockMHz
    - cassette_roundoff_error;
  if (ddelta_us > 300000.0) {
    /* Truncate silent periods */
    ddelta_us = 300000.0;
  }
  nsamples = (unsigned long)
    (ddelta_us / (1000000.0/cassette_sample_rate) + 0.5);

  cassette_roundoff_error =
    nsamples * (1000000.0/cassette_sample_rate) - ddelta_us;

  while (nsamples-- > 0) {
    put_sample(orch90_left, TRUE, cassette_file);
    put_sample(orch90_right, TRUE, cassette_file);
  }

  if (trs_event_scheduled() == orch90_flush ||
      trs_event_scheduled() == (trs_event_func) assert_state) {
    trs_cancel_event();
  }
  if (value == FLUSH) {
    trs_schedule_event((trs_event_func)assert_state, CLOSE, 5000000);
  } else {
    trs_schedule_event(orch90_flush, FLUSH,
		       (int)(250000 * z80_state.clockMHz));
  }

  sigprocmask(SIG_SETMASK, &oldset, NULL);
  last_sound = z80_state.t_count;
  cassette_transition = z80_state.t_count;
  orch90_left = new_left;
  orch90_right = new_right;
#endif
}

void
trs_cassette_update(int dummy)
{
  if (cassette_motor && cassette_state != WRITE && assert_state(READ) >= 0) {
    int newtrans = 0;
    while ((z80_state.t_count - cassette_transition) >= cassette_delta) {

	/* Simulate analog signal processing on the 500-bps cassette input */
	if (cassette_next != 0 && cassette_value == 0) {
	  cassette_flipflop = 0x80;
	}

	/* Deliver the previously read transition from the file */
	cassette_value = cassette_next;
	cassette_transition += cassette_delta;

	/* Remember last nonzero value to get hysteresis in 1500 bps 
	   zero-crossing detector */
	if (cassette_value != 0) cassette_lastnonzero = cassette_value;

	/* Read the next transition */
	newtrans = transition_in();

	/* Allow reset button */
	trs_get_event(0);
	if (z80_state.nmi) return;
    }
    /* Schedule an interrupt on the 1500-bps cassette input if needed */
    if (newtrans && cassette_speed == SPEED_1500) {
      if (cassette_next == 2 && cassette_lastnonzero != 2) {
	trs_schedule_event(trs_cassette_fall_interrupt, 1,
			   cassette_delta -
			   (z80_state.t_count - cassette_transition));
      } else if (cassette_next == 1 && cassette_lastnonzero != 1) {
	trs_schedule_event(trs_cassette_rise_interrupt, 1,
			   cassette_delta -
			   (z80_state.t_count - cassette_transition));
      } else {
	trs_schedule_event(trs_cassette_update, 0,
			   cassette_delta -
			   (z80_state.t_count - cassette_transition));
      }
    }
  }
}


int
trs_cassette_in()
{
#if CASSDEBUG3
  debug("in  %ld\n", z80_state.t_count);
#endif
  if (cassette_motor && cassette_transitionsout <= 1) {
    assert_state(READ);
  }
  /* Heuristic to detect reading with Level 1 routines.  If the
     routine paused too long after resetting the flipflop before
     reading it again, assume it must be Level 1 code.  */
  if (cassette_firstoutread > 1) {
    if ((z80_state.t_count - cassette_firstoutread)
	/ z80_state.clockMHz > DETECT_250) {
      cassette_speed = SPEED_250;
    } else {
      cassette_speed = SPEED_500;
    }
#if CASSDEBUG4
    debug("250 detector = %s (%f)\n",
	  (cassette_speed == SPEED_250) ? "yes" : "no",
	  (z80_state.t_count - cassette_firstoutread) / z80_state.clockMHz);
#endif
    cassette_firstoutread = 1; /* disable detector */
  }
  trs_cassette_clear_interrupts();
  trs_cassette_update(0);
  if (trs_model == 1) {
    return cassette_flipflop;
  } else {
    return cassette_flipflop | (cassette_lastnonzero == 1);
  }
}

void
trs_cassette_reset()
{
  assert_state(CLOSE);
}

void trs_sound_init(int ioport, int vol)
{
#if SB_SOUND
/* try to initialize SoundBlaster. Usual ioport is 0x220 */
#define MAX_TRIES 65536
  int tries;
#ifdef SOUNDDEBUG
  int major, minor;
#endif

  if (sb_address != 0) return;
  if ((ioport & 0xFFFFFF0F) != 0x200) {
    error("Invalid SoundBlaster I/O port");
    return;
  }
  sb_address = ioport;
  if (ioperm(ioport, 0x10, 1)) {
    error("unable to access SoundBlaster I/O ports: %s", strerror(errno));
    sb_address = 0;
    return;
  }

  /* Reset SoundBlaster DSP */
  outb(0x01, sb_address + 0x6);
  usleep(3);
  outb(0x00, sb_address + 0x6);
  for (tries = 0; tries < MAX_TRIES; tries++) {
    if ((inb(sb_address + 0xE) & 0x80) &&
	(inb(sb_address + 0xA) == 0xAA)) break;
  }
  if (tries == MAX_TRIES) {
    error("unable to detect SoundBlaster");
    sb_address = 0;
    return;
  }

#ifdef SOUNDDEBUG
  /* Get DSP version number */
  while (inb(sb_address + 0xC) & 0x80) /*poll*/ ;
  outb(0xE1, sb_address + 0xC);
  while ((inb(sb_address + 0xE) & 0x80) == 0) /*poll*/ ;
  major = inb(sb_address + 0xA);
  while ((inb(sb_address + 0xE) & 0x80) == 0) /*poll*/ ;
  minor = inb(sb_address + 0xA);
  debug("SoundBlaster DSP version %d.%d detected\n", major, minor);
#endif

  /* Turn on DAC speaker */
  while (inb(sb_address + 0xC) & 0x80) /*poll*/ ;
  outb(0xD1, sb_address + 0xC);

  sb_set_volume(vol);
#else /*!SB_SOUND*/
  error("xtrs: -sb is obsolete; see the man page");
#endif
}

void
sb_set_volume(int vol)
{
#if SB_SOUND
  if (sb_address == 0) return;
  /* Set up volume values */
  if (vol < 0) vol = 0;
  if (vol > 100) vol = 100;
  sb_volume = vol;
  /* Values in comments from Model I technical manual.  Model III/4 used
     a different value for one resistor in the network, so these
     voltages are not exactly right.  In particular 3 and 0 are no
     longer almost identical, but as far as I know, 3 is still unused.
     */
  sb_cassette_volume[0] = (vol*255)/200; /* 0.46 V */
  sb_cassette_volume[1] = (vol*255)/100; /* 0.85 V */
  sb_cassette_volume[2] = 0;	/* 0.00 V */
  sb_cassette_volume[3] = (vol*255)/200; /* unused, but about 0.46 V */
  sb_sound_volume[0] = 0;
  sb_sound_volume[1] = (vol*255)/100;
#endif
}

int
sb_get_volume()
{
  return sb_volume;
}
