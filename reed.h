/* Matthew Reed's hard drive format.  Thanks to Matthew for providing
   documentation.  The comments below are copied from his mail
   messages, with some additions. */

typedef struct {
  Uchar id1;       /* 0: Identifier #1: 56H */
  Uchar id2;       /* 1: Identifier #2: CBH */
  Uchar ver;       /* 2: Version of format: 10H = version 1.0 */
  Uchar cksum;     /* 3: Simple checksum: 
		      To calculate, add together bytes 0 to 31 of header
		      (excepting byte 3), then XOR result with 4CH */
  Uchar blks;      /* 4: Number of 256 byte blocks in header: should be 1 */
  Uchar mb4;       /* 5: Not used, but HDFORMAT sets to 4 */
  Uchar media;     /* 6: Media type: 0 for hard disk */
  Uchar flag1;     /* 7: Flags #1:
		      bit 7: Write protected: 0 for no, 1 for yes 
                             [xtrshard/dct ignores for now]
		      bit 6: Must be 0
		      bit 5 - 0: reserved */
  Uchar flag2;     /* 8: Flags #2: reserved */
  Uchar flag3;     /* 9: Flags #3: reserved */
  Uchar crtr;      /* 10: Created by: 
		      14H = HDFORMAT
		      42H = xtrs mkdisk
                      80H = Cervasio xtrshard port to Vavasour M4 emulator */
  Uchar dfmt;      /* 11: Disk format: 0 = LDOS/LS-DOS */
  Uchar mm;        /* 12: Creation month: mm */
  Uchar dd;        /* 13: Creation day: dd */
  Uchar yy;        /* 14: Creation year: yy (offset from 1900) */
  Uchar res1[12];  /* 15 - 26: reserved */
  Uchar dparm;     /* 27: Disk parameters: (unused with hard drives)
		      bit 7: Density: 0 = double, 1 = single
		      bit 6: Sides: 0 = one side, 1 = 2 sides
		      bit 5: First sector: 0 if sector 0, 1 if sector 1
		      bit 4: DAM convention: 0 if normal (LDOS),
		      1 if reversed (TRSDOS 1.3)
		      bit 3 - 0: reserved */
  Uchar cyl;       /* 28: Number of cylinders per disk */
  Uchar sec;       /* 29: Number of sectors per track (floppy); cyl (hard) */
  Uchar gran;      /* 30: Number of granules per track (floppy); cyl (hard)*/
  Uchar dcyl;      /* 31: Directory cylinder [mkdisk sets to 1; xtrs ignores]*/
  char label[32];  /* 32: Volume label: 31 bytes terminated by 0 */
  char filename[8];/* 64 - 71: 8 characters of filename (without extension)
		      [Cervasio addition.  xtrs actually doesn't limit this 
                       to 8 chars or strip the extension] */
  Uchar res2[184]; /* 72 - 255: reserved */
} ReedHardHeader;
