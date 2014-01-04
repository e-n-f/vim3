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
 */

/* heavily modified by Eric Fischer, etaoin@uchicago.edu,
   to support reasonable output on slow devices.
*/

#include "vim.h"
#include "globals.h"
#include "param.h"
#include "proto.h"

#include "terms.h"

Tcarr term_strings;

void 
set_term (char *termname) 
{
	reallyinittcap();

	term_strings.t_name = (char_u *)termname;
	term_strings.t_el = CLEAREOL;
	term_strings.t_il = INSERTLN;
	term_strings.t_cil = 0;         /* insert multiple lines */
	term_strings.t_dl = DELETELN;
	term_strings.t_cdl = 0;         /* delete multiple lines */
	term_strings.t_cs = SCROLLSET;  /* dummy */
	term_strings.t_ed = CLEARSCR;
	term_strings.t_ci = 0;          /* curs invisible */
	term_strings.t_cv = 0;          /* cursor visible */
	term_strings.t_cvv = 0;
	term_strings.t_tp = STANDEND;
	term_strings.t_ti = STANDOUT;
	term_strings.t_tb = STANDOUT;
	term_strings.t_se = STANDEND;
	term_strings.t_so = STANDOUT;
	term_strings.t_cm = MOVETO;     /* dummy */
	term_strings.t_sr = 0;
	term_strings.t_cri = 0;
	term_strings.t_vb = 0;
	term_strings.t_ks = (char_u *)0;
	term_strings.t_ke = (char_u *)0;
	term_strings.t_ts = (char_u *)0;
	term_strings.t_te = (char_u *)0;
	
	term_strings.t_ku = CURSU;
	term_strings.t_kd = CURSD;
	term_strings.t_kl = CURSL;
	term_strings.t_kr = CURSR;

	ttest (TRUE);
}

#if defined(TERMCAP) && defined(UNIX)
/*
 * Get Columns and Rows from the termcap. Used after a window signal if the
 * ioctl() fails. It doesn't make sense to call tgetent each time if the "co"
 * and "li" entries never change. But this may happen on some systems.
 */

/*
	void
getlinecol()
{
	char_u			tbuf[TBUFSZ];

	if (term_strings.t_name != NULL && tgetent(tbuf, term_strings.t_name) > 0)
	{
		if (Columns == 0)
			Columns = tgetnum("co");
		if (Rows == 0)
			Rows = tgetnum("li");
	}
}
*/

#endif

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
		while (*s)
			outchar(*s++);
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
 *  If time == 0  there is no waiting for the char.
 *  If time == n  we wait for n msec for a character to arrive.
 *  If time == -1 we wait forever for a character to arrive.
 *
 *  Return the number of obtained characters.
 */

	int
inchar(buf, maxlen, time)
	char_u	*buf;
	int		maxlen;
	int		time;						/* milli seconds */
{
	int				len;
	int				retesc = FALSE;		/* return ESC with gotint */
	register int 	c;
	register int	i;

	if (time == -1 || time > 100)	/* flush output before waiting */
	{
		cursor_on();
		flushbuf();
	}
	did_outofmem_msg = FALSE;	/* display out of memory message (again) */

/*
 * first try script file
 *	If interrupted: Stop reading script files.
 */
retry:
	if (scriptin[curscript] != NULL)
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
			goto retry;		/* may read other script if this one was nested */
		}
		if (c == 0)
			c = K_ZERO;		/* replace ^@ with special code */
		*buf++ = c;
		*buf = NUL;
		return 1;
	}

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
	len = GetChars(buf, maxlen, time);

	for (i = len; --i >= 0; ++buf)
		if (*buf == 0)
			*(char_u *)buf = K_ZERO;		/* replace ^@ with special code */
	*buf = NUL;								/* add trailing NUL */
	return len;
}

/*
 * Check if buf[] begins with a terminal key code.
 * Return 0 for no match, -1 for partial match, > 0 for full match.
 * With a match the replacement code is put in buf[0], the match is
 * removed and the number characters in buf is returned.
 *
 * Note: should always be called with buf == typestr!
 */
	int
check_termcode(buf)
	char_u	*buf;
{
	char_u 	**p;
	int		slen;
	int		len;

	len = STRLEN(buf);
	for (p = (char_u **)&term_strings.t_ku; p != (char_u **)&term_strings.t_undo + 1; ++p)
	{
		if (*p == NULL || (slen = STRLEN(*p)) == 0)		/* empty entry */
			continue;
		if (STRNCMP(*p, buf, (size_t)(slen > len ? len : slen)) == 0)
		{
			if (len >= slen)		/* got the complete sequence */
			{
				len -= slen;
					/* remove matched chars, taking care of noremap */
				del_typestr(slen - 1);
					/* this relies on the Key numbers to be consecutive! */
				buf[0] = K_UARROW + (p - (char_u **)&term_strings.t_ku);
				return (len + 1);
			}
			return -1;				/* got a partial sequence */
		}
	}
	return 0;						/* no match found */
}

/* tltoa -- str -> num */

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
	if (Rows < MIN_ROWS + 1)	/* need room for one window and command line */
		Rows = MIN_ROWS + 1;
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

	if (State == HITRETURN || State == SETWSIZE)	/* postpone the resizing */
	{
		State = SETWSIZE;
		return;
	}
	screenclear();
#ifdef AMIGA
	flushbuf(); 		/* must do this before mch_get_winsize for some obscure reason */
#endif /* AMIGA */
	if (mustset || mch_get_winsize() == FAIL)
	{
		Rows = height;
		Columns = width;
		check_winsize();		/* always check, to get p_scroll right */
		mch_set_winsize();
	}
	else
	{
		check_winsize();		/* always check, to get p_scroll right */
		mch_set_winsize();
	}
	if (State == HELP)
		(void)redrawhelp();
	else if (!starting)
	{
		tmp = RedrawingDisabled;
		RedrawingDisabled = FALSE;
		comp_Botline_all();
		updateScreen(CURSUPD);
		RedrawingDisabled = tmp;
		if (State == CMDLINE)
			redrawcmdline();
		else
			setcursor();
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
 * enable cursor, unless in Visual mode or no inversion possible
 */
static int cursor_is_off = FALSE;

	void
cursor_on()
{
	if (cursor_is_off && (!VIsual.lnum || highlight == NULL))
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
	setscroll (wp->w_winpos + wp->w_height - 1, wp->w_winpos);

/*	OUTSTR(tgoto((char *)T_CS, wp->w_winpos + wp->w_height - 1, wp->w_winpos));*/
}

/*
 * reset scrolling region to the whole screen
 */
	void
scroll_region_reset()
{
	setscroll (Rows - 1, 0);

/*	OUTSTR(tgoto((char *)T_CS, (int)Rows - 1, 0)); */
}
