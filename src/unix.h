/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * Unix system-dependent filenames
 */

#ifndef SYSEXRC_FILE
# define SYSEXRC_FILE	"$HOME/.exrc"
#endif

#ifndef SYSVIMRC_FILE
# define SYSVIMRC_FILE	"$HOME/.vimrc"
#endif

#ifndef EXRC_FILE
# define EXRC_FILE		".exrc"
#endif

#ifndef VIMRC_FILE
# define VIMRC_FILE		".vimrc"
#endif

#ifndef DEFVIMRC_FILE
# define DEFVIMRC_FILE	"/usr/local/etc/vimrc"
#endif

#ifndef VIM_HLP
# define VIM_HLP		"/usr/local/lib/vim.hlp"
#endif

#ifndef BACKUPDIR
# define BACKUPDIR		"$HOME"
#endif

#ifndef DEF_DIR
# define DEF_DIR		"/tmp"
#endif

#define TMPNAME1		"/tmp/viXXXXXX"
#define TMPNAME2		"/tmp/voXXXXXX"
#define TMPNAMELEN		15

#ifndef MAXMEM
# define MAXMEM			512			/* use up to 512Kbyte for buffer */
#endif
#ifndef MAXMEMTOT
# define MAXMEMTOT		2048		/* use up to 2048Kbyte for Vim */
#endif

#define BASENAMELEN		(MAXNAMLEN - 5)

#define stricmp vim_stricmp

/*
 * prototypes for functions not in unix.c
 */
#ifdef SCO
int		chmod __ARGS((const char *, mode_t));
#endif
#if !defined(linux) && !defined(__NeXT) && !defined(M_UNIX) && !defined(ISC) && !defined(USL) && !defined(SOLARIS)
int		remove __ARGS((const char *));
/*
 * If you get an error message on "const" in the lines above, try
 * adding "-Dconst=" to the options in the makefile.
 */

# if 0		/* should be in unistd.h */
void	sleep __ARGS((int));
# endif

int		rename __ARGS((const char *, const char *));
#endif

int		stricmp __ARGS((char *, char *));

#if defined(BSD_UNIX) && !defined(__STDC__)
# define strchr(ptr, c)			index((ptr), (c))
# define strrchr(ptr, c)		rindex((ptr), (c))
#endif

/*
 * Most unixes don't have these in include files.
 * If you get a "redefined" error, delete the offending line.
 */
extern int	fsync __ARGS((int));
extern char *getwd __ARGS((char *));
#if defined(system_that_does_not_have_access_in_an_include_file)
extern int access __ARGS((char *, int));
#endif

#define tputs our_tputs
#define tgoto our_tgoto
#define tgetstr our_tgetstr
#define tgetent our_tgetent

void spewchar(unsigned what);
