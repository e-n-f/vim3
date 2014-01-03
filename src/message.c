/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * message.c: functions for displaying messages on the command line
 */

#include "vim.h"
#include "globals.h"
#define MESSAGE			/* don't include prototype for smsg() */
#include "proto.h"
#include "param.h"

static int msg_check_screen __ARGS((void));

static int	lines_left = -1;			/* lines left for listing */
static int	quit_more = FALSE;			/* 'q' hit at "--more--" msg */

/*
 * msg(s) - displays the string 's' on the status line
 * return TRUE if wait_return not called
 */
	int
msg(s)
	char_u		   *s;
{
	if (!screen_valid())			/* terminal not initialized */
	{
		fprintf(stderr, (char *)s);
		fflush(stderr);
		return TRUE;
	}

	msg_start();
	if (msg_highlight)			/* actually it is highlighting instead of invert */
		start_highlight();
	msg_outtrans(s, -1);
	if (msg_highlight)
	{
		stop_highlight();
		msg_highlight = FALSE;		/* clear for next call */
	}
	msg_clr_eos();
	return msg_end();
}

/*
 * automatic prototype generation does not understand this function
 */
#ifndef PROTO
int smsg __ARGS((char_u *, long, long, long,
						long, long, long, long, long, long, long));

/* VARARGS */
	int
smsg(s, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
	char_u		*s;
	long		a1, a2, a3, a4, a5, a6, a7, a8, a9, a10;
{
	sprintf((char *)IObuff, (char *)s, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
	return msg(IObuff);
}
#endif

/*
 * emsg() - display an error message
 *
 * Rings the bell, if appropriate, and calls message() to do the real work
 *
 * return TRUE if wait_return not called
 */
	int
emsg(s)
	char_u		   *s;
{
	char_u			Buf[MAXPATHL + 30];
	int				retval;

	if (emsg_off)				/* no error messages at the moment */
		return TRUE;

	if (p_eb)
		beep_flush();			/* also includes flush_buffers() */
	else
		flush_buffers(FALSE);	/* flush internal buffers */
	(void)set_highlight('e');	/* set highlight mode for error messages */
	msg_highlight = TRUE;

/*
 * First output name of source of error message
 */
	if (sourcing_name)
	{
		++msg_scroll;			/* don't overwrite this message */
		++no_wait_return;
		sprintf((char *)Buf, "Error detected while processing %s:",
										sourcing_name);
		msg(Buf);
		msg_highlight = TRUE;
		--no_wait_return;
	}
/*
 * Msg returns TRUE if wait_return() was not called.
 * In that case may call sleep() to give the user a chance to read the message.
 * Don't call sleep() if dont_sleep is set.
 */
	if (msg(s))
	{
		if (dont_sleep || need_wait_return)
		{
			need_sleep = TRUE;		/* sleep before removing the message */
			msg_scroll = TRUE;		/* don't overwrite this message */
			sourcing_name = NULL;	/* don't repeat the sourcing name */
		}
		else
			sleep(1);			/* give the user a chance to read the message */
		retval = TRUE;
	}
	retval = FALSE;
	if (sourcing_name)
	{
		sourcing_name = NULL;		/* don't repeat the sourcing name */
		--msg_scroll;
	}
	return retval;
}

	int
emsg2(s, a1)
	char_u *s, *a1;
{
	sprintf((char *)IObuff, (char *)s, (char *)a1);
	return emsg(IObuff);
}

/*
 * like msg(), but truncate to a single line if p_shm set to 2
 */
	int
msg_trunc(s)
	char_u	*s;
{
	int		n;

	if (p_shm == 2 && (n = STRLEN(s) - sc_col + 1) > 0)
	{
		s[n] = '<';
		return msg(s + n);
	}
	else
		return msg(s);
}

/*
 * wait for the user to hit a key (normally a return)
 * if 'redraw' is TRUE, clear and redraw the screen
 * if 'redraw' is FALSE, just redraw the screen
 * if 'redraw' is -1, don't redraw at all
 */
	void
wait_return(redraw)
	int		redraw;
{
	int				c;
	int				oldState;
	int				tmpState;

	if (redraw == TRUE)
		must_redraw = CLEAR;
	skip_redraw = FALSE;			/* default: don't skip redraw */

/*
 * With the global command (and some others) we only need one return at the
 * end. Adjust cmdline_row to avoid the next message overwriting the last one.
 */
	if (no_wait_return)
	{
		need_wait_return = TRUE;
		cmdline_row = msg_row;
		if (!termcap_active)
			starttermcap();
		return;
	}
	need_wait_return = FALSE;
	need_sleep = FALSE;			/* no need to call sleep() anymore */
	msg_didany = FALSE;			/* reset lines_left at next msg_start() */
	lines_left = -1;
	oldState = State;
	if (quit_more)
	{
		c = CR;						/* just pretend CR was hit */
		quit_more = FALSE;
		got_int = FALSE;
	}
	else
	{
		State = HITRETURN;
		if (msg_didout)				/* start on a new line */
			msg_outchar('\n');
		if (got_int)
			msg_outstr((char_u *)"Interrupt: ");

		(void)set_highlight('r');
		start_highlight();
#ifdef ORG_HITRETURN
		msg_outstr("Press RETURN to continue");
		stop_highlight();
		do {
			c = vgetc();
		} while (c >= 0x100 || strchr("\r\n: ", c) == NULL);
		if (c == ':')			 		/* this can vi too (but not always!) */
			stuffcharReadbuff(c);
#else
		msg_outstr((char_u *)"Press RETURN or enter command to continue");
		stop_highlight();
		do
		{
			c = vgetc();
			got_int = FALSE;
		} while (c == Ctrl('C'));
		breakcheck();
		if (c >= 0x100 || strchr("\r\n ", c) == NULL)
			stuffcharReadbuff(c);
#endif
	}

	/*
	 * If the user hits ':' we get a command line from the next line.
	 */
	if (c == ':')
		cmdline_row = msg_row;

	if (!termcap_active)			/* start termcap before redrawing */
		starttermcap();

/*
 * If the window size changed set_winsize() will redraw the screen.
 * Otherwise the screen is only redrawn if 'redraw' is set and no ':' typed.
 */
	tmpState = State;
	State = oldState;				/* restore State before set_winsize */
	msg_check();
	if (tmpState == SETWSIZE)		/* got resize event while in vgetc() */
		set_winsize(0, 0, FALSE);
	else if (c != ':' && (redraw == TRUE || (msg_scrolled && redraw != -1)))
		updateScreen(VALID);

	if (c == ':')
		skip_redraw = TRUE;			/* skip redraw once */
	dont_wait_return = TRUE;		/* don't wait again in main() */
}

/*
 * Prepare for outputting characters in the command line.
 */
	void
msg_start()
{
	keep_msg = NULL;						/* don't display old message now */
	if (!msg_scroll && !not_full_screen)	/* overwrite last message */
		msg_pos(cmdline_row, 0);
	else if (msg_didout)					/* start message on next line */
	{
		msg_outchar('\n');
		cmdline_row = msg_row;
	}
	if (!msg_didany)
		lines_left = cmdline_row;
	msg_didout = FALSE;						/* no output on current line yet */
	cursor_off();
}

/*
 * Move message position. This should always be used after moving the cursor.
 * Use negative value if row or col does not have to be changed.
 */
	void
msg_pos(row, col)
	int		row, col;
{
	if (row >= 0)
		msg_row = row;
	if (col >= 0)
		msg_col = col;
	screen_start();
}

	void
msg_outchar(c)
	int		c;
{
	char_u		buf[3];

	if (c >= 0x100)
	{
		buf[0] = K_SPECIAL;
		buf[1] = K_SECOND(c);
		buf[2] = NUL;
	}
	else
	{
		buf[0] = c;
		buf[1] = NUL;
	}
	msg_outstr(buf);
}

	void
msg_outnum(n)
	long		n;
{
	char_u		buf[20];

	sprintf((char *)buf, "%ld", n);
	msg_outstr(buf);
}

/*
 * output 'len' characters in 'str' (including NULs) with translation
 * if 'len' is -1, output upto a NUL character
 * return the number of characters it takes on the screen
 */
	int
msg_outtrans(str, len)
	register char_u *str;
	register int   len;
{
	int retval = 0;

	if (len == -1)
		len = STRLEN(str);
	while (--len >= 0)
	{
		msg_outstr(transchar(*str));
		retval += charsize(*str);
		++str;
	}
	return retval;
}

/*
 * output the string 'str' upto a NUL character.
 * return the number of characters it takes on the screen.
 * If a character is in the range 0x80-0xa3 (0xb0-0xd3 for MSDOS), then it is
 * shown as <F1>, <C_UP> etc.  In addition, if 'all' is TRUE, then any other
 * character which has its 8th bit set is shown as M-x, where x is the
 * equivalent character without its 8th bit set.  If a character is displayed
 * in one of these special ways, is also highlighted (its highlight name is '8'
 * in the p_hl variable).  This function is used to show mappings, where we
 * want to see how to type the character/string.
 * -- webb
 */
	int
msg_outtrans_meta(str, all)
	register char_u *str;
	register int	all;	/* or just C_UP, F1 etc */
{
	int		retval = 0;
	char_u	string[20];
	char_u	**names;
	int		c;

	names = get_key_names();
	set_highlight('8');
	for (; *str; ++str)
	{
		c = *str;
		if (c == K_SPECIAL)
		{
			c = *++str;
			if (c >= KS_UARROW && c <= KS_MAXKEY)
			{
				start_highlight();
				sprintf((char *)string, "<%s>", names[c - KS_UARROW]);
				msg_outstr(string);
				retval += STRLEN(string);
				stop_highlight();
				continue;
			}
			if (c == KS_ZERO)
				c = NUL;
			else if (c == KS_SPECIAL)
				c = K_SPECIAL;
			/* else: illegal key code !? */
		}
		if ((c & 0x80) && all)
		{
			start_highlight();
			msg_outstr((char_u *)"M-");
			msg_outstr(transchar(c & 0x7f));
			retval += 2 + charsize(c & 0x7f);
			stop_highlight();
		}
		else
		{
			msg_outstr(transchar(c));
			retval += charsize(c);
		}
	}
	return retval;
}

/*
 * Return a list of the names of each special key.  To get the name of the
 * key with internal code 'key', use the index [key - KS_UARROW] into the
 * returned list.
 */
	char_u **
get_key_names()
{
	static char_u *names[] =
	{
		(char_u *)"C_UP", (char_u *)"C_DOWN", (char_u *)"C_LEFT",
		(char_u *)"C_RIGHT", (char_u *)"SC_UP", (char_u *)"SC_DOWN",
		(char_u *)"SC_LEFT", (char_u *)"SC_RIGHT", (char_u *)"F1",
		(char_u *)"F2", (char_u *)"F3", (char_u *)"F4", (char_u *)"F5",
		(char_u *)"F6", (char_u *)"F7", (char_u *)"F8", (char_u *)"F9",
		(char_u *)"F10", (char_u *)"SF1", (char_u *)"SF2", (char_u *)"SF3",
		(char_u *)"SF4", (char_u *)"SF5", (char_u *)"SF6", (char_u *)"SF7",
		(char_u *)"SF8", (char_u *)"SF9", (char_u *)"SF10",
		(char_u *)"HELP", (char_u *)"UNDO",
		(char_u *)"INSERT", (char_u *)"DEL",
		(char_u *)"HOME", (char_u *)"END",
		(char_u *)"PAGE_UP", (char_u *)"PAGE_DOWN",
		(char_u *)"MOUSE"
	};
	return names;
}

/*
 * print line for :p command
 */
	void
msg_prt_line(s)
	char_u		   *s;
{
	register int	si = 0;
	register int	c;
	register int	col = 0;

	int 			n_extra = 0;
	int             n_spaces = 0;
	char_u			*p = NULL;			/* init to make SASC shut up */
	int 			n;

	for (;;)
	{
		if (n_extra)
		{
			--n_extra;
			c = *p++;
		}
		else if (n_spaces)
		{
		    --n_spaces;
			c = ' ';
		}
		else
		{
			c = s[si++];
			if (c == TAB && !curwin->w_p_list)
			{
				/* tab amount depends on current column */
				n_spaces = curbuf->b_p_ts - col % curbuf->b_p_ts - 1;
				c = ' ';
			}
			else if (c == NUL && curwin->w_p_list)
			{
				p = (char_u *)"";
				n_extra = 1;
				c = '$';
			}
			else if (c != NUL && (n = charsize(c)) > 1)
			{
				n_extra = n - 1;
				p = transchar(c);
				c = *p++;
			}
		}

		if (c == NUL)
			break;

		msg_outchar(c);
		col++;
	}
}

/*
 * output a string to the screen at position msg_row, msg_col
 * Update msg_row and msg_col for the next message.
 */
	void
msg_outstr(s)
	char_u		*s;
{
	int		oldState;

	/*
	 * if there is no valid screen, use fprintf so we can see error messages
	 */
	if (!msg_check_screen())
	{
		fprintf(stderr, (char *)s);
		msg_didout = TRUE;			/* assume that line is not empty */
		return;
	}

	msg_didany = TRUE;			/* remember that something was outputted */
	while (*s)
	{
		/*
		 * the screen is scrolled up when:
		 * - When outputting a newline in the last row
		 * - when outputting a character in the last column of the last row
		 *   (some terminals scroll automatically, some don't. To avoid problems
		 *   we scroll ourselves)
		 */
		if (msg_row >= Rows - 1 && (*s == '\n' || msg_col >= Columns - 1))
		{
			screen_del_lines(0, 0, 1, (int)Rows);		/* always works */
			msg_row = Rows - 2;
			if (msg_col >= Columns)		/* can happen after screen resize */
				msg_col = Columns - 1;
			++msg_scrolled;
			dont_wait_return = FALSE;	/* may need wait_return in main() */
			if (cmdline_row > 0)
				--cmdline_row;
			/*
			 * if screen is completely filled wait for a character
			 */
			if (p_more && --lines_left == 0)
			{
				oldState = State;
				State = ASKMORE;
				screen_start();
				msg_moremsg();
				for (;;)
				{
					switch (vgetc())
					{
					case CR:			/* one extra line */
					case NL:
					case K_DARROW:
						lines_left = 1;
						break;
					case 'q':			/* quit */
					case Ctrl('C'):
						got_int = TRUE;
						quit_more = TRUE;
						break;
					case 'd':			/* Down half a page */
						lines_left = Rows / 2;
						break;
					case ' ':			/* one extra page */
					case K_PAGEDOWN:
						lines_left = Rows - 1;
						break;
					default:			/* no valid response */
						continue;
					}
					break;
				}
				screen_fill(Rows - 1, Rows, 0, (int)Columns, ' ', ' ');
				State = oldState;
				if (quit_more)
					return;			/* the string is not displayed! */
			}
			screen_start();
		}
		if (*s == '\n')
		{
			msg_didout = FALSE;			/* remember that line is empty */
			msg_col = 0;
			++msg_row;
		}
		else
		{
			msg_didout = TRUE;			/* remember that line is not empty */
			screen_outchar(*s, msg_row, msg_col);
			if (++msg_col >= Columns)
			{
				msg_col = 0;
				++msg_row;
			}
		}
		++s;
	}
}

	void
msg_moremsg()
{
	/*
	 * Need to restore old highlighting when we've finished with it
	 * because the output that's paging may be relying on it not
	 * changing -- webb
	 */
	remember_highlight();
	set_highlight('m');
	start_highlight();
	screen_msg((char_u *)"-- More -- (RET: line, SPACE: page, d: half page, q: quit)", (int)Rows - 1, 0);
	stop_highlight();
	recover_old_highlight();
}

/*
 * msg_check_screen - check if the screen is initialized.
 * Also check msg_row and msg_col, if they are too big it may cause a crash.
 */
	static int
msg_check_screen()
{
	if (not_full_screen || !screen_valid())
		return FALSE;
	
	if (msg_row >= Rows)
		msg_row = Rows - 1;
	if (msg_col >= Columns)
		msg_col = Columns - 1;
	return TRUE;
}

/*
 * clear from current message position to end of screen
 * Note: msg_col is not updated, so we remember the end of the message
 * for msg_check().
 */
	void
msg_clr_eos()
{
	if (!msg_check_screen())
		return;
	screen_fill(msg_row, msg_row + 1, msg_col, (int)Columns, ' ', ' ');
	screen_fill(msg_row + 1, (int)Rows, 0, (int)Columns, ' ', ' ');
}

/*
 * end putting a message on the screen
 * call wait_return if the message does not fit in the available space
 * return TRUE if wait_return not called.
 */
	int
msg_end()
{
	/*
	 * if the string is larger than the window,
	 * or the ruler option is set and we run into it,
	 * we have to redraw the window.
	 * Do not do this if we are abandoning the file or editing the command line.
	 */
	if (!exiting && msg_check() && State != CMDLINE)
	{
		wait_return(FALSE);
		return FALSE;
	}
	flushbuf();
	return TRUE;
}

/*
 * If the written message has caused the screen to scroll up, or if we
 * run into the shown command or ruler, we have to redraw the window later.
 */
	int
msg_check()
{
	if (msg_scrolled || (msg_row == Rows - 1 && msg_col >= sc_col))
	{
		redraw_later(NOT_VALID);
		redraw_cmdline = TRUE;
		return TRUE;
	}
	return FALSE;
}
