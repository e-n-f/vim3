/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

#define K_CCIRCM		0x1e	/* control circumflex */

/*
 * Keycode definitions for special keys.
 *
 * Any special key code sequences are replaced by these codes.
 */

#define KS_OFF			(0x100)

/*
 * For MSDOS some keys produce codes larger than 0xff. They are split into two
 * chars, the first one is K_NUL (same value used in term.h).
 */
#define K_NUL			(0xce)		/* for MSDOS: special key follows */

#define K_SPECIAL		(0x80)

/*
 * NULs cannot be in the input string, therefore it is replaced by
 *		K_SPECIAL	KS_ZERO
 */
#define KS_ZERO			254

/*
 * K_SPECIAL cannot be in the input string, therefore it is replaced by
 *		K_SPECIAL	KS_SPECIAL
 */
#define KS_SPECIAL		255

/*
 * get second byte when translating special key code into two bytes
 */
#define K_SECOND(c)		((c) == K_SPECIAL ? KS_SPECIAL : (c) - KS_OFF)

/*
 * careful: The next entries must be in the same order as the termcap strings
 * in term.h and the numbers must be consecutive (used by inchar()).
 * KS_UARROW must be the first and KS_MAXKEY must be the last.
 * When adding new ones, do this at the end, so that the entry numbers don't
 * change (otherwise script files using them not work anymore).
 */
enum SpecialKeys
{
	KS_NAME = 0,		/* name of this terminal entry */

	KS_UARROW,
	KS_DARROW,
	KS_LARROW,
	KS_RARROW,
	KS_SUARROW,
	KS_SDARROW,
	KS_SLARROW,
	KS_SRARROW,
	
	KS_F1,				/* function keys */
	KS_F2,
	KS_F3,
	KS_F4,
	KS_F5,
	KS_F6,
	KS_F7,
	KS_F8,
	KS_F9,
	KS_F10,
	
	KS_SF1,				/* shifted function keys */
	KS_SF2,
	KS_SF3,
	KS_SF4,
	KS_SF5,
	KS_SF6,
	KS_SF7,
	KS_SF8,
	KS_SF9,
	KS_SF10,
	
	KS_HELP,
	KS_UNDO,
	
	KS_BS,

	KS_INS,
	KS_DEL,
	KS_HOME,
	KS_END,
	KS_PAGEUP,
	KS_PAGEDOWN,
	
	KS_MOUSE,
	
	KS_MAXKEY = KS_MOUSE,
	
	KS_EL,		/* clear to end of line */
	KS_IL,		/* add new blank line */
	KS_CIL,		/* add number of blank lines */
	KS_DL,		/* delete line */
	KS_CDL,		/* delete number of lines */
	KS_CS,		/* scroll region */
	KS_ED,		/* clear screen */
	KS_CI,		/* cursor invisible */
	KS_CV,		/* cursor visible */
	KS_CVV,		/* cursor very visible */
	KS_TP,		/* normal mode */
	KS_TI,		/* reverse mode */
	KS_TB,		/* bold mode */
	KS_SE,		/* normal mode */
	KS_SO,		/* standout mode */
	KS_UE,		/* exit underscore mode */
	KS_US,		/* underscore mode */
	KS_MS,		/* save to move cursor in reverse mode */
	KS_CM,		/* cursor motion */
	KS_SR,		/* scroll reverse (backward) */
	KS_CRI,		/* cursor number of chars right */
	KS_VB,		/* visual bell */
	KS_KS,		/* put terminal in "keypad transmit" mode */
	KS_KE,		/* out of "keypad transmit" mode */
	KS_TS,		/* put terminal in termcap mode */
	KS_TE,		/* out of termcap mode */
	
	KS_CSC		/* out of termcap mode */
};

/*
 * the two byte codes are replaced with the following int when using vgetc()
 */
#define K_ZERO			(KS_OFF + KS_ZERO)

#define K_UARROW		(KS_OFF + KS_UARROW)
#define K_DARROW		(KS_OFF + KS_DARROW)
#define K_LARROW		(KS_OFF + KS_LARROW)
#define K_RARROW		(KS_OFF + KS_RARROW)
#define K_SUARROW		(KS_OFF + KS_SUARROW)
#define K_SDARROW		(KS_OFF + KS_SDARROW)
#define K_SLARROW		(KS_OFF + KS_SLARROW)
#define K_SRARROW		(KS_OFF + KS_SRARROW)

#define K_F1			(KS_OFF + KS_F1)	/* function keys */
#define K_F2			(KS_OFF + KS_F2)
#define K_F3			(KS_OFF + KS_F3)
#define K_F4			(KS_OFF + KS_F4)
#define K_F5			(KS_OFF + KS_F5)
#define K_F6			(KS_OFF + KS_F6)
#define K_F7			(KS_OFF + KS_F7)
#define K_F8			(KS_OFF + KS_F8)
#define K_F9			(KS_OFF + KS_F9)
#define K_F10			(KS_OFF + KS_F10)

#define K_SF1			(KS_OFF + KS_SF1)	/* shifted function keys */
#define K_SF2			(KS_OFF + KS_SF2)
#define K_SF3			(KS_OFF + KS_SF3)
#define K_SF4			(KS_OFF + KS_SF4)
#define K_SF5			(KS_OFF + KS_SF5)
#define K_SF6			(KS_OFF + KS_SF6)
#define K_SF7			(KS_OFF + KS_SF7)
#define K_SF8			(KS_OFF + KS_SF8)
#define K_SF9			(KS_OFF + KS_SF9)
#define K_SF10			(KS_OFF + KS_SF10)

#define K_HELP			(KS_OFF + KS_HELP)
#define K_UNDO			(KS_OFF + KS_UNDO)

#define K_BS			(KS_OFF + KS_BS)

#define K_INS			(KS_OFF + KS_INS)
#define K_DEL			(KS_OFF + KS_DEL)
#define K_HOME			(KS_OFF + KS_HOME)
#define K_END			(KS_OFF + KS_END)
#define K_PAGEUP		(KS_OFF + KS_PAGEUP)
#define K_PAGEDOWN		(KS_OFF + KS_PAGEDOWN)

#define K_MOUSE			(KS_OFF + KS_MOUSE)

#define K_MAXKEY		K_MOUSE			/* used for the last key code */
