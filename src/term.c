/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */
/*
 *
 * term.c: functions for controlling the terminal
 *
 * primitive termcap support for Amiga and MSDOS included
 *
 * NOTE: padding and variable substitution is not performed,
 * when compiling without TERMCAP, we use tputs() and tgoto() dummies.
 */

#include "vim.h"
#include "globals.h"
#include "param.h"
#include "proto.h"
#ifdef TERMCAP
# ifdef linux
#  include <termcap.h>
#  if 0		/* only required for old versions, it's now in termcap.h */
    typedef int (*outfuntype) (int);
#  endif
#  define TPUTSFUNCAST (outfuntype)
# else
#  define TPUTSFUNCAST
#  ifdef AMIGA
#   include "proto/termlib.pro"
#  else
	/* for those systems that don't have tgoto in termcap.h */
	extern char *tgoto __ARGS((char *, int, int));
#  endif
# endif
#endif

/*
 * Here are the builtin termcap entries.  They not stored as complete Tcarr
 * structures, as such a structure is to big. 
 *
 * The entries are compact, therefore they normally are included even when
 * TERMCAP is defined.  When TERMCAP is defined, the builtin entries can be
 * accessed with "builtin_amiga", "builtin_ansi", "builtin_debug", etc.
 *
 * Each termcap is list a builtin_term structures. It always starts with
 * KS_NAME, which separates the entries.  See parse_builtin_tcap() for all
 * details.
 */
struct builtin_term
{
	char		bt_entry;
	char		*bt_string;
};

static struct builtin_term *find_builtin_term __ARGS((char_u *name));
static void parse_builtin_tcap __ARGS((Tcarr *tc, char_u *s));
static void gather_termleader __ARGS((void));

struct builtin_term builtin_termcaps[] = {

#ifndef NO_BUILTIN_TCAPS

# if defined(AMIGA) || defined(ALL_BUILTIN_TCAPS) || defined(SOME_BUILTIN_TCAPS)
/*
 * Amiga console window, default for Amiga
 */
	{KS_NAME,	"amiga"},
	{KS_EL,     "\033[K"},
	{KS_IL,     "\033[L"},
#  ifdef TERMINFO
	{KS_CIL,    "\033[%p1%dL"},
#  else
	{KS_CIL,	"\033[%dL"},
#  endif
	{KS_DL,      "\033[M"},
#  ifdef TERMINFO
	{KS_CDL,     "\033[%p1%dM"},
#  else
	{KS_CDL,     "\033[%dM"},
#  endif
	{KS_ED,      "\014"},
	{KS_CI,      "\033[0 p"},
	{KS_CV,      "\033[1 p"},
	{KS_TP,      "\033[0m"},
	{KS_TI,      "\033[7m"},
	{KS_TB,      "\033[1m"},
	{KS_SE,      "\033[0m"},
	{KS_SO,      "\033[33m"},
	{KS_MS,      "\001"},
#  ifdef TERMINFO
	{KS_CM,      "\033[%i%p1%d;%p2%dH"},
#  else
	{KS_CM,      "\033[%i%d;%dH"},
#  endif
#  ifdef TERMINFO
	{KS_CRI,     "\033[%p1%dC"},
#  else
	{KS_CRI,     "\033[%dC"},
#  endif
	{KS_UARROW,  "\233A"},
	{KS_DARROW,  "\233B"},
	{KS_LARROW,  "\233D"},
	{KS_RARROW,  "\233C"},
	{KS_SUARROW, "\233T"},
	{KS_SDARROW, "\233S"},
	{KS_SLARROW, "\233 A"},
	{KS_SRARROW, "\233 @"},
	{KS_F1,      "\233\060~"},	/* some compilers don't understand "\2330" */
	{KS_F2,      "\233\061~"},
	{KS_F3,      "\233\062~"},
	{KS_F4,      "\233\063~"},
	{KS_F5,      "\233\064~"},
	{KS_F6,      "\233\065~"},
	{KS_F7,      "\233\066~"},
	{KS_F8,      "\233\067~"},
	{KS_F9,      "\233\070~"},
	{KS_F10,     "\233\071~"},
	{KS_SF1,     "\233\061\060~"},
	{KS_SF2,     "\233\061\061~"},
	{KS_SF3,     "\233\061\062~"},
	{KS_SF4,     "\233\061\063~"},
	{KS_SF5,     "\233\061\064~"},
	{KS_SF6,     "\233\061\065~"},
	{KS_SF7,     "\233\061\066~"},
	{KS_SF8,     "\233\061\067~"},
	{KS_SF9,     "\233\061\070~"},
	{KS_SF10,    "\233\061\071~"},
	{KS_HELP,    "\233?~"},
# endif

# if defined(UNIX) || defined(ALL_BUILTIN_TCAPS) || defined(SOME_BUILTIN_TCAPS)
/*
 * standard ANSI terminal, default for unix
 */
	{KS_NAME,	"ansi"},
	{KS_EL,		"\033[2K"},
	{KS_IL,		"\033[L"},
#  ifdef TERMINFO
	{KS_CIL,	"\033[%p1%dL"},
#  else
	{KS_CIL,	"\033[%dL"},
#  endif
	{KS_DL,		"\033[M"},
#  ifdef TERMINFO
	{KS_CDL,	"\033[%p1%dM"},
#  else
	{KS_CDL,	"\033[%dM"},
#  endif
	{KS_ED,		"\033[2J"},
	{KS_TP,		"\033[0m"},
	{KS_TI,		"\033[7m"},
	{KS_MS,		"\001"},
#  ifdef TERMINFO
	{KS_CM,		"\033[%i%p1%d;%p2%dH"},
#  else
	{KS_CM,		"\033[%i%d;%dH"},
#  endif
#  ifdef TERMINFO
	{KS_CRI,	"\033[%p1%dC"},
#  else
	{KS_CRI,	"\033[%dC"},
#  endif
# endif

# if defined(MSDOS) || defined(ALL_BUILTIN_TCAPS)
/*
 * These codes are valid when nansi.sys or equivalent has been installed.
 * Function keys on a PC are preceded with a NUL. These are converted into
 * K_NUL '\316' in GetChars(), because we cannot handle NULs in key codes.
 * CTRL-arrow is used instead of SHIFT-arrow.
 */
	{KS_NAME,	 "pcansi"},
	{KS_EL,      "\033[K"},
	{KS_IL,      "\033[L"},
	{KS_DL,      "\033[M"},
	{KS_ED,      "\033[2J"},
	{KS_TP,      "\033[0m"},
	{KS_TI,      "\033[7m"},
	{KS_MS,      "\001"},
#  ifdef TERMINFO
	{KS_CM,      "\033[%i%p1%d;%p2%dH"},
#  else
	{KS_CM,      "\033[%i%d;%dH"},
#  endif
#  ifdef TERMINFO
	{KS_CRI,     "\033[%p1%dC"},
#  else
	{KS_CRI,     "\033[%dC"},
#  endif
	{KS_UARROW,  "\316H"},
	{KS_DARROW,  "\316P"},
	{KS_LARROW,  "\316K"},
	{KS_RARROW,  "\316M"},
	{KS_SLARROW, "\316s"},
	{KS_SRARROW, "\316t"},
	{KS_F1,      "\316;"},
	{KS_F2,      "\316<"},
	{KS_F3,      "\316="},
	{KS_F4,      "\316>"},
	{KS_F5,      "\316?"},
	{KS_F6,      "\316@"},
	{KS_F7,      "\316A"},
	{KS_F8,      "\316B"},
	{KS_F9,      "\316C"},
	{KS_F10,     "\316D"},
	{KS_SF1,     "\316T"},
	{KS_SF2,     "\316U"},
	{KS_SF3,     "\316V"},
	{KS_SF4,     "\316W"},
	{KS_SF5,     "\316X"},
	{KS_SF6,     "\316Y"},
	{KS_SF7,     "\316Z"},
	{KS_SF8,     "\316["},
	{KS_SF9,     "\316\\"},
	{KS_SF10,    "\316]"},
	{KS_INS,	 "\316R"},
	{KS_DEL,	 "\316S"},
	{KS_HOME,	 "\316G"},
	{KS_END,	 "\316O"},
	{KS_PAGEDOWN,"\316Q"},
	{KS_PAGEUP,	 "\316I"},
# endif
    
# if defined(MSDOS) || defined(ALL_BUILTIN_TCAPS) || defined(SOME_BUILTIN_TCAPS)
/*
 * These codes are valid for the pc video.  The entries that start with ESC |
 * are translated into conio calls in msdos.c. Default for MSDOS.
 */
	{KS_NAME,	 "pcterm"},
	{KS_EL,      "\033|K"},
	{KS_IL,      "\033|L"},
	{KS_DL,      "\033|M"},
#  ifdef TERMINFO
	{KS_CS,      "\033|%i%p1%d;%p2%dr"},
#  else
	{KS_CS,      "\033|%i%d;%dr"},
#  endif
	{KS_ED,      "\033|J"},
	{KS_TP,      "\033|0m"},
	{KS_TI,      "\033|112m"},
	{KS_TB,      "\033|63m"},
	{KS_SE,      "\033|0m"},
	{KS_SO,      "\033|31m"},
	{KS_MS,      "\001"},
#  ifdef TERMINFO
	{KS_CM,      "\033|%i%p1%d;%p2%dH"},
#  else
	{KS_CM,      "\033|%i%d;%dH"},
#  endif
	{KS_UARROW,  "\316H"},
	{KS_DARROW,  "\316P"},
	{KS_LARROW,  "\316K"},
	{KS_RARROW,  "\316M"},
	{KS_SLARROW, "\316s"},
	{KS_SRARROW, "\316t"},
	{KS_F1,      "\316;"},
	{KS_F2,      "\316<"},
	{KS_F3,      "\316="},
	{KS_F4,      "\316>"},
	{KS_F5,      "\316?"},
	{KS_F6,      "\316@"},
	{KS_F7,      "\316A"},
	{KS_F8,      "\316B"},
	{KS_F9,      "\316C"},
	{KS_F10,     "\316D"},
	{KS_SF1,     "\316T"},
	{KS_SF2,     "\316U"},
	{KS_SF3,     "\316V"},
	{KS_SF4,     "\316W"},
	{KS_SF5,     "\316X"},
	{KS_SF6,     "\316Y"},
	{KS_SF7,     "\316Z"},
	{KS_SF8,     "\316["},
	{KS_SF9,     "\316\\"},
	{KS_SF10,    "\316]"},
	{KS_INS,	 "\316R"},
	{KS_DEL,	 "\316S"},
	{KS_HOME,	 "\316G"},
	{KS_END,	 "\316O"},
	{KS_PAGEDOWN,"\316Q"},
	{KS_PAGEUP,	 "\316I"},
# endif

# if defined(NT) || defined(ALL_BUILTIN_TCAPS)
/*
 * These codes are valid for the NT Console .  The entries that start with
 * ESC | are translated into console calls in winnt.c.
 */
	{KS_NAME,	 "ntconsole"},
	{KS_EL,      "\033|K"},
	{KS_IL,      "\033|L"},
#  ifdef TERMINFO
	{KS_CIL,	 "\033|%p1%dL"},
#  else
	{KS_CIL,	 "\033|%dL"},
#  endif
	{KS_DL,      "\033|M"},
#  ifdef TERMINFO
	{KS_CDL,     "\033|%p1%dM"},
#  else
	{KS_CDL,     "\033|%dM"},
#  endif
	{KS_ED,      "\033|J"},
	{KS_CI,      "\033|v"},
	{KS_CV,      "\033|V"},
	{KS_TP,      "\033|0m"},
	{KS_TI,      "\033|112m"},
	{KS_TB,      "\033|63m"},
	{KS_SE,      "\033|0m"},
	{KS_SO,      "\033|31m"},
	{KS_MS,      "\001"},
#  ifdef TERMINFO
	{KS_CM,      "\033|%i%p1%d;%p2%dH"},
#  else
	{KS_CM,      "\033|%i%d;%dH"},
#  endif
	{KS_UARROW,  "\316H"},
	{KS_DARROW,  "\316P"},
	{KS_LARROW,  "\316K"},
	{KS_RARROW,  "\316M"},
	{KS_SLARROW, "\316s"},
	{KS_SRARROW, "\316t"},
	{KS_F1,      "\316;"},
	{KS_F2,      "\316<"},
	{KS_F3,      "\316="},
	{KS_F4,      "\316>"},
	{KS_F5,      "\316?"},
	{KS_F6,      "\316@"},
	{KS_F7,      "\316A"},
	{KS_F8,      "\316B"},
	{KS_F9,      "\316C"},
	{KS_F10,     "\316D"},
	{KS_SF1,     "\316T"},
	{KS_SF2,     "\316U"},
	{KS_SF3,     "\316V"},
	{KS_SF4,     "\316W"},
	{KS_SF5,     "\316X"},
	{KS_SF6,     "\316Y"},
	{KS_SF7,     "\316Z"},
	{KS_SF8,     "\316["},
	{KS_SF9,     "\316\\"},
	{KS_SF10,    "\316]"},
	{KS_INS,	 "\316R"},
	{KS_DEL,	 "\316S"},
	{KS_HOME,	 "\316G"},
	{KS_END,	 "\316O"},
	{KS_PAGEDOWN,"\316Q"},
	{KS_PAGEUP,	 "\316I"},
# endif

# ifdef ALL_BUILTIN_TCAPS
/*
 * Ordinary vt52
 */
	{KS_NAME,	 "vt52"},
	{KS_EL,      "\033K"},
	{KS_IL,      "\033T"},
	{KS_DL,      "\033U"},
	{KS_ED,      "\014"},
	{KS_TP,      "\033SO"},
	{KS_TI,      "\033S2"},
	{KS_MS,      "\001"},
	{KS_CM,      "\033Y%+ %+ "},
# endif

# if defined(UNIX) || defined(ALL_BUILTIN_TCAPS) || defined(SOME_BUILTIN_TCAPS)
/*
 * The xterm termcap is missing F14 and F15, because they send the same
 * codes as the undo and help key, although they don't work on all keyboards.
 */
	{KS_NAME,	 "xterm"},
	{KS_EL,      "\033[K"},
	{KS_IL,      "\033[L"},
#  ifdef TERMINFO
	{KS_CIL,     "\033[%p1%dL"},
#  else
	{KS_CIL,     "\033[%dL"},
#  endif
	{KS_DL,      "\033[M"},
#  ifdef TERMINFO
	{KS_CDL,     "\033[%p1%dM"},
#  else
	{KS_CDL,     "\033[%dM"},
#  endif
#  ifdef TERMINFO
	{KS_CS,      "\033[%i%p1%d;%p2%dr"},
#  else
	{KS_CS,      "\033[%i%d;%dr"},
#  endif
	{KS_ED,      "\033[H\033[2J"},
	{KS_TP,      "\033[m"},
	{KS_TI,      "\033[7m"},
	{KS_TB,      "\033[1m"},
	{KS_UE,      "\033[m"},
	{KS_US,      "\033[4m"},
	{KS_MS,      "\001"},
#  ifdef TERMINFO
	{KS_CM,      "\033[%i%p1%d;%p2%dH"},
#  else
	{KS_CM,      "\033[%i%d;%dH"},
#  endif
	{KS_SR,      "\033M"},
#  ifdef TERMINFO
	{KS_CRI,     "\033[%p1%dC"},
#  else
	{KS_CRI,     "\033[%dC"},
#  endif
	{KS_KS,      "\033[?1h\033="},
	{KS_KE,      "\033[?1l\033>"},
#  if 0					/* these seem not to work very well */
	{KS_TS,      "\0337\033[?47h"},
	{KS_TE,      "\033[2J\033[?47l\0338"},
#  endif
	{KS_UARROW,  "\033OA"},
	{KS_DARROW,  "\033OB"},
	{KS_LARROW,  "\033OD"},
	{KS_RARROW,  "\033OC"},
	{KS_SUARROW, "\033Ox"},
	{KS_SDARROW, "\033Or"},
	{KS_SLARROW, "\033Ot"},
	{KS_SRARROW, "\033Ov"},
	{KS_F1,      "\033[11~"},
	{KS_F2,      "\033[12~"},
	{KS_F3,      "\033[13~"},
	{KS_F4,      "\033[14~"},
	{KS_F5,      "\033[15~"},
	{KS_F6,      "\033[17~"},
	{KS_F7,      "\033[18~"},
	{KS_F8,      "\033[19~"},
	{KS_F9,      "\033[20~"},
	{KS_F10,     "\033[21~"},
	{KS_SF1,     "\033[23~"},
	{KS_SF2,     "\033[24~"},
	{KS_SF3,     "\033[25~"},
	{KS_SF6,     "\033[29~"},
	{KS_SF7,     "\033[31~"},
	{KS_SF8,     "\033[32~"},
	{KS_SF9,     "\033[33~"},
	{KS_SF10,    "\033[34~"},
	{KS_HELP,    "\033[28~"},
	{KS_UNDO,    "\033[26~"},
	{KS_INS,	 "\033[2~"},
	{KS_HOME,	 "\033[7~"},
	{KS_END,	 "\033[8~"},
	{KS_PAGEUP,	 "\033[5~"},
	{KS_PAGEDOWN,"\033[6~"},
# endif

# if defined(DEBUG) || defined(ALL_BUILTIN_TCAPS)
/*
 * for debugging
 */
	{KS_NAME,	 "debug"},
	{KS_EL,      "[EL]"},
	{KS_IL,      "[IL]"},
#  ifdef TERMINFO
	{KS_CIL,     "[CIL%p1%d]"},
#  else
	{KS_CIL,     "[CIL%d]"},
#  endif
	{KS_DL,      "[DL]"},
#  ifdef TERMINFO
	{KS_CDL,     "[CDL%p1%d]"},
#  else
	{KS_CDL,     "[CDL%d]"},
#  endif
#  ifdef TERMINFO
	{KS_CS,      "[%dCS%p1%d]"},
#  else
	{KS_CS,      "[%dCS%d]"},
#  endif
	{KS_ED,      "[ED]"},
	{KS_CI,      "[CI]"},
	{KS_CV,      "[CV]"},
	{KS_CVV,     "[CVV]"},
	{KS_TP,      "[TP]"},
	{KS_TI,      "[TI]"},
	{KS_TB,      "[TB]"},
	{KS_SE,      "[SE]"},
	{KS_SO,      "[SO]"},
	{KS_UE,      "[UE]"},
	{KS_US,      "[US]"},
	{KS_MS,      "[MS]"},
#  ifdef TERMINFO
	{KS_CM,      "[%p1%dCM%p2%d]"},
#  else
	{KS_CM,      "[%dCM%d]"},
#  endif
	{KS_SR,      "[SR]"},
#  ifdef TERMINFO
	{KS_CRI,     "[CRI%p1%d]"},
#  else
	{KS_CRI,     "[CRI%d]"},
#  endif
	{KS_VB,      "[VB]"},
	{KS_KS,      "[KS]"},
	{KS_KE,      "[KE]"},
	{KS_TS,      "[TI]"},
	{KS_TE,      "[TE]"},
	{KS_UARROW,  "[KU]"},
	{KS_DARROW,  "[KD]"},
	{KS_LARROW,  "[KL]"},
	{KS_RARROW,  "[KR]"},
	{KS_SUARROW, "[SKU]"},
	{KS_SDARROW, "[SKD]"},
	{KS_SLARROW, "[SKL]"},
	{KS_SRARROW, "[SKR]"},
	{KS_F1,      "[F1]"},
	{KS_F2,      "[F2]"},
	{KS_F3,      "[F3]"},
	{KS_F4,      "[F4]"},
	{KS_F5,      "[F5]"},
	{KS_F6,      "[F6]"},
	{KS_F7,      "[F7]"},
	{KS_F8,      "[F8]"},
	{KS_F9,      "[F9]"},
	{KS_F10,     "[F10]"},
	{KS_SF1,     "[SF1]"},
	{KS_SF2,     "[SF2]"},
	{KS_SF3,     "[SF3]"},
	{KS_SF4,     "[SF4]"},
	{KS_SF5,     "[SF5]"},
	{KS_SF6,     "[SF6]"},
	{KS_SF7,     "[SF7]"},
	{KS_SF8,     "[SF8]"},
	{KS_SF9,     "[SF9]"},
	{KS_SF10,    "[SF10]"},
	{KS_HELP,    "[HELP]"},
	{KS_UNDO,    "[UNDO]"},
	{KS_INS,	 "[INS]"},
	{KS_DEL,	 "[DEL]"},
	{KS_HOME,    "[HOME]"},
	{KS_END,     "[END]"},
	{KS_PAGEUP,  "[PAGEUP]"},
	{KS_PAGEDOWN,"[PAGEDOWN]"},
	{KS_MOUSE,   "[MOUSE]"},
# endif

#endif /* NO_BUILTIN_TCAPS */

/*
 * The most minimal terminal: only clear screen and cursor positioning
 * Always included.
 */
	{KS_NAME,	 "dumb"},
	{KS_ED,      "\014"},
#ifdef TERMINFO
	{KS_CM,      "\033[%i%p1%d;%p2%dH"},
#else
	{KS_CM,      "\033[%i%d;%dH"},
#endif

/*
 * end marker
 */
 	{KS_NAME,	 ""}

};		/* end of builtin_termcaps */

/*
 * DEFAULT_TERM is used, when no terminal is specified with -T option or $TERM.
 */
#ifdef AMIGA
# define DEFAULT_TERM	"amiga"
#endif /* AMIGA */

#ifdef NT
# define DEFAULT_TERM	"ntconsole"
#else
# ifdef MSDOS
#  define DEFAULT_TERM	"pcterm"
# endif /* MSDOS */                                   
#endif /* NT */ 

#ifdef UNIX
# define DEFAULT_TERM	"ansi"
#endif /* UNIX */

/*
 * Term_strings contains currently used terminal strings.
 * It is initialized with the default values by parse_builtin_tcap().
 * The values can be changed by setting the parameter with the same name.
 */
Tcarr term_strings;

static char	*termleader;			/* for check_termcode() */

	static struct builtin_term *
find_builtin_term(name)
	char_u		*name;
{
	struct builtin_term *p;

	for (p = &(builtin_termcaps[0]);
				p->bt_string[0] != NUL && STRCMP(name, p->bt_string); )
	{
		do
			++p;
		while (p->bt_entry != KS_NAME);
	}
	return p;
}

/*
 * Parsing of the builtin termcap entries.
 * Caller should check if 'name' is a valid builtin term.
 * The terminal's name is not set, as this is already done in termcapinit().
 */
	static void
parse_builtin_tcap(tc, name)
	Tcarr	*tc;
	char_u	*name;
{
	struct builtin_term		*p;

	p = find_builtin_term(name);
	for (++p; p->bt_entry != KS_NAME; ++p)
		((char **)tc)[p->bt_entry] = p->bt_string;
}

#ifdef TERMCAP
# ifndef linux		/* included in <termlib.h> */
#  ifndef AMIGA		/* included in proto/termlib.pro */
int				tgetent __PARMS((char *, char *));
int				tgetnum __PARMS((char *));
#if defined(linux) || defined(__sgi)
char			*tgetstr __PARMS((char *, char **));
#else
char			*tgetstr __PARMS((char *, char *));
#endif
int				tgetflag __PARMS((char *));
int				tputs();
#  endif /* AMIGA */
#  ifndef hpux
extern short	ospeed;
#  endif
# endif /* linux */
# ifndef hpux
char		*UP, *BC, PC;		/* should be extern, but some don't have them */
# endif
#endif /* TERMCAP */

	/* I don't understand this.  Whether we use vim's tgetstr or not, we
	 * still have the prototype for it in proto/termlib.c as needing a
	 * (char **) for the second argument, so why didn't any compilers
	 * complain when it was passed a (char *) as cast below???? -- webb
	 */
#if defined(linux) || defined(__sgi)
# define TGETSTR(s, p)	(char_u *)tgetstr((s), (char **)(p))
#else
# define TGETSTR(s, p)	(char_u *)tgetstr((s), (char *)(p))
#endif
#define TGETENT(b, t)	tgetent((char *)b, (char *)t)

	void
set_term(term)
	char_u *term;
{
	struct builtin_term *termp;
#ifdef TERMCAP
	int builtin = 0;
#endif
	int width = 0, height = 0;

	if (!STRNCMP(term, "builtin_", (size_t)8))
	{
		term += 8;
#ifdef TERMCAP
		builtin = 1;
#endif
	}
#ifdef TERMCAP
	else
	{
		char_u			*p;
		static char_u	tstrbuf[TBUFSZ];
		char_u			tbuf[TBUFSZ];
		char_u			*tp = tstrbuf;
		int				i;

		i = TGETENT(tbuf, term);
		if (i == -1)
		{
			EMSG("Cannot open termcap file");
			builtin = 1;
		}
		else if (i == 0)
		{
			EMSG("terminal entry not found");
			builtin = 1;
		}
		else
		{
			clear_termparam();		/* clear old parameters */
		/* output strings */
			T_EL = TGETSTR("ce", &tp);
			T_IL = TGETSTR("al", &tp);
			T_CIL = TGETSTR("AL", &tp);
			T_DL = TGETSTR("dl", &tp);
			T_CDL = TGETSTR("DL", &tp);
			T_CS = TGETSTR("cs", &tp);
			T_ED = TGETSTR("cl", &tp);
			T_CI = TGETSTR("vi", &tp);
			T_CV = TGETSTR("ve", &tp);
			T_CVV = TGETSTR("vs", &tp);
			T_TP = TGETSTR("me", &tp);
			T_TI = TGETSTR("mr", &tp);
			T_TB = TGETSTR("md", &tp);
			T_SE = TGETSTR("se", &tp);
			T_SO = TGETSTR("so", &tp);
			T_UE = TGETSTR("ue", &tp);
			T_US = TGETSTR("us", &tp);
			T_CM = TGETSTR("cm", &tp);
			T_SR = TGETSTR("sr", &tp);
			T_CRI = TGETSTR("RI", &tp);
			T_VB = TGETSTR("vb", &tp);
			T_KS = TGETSTR("ks", &tp);
			T_KE = TGETSTR("ke", &tp);
			T_TS = TGETSTR("ti", &tp);
			T_TE = TGETSTR("te", &tp);

		/* key codes */
			term_strings.t_ku = TGETSTR("ku", &tp);
			term_strings.t_kd = TGETSTR("kd", &tp);
			term_strings.t_kl = TGETSTR("kl", &tp);
				/* if cursor-left == backspace, ignore it (televideo 925) */
			if (term_strings.t_kl != NULL && *term_strings.t_kl == Ctrl('H'))
				term_strings.t_kl = NULL;
			term_strings.t_kr = TGETSTR("kr", &tp);
			/* term_strings.t_sku = TGETSTR("", &tp); termcap code unknown */
			/* term_strings.t_skd = TGETSTR("", &tp); termcap code unknown */
#ifdef ARCHIE
            /* Termcap code made up! */
            term_strings.t_sku = tgetstr("su", &tp);
            term_strings.t_skd = tgetstr("sd", &tp);
#else
            term_strings.t_sku = NULL;
            term_strings.t_skd = NULL;
#endif
			term_strings.t_skl = TGETSTR("#4", &tp);
			term_strings.t_skr = TGETSTR("%i", &tp);
			term_strings.t_f1 = TGETSTR("k1", &tp);
			term_strings.t_f2 = TGETSTR("k2", &tp);
			term_strings.t_f3 = TGETSTR("k3", &tp);
			term_strings.t_f4 = TGETSTR("k4", &tp);
			term_strings.t_f5 = TGETSTR("k5", &tp);
			term_strings.t_f6 = TGETSTR("k6", &tp);
			term_strings.t_f7 = TGETSTR("k7", &tp);
			term_strings.t_f8 = TGETSTR("k8", &tp);
			term_strings.t_f9 = TGETSTR("k9", &tp);
			term_strings.t_f10 = TGETSTR("k;", &tp);
			term_strings.t_sf1 = TGETSTR("F1", &tp);	/* really function keys 11-20 */
			term_strings.t_sf2 = TGETSTR("F2", &tp);
			term_strings.t_sf3 = TGETSTR("F3", &tp);
			term_strings.t_sf4 = TGETSTR("F4", &tp);
			term_strings.t_sf5 = TGETSTR("F5", &tp);
			term_strings.t_sf6 = TGETSTR("F6", &tp);
			term_strings.t_sf7 = TGETSTR("F7", &tp);
			term_strings.t_sf8 = TGETSTR("F8", &tp);
			term_strings.t_sf9 = TGETSTR("F9", &tp);
			term_strings.t_sf10 = TGETSTR("FA", &tp);
			term_strings.t_help = TGETSTR("%1", &tp);
			term_strings.t_undo = TGETSTR("&8", &tp);
			term_strings.t_ins = TGETSTR("kI", &tp);
			term_strings.t_del = TGETSTR("kD", &tp);
			term_strings.t_home = TGETSTR("kh", &tp);
			term_strings.t_end = TGETSTR("@7", &tp);
			term_strings.t_pu = TGETSTR("kP", &tp);
			term_strings.t_pd = TGETSTR("kN", &tp);

			height = tgetnum("li");
			width = tgetnum("co");

			T_MS = tgetflag("ms") ? (char_u *)"yes" : (char_u *)NULL;

# ifndef hpux
			BC = (char *)TGETSTR("bc", &tp);
			UP = (char *)TGETSTR("up", &tp);
			p = TGETSTR("pc", &tp);
			if (p)
				PC = *p;
			ospeed = 0;
# endif
		}
	}
	if (builtin)
#endif
	{
		/*
		 * search for 'term' in builtin_termcaps[]
		 */
		termp = find_builtin_term(term);
		if (termp->bt_string[0] == NUL)		/* did not find it */
		{
			fprintf(stderr, "'%s' not known. Available builtin terminals are:\r\n", term);
			for (termp = &(builtin_termcaps[0]); termp->bt_string[0] != NUL; )
			{
#ifdef TERMCAP
				fprintf(stderr, "\tbuiltin_%s\r\n", termp->bt_string);
#else
				fprintf(stderr, "\t%s\r\n", termp->bt_string);
#endif
				do
					++termp;
				while (termp->bt_entry != KS_NAME);
			}
			if (!starting)		/* when user typed :set term=xxx, quit here */
			{
				wait_return(TRUE);
				return;
			}
			sleep(2);
			term = DEFAULT_TERM;
			fprintf(stderr, "defaulting to '%s'\r\n", term);
			sleep(2);
			free(term_strings.t_name);
			term_strings.t_name = strsave(term);
		}
		clear_termparam();		/* clear old parameters */
		parse_builtin_tcap(&term_strings, term);
	}
/*
 * special: There is no info in the termcap about whether the cursor
 * positioning is relative to the start of the screen or to the start of the
 * scrolling region.  We just guess here. Only msdos pcterm is known to do it
 * relative.
 */
	if (STRCMP(term, "pcterm") == 0)
		T_CSC = (char_u *)"yes";
	else
		T_CSC = NULL;

	/*
	 * recognize mouse events in the input stream for xterm or msdos
	 */
#ifdef UNIX
	if (is_xterm(term))
		term_strings.t_mouse = strsave("\033[M");
#endif
#ifdef MSDOS
	term_strings.t_mouse = strsave("\233M");
#endif

#if defined(AMIGA) || defined(MSDOS)
		/* DEFAULT_TERM indicates that it is the machine console. */
	if (STRCMP(term, DEFAULT_TERM))
		term_console = FALSE;
	else
	{
		term_console = TRUE;
# ifdef AMIGA
		win_resize_on();		/* enable window resizing reports */
# endif
	}
#endif
	ttest(TRUE);
		/* display initial screen after ttest() checking. jw. */
	if (width <= 0 || height <= 0)
    {
		/* termcap failed to report size */
		/* set defaults, in case mch_get_winsize also fails */
		width = 80;
#ifdef MSDOS
		height = 25;		/* console is often 25 lines */
#else
		height = 24;		/* most terminals are 24 lines */
#endif
	}
	set_winsize(width, height, FALSE);	/* may change Rows */
}

#if defined(TERMCAP) && defined(UNIX)
/*
 * Get Columns and Rows from the termcap. Used after a window signal if the
 * ioctl() fails. It doesn't make sense to call tgetent each time if the "co"
 * and "li" entries never change. But this may happen on some systems.
 */
	void
getlinecol()
{
	char_u			tbuf[TBUFSZ];

	if (term_strings.t_name != NULL && TGETENT(tbuf, term_strings.t_name) > 0)
	{
		if (Columns == 0)
			Columns = tgetnum("co");
		if (Rows == 0)
			Rows = tgetnum("li");
	}
}
#endif

static char_u *tltoa __PARMS((unsigned long));

	static char_u *
tltoa(i)
	unsigned long i;
{
	static char_u buf[16];
	char_u		*p;

	p = buf + 15;
	*p = '\0';
	do
	{
		--p;
		*p = i % 10 + '0';
		i /= 10;
    }
	while (i > 0 && p > buf);
	return p;
}

#ifndef TERMCAP

/*
 * minimal tgoto() implementation.
 * no padding and we only parse for %i %d and %+char
 */

	char *
tgoto(cm, x, y)
	char *cm;
	int x, y;
{
	static char buf[30];
	char *p, *s, *e;

	if (!cm)
		return "OOPS";
	e = buf + 29;
	for (s = buf; s < e && *cm; cm++)
    {
		if (*cm != '%')
        {
			*s++ = *cm;
			continue;
		}
		switch (*++cm)
        {
		case 'd':
			p = (char *)tltoa((unsigned long)y);
			y = x;
			while (*p)
				*s++ = *p++;
			break;
		case 'i':
			x++;
			y++;
			break;
		case '+':
			*s++ = (char)(*++cm + y);
			y = x;
			break;
        case '%':
			*s++ = *cm;
			break;
		default:
			return "OOPS";
		}
    }
	*s = '\0';
	return buf;
}

#endif /* TERMCAP */

/*
 * Termcapinit is called from main() to initialize the terminal.
 * The optional argument is given with the -T command line option.
 */
	void
termcapinit(term)
	char_u *term;
{
	if (!term)
		term = vimgetenv((char_u *)"TERM");
	if (!term || !*term)
		term = DEFAULT_TERM;
	term_strings.t_name = strsave(term);
	set_term(term);
}

/*
 * the number of calls to mch_write is reduced by using the buffer "outbuf"
 */
#undef BSIZE			/* hpux has BSIZE in sys/param.h */
#define BSIZE	2048
static char_u			outbuf[BSIZE];
static int				bpos = 0;		/* number of chars in outbuf */

/*
 * flushbuf(): flush the output buffer
 */
	void
flushbuf()
{
	if (bpos != 0)
	{
		mch_write(outbuf, bpos);
		bpos = 0;
	}
}

/*
 * outchar(c): put a character into the output buffer.
 *			   Flush it if it becomes full.
 */
	void
outchar(c)
	unsigned	c;
{
#ifdef UNIX
	if (c == '\n')		/* turn LF into CR-LF (CRMOD does not seem to do this) */
		outchar('\r');
#endif

	outbuf[bpos] = c;

	if (p_nb)			/* for testing: unbuffered screen output (not for MSDOS) */
		mch_write(outbuf, 1);
	else
		++bpos;

	if (bpos >= BSIZE)
		flushbuf();
}

/*
 * a never-padding outstr.
 * use this whenever you don't want to run the string through tputs.
 * tputs above is harmless, but tputs from the termcap library 
 * is likely to strip off leading digits, that it mistakes for padding
 * information. (jw)
 */
	void
outstrn(s)
	char_u *s;
{
	if (bpos > BSIZE - 20)		/* avoid terminal strings being split up */
		flushbuf();
	while (*s)
		outchar(*s++);
}

/*
 * outstr(s): put a string character at a time into the output buffer.
 * If TERMCAP is defined use the termcap parser. (jw)
 */
	void
outstr(s)
	register char_u			 *s;
{
	if (bpos > BSIZE - 20)		/* avoid terminal strings being split up */
		flushbuf();
	if (s)
#ifdef TERMCAP
		tputs((char *)s, 1, TPUTSFUNCAST outchar);
#else
		while (*s)
			outchar(*s++);
#endif
}

/* 
 * cursor positioning using termcap parser. (jw)
 */
	void
windgoto(row, col)
	int		row;
	int		col;
{
	OUTSTR(tgoto((char *)T_CM, col, row));
}

/*
 * Set cursor to current position.
 * Should be optimized for minimal terminal output.
 */

	void
setcursor()
{
	if (!RedrawingDisabled)
		windgoto(curwin->w_winpos + curwin->w_row, curwin->w_col);
}

	void
ttest(pairs)
	int	pairs;
{
	char buf[70];
	char *s = "terminal capability %s required.\n";
	char *t = NULL;

  /* hard requirements */
	if (!T_ED || !*T_ED)	/* erase display */
		t = "cl";
	if (!T_CM || !*T_CM)	/* cursor motion */
		t = "cm";

	if (t)
    {
    	sprintf(buf, s, t);
    	EMSG(buf);
    }

/*
 * if "cs" defined, use a scroll region, it's faster.
 */
	if (T_CS && *T_CS != NUL)
		scroll_region = TRUE;
	else
		scroll_region = FALSE;

	if (pairs)
	{
	  /* optional pairs */
			/* TP goes to normal mode for TI (invert) and TB (bold) */
		if ((!T_TP || !*T_TP))
			T_TP = T_TI = T_TB = NULL;
		if ((!T_SO || !*T_SO) ^ (!T_SE || !*T_SE))
			T_SO = T_SE = NULL;
		if ((!T_US || !*T_US) ^ (!T_UE || !*T_UE))
			T_US = T_UE = NULL;
			/* T_CV is needed even though T_CI is not defined */
		if ((!T_CV || !*T_CV))
			T_CI = NULL;
			/* if 'mr' or 'me' is not defined use 'so' and 'se' */
		if (T_TP == NULL || *T_TP == NUL)
		{
			T_TP = T_SE;
			T_TI = T_SO;
			T_TB = T_SO;
		}
			/* if 'so' or 'se' is not defined use 'mr' and 'me' */
		if (T_SO == NULL || *T_SO == NUL)
		{
			T_SE = T_TP;
			if (T_TI == NULL)
				T_SO = T_TB;
			else
				T_SO = T_TI;
		}
	}
	gather_termleader();
}

/*
 * inchar() - get one character from
 *		1. a scriptfile
 *		2. the keyboard
 *
 *  As much characters as we can get (upto 'maxlen') are put in buf and
 *  NUL terminated (buffer length must be 'maxlen' + 1).
 *
 *	If we got an interrupt all input is read until none is available.
 *
 *  If wait_time == 0  there is no waiting for the char.
 *  If wait_time == n  we wait for n msec for a character to arrive.
 *  If wait_time == -1 we wait forever for a character to arrive.
 *
 *  Return the number of obtained characters.
 */

	int
inchar(buf, maxlen, wait_time)
	char_u	*buf;
	int		maxlen;
	int		wait_time;						/* milli seconds */
{
	int				len = 0;			/* init for GCC */
	int				retesc = FALSE;		/* return ESC with gotint */
	register int 	c;
	register int	i;

	if (wait_time == -1 || wait_time > 100)	/* flush output before waiting */
	{
		cursor_on();
		flushbuf();
	}
	did_outofmem_msg = FALSE;	/* display out of memory message (again) */
	did_swapwrite_msg = FALSE;	/* display swap file write error again */
	undo_off = FALSE;			/* restart undo now */

/*
 * first try script file
 *	If interrupted: Stop reading script files.
 */
	c = -1;
	while (scriptin[curscript] != NULL && c < 0)
	{
		if (got_int || (c = getc(scriptin[curscript])) < 0)	/* reached EOF */
		{
				/* when reading script file is interrupted, return an ESC to
									get back to normal mode */
			if (got_int)
				retesc = TRUE;
			fclose(scriptin[curscript]);
			scriptin[curscript] = NULL;
			if (curscript > 0)
				--curscript;
		}
		else
		{
			buf[0] = c;
			len = 1;
		}
	}

	if (c < 0)			/* did not get a character from script */
	{
	/*
	 * If we got an interrupt, skip all previously typed characters and
	 * return TRUE if quit reading script file.
	 */
		if (got_int)			/* skip typed characters */
		{
			while (GetChars(buf, maxlen, T_PEEK))
				;
			return retesc;
		}
			/* fill up to half the buffer, because each character may be
			 * doubled below */
		len = GetChars(buf, maxlen / 2, wait_time);
	}

	/*
	 * Two characters are special: NUL and K_SPECIAL.
	 * Replace       NUL by K_SPECIAL KS_ZERO
	 * Replace K_SPECIAL by K_SPECIAL KS_SPECIAL
	 */
	for (i = len; --i >= 0; ++buf)
	{
		if (buf[0] == NUL || buf[0] == K_SPECIAL)
		{
			memmove(buf + 2, buf + 1, i);
			buf[1] = (buf[0] == NUL ? KS_ZERO : KS_SPECIAL);
			buf[0] = K_SPECIAL;
			++buf;
			++len;
		}
	}
	*buf = NUL;								/* add trailing NUL */
	return len;
}

/*
 * Check if typestr[] contains a terminal key code.
 * Return 0 for no match, -1 for partial match, > 0 for full match.
 * With a match, the match is removed, the replacement code is inserted in
 * typestr[] and the number of characters in typestr[] is returned.
 */
	int
check_termcode()
{
	char_u 	**p;
	int		slen;
	int		len;
	int		offset;

	/*
	 * Check at all positions in typestr[], to catch something like "x<C_UP>"
	 */
	for (offset = 0; typestr[offset] != NUL; ++offset)
	{
		/*
		 * Skip this position if the character does not appear as the first
		 * character in term_strings. This speeds up a lot, since most
		 * termcodes start with the same character.
		 */
		if (STRCHR(termleader, typestr[offset]) == NULL)
			continue;

		/*
		 * skip this position if p_ek is not set and typestr[offset] is an ESC
		 * and we are in insert mode
		 */
		if (typestr[offset] == ESC && !p_ek && (State & INSERT))
			continue;
		len = STRLEN(typestr + offset);
		for (p = (char_u **)&term_strings.T_FIRST_KEY;
						p != (char_u **)&term_strings.T_LAST_KEY + 1; ++p)
		{
			/*
			 * Ignore the entry if it is blank or when we are not at the
			 * start of typestr[] and there are not enough characters to make
			 * a match
			 */
			if (*p == NULL || (slen = STRLEN(*p)) == 0 ||
											(offset && len < slen))
				continue;
			if (STRNCMP(*p, typestr + offset, (size_t)(slen > len ?
														len : slen)) == 0)
			{
				if (len < slen)				/* got a partial sequence */
					return -1;				/* need to get more chars */
			
#if defined(UNIX) || defined(MSDOS)
				/*
				 * If it is an xterm or msdos mouse click, get the coordinates.
				 * we get "ESC[Mscr" for xterm and "CSIMscr" for msdos, where
				 *	s == encoded mouse button state (0x20 = left, 0x22 = right)
				 *	c == column + ' ' + 1 == column + 33
				 *	r == row + ' ' + 1 == row + 33
				 *
				 * The coordinates are passed on through global variables. Ugly,
				 * but this avoids trouble with mouse clicks at an unexpected
				 * moment.
				 */
				if (p == (char_u **)&term_strings.t_mouse)
				{
					if (len < slen + 3)		/* not enough coordinates */
						return -1;
					mouse_code = typestr[slen + offset];
					mouse_col = typestr[slen + offset + 1] - '!';
					mouse_row = typestr[slen + offset + 2] - '!';
					slen += 3;
				}
#endif
				if (slen > 2)
						/* remove matched chars, taking care of noremap */
					del_typestr(slen - 2, offset);
				else if (slen == 1)
						/* insert an extra space for K_SPECIAL */
					ins_typestr((char_u *)" ", FALSE, offset, FALSE);

					/* this relies on the Key numbers to be consecutive! */
				typestr[offset] = K_SPECIAL;
				typestr[offset + 1] = KS_UARROW +
								(p - (char_u **)&term_strings.T_FIRST_KEY);
				return (len - slen + 2 + offset);
			}
		}
	}
	return 0;						/* no match found */
}

/*
 * Replace any terminal code strings in buf[] with the equivalent internal vim
 * representation.  Any strings like "<C_UP>" are also replaced -- webb
 * buf[] should have extra space for inserting K_SPECIAL codes!
 */
	void
replace_termcodes(buf)
	char_u	*buf;
{
	char_u	**names = get_key_names();
	char_u	*namep;
	int		n;
	char_u 	**p;
	char_u	*bp;
	int		slen;

	while (*buf != NUL)
	{
		/* See if it's a string like "<C_UP>" */
		if (buf[0] == '<')
		{
			for (n = KS_UARROW; n <= KS_MAXKEY; n++)
			{
				namep = names[n - KS_UARROW];
				bp = buf + 1;
				for (; *namep != NUL; namep++, bp++)
					if (TO_LOWER(*namep) != TO_LOWER(*bp))
						break;
				if (*namep == NUL && *bp == '>')
				{
					*buf++ = K_SPECIAL;
					*buf = n;
					STRCPY(buf + 1, bp + 1);
					break;
				}
			}
		}
		else
		{
			/* See if it's an actual key-code */
			for (p = (char_u **)&term_strings.T_FIRST_KEY;
				p != (char_u **)&term_strings.T_LAST_KEY + 1; ++p)
			{
				if (*p == NULL || (slen = STRLEN(*p)) == 0)	/* empty entry */
					continue;
				if (STRNCMP(*p, buf, (size_t)slen) == 0)
				{
					if (slen > 2)		/* delete extra characters */
						STRCPY(buf + 2, buf + slen);
					else if (slen == 1)	/* make room for one more character */
						memmove(buf + 1, buf, STRLEN(buf) + 1);
					buf[0] = K_SPECIAL;
						/* This relies on the Key numbers to be consecutive! */
					buf[1] = KS_UARROW + (p - (char_u **)&term_strings.T_FIRST_KEY);
					break;
				}
			}
		}
		buf++;
	}
}

/*
 * Gather the first characters in the terminal key codes into a string.
 * Used to speed up check_termcode().
 */
	static void
gather_termleader()
{
	char_u	*string;
	char_u 	**p;
	int		len;

	string = alloc(KS_MAXKEY + 1);
	if (string == NULL)
		return;
	*string = NUL;
	len = 0;

	for (p = (char_u **)&term_strings.T_FIRST_KEY;
						p != (char_u **)&term_strings.T_LAST_KEY + 1; ++p)
	{
		if (*p == NULL || **p == NUL)		/* empty entry */
			continue;
		if (STRCHR(string, **p) == NULL)	/* found a new character */
		{
			string[len++] = **p;
			string[len] = NUL;
		}
	}
	free(termleader);
	termleader = string;
}

/*
 * outnum - output a (big) number fast
 */
	void
outnum(n)
	register long n;
{
	OUTSTRN(tltoa((unsigned long)n));
}
 
	void
check_winsize()
{
	if (Columns < MIN_COLUMNS)
		Columns = MIN_COLUMNS;
	else if (Columns > MAX_COLUMNS)
		Columns = MAX_COLUMNS;
	if (Rows < min_rows())		/* need room for one window and command line */
		Rows = min_rows();
	screen_new_rows();			/* may need to update window sizes */
}

/*
 * set window size
 * If 'mustset' is TRUE, we must set Rows and Columns, do not get real
 * window size (this is used for the :win command).
 * If 'mustset' is FALSE, we may try to get the real window size and if
 * it fails use 'width' and 'height'.
 */
	void
set_winsize(width, height, mustset)
	int		width, height;
	int		mustset;
{
	register int 		tmp;

	if (width < 0 || height < 0)	/* just checking... */
		return;

									/* postpone the resizing */
	if (State == HITRETURN || State == SETWSIZE)
	{
		State = SETWSIZE;
		return;
	}
	if (State != ASKMORE)
		screenclear();
#ifdef AMIGA
	flushbuf(); 		/* must do this before mch_get_winsize for some
							obscure reason */
#endif /* AMIGA */
	if (mustset || mch_get_winsize() == FAIL)
	{
		Rows = height;
		Columns = width;
		check_winsize();		/* always check, to get p_scroll right */
		mch_set_winsize();
	}
	else
		check_winsize();		/* always check, to get p_scroll right */
	if (State == HELP)
		(void)redrawhelp();
	else if (!starting)
	{
		comp_Botline_all();
		if (State == ASKMORE)	/* don't redraw, just adjust screen size */
		{
			screenalloc(FALSE);
			msg_moremsg();		/* display --more-- message again */
			msg_row = Rows - 1;
		}
		else
		{
			tmp = RedrawingDisabled;
			RedrawingDisabled = FALSE;
			updateScreen(CURSUPD);
			RedrawingDisabled = tmp;
			if (State == CMDLINE)
				redrawcmdline();
			else
				setcursor();
		}
	}
	flushbuf();
}

	void
settmode(raw)
	int	 raw;
{
	static int		oldraw = FALSE;

	if (oldraw == raw)		/* skip if already in desired mode */
		return;
	oldraw = raw;

	mch_settmode(raw);	/* machine specific function */
}

	void
starttermcap()
{
	outstr(T_TS);	/* start termcap mode */
	outstr(T_KS);	/* start "keypad transmit" mode */
	flushbuf();
	termcap_active = TRUE;
}

	void
stoptermcap()
{
	outstr(T_KE);	/* stop "keypad transmit" mode */
	flushbuf();
	termcap_active = FALSE;
	cursor_on();	/* just in case it is still off */
	outstr(T_TE);	/* stop termcap mode */
}

/*
 * By outputting the 'cursor very visible' termcap code, for some windowed
 * terminals this makes the screen scrolled to the right position
 * Used when starting Vim or returning from a shell.
 */
	void
scroll_start()
{
	if (T_CVV != NULL && *T_CVV)
	{
		outstr(T_CVV);
		outstr(T_CV);
	}
}

/*
 * enable cursor, unless in Visual mode or no inversion possible
 */
static int cursor_is_off = FALSE;

	void
cursor_on()
{
	if (cursor_is_off && (VIsual.lnum == 0 || highlight == NULL))
	{
		outstr(T_CV);
		cursor_is_off = FALSE;
	}
}

	void
cursor_off()
{
	if (!cursor_is_off)
		outstr(T_CI);			/* disable cursor */
	cursor_is_off = TRUE;
}

/*
 * set scrolling region for window 'wp'
 */
	void
scroll_region_set(wp)
	WIN		*wp;
{
	OUTSTR(tgoto((char *)T_CS, wp->w_winpos + wp->w_height - 1, wp->w_winpos));
}

/*
 * reset scrolling region to the whole screen
 */
	void
scroll_region_reset()
{
	OUTSTR(tgoto((char *)T_CS, (int)Rows - 1, 0));
}
