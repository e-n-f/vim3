/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * mark.c: functions for setting marks and jumping to them
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"

/*
 * This file contains routines to maintain and manipulate marks.
 */

static struct filemark namedfm[NMARKS];		/* new marks with file nr */

/*
 * setmark(c) - set named mark 'c' at current cursor position
 *
 * Returns OK on success, FAIL if no room for mark or bad name given.
 */
	int
setmark(c)
	int			c;
{
	int 		i;

	if (c >= 0x100)
		return FAIL;
	if (islower(c))
	{
		i = c - 'a';
		curbuf->b_namedm[i] = curwin->w_cursor;
		return OK;
	}
	if (isupper(c))
	{
		i = c - 'A';
		namedfm[i].mark = curwin->w_cursor;
		namedfm[i].fnum = curbuf->b_fnum;
		return OK;
	}
	return FAIL;
}

/*
 * setpcmark() - set the previous context mark to the current position
 *				 and insert it into the jump list
 */
	void
setpcmark()
{
	int i, to;
#ifdef ROTATE
	struct filemark tempmark;
#endif

	curwin->w_prev_pcmark = curwin->w_pcmark;
	curwin->w_pcmark = curwin->w_cursor;

#ifndef ROTATE
	/*
	 * simply add the new entry at the end of the list
	 */
	curwin->w_jumplistidx = curwin->w_jumplistlen;
#else
	/*
	 * If last used entry is not at the top, put it at the top by rotating
	 * the stack until it is (the newer entries will be at the bottom).
	 * Keep one entry (the last used one) at the top.
	 */
	if (curwin->w_jumplistidx < curwin->w_jumplistlen)
		++curwin->w_jumplistidx;
	while (curwin->w_jumplistidx < curwin->w_jumplistlen)
	{
		tempmark = curwin->w_jumplist[curwin->w_jumplistlen - 1];
		for (i = curwin->w_jumplistlen - 1; i > 0; --i)
			curwin->w_jumplist[i] = curwin->w_jumplist[i - 1];
		curwin->w_jumplist[0] = tempmark;
		++curwin->w_jumplistidx;
	}
#endif

	/*
	 * If entry already exists, remove it from the jump list.  New entry will
	 * be added at the front of the list.
	 */
	for (to = 0, i = 0; i < curwin->w_jumplistlen; i++)
	{
		if (curwin->w_jumplist[i].mark.lnum != curwin->w_pcmark.lnum ||
			curwin->w_jumplist[i].fnum != curbuf->b_fnum)
		{
			curwin->w_jumplist[to++] = curwin->w_jumplist[i];
		}
	}
	curwin->w_jumplistlen = to;

	/* If jumplist is full: remove oldest entry */
	if (++curwin->w_jumplistlen > JUMPLISTSIZE)
	{
		curwin->w_jumplistlen = JUMPLISTSIZE;
		for (i = 1; i < curwin->w_jumplistlen; ++i)
			curwin->w_jumplist[i - 1] = curwin->w_jumplist[i];
	}
	curwin->w_jumplistidx = curwin->w_jumplistlen - 1;

#ifdef ARCHIE
	/* Workaround for a bug in gcc 2.4.5 R2 on the Archimedes
	 * Should be fixed in 2.5.x.
	 */
	curwin->w_jumplist[curwin->w_jumplistidx].mark.ptr = curwin->w_pcmark.ptr;
	curwin->w_jumplist[curwin->w_jumplistidx].mark.col = curwin->w_pcmark.col;
#else
	curwin->w_jumplist[curwin->w_jumplistidx].mark = curwin->w_pcmark;
#endif
	curwin->w_jumplist[curwin->w_jumplistidx].fnum = curbuf->b_fnum;
	++curwin->w_jumplistidx;
}

/*
 * checkpcmark() - To change context, call setpcmark(), then move the current
 *				   position to where ever, then call checkpcmark().  This
 *				   ensures that the previous context will only be changed if
 *				   the cursor moved to a different line. -- webb.
 *				   If pcmark was deleted (with "dG") the previous mark is restored.
 */
	void
checkpcmark()
{
	if (curwin->w_prev_pcmark.lnum != 0 &&
			(curwin->w_pcmark.lnum == curwin->w_cursor.lnum ||
			curwin->w_pcmark.lnum == 0))
	{
		curwin->w_pcmark = curwin->w_prev_pcmark;
		curwin->w_prev_pcmark.lnum = 0;			/* Show it has been checked */
	}
}

/*
 * move "count" positions in the jump list (count may be negative)
 */
	FPOS *
movemark(count)
	int count;
{
	FPOS		*pos;

	if (curwin->w_jumplistlen == 0)			/* nothing to jump to */
		return (FPOS *)NULL;

	if (curwin->w_jumplistidx + count < 0 ||
						curwin->w_jumplistidx + count >= curwin->w_jumplistlen)
		return (FPOS *)NULL;

	/*
	 * if first CTRL-O or CTRL-I command after a jump, add cursor position to list
	 */
	if (curwin->w_jumplistidx == curwin->w_jumplistlen)
	{
		setpcmark();
		--curwin->w_jumplistidx;		/* skip the new entry */
	}

	curwin->w_jumplistidx += count;
												/* jump to other file */
	if (curwin->w_jumplist[curwin->w_jumplistidx].fnum != curbuf->b_fnum)
	{
		if (buflist_getfile(curwin->w_jumplist[curwin->w_jumplistidx].fnum,
					curwin->w_jumplist[curwin->w_jumplistidx].mark.lnum, FALSE) == FAIL)
			return (FPOS *)NULL;
		curwin->w_cursor.col = curwin->w_jumplist[curwin->w_jumplistidx].mark.col;
		pos = (FPOS *)-1;
	}
	else
		pos = &(curwin->w_jumplist[curwin->w_jumplistidx].mark);
	return pos;
}

/*
 * getmark(c) - find mark for char 'c'
 *
 * Return pointer to FPOS if found
 *        NULL if no such mark.
 *        -1 if mark is in other file (only if changefile is TRUE)
 */
	FPOS *
getmark(c, changefile)
	int			c;
	int			changefile;
{
	FPOS	*posp;
	static	FPOS	pos_copy;

	posp = NULL;
	if (c >= 0x100)
		;
	else if (c == '\'' || c == '`')		/* previous context mark */
	{
		pos_copy = curwin->w_pcmark;	/* need to make a copy because b_pcmark */
		posp = &pos_copy;				/*   may be changed soon */
	}
	else if (c == '[')					/* to start of previous operator */
	{
		if (curbuf->b_startop.lnum > 0 &&
						curbuf->b_startop.lnum <= curbuf->b_ml.ml_line_count)
			posp = &(curbuf->b_startop);
	}
	else if (c == ']')					/* to end of previous operator */
	{
		if (curbuf->b_endop.lnum > 0 &&
						curbuf->b_endop.lnum <= curbuf->b_ml.ml_line_count)
			posp = &(curbuf->b_endop);
	}
	else if (c == '<')
		posp = &VIsual;
	else if (islower(c))				/* normal named mark */
		posp = &(curbuf->b_namedm[c - 'a']);
	else if (isupper(c))				/* named file mark */
	{
		c -= 'A';
		posp = &(namedfm[c].mark);
		if (namedfm[c].fnum != curbuf->b_fnum &&
									namedfm[c].mark.lnum != 0 && changefile)
		{
			if (buflist_getfile(namedfm[c].fnum, namedfm[c].mark.lnum, TRUE) == OK)
			{
				curwin->w_cursor.col = namedfm[c].mark.col;
				posp = (FPOS *)-1;
			}
		}
	}
	return posp;
}

/*
 * clrallmarks() - clear all marks in the buffer 'buf'
 *
 * Used mainly when trashing the entire buffer during ":e" type commands
 */
	void
clrallmarks(buf)
	BUF		*buf;
{
	static int 			i = -1;

	if (i == -1)		/* first call ever: initialize */
		for (i = 0; i < NMARKS; i++)
			namedfm[i].mark.lnum = 0;

	for (i = 0; i < NMARKS; i++)
		buf->b_namedm[i].lnum = 0;
	buf->b_startop.lnum = 0;		/* start/end op mark cleared */
	buf->b_endop.lnum = 0;
}

/*
 * get name of file from a filemark
 */
	char_u *
fm_getname(fmark)
	struct filemark *fmark;
{
	char_u		*name;

	if (fmark->fnum != curbuf->b_fnum)				/* not current file */
	{
		name = buflist_nr2name(fmark->fnum);
		if (name == NULL)
			return (char_u *)"-unknown-";
		return name;
	}
	return (char_u *)"-current-";
}

/*
 * print the marks (use the occasion to update the line numbers)
 */
	void
domarks(arg)
	char_u		*arg;
{
	int			i;
	char_u		*name;

	if (arg != NULL && *arg == NUL)
		arg = NULL;

	set_highlight('t');		/* Highlight title */
	start_highlight();
	MSG_OUTSTR("\nmark line  col file");
	stop_highlight();
	for (i = 0; i < NMARKS; ++i)
	{
		if (curbuf->b_namedm[i].lnum != 0 &&
						(arg == NULL || STRCHR(arg, i +'a') != NULL))
		{
			sprintf((char *)IObuff, "\n %c %5ld  %3d", i + 'a',
												curbuf->b_namedm[i].lnum,
												curbuf->b_namedm[i].col);
			msg_outstr(IObuff);
		}
		flushbuf();
	}
	for (i = 0; i < NMARKS; ++i)
	{
		if (namedfm[i].mark.lnum != 0
			&& (arg == NULL || STRCHR(arg, i +'A') != NULL))
		{
			name = fm_getname(&namedfm[i]);
			if (name == NULL)		/* file name not available */
				continue;

			msg_outchar('\n');
			sprintf((char *)IObuff, " %c %5ld  %3d %s",
				i + 'A',
				namedfm[i].mark.lnum,
				namedfm[i].mark.col,
				name);
			msg_outtrans(IObuff);
		}
		flushbuf();				/* show one line at a time */
	}
}

/*
 * print the jumplist
 */
	void
dojumps()
{
	int			i;
	char_u		*name;

	set_highlight('t');		/* Highlight title */
	start_highlight();
	MSG_OUTSTR("\n jump line  file");
	stop_highlight();
	for (i = 0; i < curwin->w_jumplistlen; ++i)
	{
		if (curwin->w_jumplist[i].mark.lnum != 0)
		{
			name = fm_getname(&curwin->w_jumplist[i]);
			if (name == NULL)		/* file name not available */
				continue;

			msg_outchar('\n');
			sprintf((char *)IObuff, "%c %2d %5ld  %s",
				i == curwin->w_jumplistidx ? '>' : ' ',
				i + 1,
				curwin->w_jumplist[i].mark.lnum,
				name);
			msg_outtrans(IObuff);
		}
		flushbuf();
	}
	if (curwin->w_jumplistidx == curwin->w_jumplistlen)
		MSG_OUTSTR("\n>");
}

/*
 * adjust marks between line1 and line2 (inclusive) to move 'amount' lines
 * If 'amount' is MAXLNUM the mark is made invalid.
 */

#define one_adjust(add) \
	{ \
		lp = add; \
		if (*lp >= line1 && *lp <= line2) \
		{ \
			if (amount == MAXLNUM) \
				*lp = 0; \
			else \
				*lp += amount; \
		} \
	}

	void
mark_adjust(line1, line2, amount)
	linenr_t	line1;
	linenr_t	line2;
	long		amount;
{
	int			i;
	int			fnum = curbuf->b_fnum;
	linenr_t	*lp;
	WIN			*win;

	if (line2 < line1)		/* nothing to do */
		return;

/* named marks, lower case and upper case */
	for (i = 0; i < NMARKS; i++)
	{
		one_adjust(&(curbuf->b_namedm[i].lnum));
		if (namedfm[i].fnum == fnum)
			one_adjust(&(namedfm[i].mark.lnum));
	}

/* previous context mark */
	one_adjust(&(curwin->w_pcmark.lnum));

/* previous pcmark */
	one_adjust(&(curwin->w_prev_pcmark.lnum));

/* marks in the tag stack */
	for (i = 0; i < curwin->w_tagstacklen; i++)
	{
		if (curwin->w_tagstack[i].fmark.fnum == fnum)
		{
			lp = &(curwin->w_tagstack[i].fmark.mark.lnum);
			if (*lp >= line1 && *lp <= line2)
			{
				if (amount == MAXLNUM)
					*lp = line1;		/* don't delete it, just put at first
											deleted line */
				else
					*lp += amount;
			}
		}
	}

/* quickfix marks */
	qf_mark_adjust(line1, line2, amount);

/* jumplist marks */
	for (win = firstwin; win != NULL; win = win->w_next)
	{
		for (i = 0; i < win->w_jumplistlen; ++i)
			if (win->w_jumplist[i].fnum == fnum)
				one_adjust(&(win->w_jumplist[i].mark.lnum));
		/*
		 * also adjust the line at the top of the window and the cursor
		 * position for windows with the same buffer.
		 */
		if (win != curwin && win->w_buffer == curbuf)
		{
			if (win->w_topline >= line1 && win->w_topline <= line2)
			{
				if (amount == MAXLNUM)		/* topline is deleted */
				{
					if (line1 == 0)
						win->w_topline = 1;
					else
						win->w_topline = line1;
				}
				else					/* keep topline on the same line */
					win->w_topline += amount;
			}
			if (win->w_cursor.lnum >= line1 && win->w_cursor.lnum <= line2)
			{
				if (amount == MAXLNUM)		/* line with cursor is deleted */
				{
					win->w_cursor.lnum = line1;
					win->w_cursor.col = 0;
				}
				else					/* keep cursor on the same line */
					win->w_cursor.lnum += amount;
			}
		}
	}
}
