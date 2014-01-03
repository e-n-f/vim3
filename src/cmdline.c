/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * cmdline.c: functions for reading in the command line and executing it
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"
#include "cmdtab.h"
#include "ops.h"			/* included because we call functions in ops.c */
#include "fcntl.h"			/* for chdir() */

/*
 * variables shared between getcmdline() and redrawcmdline()
 */
static int		 cmdlen;		/* number of chars on command line */
static int		 cmdpos;		/* current cursor position */
static int		 cmdspos;		/* cursor column on screen */
static int		 cmdfirstc; 	/* ':', '/' or '?' */
static char_u	*cmdbuff;		/* pointer to command line buffer */

/*
 * The next two variables contain the bounds of any range given in a command.
 * They are set by docmdline().
 */
static linenr_t 	line1, line2;

static int			forceit;
static int			regname;
static int			quitmore = 0;
static int  		cmd_numfiles = -1;	  /* number of files found by
													filename completion */
/*
 * There are two history tables:
 * 0: colon commands
 * 1: search commands
 */
static	 char_u		**(history[2]) = {NULL, NULL};	/* history tables */
static	 int		hisidx[2] = {-1, -1};			/* last entered entry */
static	 int		hislen = 0; 		/* actual lengt of history tables */

static void		init_history __ARGS((void));

static int		is_in_history __ARGS((int, char_u *, int));
static void		putcmdline __ARGS((int, char_u *));
static void		cursorcmd __ARGS((void));
static int		ccheck_abbr __ARGS((int));
static char_u	*DoOneCmd __ARGS((char_u *, int));
static int		buf_write_all __ARGS((BUF *));
static int		dowrite __ARGS((char_u *, int));
static char_u	*getargcmd __ARGS((char_u **));
static void		domake __ARGS((char_u *));
static int		doarglist __ARGS((char_u *));
static int		check_readonly __ARGS((void));
static int		check_changed __ARGS((BUF *, int, int));
static int		check_changed_any __ARGS((int));
static int		check_more __ARGS((int));
static void		vim_strncpy __ARGS((char_u *, char_u *, int));
static int		nextwild __ARGS((char_u *, int));
static int		showmatches __ARGS((char_u *));
static linenr_t get_address __ARGS((char_u **));
static void		set_expand_context __ARGS((int, char_u *));
static char_u	*set_one_cmd_context __ARGS((int, char_u *));
static int		ExpandFromContext __ARGS((char_u *, int *, char_u ***, int, int));

/*
 * init_history() - initialize the command line history
 */
	static void
init_history()
{
	int		newlen;			/* new length of history table */
	char_u	**temp;
	register int i;
	int		j = -1;
	int		type;

	/*
	 * If size of history table changed, reallocate it
	 */
	newlen = (int)p_hi;
	if (newlen != hislen)						/* history length changed */
	{
		for (type = 0; type <= 1; ++type)		/* adjust both history tables */
		{
			if (newlen)
				temp = (char_u **)lalloc((long_u)(newlen * sizeof(char_u *)),
									TRUE);
			else
				temp = NULL;
			if (newlen == 0 || temp != NULL)
			{
				if (newlen > hislen)			/* array becomes bigger */
				{
					for (i = 0; i <= hisidx[type]; ++i)
						temp[i] = history[type][i];
					j = i;
					for ( ; i <= newlen - (hislen - hisidx[type]); ++i)
						temp[i] = NULL;
					for ( ; j < hislen; ++i, ++j)
						temp[i] = history[type][j];
				}
				else							/* array becomes smaller */
				{
					j = hisidx[type];
					for (i = newlen - 1; ; --i)
					{
						if (i >= 0)				/* copy newest entries */
							temp[i] = history[type][j];
						else					/* remove older entries */
							free(history[type][j]);
						if (--j < 0)
							j = hislen - 1;
						if (j == hisidx[type])
							break;
					}
					hisidx[type] = newlen - 1;
				}
				free(history[type]);
				history[type] = temp;
			}
		}
		hislen = newlen;
	}
}

/*
 * check if command line 'str' is already in history
 * 'type' is 0 for ':' commands, '1' for search commands
 * if 'move_to_front' is TRUE, matching entry is moved to end of history
 */
	static int
is_in_history(type, str, move_to_front)
	int		type;
	char_u	*str;
	int		move_to_front;		/* Move the entry to the front if it exists */
{
	int		i;
	int		last_i = -1;

	if (hisidx[type] < 0)
		return FALSE;
	i = hisidx[type];
	do
	{
		if (history[type][i] == NULL)
			return FALSE;
		if (STRCMP(str, history[type][i]) == 0)
		{
			if (!move_to_front)
				return TRUE;
			last_i = i;
			break;
		}
		if (--i < 0)
			i = hislen - 1;
	} while (i != hisidx[type]);

	if (last_i >= 0)
	{
		str = history[type][i];
		while (i != hisidx[type])
		{
			if (++i >= hislen)
				i = 0;
			history[type][last_i] = history[type][i];
			last_i = i;
		}
		history[type][i] = str;
		return TRUE;
	}
	return FALSE;
}

/*
 * getcmdline() - accept a command line starting with ':', '!', '/', or '?'
 *
 * For searches the optional matching '?' or '/' is removed.
 *
 * Return OK if there is a commandline, FAIL if not
 */

	int
getcmdline(firstc, buff)
	int			firstc; 	/* either ':', '/', or '?' */
	char_u		*buff;	 	/* buffer for command string */
{
	register int	 	c;
			 int		cc;
	register int		i;
			 int		retval;
			 int		hiscnt;				/* current history line in use */
			 char_u		*lookfor = NULL;	/* string to match */
			 int		j = -1;
			 int		gotesc = FALSE;		/* TRUE when last char typed was <ESC> */
			 int		do_abbr;			/* when TRUE check for abbr. */
			 int		type;				/* history type to be used */
			 FPOS		old_cursor;
			 colnr_t	old_curswant = curwin->w_curswant;
			 int		did_incsearch = FALSE;
			 int		incsearch_postponed = FALSE;
			 int		overstrike = FALSE;	/* typing mode */

	old_cursor = curwin->w_cursor;
/*
 * set some variables for redrawcmd()
 */
	cmdfirstc = firstc;
	cmdbuff = buff;
	cmdlen = cmdpos = 0;
	cmdspos = 1;
	State = CMDLINE;
	gotocmdline(TRUE);
	msg_outchar(firstc);

	init_history();
	hiscnt = hislen;			/* set hiscnt to impossible history value */
	type = (firstc == ':' ? 0 : 1);

#ifdef DIGRAPHS
	dodigraph(-1);				/* init digraph typahead */
#endif

	/* collect the command string, handling '\b', @ and much more */
	for (;;)
	{
		cursorcmd();	/* set the cursor on the right spot */
		c = vgetc();
		if (c == Ctrl('C'))
			got_int = FALSE;

		if (lookfor && c != K_SDARROW && c != K_SUARROW &&
				c != K_DARROW && c != K_UARROW &&
				c != K_PAGEDOWN && c != K_PAGEUP &&
				(cmd_numfiles > 0 || (c != Ctrl('P') && c != Ctrl('N'))))
		{
			free(lookfor);
			lookfor = NULL;
		}

		if (cmd_numfiles != -1 && !(c == p_wc && KeyTyped) && c != Ctrl('N') &&
						c != Ctrl('P') && c != Ctrl('A') && c != Ctrl('L'))
			(void)ExpandOne(NULL, FALSE, -2);	/* may free expanded file names */

#ifdef DIGRAPHS
		c = dodigraph(c);
#endif

		if (c == '\n' || c == '\r' || (c == ESC && !KeyTyped))
		{
			if (ccheck_abbr(c + 0x200))
				goto cmdline_changed;
			outchar('\r');		/* show that we got the return */
			flushbuf();
			break;
		}

			/* hitting <ESC> twice means: abandon command line */
			/* wildcard expansion is only done when the key is really typed, not
			   when it comes from a macro */
		if (c == p_wc && !gotesc && KeyTyped)
		{
			if (cmd_numfiles > 0)	/* typed p_wc twice */
				i = nextwild(buff, 3);
			else					/* typed p_wc first time */
				i = nextwild(buff, 0);
			if (c == ESC)
				gotesc = TRUE;
			if (i)
				goto cmdline_changed;
		}
		gotesc = FALSE;

		if (c == K_ZERO)		/* NUL is stored as NL */
			c = '\n';

		do_abbr = TRUE;			/* default: check for abbreviation */
		switch (c)
		{
		case BS:
		case DEL:
		case K_DEL:
		case Ctrl('W'):
				/*
				 * delete current character is the same as backspace on next
				 * character, except at end of line
				 */
				if ((c == DEL || c == K_DEL) && cmdpos != cmdlen)
					++cmdpos;
				if (cmdpos > 0)
				{
					j = cmdpos;
					if (c == Ctrl('W'))
					{
						while (cmdpos && isspace(buff[cmdpos - 1]))
							--cmdpos;
						i = isidchar(buff[cmdpos - 1]);
						while (cmdpos && !isspace(buff[cmdpos - 1]) && isidchar(buff[cmdpos - 1]) == i)
							--cmdpos;
					}
					else
						--cmdpos;
					cmdlen -= j - cmdpos;
					i = cmdpos;
					while (i < cmdlen)
						buff[i++] = buff[j++];
					redrawcmd();
				}
				else if (cmdlen == 0 && c != Ctrl('W'))
				{
					retval = FAIL;
					msg_pos(-1, 0);
					msg_outchar(' ');	/* delete ':' */
					goto returncmd; 	/* back to cmd mode */
				}
				goto cmdline_changed;

		case K_INS:
				overstrike = !overstrike;
				goto cmdline_not_changed;

/*		case '@':	only in very old vi */
		case Ctrl('U'):
				cmdpos = 0;
				cmdlen = 0;
				cmdspos = 1;
				redrawcmd();
				goto cmdline_changed;

		case ESC:			/* get here if p_wc != ESC or when ESC typed twice */
		case Ctrl('C'):
do_esc:
				retval = FAIL;
				MSG("");
				goto returncmd; 	/* back to cmd mode */

		case Ctrl('D'):
			{
				if (showmatches(buff) == FAIL)
					break;		/* Use ^D as normal char instead */

				redrawcmd();
				continue;		/* don't do incremental search now */
			}

		case K_RARROW:
		case K_SRARROW:
				do
				{
						if (cmdpos >= cmdlen)
								break;
						cmdspos += charsize(buff[cmdpos]);
						++cmdpos;
				}
				while (c == K_SRARROW && buff[cmdpos] != ' ');
				goto cmdline_not_changed;

		case K_LARROW:
		case K_SLARROW:
				do
				{
						if (cmdpos <= 0)
								break;
						--cmdpos;
						cmdspos -= charsize(buff[cmdpos]);
				}
				while (c == K_SLARROW && buff[cmdpos - 1] != ' ');
				goto cmdline_not_changed;

#if defined(UNIX) || defined(MSDOS)
		case K_MOUSE:
				cmdspos = 1;
				for (cmdpos = 0; cmdpos < cmdlen; ++cmdpos)
				{
					i = charsize(cmdbuff[cmdpos]);
					if (mouse_row <= cmdline_row + cmdspos / Columns &&
										mouse_col < cmdspos % Columns + i)
						break;
					cmdspos += i;
				}
				goto cmdline_not_changed;
#endif

		case Ctrl('B'):		/* begin of command line */
		case K_HOME:
				cmdpos = 0;
				cmdspos = 1;
				goto cmdline_not_changed;

		case Ctrl('E'):		/* end of command line */
		case K_END:
				cmdpos = cmdlen;
				buff[cmdlen] = NUL;
				cmdspos = strsize(buff) + 1;
				goto cmdline_not_changed;

		case Ctrl('A'):		/* all matches */
				if (!nextwild(buff, 4))
					break;
				goto cmdline_changed;

		case Ctrl('L'):		/* longest common part */
				if (!nextwild(buff, 5))
					break;
				goto cmdline_changed;

		case Ctrl('N'):		/* next match */
		case Ctrl('P'):		/* previous match */
				if (cmd_numfiles > 0)
				{
					if (!nextwild(buff, (c == Ctrl('P')) ? 2 : 1))
						break;
					goto cmdline_changed;
				}

		case K_UARROW:
		case K_DARROW:
		case K_SUARROW:
		case K_SDARROW:
		case K_PAGEUP:
		case K_PAGEDOWN:
				if (hislen == 0)		/* no history */
					goto cmdline_not_changed;

				i = hiscnt;
			
					/* save current command string */
	/* Always save the current command line so that we can restore it later
	 * -- webb
	 */
				buff[cmdpos] = NUL;
				if (lookfor == NULL && (lookfor = strsave(buff)) == NULL)
					goto cmdline_not_changed;

				j = STRLEN(lookfor);
				for (;;)
				{
						/* one step backwards */
					if (c == K_UARROW || c == K_SUARROW || c == Ctrl('P') ||
							c == K_PAGEUP)
					{
						if (hiscnt == hislen)	/* first time */
							hiscnt = hisidx[type];
						else if (hiscnt == 0 && hisidx[type] != hislen - 1)
							hiscnt = hislen - 1;
						else if (hiscnt != hisidx[type] + 1)
							--hiscnt;
						else					/* at top of list */
						{
							hiscnt = i;
							break;
						}
					}
					else	/* one step forwards */
					{
						if (hiscnt == hisidx[type])	/* on last entry, clear the line */
						{
							hiscnt = hislen;
							break;
						}
						if (hiscnt == hislen)	/* not on a history line, nothing to do */
							break;
						if (hiscnt == hislen - 1)	/* wrap around */
							hiscnt = 0;
						else
							++hiscnt;
					}
					if (hiscnt < 0 || history[type][hiscnt] == NULL)
					{
						hiscnt = i;
						break;
					}
					if ((c != K_SUARROW && c != K_SDARROW && c != K_PAGEUP &&
							c != K_PAGEDOWN) || hiscnt == i ||
							STRNCMP(history[type][hiscnt], lookfor, (size_t)j) == 0)
						break;
				}

				if (hiscnt != i)		/* jumped to other entry */
				{
					if (hiscnt == hislen)
						STRCPY(buff, lookfor);
					else
						STRCPY(buff, history[type][hiscnt]);
					cmdpos = cmdlen = STRLEN(buff);
					redrawcmd();
					goto cmdline_changed;
				}
				beep_flush();
				goto cmdline_not_changed;

		case Ctrl('V'):
				putcmdline('^', buff);
				c = get_literal();			/* get next (two) character(s) */
				do_abbr = FALSE;			/* don't do abbreviation now */
				break;

#ifdef DIGRAPHS
		case Ctrl('K'):
				putcmdline('?', buff);
			  	c = vgetc();
				if (c == ESC)
					goto do_esc;
				if (charsize(c) == 1)
					putcmdline(c, buff);
				cc = vgetc();
				if (cc == ESC)
					goto do_esc;
				c = getdigraph(c, cc, TRUE);
				break;
#endif /* DIGRAPHS */
		}

		/* we come here if we have a normal character */

		if (do_abbr && (c >= 0x100 || !isidchar(c)) && ccheck_abbr(c))
			goto cmdline_changed;

		if (cmdlen < CMDBUFFSIZE - 3)
		{
			/*
			 * If a special key was typed that is not used as a command,
			 * insert it in the buffer as two chars: K_SPECIAL, KS_..
			 * This is useful when mapping function keys.
			 */
			while (c > 0)
			{
				if (c >= 0x100)
				{
					i = K_SPECIAL;
					c = K_SECOND(c);
				}
				else
				{
					i = c;
					c = -1;
				}
				if (!overstrike)
				{
					memmove(buff + cmdpos + 1, buff + cmdpos, cmdlen - cmdpos);
					++cmdlen;
				}
				buff[cmdpos] = i;
				msg_outtrans(buff + cmdpos, cmdlen - cmdpos);
				++cmdpos;
				cmdspos += charsize(i);
			}
		}
		msg_check();
		goto cmdline_changed;

/*
 * This part implements incremental searches for "/" and "?"
 * Jump to cmdline_not_changed when a character has been read but the command
 * line did not change. Then we only search and redraw if something changed in
 * the past.
 * Jump to cmdline_changed when the command line did change.
 * (Sorry for the goto's, I know it is ugly).
 */
cmdline_not_changed:
		if (!incsearch_postponed)
			continue;

cmdline_changed:
		if (p_is && (firstc == '/' || firstc == '?') && KeyTyped)
		{
				/* if there is a character waiting, search and redraw later */
			if (char_avail())
			{
				incsearch_postponed = TRUE;
				continue;
			}
			incsearch_postponed = FALSE;
			curwin->w_cursor = old_cursor;	/* start at old position */

				/* If there is no command line, don't do anything */
			if (cmdlen == 0)
				i = 0;
			else
			{
				buff[cmdlen] = NUL;
				emsg_off = TRUE;	/* So it doesn't beep if bad expr */
				keep_old_search_pattern = TRUE;
				tag_busy = TRUE;
				i = dosearch(firstc, buff, FALSE, 1, FALSE, FALSE);
				keep_old_search_pattern = FALSE;
				tag_busy = FALSE;
				emsg_off = FALSE;
			}
			if (i)
			{
				highlight_match = TRUE;			/* highlight position */
				cursupdate();
			}
			else
			{
				highlight_match = FALSE;			/* don't highlight */
				/* beep(); */ /* even beeps when invalid expr, e.g. "[" */
			}
			updateScreen(NOT_VALID);
			redrawcmdline();
			did_incsearch = TRUE;
		}
	}
	retval = OK;				/* when we get here we have a valid command line */

returncmd:
	if (did_incsearch)
	{
		curwin->w_cursor = old_cursor;
		curwin->w_curswant = old_curswant;
		highlight_match = FALSE;
		redraw_later(NOT_VALID);
	}
	buff[cmdlen] = NUL;
	/*
	 * put line in history buffer (only when it was typed)
	 */
	if (cmdlen != 0 && KeyTyped)
	{
		if (hislen != 0 && !is_in_history(type, buff, TRUE))
		{
			if (++hisidx[type] == hislen)
				hisidx[type] = 0;
			free(history[type][hisidx[type]]);
			history[type][hisidx[type]] = strsave(buff);
		}
		if (firstc == ':')
		{
			free(new_last_cmdline);
			new_last_cmdline = strsave(buff);
		}
	}

	/*
	 * If the screen was shifted up, redraw the whole screen (later).
	 * If the line is too long, clear it, so ruler and shown command do
	 * not get printed in the middle of it.
	 */
	msg_check();
	State = NORMAL;
	return retval;
}

/*
 * put a character on the command line.
 * Used for CTRL-V and CTRL-K
 */
	static void
putcmdline(c, buff)
	int		c;
	char_u	*buff;
{
	char_u	buf[2];

	buf[0] = c;
	buf[1] = 0;
	msg_outtrans(buf, 1);
	msg_outtrans(buff + cmdpos, cmdlen - cmdpos);
	cursorcmd();
}

/*
 * this fuction is called when the screen size changes
 */
	void
redrawcmdline()
{
	msg_scrolled = 0;
	compute_cmdrow();
	redrawcmd();
	cursorcmd();
}

	void
compute_cmdrow()
{
	cmdline_row = lastwin->w_winpos + lastwin->w_height + lastwin->w_status_height;
}

/*
 * Redraw what is currently on the command line.
 */
	void
redrawcmd()
{
	register int	i;

	msg_start();
	msg_outchar(cmdfirstc);
	msg_outtrans(cmdbuff, cmdlen);
	msg_clr_eos();

	cmdspos = 1;
	for (i = 0; i < cmdlen && i < cmdpos; ++i)
		cmdspos += charsize(cmdbuff[i]);
}

	static void
cursorcmd()
{
	msg_pos(cmdline_row + (cmdspos / (int)Columns), cmdspos % (int)Columns);
	windgoto(msg_row, msg_col);
}

/*
 * Check the word in front of the cursor for an abbreviation.
 * Called when the non-id character "c" has been entered.
 * When an abbreviation is recognized it is removed from the text with
 * backspaces and the replacement string is inserted, followed by "c".
 */
	static int
ccheck_abbr(c)
	int c;
{
	if (p_paste || no_abbr)			/* no abbreviations or in paste mode */
		return FALSE;
	
	return check_abbr(c, cmdbuff, cmdpos, 0);
}

/*
 * docmdline(): execute an Ex command line
 *
 * 1. If no line given, get one.
 * 2. Split up in parts separated with '|'.
 *
 * This function may be called recursively!
 * 
 * If 'sourcing' is TRUE, the command will be included in the error message.
 * If 'repeating' is TRUE, there is no wait_return() and friends.
 *
 * return FAIL if commandline could not be executed, OK otherwise
 */
	int
docmdline(cmdline, sourcing, repeating)
	char_u		*cmdline;
	int			sourcing;
	int			repeating;
{
	char_u		buff[CMDBUFFSIZE];		/* command line */
	char_u		*nextcomm;
	static int	recursive = 0;			/* recursive depth */

/*
 * 1. If no line given: get one.
 */
	if (cmdline == NULL)
	{
		if (getcmdline(':', buff) == FAIL)
		{
				/* don't call wait_return for aborted command line */
			need_wait_return = FALSE;
			dont_wait_return = TRUE;
			return FAIL;
		}
	}
	else
	{
		if (STRLEN(cmdline) > (size_t)(CMDBUFFSIZE - 2))
		{
			emsg(e_toolong);
			return FAIL;
		}
		/* Make a copy of the command so we can mess with it. */
		STRCPY(buff, cmdline);
	}

/*
 * All output from the commands is put below each other, without waiting for a
 * return. Don't do this when executing commands from a script or when being
 * called recursive (e.g. for ":e +command file").
 */
	if (!repeating && !recursive)
	{
		msg_didany = FALSE;		/* no output yet */
		msg_start();
		msg_scroll = TRUE;		/* put messages below each other */
		++dont_sleep;			/* don't sleep in emsg() */
		++no_wait_return;		/* dont wait for return until finished */
		++RedrawingDisabled;
	}

/*
 * 2. Loop for each '|' separated command.
 *    DoOneCmd will set nextcommand to NULL if there is no trailing '|'.
 */
	++recursive;
	for (;;)
	{
		nextcomm = DoOneCmd(buff, sourcing);
		if (nextcomm == NULL)
			break;
		STRCPY(buff, nextcomm);
	}
	--recursive;

/*
 * if there was too much output to fit on the command line, ask the user to
 * hit return before redrawing the screen. With the ":global" command we do
 * this only once after the command is finished.
 */
	if (!repeating && !recursive)
	{
		--RedrawingDisabled;
		--dont_sleep;
		--no_wait_return;
		msg_scroll = FALSE;
		if (need_wait_return || msg_check())
			wait_return(FALSE);
	}

/*
 * If the command was typed, remember it for register :
 * Do this AFTER executing the command to make :@: work.
 */
	if (cmdline == NULL && new_last_cmdline != NULL)
	{
		free(last_cmdline);
		last_cmdline = new_last_cmdline;
		new_last_cmdline = NULL;
	}
	return OK;
}

/*
 * Execute one Ex command.
 *
 * If 'sourcing' is TRUE, the command will be included in the error message.
 *
 * 2. skip comment lines and leading space
 * 3. parse range
 * 4. parse command
 * 5. parse arguments
 * 6. switch on command name
 *
 * This function may be called recursively!
 */
	static char_u *
DoOneCmd(buff, sourcing)
	char_u		*buff;
	int			sourcing;
{
	char_u				cmdbuf[CMDBUFFSIZE];	/* for '%' and '#' expansion */
	char_u				c;
	register char_u		*p;
	char_u				*q;
	char_u				*cmd, *arg;
	char_u				*editcmd = NULL;		/* +command arg. for doecmd() */
	linenr_t 			doecmdlnum = 0;			/* lnum in new file for doecmd() */
	int 				i = 0;					/* init to shut up gcc */
	int					cmdidx;
	int					argt;
	register linenr_t	lnum;
	long				n;
	int					addr_count;	/* number of address specifications */
	FPOS				pos;
	int					append = FALSE;			/* write with append */
	int					usefilter = FALSE;		/* filter instead of file name */
	char_u				*nextcomm = NULL;		/* no next command yet */
	int					amount = 0;				/* for ":>" and ":<"; init for gcc */
	char_u				*errormsg = NULL;		/* error message */
	WIN					*old_curwin = NULL;		/* init for GCC */

	if (quitmore)
		--quitmore;		/* when not editing the last file :q has to be typed twice */
/*
 * 2. skip comment lines and leading space, colons or bars
 */
	for (cmd = buff; *cmd && strchr(" \t:|", *cmd) != NULL; cmd++)
		;

	if (*cmd == '"' || *cmd == NUL)	/* ignore comment and empty lines */
		goto doend;

/*
 * 3. parse a range specifier of the form: addr [,addr] [;addr] ..
 *
 * where 'addr' is:
 *
 * %		  (entire file)
 * $  [+-NUM]
 * 'x [+-NUM] (where x denotes a currently defined mark)
 * .  [+-NUM]
 * [+-NUM]..
 * NUM
 *
 * The cmd pointer is updated to point to the first character following the
 * range spec. If an initial address is found, but no second, the upper bound
 * is equal to the lower.
 */

	addr_count = 0;
	--cmd;
	do
	{
		++cmd;							/* skip ',' or ';' */
		line1 = line2;
		line2 = curwin->w_cursor.lnum;	/* default is current line number */
		skipwhite(&cmd);
		lnum = get_address(&cmd);
		if (cmd == NULL)				/* error detected */
			goto doend;
		if (lnum == MAXLNUM)
		{
			if (*cmd == '%')            /* '%' - all lines */
			{
				++cmd;
				line1 = 1;
				line2 = curbuf->b_ml.ml_line_count;
				++addr_count;
			}
		}
		else
			line2 = lnum;
		addr_count++;

		if (*cmd == ';')
		{
			if (line2 == 0)
				curwin->w_cursor.lnum = 1;
			else
				curwin->w_cursor.lnum = line2;
		}
	} while (*cmd == ',' || *cmd == ';');

	/* One address given: set start and end lines */
	if (addr_count == 1)
	{
		line1 = line2;
			/* ... but only implicit: really no address given */
		if (lnum == MAXLNUM)
			addr_count = 0;
	}

/*
 * 4. parse command
 */

	skipwhite(&cmd);

	/*
	 * If we got a line, but no command, then go to the line.
	 * If we find a '|' or '\n' we set nextcomm.
	 */
	if (*cmd == NUL || *cmd == '"' ||
			((*cmd == '|' || *cmd == '\n') &&
					(nextcomm = cmd + 1) != NULL))		/* just an assignment */
	{
		if (addr_count != 0)
		{
			/*
			 * strange vi behaviour: ":3" jumps to line 3
			 * ":3|..." prints line 3
			 */
			if (*cmd == '|')
			{
				cmdidx = CMD_print;
				goto cmdswitch;			/* UGLY goto */
			}
			if (line2 == 0)
				curwin->w_cursor.lnum = 1;
			else if (line2 > curbuf->b_ml.ml_line_count)
				curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
			else
				curwin->w_cursor.lnum = line2;
			beginline(MAYBE);
			cursupdate();
		}
		goto doend;
	}

	/*
	 * Isolate the command and search for it in the command table.
	 * Exeptions:
	 * - the 'k' command can directly be followed by any character.
	 * - the 's' command can be followed directly by 'c', 'g' or 'r'
	 *		but :sre[wind] is another command.
	 */
	if (*cmd == 'k')
	{
		cmdidx = CMD_k;
		p = cmd + 1;
	}
	else if (*cmd == 's' && strchr("cgr", cmd[1]) != NULL && STRNCMP("sre", cmd, (size_t)3) != 0)
	{
		cmdidx = CMD_substitute;
		p = cmd + 1;
	}
	else
	{
		p = cmd;
		while (isalpha(*p))
			++p;
		if (p == cmd && strchr("@!=><&~#", *p) != NULL)	/* non-alpha command */
			++p;
		i = (int)(p - cmd);

		for (cmdidx = 0; cmdidx < CMD_SIZE; ++cmdidx)
			if (STRNCMP(cmdnames[cmdidx].cmd_name, (char *)cmd, (size_t)i) == 0)
				break;
		if (i == 0 || cmdidx == CMD_SIZE)
		{
			errormsg = e_invcmd;
			goto doend;
		}
	}

	if (*p == '!')					/* forced commands */
	{
		++p;
		forceit = TRUE;
	}
	else
		forceit = FALSE;

/*
 * 5. parse arguments
 */
	argt = cmdnames[cmdidx].cmd_argt;

	if (!(argt & RANGE) && addr_count)		/* no range allowed */
	{
		errormsg = e_norange;
		goto doend;
	}

	if (!(argt & BANG) && forceit)			/* no <!> allowed */
	{
		errormsg = e_nobang;
		goto doend;
	}

/*
 * If the range is backwards, ask for confirmation and, if given, swap
 * line1 & line2 so it's forwards again.
 * When global command is busy, don't ask, will fail below.
 */
	if (!global_busy && line1 > line2)
	{
		if (sourcing)
		{
			errormsg = (char_u *)"Backwards range given";
			goto doend;
		}
		else if (ask_yesno((char_u *)"Backwards range given, OK to swap", FALSE) != 'y')
			goto doend;
		lnum = line1;
		line1 = line2;
		line2 = lnum;
	}
	/*
	 * don't complain about the range if it is not used
	 * (could happen if line_count is accidently set to 0)
	 */
	if (line1 < 0 || line2 < 0  || line1 > line2 || ((argt & RANGE) &&
					!(argt & NOTADR) && line2 > curbuf->b_ml.ml_line_count))
	{
		errormsg = e_invrange;
		goto doend;
	}

	if ((argt & NOTADR) && addr_count == 0)		/* default is 1, not cursor */
		line2 = 1;

	if (!(argt & ZEROR))			/* zero in range not allowed */
	{
		if (line1 == 0)
			line1 = 1;
		if (line2 == 0)
			line2 = 1;
	}

	/*
	 * for the :make command we insert the 'makeprg' option here,
	 * so things like % get expanded
	 */
	if (cmdidx == CMD_make)
	{
		if (STRLEN(p_mp) + STRLEN(p) + 2 >= (unsigned)CMDBUFFSIZE)
		{
			errormsg = e_toolong;
			goto doend;
		}
		STRCPY(cmdbuf, p_mp);
		STRCAT(cmdbuf, " ");
		STRCAT(cmdbuf, p);
		STRCPY(buff,   cmdbuf);
		p = buff;
	}

	arg = p;						/* remember start of argument */
	skipwhite(&arg);

	if ((argt & NEEDARG) && *arg == NUL)
	{
		errormsg = e_argreq;
		goto doend;
	}

	if (cmdidx == CMD_write)
	{
		if (*arg == '>')						/* append */
		{
			if (*++arg != '>')				/* typed wrong */
			{
				errormsg = (char_u *)"Use w or w>>";
				goto doend;
			}
			++arg;
			skipwhite(&arg);
			append = TRUE;
		}
		else if (*arg == '!')					/* :w !filter */
		{
			++arg;
			usefilter = TRUE;
		}
	}

	if (cmdidx == CMD_read)
	{
		usefilter = forceit;					/* :r! filter if forceit */
		if (*arg == '!')						/* :r !filter */
		{
			++arg;
			usefilter = TRUE;
		}
	}

	if (cmdidx == CMD_lshift || cmdidx == CMD_rshift)
	{
		amount = 1;
		while (*arg == *cmd)		/* count number of '>' or '<' */
		{
			++arg;
			++amount;
		}
		skipwhite(&arg);
	}

	/*
	 * Check for "+command" argument, before checking for next command.
	 * Don't do this for ":read !cmd" and ":write !cmd".
	 */
	if ((argt & EDITCMD) && !usefilter)
		editcmd = getargcmd(&arg);

	/*
	 * Check for '|' to separate commands and '"' to start comments.
	 * Don't do this for ":read !cmd" and ":write !cmd".
	 */
	if ((argt & TRLBAR) && !usefilter)
	{
		p = arg;
		while (*p)
		{
			if (*p == Ctrl('V'))
			{
				if ((argt & USECTRLV) && p[1] != NUL)
					++p;				/* skip CTRL-V and next char */
				else
					STRCPY(p, p + 1);	/* remove CTRL-V and skip next char */
			}
			else if ((*p == '"' && !(argt & NOTRLCOM)) ||
										*p == '|' || *p == '\n')
			{
				if (*(p - 1) == '\\')	/* remove the backslash */
				{
					STRCPY(p - 1, p);
					--p;
				}
				else
				{
					if (*p == '|' || *p == '\n')
						nextcomm = p + 1;
					*p = NUL;
					break;
				}
			}
			++p;
		}
		if (!(argt & NOTRLCOM))			/* remove trailing spaces */
			del_trailing_spaces(arg);
	}

	if ((argt & DFLALL) && addr_count == 0)
	{
		line1 = 1;
		line2 = curbuf->b_ml.ml_line_count;
	}

	regname = 0;
		/* accept numbered register only when no count allowed (:put) */
	if ((argt & REGSTR) && *arg != NUL && is_yank_buffer(*arg, FALSE) && !((argt & COUNT) && isdigit(*arg)))
	{
		regname = *arg;
		++arg;
		skipwhite(&arg);
	}

	if ((argt & COUNT) && isdigit(*arg))
	{
		n = getdigits(&arg);
		skipwhite(&arg);
		if (n <= 0)
		{
			errormsg = e_zerocount;
			goto doend;
		}
		if (argt & NOTADR)		/* e.g. :buffer 2, :sleep 3 */
		{
			line2 = n;
			if (addr_count == 0)
				addr_count = 1;
		}
		else
		{
			line1 = line2;
			line2 += n - 1;
			++addr_count;
		}
	}

	if (!(argt & EXTRA) && strchr("|\"", *arg) == NULL)	/* no arguments allowed */
	{
		errormsg = e_trailing;
		goto doend;
	}

	/*
	 * change '%' to curbuf->b_filename, '#' to curwin->w_altfile
	 */
	if (argt & XFILE)
	{
		for (p = arg; *p; ++p)
		{
			c = *p;
			if (c != '%' && c != '#')	/* nothing to expand */
				continue;
			if (*(p - 1) == '\\')		/* remove escaped char */
			{
				STRCPY(p - 1, p);
				--p;
				continue;
			}

			if (c == '%')				/* current file */
			{
				if (check_fname() == FAIL)
					goto doend;
				q = curbuf->b_xfilename;
				n = 1;					/* length of what we expand */
			}
			else						/* '#': alternate file */
			{
				q = p + 1;
				i = (int)getdigits(&q);
				n = q - p;				/* length of what we expand */

				if (buflist_name_nr(i, &q, &doecmdlnum) == FAIL)
				{
					errormsg = e_noalt;
					goto doend;
				}
			}
			i = STRLEN(arg) + STRLEN(q) + 3;
			if (nextcomm)
				i += STRLEN(nextcomm);
			if (i > CMDBUFFSIZE)
			{
				errormsg = e_toolong;
				goto doend;
			}
			/*
			 * we built the new argument in cmdbuf[], then copy it back to buff[]
			 */
			*p = NUL;							/* truncate at the '#' or '%' */
			STRCPY(cmdbuf, arg);				/* copy up to there */
			i = p - arg;						/* remember the lenght */
			STRCAT(cmdbuf, q);					/* append the file name */
			if (*(p + n) == '<')				/* may remove extension */
			{
				++n;
				if ((arg = (char_u *)strrchr((char *)q, '.')) != NULL &&
								arg >= gettail(q))
					*(cmdbuf + (arg - q) + i) = NUL;
			}
			i = STRLEN(cmdbuf);					/* remember the end of the filename */
			STRCAT(cmdbuf, p+n);				/* append what is after '#' or '%' */
			p = buff + i - 1;					/* remember where to continue */
			if (nextcomm)						/* append next command */
			{
				i = STRLEN(cmdbuf) + 1;
				STRCPY(cmdbuf + i, nextcomm);
				nextcomm = buff + i;
			}
			STRCPY(buff, cmdbuf);				/* copy back to buff[] */
			arg = buff;
		}

		/*
		 * One file argument: expand wildcards.
		 * Don't do this with ":r !command" or ":w !command".
		 */
		if ((argt & NOSPC) && !usefilter)
		{
#if defined(UNIX) || defined(MSDOS)
			/*
			 * Only for Unix and MSDOS we check for more than one file name.
			 * For other systems spaces are considered to be part
			 * of the file name.
			 */
			for (p = arg; *p; ++p)
			{
							/* skip escaped characters */
				if (p[1] && (*p == '\\' || *p == Ctrl('V')))
					++p;
				else if (iswhite(*p))
				{
					errormsg = (char_u *)"Only one file name allowed";
					goto doend;
				}
			}
#endif
			if (has_wildcard(arg))
			{
				if ((p = ExpandOne(arg, TRUE, -1)) == NULL)
					goto doend;
				if (STRLEN(p) + arg - buff < CMDBUFFSIZE - 2)
					STRCPY(arg, p);
				else
					emsg(e_toolong);
				free(p);
			}
		}
	}

/*
 * 6. switch on command name
 */
cmdswitch:
	switch (cmdidx)
	{
		/*
		 * quit current window, quit Vim if closed the last window
		 */
		case CMD_quit:
						/* if more files or windows we won't exit */
				if (check_more(FALSE) == OK && firstwin == lastwin)
					exiting = TRUE;
				if (check_changed(curbuf, FALSE, FALSE) ||
							check_more(TRUE) == FAIL ||
							(firstwin == lastwin && check_changed_any(FALSE)))
				{
					exiting = FALSE;
					settmode(1);
					break;
				}
				if (firstwin == lastwin)	/* quit last window */
					getout(0);
				close_window(curwin, TRUE);	/* may free buffer */
				break;

		/*
		 * try to quit all windows
		 */
		case CMD_qall:
				exiting = TRUE;
				if (!check_changed_any(FALSE))
					getout(0);
				exiting = FALSE;
				settmode(1);
				break;

		/*
		 * close current window, unless it is the last one
		 */
		case CMD_close:
				close_window(curwin, FALSE);	/* don't free buffer */
				break;

		/*
		 * close all but current window, unless it is the last one
		 */
		case CMD_only:
				close_others(TRUE);
				break;

		case CMD_stop:
		case CMD_suspend:
				if (!forceit)
					autowrite_all();
				windgoto((int)Rows - 1, 0);
				outchar('\n');
				flushbuf();
				stoptermcap();
				mch_restore_title(3);	/* restore window titles */
				mch_suspend();			/* call machine specific function */
				maketitle();
				starttermcap();
				scroll_start();			/* scroll screen before redrawing */
				must_redraw = CLEAR;
				set_winsize(0, 0, FALSE); /* May have resized window -- webb */
				break;

		case CMD_exit:
		case CMD_xit:
		case CMD_wq:
							/* if more files or windows we won't exit */
				if (check_more(FALSE) == OK && firstwin == lastwin)
					exiting = TRUE;
				if (((cmdidx == CMD_wq ||
						(curbuf->b_nwindows == 1 && curbuf->b_changed)) &&
						(check_readonly() ||
								dowrite(arg, FALSE) == FAIL)) ||
					check_more(TRUE) == FAIL || 
					(firstwin == lastwin && check_changed_any(FALSE)))
				{
					exiting = FALSE;
					settmode(1);
					break;
				}
				if (firstwin == lastwin)	/* quit last window, exit Vim */
					getout(0);
				close_window(curwin, TRUE);	/* quit current window, may free buffer */
				break;

		case CMD_xall:		/* write all changed files and exit */
		case CMD_wqall:		/* write all changed files and quit */
				exiting = TRUE;
				/* FALLTHROUGH */

		case CMD_wall:		/* write all changed files */
				{
					BUF		*buf;
					int		error = 0;

					for (buf = firstbuf; buf != NULL; buf = buf->b_next)
					{
						if (buf->b_changed)
						{
							if (buf->b_filename == NULL)
							{
								emsg(e_noname);
								++error;
							}
							else if (!forceit && buf->b_p_ro)
							{
								EMSG2("\"%s\" is readonly, use ! to write anyway", buf->b_xfilename);
								++error;
							}
							else if (buf_write_all(buf) == FAIL)
								++error;
						}
					}
					if (exiting)
					{
						if (!error)
							getout(0);			/* exit Vim */
						exiting = FALSE;
						settmode(1);
					}
				}
				break;

		case CMD_preserve:					/* put everything in .swp file */
				ml_preserve(curbuf, TRUE);
				break;

		case CMD_recover:					/* recover file */
				recoverymode = TRUE;
				if (!check_changed(curbuf, FALSE, TRUE) &&
							(*arg == NUL || setfname(arg, NULL, TRUE) == OK) &&
							check_fname() == OK)
				{
					dellines(curbuf->b_ml.ml_line_count, FALSE, FALSE);
					ml_recover();
				}
				recoverymode = FALSE;
				break;

		case CMD_args:		
					/*
					 * ":args file": handle like :next
					 */
				if (*arg != NUL && *arg != '|' && *arg != '\n')
					goto do_next;

				if (arg_count == 0)				/* no file name list */
				{
					if (check_fname() == OK)	/* check for no file name */
						smsg((char_u *)"[%s]", curbuf->b_filename);
					break;
				}
				/*
				 * Overwrite the command, in most cases there is no scrolling
				 * required and no wait_return().
				 */
				gotocmdline(TRUE);
				for (i = 0; i < arg_count; ++i)
				{
					if (i == curwin->w_arg_idx)
						msg_outchar('[');
					msg_outstr(arg_files[i]);
					if (i == curwin->w_arg_idx)
						msg_outchar(']');
					msg_outchar(' ');
				}
				break;

		case CMD_wnext:
		case CMD_wNext:
		case CMD_wprevious:
				if (cmd[1] == 'n')
					i = curwin->w_arg_idx + (int)line2;
				else
					i = curwin->w_arg_idx - (int)line2;
				line1 = 1;
				line2 = curbuf->b_ml.ml_line_count;
				if (dowrite(arg, FALSE) == FAIL)
					break;
				goto donextfile;

		case CMD_next:
		case CMD_snext:
do_next:
					/*
					 * check for changed buffer now, if this fails the
					 * argument list is not redefined.
					 */
				if (!(p_hid || cmdidx == CMD_snext) &&
								check_changed(curbuf, TRUE, FALSE))
					break;

				if (*arg != NUL)				/* redefine file list */
				{
					if (doarglist(arg) == FAIL)
						break;
					i = 0;
				}
				else
					i = curwin->w_arg_idx + (int)line2;

donextfile:		if (i < 0 || i >= arg_count)
				{
					if (arg_count == 1)
						EMSG("There is only one file to edit");
					else if (i < 0)
						EMSG("Cannot go before first file");
					else
						EMSG("Cannot go beyond last file");
					break;
				}
				if (*cmd == 's')		/* split window first */
				{
					if (win_split(0L, FALSE) == FAIL)
						break;
				}
				else
				{
					register int other = FALSE;

					/*
					 * if 'hidden' set, only check for changed file when
					 * re-editing the same buffer
					 */
					other = TRUE;
					if (p_hid)
						other = otherfile(fix_fname(arg_files[i]));
					if ((!p_hid || !other) &&
										check_changed(curbuf, TRUE, !other))
					break;
				}
				curwin->w_arg_idx = i;
				if (i == arg_count - 1)
					arg_had_last = TRUE;
				(void)doecmd(0, arg_files[curwin->w_arg_idx],
										NULL, editcmd, p_hid, doecmdlnum);
				break;

		case CMD_previous:
		case CMD_sprevious:
		case CMD_Next:
		case CMD_sNext:
				i = curwin->w_arg_idx - (int)line2;
				goto donextfile;

		case CMD_rewind:
		case CMD_srewind:
				i = 0;
				goto donextfile;

		case CMD_last:
		case CMD_slast:
				i = arg_count - 1;
				goto donextfile;

		case CMD_argument:
		case CMD_sargument:
				if (addr_count)
					i = line2 - 1;
				else
					i = curwin->w_arg_idx;
				goto donextfile;

		case CMD_all:
		case CMD_sall:
				do_arg_all();		/* open a window for each argument */
				break;

		case CMD_buffer:			/* :[N]buffer [N]	 to buffer N */
		case CMD_sbuffer:			/* :[N]sbuffer [N]	 to buffer N */
				if (addr_count == 0)		/* default is current buffer */
					(void)do_buffer(*cmd == 's', 0, FORWARD, 0, 0);
				else
					(void)do_buffer(*cmd == 's', 1, FORWARD, (int)line2, 0);
				break;

		case CMD_bmodified:			/* :[N]bmod	[N]	  to next modified buffer */
		case CMD_sbmodified:		/* :[N]sbmod [N]  to next modified buffer */
				(void)do_buffer(*cmd == 's', 3, FORWARD, (int)line2, 0);
				break;

		case CMD_bnext:				/* :[N]bnext [N]	 to next buffer */
		case CMD_sbnext:			/* :[N]sbnext [N]	 to next buffer */
				(void)do_buffer(*cmd == 's', 0, FORWARD, (int)line2, 0);
				break;

		case CMD_bNext:				/* :[N]bNext [N]	 to previous buffer */
		case CMD_bprevious:			/* :[N]bprevious [N] to previous buffer */
		case CMD_sbNext:			/* :[N]sbNext [N]	  to previous buffer */
		case CMD_sbprevious:		/* :[N]sbprevious [N] to previous buffer */
				(void)do_buffer(*cmd == 's', 0, BACKWARD, (int)line2, 0);
				break;

		case CMD_brewind:			/* :brewind			 to first buffer */
		case CMD_sbrewind:			/* :sbrewind		 to first buffer */
				(void)do_buffer(*cmd == 's', 1, FORWARD, 0, 0);
				break;

		case CMD_blast:				/* :blast        	 to last buffer */
		case CMD_sblast:			/* :sblast        	 to last buffer */
				(void)do_buffer(*cmd == 's', 2, FORWARD, 0, 0);
				break;

		case CMD_bunload:			/* :[N]bunload[!] [N] unload buffer */
				i = 2;
		case CMD_bdelete:			/* :[N]bdelete[!] [N] delete buffer */
				if (cmdidx == CMD_bdelete)
					i = 3;
				/*
				 * addr_count == 0: ":bdel" - delete current buffer
				 * addr_count == 1: ":N bdel" or ":bdel N [N ..] - first delete
				 *					buffer 'line2', then any other arguments.
				 * addr_count == 2: ":N,N bdel" - delete buffers in range
				 */
				if (addr_count == 0)
					(void)do_buffer(i, 0, FORWARD, 0, forceit);
				else
				{
					int do_current = 0;			/* delete current buffer? */
					int	deleted = 0;			/* number of buffers deleted */

					if (addr_count == 2)
						n = line1;
					else
						n = line2;
					for ( ;!got_int; breakcheck())
					{
						/*
						 * delete the current buffer last, otherwise when the
						 * current buffer is deleted, the next buffer becomes
						 * the current one and will be loaded, which may then
						 * also be deleted, etc.
						 */
						if (n == curbuf->b_fnum)
							do_current = n;
						else if (do_buffer(i, 1, FORWARD, (int)n, forceit) == OK)
							++deleted;
						if (addr_count == 2)
						{
							if (++n > line2)
								break;
						}
						else
						{
							skipwhite(&arg);
							if (*arg == NUL)
								break;
							if (!isdigit(*arg))
							{
								emsg(e_trailing);
								break;
							}
							n = getdigits(&arg);
						}
					}
					if (!got_int && do_current && do_buffer(i, 1, FORWARD, (int)do_current, forceit) == OK)
						++deleted;

					if (deleted == 0)
						EMSG2("No buffers were %s",
											i == 2 ? "unloaded" : "deleted");
					else
						smsg((char_u *)"%d buffer%s %s", deleted,
											plural(deleted),
											i == 2 ? "unloaded" : "deleted");
				}
				break;

		case CMD_unhide:
		case CMD_sunhide:
				(void)do_buffer_all(FALSE);	/* open a window for loaded buffers */
				break;

		case CMD_ball:
		case CMD_sball:
				(void)do_buffer_all(TRUE);	/* open a window for every buffer */
				break;

		case CMD_buffers:
		case CMD_files:
		case CMD_ls:
				buflist_list();
				break;

		case CMD_write:
				if (usefilter)		/* input lines to shell command */
					dofilter(line1, line2, arg, TRUE, FALSE);
				else
					(void)dowrite(arg, append);
				break;

			/*
			 * set screen mode
			 * if no argument given, just get the screen size and redraw
			 */
		case CMD_mode:
				if (*arg == NUL || mch_screenmode(arg) != FAIL)
					set_winsize(0, 0, FALSE);
				break;

				/*
				 * set, increment or decrement current window height
				 */
		case CMD_resize:
				n = atol((char *)arg);
				if (*arg == '-' || *arg == '+')
					win_setheight(curwin->w_height + (int)n);
				else
				{
					if (n == 0)		/* default is very high */
						n = 9999;
					win_setheight((int)n);
				}
				break;

				/*
				 * :split [[+command] file]  split window with current or new file
				 * :new [[+command] file]    split window with no or new file
				 */
		case CMD_split:
		case CMD_new:
				old_curwin = curwin;
				if (win_split(addr_count ? line2 : 0L, FALSE) == FAIL)
					break;
				/*FALLTHROUGH*/

		case CMD_edit:
		case CMD_ex:
		case CMD_visual:
				if ((cmdidx == CMD_new) && *arg == NUL)
					(void)doecmd(0, NULL, NULL, editcmd, TRUE, (linenr_t)1);
				else if (cmdidx != CMD_split || *arg != NUL)
					(void)doecmd(0, arg, NULL, editcmd, p_hid, doecmdlnum);
				else
					updateScreen(NOT_VALID);
					/* if ":split file" worked, set alternate filename in
					 * old window to new file */
				if ((cmdidx == CMD_new || cmdidx == CMD_split) &&
								*arg != NUL && curwin != old_curwin &&
								old_curwin->w_buffer != curbuf)
					old_curwin->w_alt_fnum = curbuf->b_fnum;
				break;

		case CMD_file:
				i = p_shm;
				if (forceit && p_shm == 2)
					p_shm = 1;
				if (*arg != NUL)
				{
					curwin->w_alt_fnum = curbuf->b_fnum;
					buflist_altlnum();
					if (setfname(arg, NULL, TRUE) == FAIL)
						break;
					curbuf->b_notedited = TRUE;
				}
				fileinfo(did_cd);		/* print full filename if :cd used */
				p_shm = i;
				break;

		case CMD_swapname:
				if (curbuf->b_ml.ml_mfp == NULL ||
								(p = curbuf->b_ml.ml_mfp->mf_fname) == NULL)
					MSG("No swap file");
				else
					msg(p);
				break;

		case CMD_mfstat:		/* print memfile statistics, for debugging */
				mf_statistics();
				break;

		case CMD_read:
				if (usefilter)
				{
					dofilter(line1, line2, arg, FALSE, TRUE);			/* :r!cmd */
					break;
				}
				if (u_save(line2, (linenr_t)(line2 + 1)) == FAIL)
					break;
				if (*arg == NUL)
				{
					if (check_fname() == FAIL)	/* check for no file name at all */
						break;
					i = readfile(curbuf->b_filename, curbuf->b_sfilename, line2, FALSE, (linenr_t)0, MAXLNUM);
				}
				else
					i = readfile(arg, NULL, line2, FALSE, (linenr_t)0, MAXLNUM);
				if (i == FAIL)
				{
					emsg2(e_notopen, arg);
					break;
				}
				updateScreen(NOT_VALID);
				break;

		case CMD_cd:
		case CMD_chdir:
#ifdef UNIX
				/*
				 * for UNIX ":cd" means: go to home directory
				 */
				if (*arg == NUL)	 /* use cmdbuf for home directory name */
				{
					expand_env((char_u *)"$HOME", cmdbuf, CMDBUFFSIZE);
					arg = cmdbuf;
				}
#endif
				if (*arg != NUL)
				{
					if (!did_cd)
					{
						BUF		*buf;

							/* use full path from now on for names of files
							 * being edited and swap files */
						for (buf = firstbuf; buf != NULL; buf = buf->b_next)
						{
							buf->b_xfilename = buf->b_filename;
							mf_fullname(buf->b_ml.ml_mfp);
						}
						status_redraw_all();
					}
					did_cd = TRUE;
					if (chdir((char *)arg))
						emsg(e_failed);
					break;
				}
				/*FALLTHROUGH*/

		case CMD_pwd:
				if (vim_dirname(NameBuff, MAXPATHL) == OK)
					msg(NameBuff);
				else
					emsg(e_unknown);
				break;

		case CMD_equal:
				smsg((char_u *)"line %ld", (long)line2);
				break;

		case CMD_list:
				i = curwin->w_p_list;
				curwin->w_p_list = 1;
		case CMD_number:				/* :nu */
		case CMD_pound:					/* :# */
		case CMD_print:					/* :p */
				for ( ;!got_int; breakcheck())
				{
					msg_outchar('\n');
					if (curwin->w_p_nu || cmdidx == CMD_number || cmdidx == CMD_pound)
					{
						sprintf((char *)IObuff, "%7ld ", (long)line1);
						set_highlight('n');		/* Highlight line numbers */
						start_highlight();
						msg_outstr(IObuff);
						stop_highlight();
					}
					msg_prt_line(ml_get(line1));
					if (++line1 > line2)
						break;
					flushbuf();			/* show one line at a time */
				}
				setpcmark();
				curwin->w_cursor.lnum = line2;	/* put cursor at last line */

				if (cmdidx == CMD_list)
					curwin->w_p_list = i;

				break;

		case CMD_shell:
				doshell(NULL);
				break;

		case CMD_sleep:
				sleep((int)line2);
				break;

		case CMD_stag:
				postponed_split = TRUE;
				/*FALLTHROUGH*/
		case CMD_tag:
				dotag(arg, 0, addr_count ? (int)line2 : 1);
				break;

		case CMD_pop:
				dotag((char_u *)"", 1, addr_count ? (int)line2 : 1);
				break;

		case CMD_tags:
				dotags();
				break;

		case CMD_marks:
				domarks();
				break;

		case CMD_jumps:
				dojumps();
				break;

		case CMD_checkpath:
				find_pattern_in_path(NULL, 0, FALSE, CHECK_PATH, 1, 1,
											(linenr_t)1, (linenr_t)MAXLNUM);
				break;

		case CMD_digraphs:
#ifdef DIGRAPHS
				if (*arg)
					putdigraph(arg);
				else
					listdigraphs();
#else
				EMSG("No digraphs in this version");
#endif /* DIGRAPHS */
				break;

		case CMD_set:
				(void)doset(arg);
				break;

		case CMD_autocmd:
				do_autocmd(arg, forceit);	/* manipulate the auto commands */
				break;

		case CMD_doautocmd:
				apply_autocmds(arg);		/* apply the automa commands */
				break;

		case CMD_abbreviate:
		case CMD_cabbrev:
		case CMD_iabbrev:
		case CMD_cnoreabbrev:
		case CMD_inoreabbrev:
		case CMD_noreabbrev:
		case CMD_unabbreviate:
		case CMD_cunabbrev:
		case CMD_iunabbrev:
				i = ABBREV;
				goto doabbr;		/* almost the same as mapping */

		case CMD_cmap:
		case CMD_imap:
		case CMD_map:
		case CMD_cnoremap:
		case CMD_inoremap:
		case CMD_noremap:
				/*
				 * If we are sourcing .exrc or .vimrc in current directory we
				 * print the mappings for security reasons.
				 */
				if (secure)
				{
					secure = 2;
					msg_outtrans(cmd, -1);
					msg_outchar('\n');
				}
		case CMD_cunmap:
		case CMD_iunmap:
		case CMD_unmap:
				i = 0;
doabbr:
				if (*cmd == 'c')		/* cmap, cunmap, cnoremap, etc. */
				{
					i += CMDLINE;
					++cmd;
				}
				else if (*cmd == 'i')	/* imap, iunmap, inoremap, etc. */
				{
					i += INSERT;
					++cmd;
				}
				else if (forceit || i)	/* map!, unmap!, noremap!, abbrev */
					i += INSERT + CMDLINE;
				else
					i += NORMAL;			/* map, unmap, noremap */
				switch (domap((*cmd == 'n') ? 2 : (*cmd == 'u'), arg, i))
				{
					case 1: emsg(e_invarg);
							break;
					case 2: emsg(e_nomap);
							break;
					case 3: emsg(e_ambmap);
							break;
				}
				break;

		case CMD_display:
				dodis();		/* display buffer contents */
				break;

		case CMD_help:
				help();
				break;

		case CMD_version:
				msg(longVersion);
				break;

		case CMD_winsize:					/* obsolete command */
				line1 = getdigits(&arg);
				skipwhite(&arg);
				line2 = getdigits(&arg);
				set_winsize((int)line1, (int)line2, TRUE);
				break;

		case CMD_delete:
		case CMD_yank:
		case CMD_rshift:
		case CMD_lshift:
				yankbuffer = regname;
				curbuf->b_startop.lnum = line1;
				curbuf->b_endop.lnum = line2;
				nlines = line2 - line1 + 1;
				mtype = MLINE;
				curwin->w_cursor.lnum = line1;
				switch (cmdidx)
				{
				case CMD_delete:
					dodelete();
					break;
				case CMD_yank:
					(void)doyank(FALSE);
					curwin->w_cursor.lnum = line2;		/* put cursor on last line */
					break;
				case CMD_rshift:
					doshift(RSHIFT, FALSE, amount);
					break;
				case CMD_lshift:
					doshift(LSHIFT, FALSE, amount);
					break;
				}
				break;

		case CMD_put:
				yankbuffer = regname;
				curwin->w_cursor.lnum = line2;
				doput(forceit ? BACKWARD : FORWARD, -1L, FALSE);
				break;

		case CMD_t:
		case CMD_copy:
		case CMD_move:
				n = get_address(&arg);
				if (arg == NULL)			/* error detected */
				{
					nextcomm = NULL;
					goto doend;
				}
				/*
				 * move or copy lines from 'line1'-'line2' to below line 'n'
				 */
				if (n == MAXLNUM || n < 0 || n > curbuf->b_ml.ml_line_count)
				{
					emsg(e_invaddr);
					break;
				}

				if (cmdidx == CMD_move)
				{
					if (do_move(line1, line2, n) == FAIL)
						break;
				}
				else
					do_copy(line1, line2, n);
				u_clearline();
				beginline(MAYBE);
				updateScreen(NOT_VALID);
				break;

		case CMD_and:			/* :& */
		case CMD_tilde:			/* :~ */
		case CMD_substitute:	/* :s */
				dosub(line1, line2, arg, &nextcomm,
							cmdidx == CMD_substitute ? 0 :
							cmdidx == CMD_and ? 1 : 2);
				break;

		case CMD_join:
				curwin->w_cursor.lnum = line1;
				if (line1 == line2)
				{
					if (addr_count >= 2)	/* :2,2join does nothing */
						break;
					if (line2 == curbuf->b_ml.ml_line_count)
					{
						beep_flush();
						break;
					}
					++line2;
				}
				dodojoin(line2 - line1 + 1, !forceit, TRUE);
				break;

		case CMD_global:
				if (forceit)
					*cmd = 'v';
		case CMD_vglobal:
				doglob(*cmd, line1, line2, arg);
				break;

		case CMD_at:				/* :[addr]@r */
				curwin->w_cursor.lnum = line2;
									/* put the register in mapbuf */
				if (doexecbuf(*arg) == FAIL)
					beep_flush();
				else
									/* execute from the mapbuf */
					(void)docmdline((char_u *)NULL, TRUE, TRUE);
				break;

		case CMD_bang:
				dobang(addr_count, line1, line2, forceit, arg);
				break;

		case CMD_undo:
				u_undo(1);
				break;

		case CMD_redo:
				u_redo(1);
				break;

		case CMD_source:
				if (forceit)					/* :so! read vi commands */
					(void)openscript(arg);
				else if (dosource(arg) == FAIL)	/* :so read ex commands */
					emsg2(e_notopen, arg);
				break;

#ifdef VIMINFO
		case CMD_rviminfo:
				n = p_viminfo;
				if (!p_viminfo)
					p_viminfo = 100;
				if (read_viminfo(arg, TRUE, TRUE, forceit) == FAIL)
					emsg("Cannot open viminfo file for reading");
				p_viminfo = n;
				break;

		case CMD_wviminfo:
				n = p_viminfo;
				if (!p_viminfo)
					p_viminfo = 100;
				write_viminfo(arg, forceit);
				p_viminfo = n;
				break;
#endif /* VIMINFO */

		case CMD_mkvimrc:
				if (*arg == NUL)
					arg = (char_u *)VIMRC_FILE;
				/*FALLTHROUGH*/

		case CMD_mkexrc:
				{
					FILE	*fd;

					if (*arg == NUL)
						arg = (char_u *)EXRC_FILE;
#ifdef UNIX
						/* with Unix it is possible to open a directory */
					if (isdir(arg) == TRUE)
					{
						EMSG2("\"%s\" is a directory", arg);
						break;
					}
#endif
					if (!forceit && (fd = fopen((char *)arg, "r")) != NULL)
					{
						fclose(fd);
						EMSG2("\"%s\" exists (use ! to override)", arg);
						break;
					}

					if ((fd = fopen((char *)arg, "w")) == NULL)
					{
						EMSG2("Cannot open \"%s\" for writing", arg);
						break;
					}
					if (makemap(fd) == FAIL || makeset(fd) == FAIL || fclose(fd))
						emsg(e_write);
					break;
				}

		case CMD_cc:
					qf_jump(0, addr_count ? (int)line2 : 0);
					break;

		case CMD_cfile:
					if (*arg != NUL)
					{
						/*
						 * Great trick: Insert 'ef=' before arg.
						 * Always ok, because "cf " must be there.
						 */
						arg -= 3;
						arg[0] = 'e';
						arg[1] = 'f';
						arg[2] = '=';
						(void)doset(arg);
					}
					(void)qf_init();
					break;

		case CMD_clist:
					qf_list(forceit);
					break;

		case CMD_cnext:
					qf_jump(FORWARD, addr_count ? (int)line2 : 1);
					break;

		case CMD_cprevious:
					qf_jump(BACKWARD, addr_count ? (int)line2 : 1);
					break;

		case CMD_cquit:
					getout(1);		/* this does not always work. why? */

		case CMD_mark:
		case CMD_k:
					pos = curwin->w_cursor;			/* save curwin->w_cursor */
					curwin->w_cursor.lnum = line2;
					beginline(MAYBE);
					(void)setmark(*arg);			/* set mark */
					curwin->w_cursor = pos;			/* restore curwin->w_cursor */
					break;

		case CMD_center:
		case CMD_right:
		case CMD_left:
					do_align(line1, line2, atoi((char *)arg),
							cmdidx == CMD_center ? 0 : cmdidx == CMD_right ? 1 : -1);
					break;

		case CMD_retab:
				n = getdigits(&arg);
				do_retab(line1, line2, n, forceit);
				u_clearline();
				updateScreen(NOT_VALID);
				break;

		case CMD_make:
				domake(arg);
				break;

		case CMD_isearch:
		case CMD_dsearch:
				n = ACTION_SHOW;
				goto find_pat;

		case CMD_ilist:
		case CMD_dlist:
				n = ACTION_SHOW_ALL;
				goto find_pat;

		case CMD_ijump:
		case CMD_djump:
				n = ACTION_GOTO;
				goto find_pat;

		case CMD_isplit:
		case CMD_dsplit:
				n = ACTION_SPLIT;
find_pat:
				find_pattern_in_path(arg, STRLEN(arg), !forceit,
					*cmd == 'd' ?  FIND_DEFINE : FIND_ANY,
					1L,
					n,
					line1, line2);
				break;

		default:
					/* Normal illegal commands have already been handled */
					emsg((char_u *)"Sorry, this command is not implemented");
	}


doend:
	if (errormsg != NULL)
	{
		if (sourcing)
		{
			sprintf((char *)IObuff, "%s: %s", errormsg, buff);
			emsg(IObuff);
		}
		else
			emsg(errormsg);
	}
	forceit = FALSE;		/* reset now so it can be used in getfile() */
	if (nextcomm && *nextcomm == NUL)		/* not really a next command */
		nextcomm = NULL;
	return nextcomm;
}

/*
 * if 'autowrite' option set, try to write the file
 *
 * return FAIL for failure, OK otherwise
 */
	int
autowrite(buf)
	BUF		*buf;
{
	if (!p_aw || (!forceit && buf->b_p_ro) || buf->b_filename == NULL)
		return FAIL;
	return buf_write_all(buf);
}

/*
 * flush all buffers, except the ones that are readonly
 */
	void
autowrite_all()
{
	BUF		*buf;

	if (!p_aw)
		return;
	for (buf = firstbuf; buf; buf = buf->b_next)
		if (buf->b_changed && !buf->b_p_ro)
			(void)buf_write_all(buf);
}

/*
 * flush the contents of a buffer, unless it has no file name
 *
 * return FAIL for failure, OK otherwise
 */
	static int
buf_write_all(buf)
	BUF		*buf;
{
	return (buf_write(buf, buf->b_filename, buf->b_sfilename,
						(linenr_t)1, buf->b_ml.ml_line_count, 0, 0, TRUE));
}

/*
 * write current buffer to file 'fname'
 * if 'append' is TRUE, append to the file
 *
 * if *fname == NUL write to current file
 * if b_notedited is TRUE, check for overwriting current file
 *
 * return FAIL for failure, OK otherwise
 */
	static int
dowrite(fname, append)
	char_u	*fname;
	int		append;
{
	FILE	*fd;
	int		other;
	char_u	*sfname = NULL;				/* init to shut up gcc */

	if (*fname == NUL)
		other = FALSE;
	else
	{
		sfname = fname;
		fname = fix_fname(fname);
		other = otherfile(fname);
	}

	/*
	 * if we have a new file name put it in the list of alternate file names
	 */
	if (other)
		setaltfname(fname, sfname, (linenr_t)1);

	/*
	 * writing to the current file is not allowed in readonly mode
	 * and need a file name
	 */
	if (!other && (check_readonly() || check_fname() == FAIL))
		return FAIL;

	if (!other)
	{
		fname = curbuf->b_filename;
		sfname = curbuf->b_sfilename;
	}

	/*
	 * write to other file or b_notedited set or not writing the whole file:
	 * overwriting only allowed with '!'
	 */
	if ((other || curbuf->b_notedited || line1 != 1 ||
			line2 != curbuf->b_ml.ml_line_count) && !forceit &&
			!append && !p_wa && (fd = fopen((char *)fname, "r")) != NULL)
	{								/* don't overwrite existing file */
		fclose(fd);
#ifdef UNIX
			/* with UNIX it is possible to open a directory */
		if (isdir(fname) == TRUE)
			EMSG2("\"%s\" is a directory", fname);
		else
#endif
			emsg(e_exists);
		return FAIL;
	}
	return (buf_write(curbuf, fname, sfname, line1, line2,
													append, forceit, TRUE));
}

/*
 * start editing a new file
 *
 *     fnum: file number; if zero use fname/sfname
 *    fname: the file name
 *				- full path if sfname used,
 *				- any file name if sfname is NULL
 *				- empty string to re-edit with the same file name (but may be
 *					in a different directory)
 *				- NULL to start an empty buffer
 *   sfname: the short file name (or NULL)
 *  command: the command to be executed after loading the file
 *     hide: if TRUE don't free the current buffer
 *  newlnum: put cursor on this line number (if possible)
 *
 * return FAIL for failure, OK otherwise
 */
	int
doecmd(fnum, fname, sfname, command, hide, newlnum)
	int			fnum;
	char_u		*fname;
	char_u		*sfname;
	char_u		*command;
	int			hide;
	linenr_t	newlnum;
{
	int			other_file;				/* TRUE if editing another file */
	int			oldbuf = FALSE;			/* TRUE if using existing buffer */
	BUF			*buf;

	if (fnum != 0)
	{
		if (fnum == curbuf->b_fnum)		/* file is already being edited */
			return OK;					/* nothing to do */
		other_file = TRUE;
	}
	else
	{
			/* if no short name given, use fname for short name */
		if (sfname == NULL)
			sfname = fname;

		if (fname == NULL)
			other_file = TRUE;
											/* there is no file name */
		else if (*fname == NUL && curbuf->b_filename == NULL)
			other_file = FALSE;
		else
		{
			if (*fname == NUL)				/* re-edit with same file name */
			{
				fname = curbuf->b_filename;
				sfname = curbuf->b_sfilename;
			}
			fname = fix_fname(fname);		/* may expand to full path name */
			other_file = otherfile(fname);
		}
	}
/*
 * if the file was changed we may not be allowed to abandon it
 * - if we are going to re-edit the same file
 * - or if we are the only window on this file and if hide is FALSE
 */
	if ((!other_file || (curbuf->b_nwindows == 1 && !hide)) &&
						check_changed(curbuf, FALSE, !other_file))
	{
		if (fnum == 0 && other_file && fname != NULL)
			setaltfname(fname, sfname, (linenr_t)1);
		return FAIL;
	}
/*
 * If we are starting to edit another file, open a (new) buffer.
 * Otherwise we re-use the current buffer.
 */
	if (other_file)
	{
		curwin->w_alt_fnum = curbuf->b_fnum;
		buflist_altlnum();

		if (fnum)
			buf = buflist_findnr(fnum);
		else
			buf = buflist_new(fname, sfname, 1L, TRUE);
		if (buf == NULL)
			return FAIL;
		if (buf->b_ml.ml_mfp == NULL)		/* no memfile yet */
		{
			oldbuf = FALSE;
			buf->b_nwindows = 1;
		}
		else								/* existing memfile */
		{
			oldbuf = TRUE;
			++buf->b_nwindows;
		}
		/*
		 * make the (new) buffer the one used by the current window
		 * if the old buffer becomes unused, free it if hide is FALSE
		 * If the current buffer was empty and has no file name, curbuf
		 * is returned by buflist_new().
		 */
		if (buf != curbuf)
		{
			close_buffer(curbuf, !hide, FALSE);
			curwin->w_buffer = buf;
			curbuf = buf;
		}

		curwin->w_pcmark.lnum = 1;
		curwin->w_pcmark.col = 0;
	}
	else if (check_fname() == FAIL)
		return FAIL;

/*
 * If we get here we are sure to start editing
 */
		/* don't redraw until the cursor is in the right line */
	++RedrawingDisabled;

/*
 * other_file	oldbuf
 *	FALSE		FALSE		re-edit same file, buffer is re-used
 *	FALSE		TRUE		not posible
 *  TRUE		FALSE		start editing new file, new buffer
 *  TRUE		TRUE		start editing in existing buffer (nothing to do)
 */
	if (!other_file)					/* re-use the buffer */
	{
		if (newlnum == 0)
			newlnum = curwin->w_cursor.lnum;
		buf_freeall(curbuf);			/* free all things for buffer */
		buf_clear(curbuf);
		curbuf->b_startop.lnum = 0;		/* clear '[ and '] marks */
		curbuf->b_endop.lnum = 0;
	}

	if (!oldbuf)						/* need to read the file */
		(void)open_buffer();
	win_init(curwin);
	maketitle();

	if (newlnum && command == NULL)
	{
		curwin->w_cursor.lnum = newlnum;
		check_cursor();
		beginline(MAYBE);
	}
	check_cursor();

	/*
	 * Did not read the file, need to show some info about the file.
	 * Do this after setting the cursor.
	 */
	if (oldbuf)
		fileinfo(did_cd);

	if (command != NULL)
		docmdline(command, TRUE, FALSE);
	--RedrawingDisabled;
	if (!skip_redraw)
		updateScreen(CURSUPD);			/* redraw now */

	if (p_im)
		stuffReadbuff((char_u *)"i");	/* start editing in insert mode */
	return OK;
}

/*
 * get + command from ex argument
 */
	static char_u *
getargcmd(argp)
	char_u **argp;
{
	char_u *arg = *argp;
	char_u *command = NULL;

	if (*arg == '+')		/* +[command] */
	{
		++arg;
		if (isspace(*arg))
			command = (char_u *)"$";
		else
		{
			command = arg;
			/*
			 * should check for "\ " (but vi has a bug that prevents it to work)
			 */
			skiptowhite(&arg);
		}
		if (*arg)
			*arg++ = NUL;	/* terminate command with NUL */
		
		skipwhite(&arg);	/* skip over spaces */
		*argp = arg;
	}
	return command;
}

	static void
domake(arg)
	char_u *arg;
{
	if (*p_ef == NUL)
	{
		EMSG("errorfile option not set");
		return;
	}
	if (curbuf->b_changed)
		(void)autowrite(curbuf);
	remove((char *)p_ef);
	msg_outchar(':');
	msg_outstr(arg);		/* show what we are doing */
	sprintf((char *)IObuff, "%s %s %s", arg, p_sp, p_ef);
	doshell(IObuff);
#ifdef AMIGA
	flushbuf();
				/* read window status report and redraw before message */
	(void)char_avail();
#endif
	(void)qf_init();
	remove((char *)p_ef);
}

/* 
 * Redefine the argument list to 'str'.
 *
 * Return FAIL for failure, OK otherwise.
 */
	static int
doarglist(str)
	char_u *str;
{
	int		new_count = 0;
	char_u	**new_files = NULL;
	int		exp_count;
	char_u	**exp_files;
	char_u	**t;
	char_u	*p;
	int		inquote;
	int		i;

	while (*str)
	{
		/*
		 * create a new entry in new_files[]
		 */
		t = (char_u **)lalloc((long_u)(sizeof(char_u *) * (new_count + 1)), TRUE);
		if (t != NULL)
			for (i = new_count; --i >= 0; )
				t[i] = new_files[i];
		free(new_files);
		if (t == NULL)
			return FAIL;
		new_files = t;
		new_files[new_count++] = str;

		/*
		 * isolate one argument, taking quotes
		 */
		inquote = FALSE;
		for (p = str; *str; ++str)
		{
			/*
			 * for MSDOS a backslash is part of a file name.
			 * Only skip ", space and tab.
			 */
#ifdef MSDOS
			if (*str == '\\' && (str[1] == '"' || str[1] == ' ' ||
														str[1] == '\t'))
#else
			if (*str == '\\' && str[1] != NUL)
#endif
				*p++ = *++str;
			else
			{
				if (!inquote && isspace(*str))
					break;
				if (*str == '"')
					inquote ^= TRUE;
				else
					*p++ = *str;
			}
		}
		skipwhite(&str);
		*p = NUL;
	}
	
	i = ExpandWildCards(new_count, new_files, &exp_count,
												&exp_files, FALSE, TRUE);
	free(new_files);
	if (i == FAIL)
		return FAIL;
	if (exp_count == 0)
	{
		emsg(e_nomatch);
		return FAIL;
	}
	if (arg_exp)				/* arg_files[] has been allocated, free it */
		FreeWild(arg_count, arg_files);
	else
		arg_exp = TRUE;
	arg_files = exp_files;
	arg_count = exp_count;
	arg_had_last = FALSE;

	/*
	 * put all file names in the buffer list
	 */
	for (i = 0; i < arg_count; ++i)
		(void)buflist_add(arg_files[i]);

	return OK;
}

	void
gotocmdline(clr)
	int				clr;
{
	msg_start();
	if (clr)				/* clear the bottom line(s) */
		msg_clr_eos();		/* will reset clear_cmdline */
	windgoto(cmdline_row, 0);
}

	static int
check_readonly()
{
	if (!forceit && curbuf->b_p_ro)
	{
		emsg(e_readonly);
		return TRUE;
	}
	return FALSE;
}

/*
 * return TRUE if buffer was changed and cannot be abandoned.
 */
	static int
check_changed(buf, checkaw, mult_win)
	BUF		*buf;
	int		checkaw;		/* do autowrite if buffer was changed */
	int		mult_win;		/* check also when several windows for the buffer */
{
	if (	!forceit &&
			buf->b_changed && (mult_win || buf->b_nwindows <= 1) &&
			(!checkaw || autowrite(buf) == FAIL))
	{
		emsg(e_nowrtmsg);
		return TRUE;
	}
	return FALSE;
}

/*
 * return TRUE if any buffer was changed and cannot be abandoned.
 */
	static int
check_changed_any(checkaw)
	int		checkaw;		/* do autowrite if buffer was changed */
{
	BUF		*buf;

	if (!forceit)
	{
		for (buf = firstbuf; buf != NULL; buf = buf->b_next)
		{
			if (buf->b_changed && (!checkaw || autowrite(buf) == FAIL))
			{
				EMSG2("No write since last change for buffer \"%s\"",
						buf->b_xfilename == NULL ?
									(char_u *)"No File" : buf->b_xfilename);
				return TRUE;
			}
		}
	}
	return FALSE;
}

/*
 * return FAIL if there is no filename, OK if there is one
 * give error message for FAIL
 */
	int
check_fname()
{
	if (curbuf->b_filename == NULL)
	{
		emsg(e_noname);
		return FAIL;
	}
	return OK;
}

/*
 * - if there are more files to edit
 * - and this is the last window
 * - and forceit not used
 * - and not repeated twice on a row
 *	  return FAIL and give error message if 'message' TRUE
 * return OK otherwise
 */
	static int
check_more(message)
	int message;			/* when FALSE check only, no messages */
{
	if (!forceit && firstwin == lastwin && arg_count > 1 && !arg_had_last &&
									quitmore == 0)
	{
		if (message)
		{
			emsg2((char_u *)"%ld more files to edit",
						(char_u *)(long)(arg_count - curwin->w_arg_idx - 1));
			quitmore = 2;			/* next try to quit is allowed */
		}
		return FAIL;
	}
	return OK;
}

/*
 * try to abandon current file and edit a new or existing file
 * 'fnum' is the number of the file, if zero use fname/sfname
 *
 * return 1 for "normal" error, 2 for "not written" error, 0 for success
 * -1 for succesfully opening another file
 * 'lnum' is the line number for the cursor in the new file (if non-zero).
 */
	int
getfile(fnum, fname, sfname, setpm, lnum)
	int			fnum;
	char_u		*fname;
	char_u		*sfname;
	int			setpm;
	linenr_t	lnum;
{
	int other;

	if (fnum == 0)
	{
		fname_expand(&fname, &sfname);	/* make fname full path, set sfname */
		other = otherfile(fname);
	}
	else
		other = (fnum != curbuf->b_fnum);

	if (other && !forceit && curbuf->b_nwindows == 1 &&
			!p_hid && curbuf->b_changed && autowrite(curbuf) == FAIL)
	{
		emsg(e_nowrtmsg);
		return 2;		/* file has been changed */
	}
	if (setpm)
		setpcmark();
	if (!other)
	{
		if (lnum != 0)
			curwin->w_cursor.lnum = lnum;
		check_cursor();
		beginline(MAYBE);

		return 0;		/* it's in the same file */
	}
	if (doecmd(fnum, fname, sfname, NULL, p_hid, lnum) == OK)
		return -1;		/* opened another file */
	return 1;			/* error encountered */
}

/*
 * vim_strncpy()
 *
 * This is here because strncpy() does not guarantee successful results when
 * the to and from strings overlap.  It is only currently called from nextwild()
 * which copies part of the command line to another part of the command line.
 * This produced garbage when expanding files etc in the middle of the command
 * line (on my terminal, anyway) -- webb.
 */
	static void
vim_strncpy(to, from, len)
	char_u *to;
	char_u *from;
	int len;
{
	int i;

	if (to <= from)
	{
		while (len-- && *from)
			*to++ = *from++;
		if (len >= 0)
			*to = *from;	/* Copy NUL */
	}
	else
	{
		for (i = 0; i < len; i++)
		{
			to++;
			if (*from++ == NUL)
			{
				i++;
				break;
			}
		}
		for (; i > 0; i--)
			*--to = *--from;
	}
}

/* Return FALSE if this is not an appropriate context in which to do
 * completion of anything, & TRUE if it is (even if there are no matches).  For
 * the caller, this means that the character is just passed through like a
 * normal character (instead of being expanded).  This allows :s/^I^D etc.
 */
	static int
nextwild(buff, type)
	char_u *buff;
	int		type;
{
	int		i;
	char_u	*p1;
	char_u	*p2 = NULL;
	int		oldlen;
	int		difflen;

	if (cmd_numfiles == -1)
		set_expand_context(cmdfirstc, cmdbuff);
	if (expand_context == EXPAND_UNSUCCESSFUL)
	{
		beep_flush();
		return OK;	/* Something illegal on command line */
	}
	if (expand_context == EXPAND_NOTHING)
	{
		/* Caller can use the character as a normal char instead */
		return FAIL;
	}
	expand_interactively = TRUE;

	msg_outstr((char_u *)"...");		/* show that we are busy */
	flushbuf();

	i = expand_pattern - buff;
	oldlen = cmdpos - i;

		/* add a "*" to the file name and expand it */
	if ((p1 = addstar(&buff[i], oldlen)) != NULL)
	{
		if ((p2 = ExpandOne(p1, FALSE, type)) != NULL)
		{
			if (cmdlen + (difflen = STRLEN(p2) - oldlen) > CMDBUFFSIZE - 4)
				emsg(e_toolong);
			else
			{
				vim_strncpy(&buff[cmdpos + difflen], &buff[cmdpos], cmdlen - cmdpos);
				STRNCPY(&buff[i], p2, STRLEN(p2));
				cmdlen += difflen;
				cmdpos += difflen;
			}
			free(p2);
		}
		free(p1);
	}
	redrawcmd();
	if (cmd_numfiles <= 0 && p2 == NULL)
		beep_flush();
	else if (cmd_numfiles == 1)
	{
		(void)ExpandOne(NULL, FALSE, -2);	/* free expanded "file" names */
		cmd_numfiles = -1;
	}
	expand_interactively = FALSE;
	return OK;
}

/*
 * Do wildcard expansion on the string 'str'.
 * Return a pointer to alloced memory containing the new string.
 * Return NULL for failure.
 *
 * mode = -2: only release file names
 * mode = -1: normal expansion, do not keep file names
 * mode =  0: normal expansion, keep file names
 * mode =  1: use next match in multiple match
 * mode =  2: use previous match in multiple match
 * mode =  3: use next match in multiple match and wrap to first
 * mode =  4: return all matches concatenated
 * mode =  5: return longest matched part
 */
	char_u *
ExpandOne(str, list_notfound, mode)
	char_u	*str;
	int		list_notfound;
	int		mode;
{
	char_u		*ss = NULL;
	static char_u **cmd_files = NULL;	  /* list of input files */
	static int	findex;
	int			i, found = 0;
	int			multmatch = FALSE;
	long_u		len;
	char_u		*filesuf, *setsuf, *nextsetsuf;
	int			filesuflen, setsuflen;

/*
 * first handle the case of using an old match
 */
	if (mode >= 1 && mode < 4)
	{
		if (cmd_numfiles > 0)
		{
			if (mode == 2)
				--findex;
			else	/* mode == 1 || mode == 3 */
				++findex;
			if (findex < 0)
				findex = 0;
			if (findex > cmd_numfiles - 1)
			{
				if (mode == 3)
					findex = 0;
				else
					findex = cmd_numfiles - 1;
			}
			return strsave(cmd_files[findex]);
		}
		else
			return NULL;
	}

/* free old names */
	if (cmd_numfiles != -1 && mode < 4)
	{
		FreeWild(cmd_numfiles, cmd_files);
		cmd_numfiles = -1;
	}
	findex = 0;

	if (mode == -2)		/* only release file name */
		return NULL;

	if (cmd_numfiles == -1)
	{
		if (ExpandFromContext((char_u *)str, &cmd_numfiles, &cmd_files, FALSE,
														list_notfound) == FAIL)
			/* error: do nothing */;
		else if (cmd_numfiles == 0)
		{
			if (!expand_interactively)
				emsg(e_nomatch);
		}
		else if (mode < 4)
		{
			if (cmd_numfiles > 1)		/* more than one match; check suffixes */
			{
				found = -2;
				for (i = 0; i < cmd_numfiles; ++i)
				{
					if ((filesuf = STRRCHR(cmd_files[i], '.')) != NULL)
					{
						filesuflen = STRLEN(filesuf);
						for (setsuf = p_su; *setsuf; setsuf = nextsetsuf)
						{
							if ((nextsetsuf = STRCHR(setsuf + 1, '.')) == NULL)
								nextsetsuf = setsuf + STRLEN(setsuf);
							setsuflen = (int)(nextsetsuf - setsuf);
							if (filesuflen == setsuflen &&
										STRNCMP(setsuf, filesuf, (size_t)setsuflen) == 0)
								break;
						}
						if (*setsuf)				/* suffix matched: ignore file */
							continue;
					}
					if (found >= 0)
					{
						multmatch = TRUE;
						break;
					}
					found = i;
				}
			}
			if (multmatch || found < 0)
			{
				/* Can we ever get here unless it's while expanding
				 * interactively?  If not, we can get rid of this all together.
				 * Don't really want to wait for this message (and possibly
				 * have to hit return to continue!).
				 */
				if (!expand_interactively)
					emsg(e_toomany);
				else
					beep_flush();
				found = 0;				/* return first one */
				multmatch = TRUE;		/* for found < 0 */
			}
			if (found >= 0 && !(multmatch && mode == -1))
				ss = strsave(cmd_files[found]);
		}
	}

	if (mode == 5 && cmd_numfiles > 0)		/* find longest common part */
	{
		for (len = 0; cmd_files[0][len]; ++len)
		{
			for (i = 0; i < cmd_numfiles; ++i)
			{
#ifdef AMIGA
				if (toupper(cmd_files[i][len]) != toupper(cmd_files[0][len]))
#else
				if (cmd_files[i][len] != cmd_files[0][len])
#endif
					break;
			}
			if (i < cmd_numfiles)
				break;
		}
		ss = alloc((unsigned)len + 1);
		if (ss)
		{
			STRNCPY(ss, cmd_files[0], (size_t)len);
			ss[len] = NUL;
		}
		multmatch = TRUE;					/* don't free the names */
		findex = -1;						/* next p_wc gets first one */
	}

	if (mode == 4 && cmd_numfiles > 0)		/* concatenate all file names */
	{
		len = 0;
		for (i = 0; i < cmd_numfiles; ++i)
			len += STRLEN(cmd_files[i]) + 1;
		ss = lalloc(len, TRUE);
		if (ss)
		{
			*ss = NUL;
			for (i = 0; i < cmd_numfiles; ++i)
			{
				STRCAT(ss, cmd_files[i]);
				if (i != cmd_numfiles - 1)
					STRCAT(ss, " ");
			}
		}
	}

	if (mode == -1 || mode == 4)
	{
		FreeWild(cmd_numfiles, cmd_files);
		cmd_numfiles = -1;
	}
	return ss;
}

/*
 * show all filenames that match the string "file" with length "len"
 */
	static int
showmatches(buff)
	char_u *buff;
{
	char_u *file_str;
	int num_files;
	char_u **files_found;
	int i, j, k;
	int maxlen;
	int lines;
	int columns;

	set_expand_context(cmdfirstc, cmdbuff);
	if (expand_context == EXPAND_UNSUCCESSFUL)
	{
		beep_flush();
		return OK;	/* Something illegal on command line */
	}
	if (expand_context == EXPAND_NOTHING)
	{
		/* Caller can use the character as a normal char instead */
		return FAIL;
	}
	expand_interactively = TRUE;

	/* add star to file name, or convert to regexp if not expanding files! */
	file_str = addstar(expand_pattern, (int)(buff + cmdpos - expand_pattern));
	if (file_str == NULL)
	{
		expand_interactively = FALSE;
		return OK;
	}

	msg_outchar('\n');
	flushbuf();
	cmdline_row = msg_row;
	msg_didany = FALSE;					/* lines_left will be set */
	msg_start();						/* prepare for paging */

	/* find all files that match the description */
	if (ExpandFromContext(file_str, &num_files, &files_found, FALSE, FALSE) == FAIL)
	{
		num_files = 0;
		files_found = (char_u **)"";
	}

	/* find the maximum length of the file names */
	maxlen = 0;
	for (i = 0; i < num_files; ++i)
	{
		j = STRLEN(files_found[i]);
		if (j > maxlen)
			maxlen = j;
	}

	/* compute the number of columns and lines for the listing */
	maxlen += 2;	/* two spaces between file names */
	columns = ((int)Columns + 2) / maxlen;
	if (columns < 1)
		columns = 1;
	lines = (num_files + columns - 1) / columns;

	(void)set_highlight('d');	/* find out highlight mode for directories */

	/* list the files line by line */
	for (i = 0; i < lines; ++i)
	{
		for (k = i; k < num_files; k += lines)
		{
			if (k > i)
				for (j = maxlen - STRLEN(files_found[k - lines]); --j >= 0; )
					msg_outchar(' ');
			if (expand_context == EXPAND_FILES)
				j = isdir(files_found[k]);	/* highlight directories */
			else
				j = FALSE;
			if (j)
			{
				start_highlight();
				screen_start();		/* don't output spaces to position cursor */
			}
			msg_outstr(files_found[k]);
			if (j)
				stop_highlight();
		}
		msg_outchar('\n');
		flushbuf();					/* show one line at a time */
		if (got_int)
		{
			got_int = FALSE;
			break;
		}
	}
	free(file_str);
	FreeWild(num_files, files_found);

/*
 * we redraw the command below the lines that we have just listed
 * This is a bit tricky, but it saves a lot of screen updating.
 */
	cmdline_row = msg_row;		/* will put it back later */

	expand_interactively = FALSE;
	return OK;
}

/*
 * copy the file name into allocated memory and add a '*' at the end
 */
	char_u *
addstar(fname, len)
	char_u	*fname;
	int		len;
{
	char_u	*retval;
	int		i, j;
	int		new_len;
	char_u	save_char;

	if (expand_interactively && expand_context != EXPAND_FILES &&
		expand_context != EXPAND_DIRECTORIES)
	{
		/* Matching will be done internally (on something other than files).
		 * So we convert the file-matching-type wildcards into our kind for
		 * use with regcomp().  First work out how long it will be:
		 */
		new_len = len + 2;				/* +2 for '^' at start, NUL at end */
		for (i = 0; i < len; i++)
			if (fname[i] == '*')
				new_len++;				/* '*' needs to be replaced by '.*' */
		retval = alloc(new_len);
		if (retval != NULL)
		{
			retval[0] = '^';
			for (i = 0, j = 1; i < len; i++, j++)
				if (fname[i] == '*')
				{
					retval[j++] = '.';
					retval[j] = '*';
				}
				else if (fname[i] == '?')
					retval[j] = '.';
				else
					retval[j] = fname[i];
			retval[j] = NUL;
		}
	}
	else
	{
		retval = alloc(len + 4);
		if (retval != NULL)
		{
			STRNCPY(retval, fname, (size_t)len);
			/*
			 * Don't add a star to ~ or ~user
			 */
			save_char = fname[j = len];
			fname[j] = NUL;
			if (gettail(fname)[0] != '~')
			{
#ifdef MSDOS
			/*
			 * if there is no dot in the file name, add "*.*" instead of "*".
			 */
				for (i = len - 1; i >= 0; --i)
					if (strchr(".\\/:", retval[i]))
						break;
				if (i < 0 || retval[i] != '.')
				{
					retval[len++] = '*';
					retval[len++] = '.';
				}
#endif
				retval[len++] = '*';
			}
			retval[len] = NUL;
			fname[j] = save_char;
		}
	}
	return retval;
}

/*
 * dosource: read the file "fname" and execute its lines as EX commands
 *
 * This function may be called recursively!
 *
 * return FAIL if file could not be opened, OK otherwise
 */
	int
dosource(fname)
	register char_u *fname;
{
	register FILE	*fp;
	register int	len;
#ifdef MSDOS
	int				error = FALSE;
#endif

	expand_env(fname, NameBuff, MAXPATHL);		/* use NameBuff for expanded name */
	if ((fp = fopen((char *)NameBuff, READBIN)) == NULL)
		return FAIL;

	sourcing_name = fname;
	++dont_sleep;			/* don't call sleep() in emsg() */
	len = 0;
	while (fgets((char *)IObuff + len, IOSIZE - len, fp) != NULL && !got_int)
	{
		len = STRLEN(IObuff) - 1;
		if (len >= 0 && IObuff[len] == '\n')	/* remove trailing newline */
		{
#ifdef MSDOS
			if (len > 0 && IObuff[len - 1] == '\r') /* trailing CR-LF */
				--len;
			else
			{
				if (!error)
					EMSG("Warning: Wrong line separator, ^M may be missing");
				error = TRUE;		/* lines like ":map xx yy^M" will fail */
			}
#endif
				/* escaped newline, read more */
			if (len > 0 && len < IOSIZE && IObuff[len - 1] == Ctrl('V'))
			{
				IObuff[len - 1] = '\n';		/* remove CTRL-V */
				continue;
			}
			IObuff[len] = NUL;
		}
		breakcheck();		/* check for ^C here, so recursive :so will be broken */
		docmdline(IObuff, TRUE, TRUE);
		len = 0;
	}
	fclose(fp);
	if (got_int)
		emsg(e_interr);
	--dont_sleep;
	sourcing_name = NULL;
	return OK;
}

/*
 * get a single EX address
 * 
 * Set ptr to the next character after the part that was interpreted.
 * Set ptr to NULL when an error is encountered.
 */
	static linenr_t
get_address(ptr)
	char_u		**ptr;
{
	linenr_t	cursor_lnum = curwin->w_cursor.lnum;
	int			c;
	int			i;
	long		n;
	char_u  	*cmd;
	FPOS		pos;
	FPOS		*fp;
	linenr_t	lnum;

	cmd = *ptr;
	skipwhite(&cmd);
	lnum = MAXLNUM;
	do
	{
		switch (*cmd)
		{
			case '.': 						/* '.' - Cursor position */
						++cmd;
						lnum = cursor_lnum;
						break;

			case '$': 						/* '$' - last line */
						++cmd;
						lnum = curbuf->b_ml.ml_line_count;
						break;

			case '\'': 						/* ''' - mark */
						if (*++cmd == NUL ||
									(fp = getmark(*cmd++, FALSE)) == NULL)
						{
							emsg(e_umark);
							cmd = NULL;
							goto error;
						}
						lnum = fp->lnum;
						break;

			case '/':
			case '?':						/* '/' or '?' - search */
						c = *cmd++;
						pos = curwin->w_cursor;		/* save curwin->w_cursor */
						curwin->w_cursor.col = -1;	/* searchit() will increment the col */
						if (c == '/')
							++curwin->w_cursor.lnum;
						searchcmdlen = 0;
						if (!dosearch(c, cmd, FALSE, (long)1, FALSE, TRUE))
						{
							cmd = NULL;
							curwin->w_cursor = pos;
							goto error;
						}
						lnum = curwin->w_cursor.lnum;
						curwin->w_cursor = pos;
											/* adjust command string pointer */
						cmd += searchcmdlen;
						break;

			case '\\':				/* "\?", "\/" or "\&", repeat search */
						++cmd;
						if (*cmd == '&')
							i = 1;
						else if (*cmd == '?' || *cmd == '/')
							i = 0;
						else
						{
							emsg(e_backslash);
							cmd = NULL;
							goto error;
						}
						pos = curwin->w_cursor;
						if (*cmd != '?')
							++pos.lnum;
						pos.col = -1;
						if (searchit(&pos, *cmd == '?' ? BACKWARD : FORWARD,
										(char_u *)"", 1L, FALSE, TRUE, i) == OK)
							lnum = pos.lnum;
						else
						{
							cmd = NULL;
							goto error;
						}
						++cmd;
						break;

			default:
						if (isdigit(*cmd))		/* absolute line number */
							lnum = getdigits(&cmd);
		}
		
		for (;;)
		{
			skipwhite(&cmd);
			if (*cmd != '-' && *cmd != '+' && !isdigit(*cmd))
				break;

			if (lnum == MAXLNUM)
				lnum = cursor_lnum;		/* "+1" is same as ".+1" */
			if (isdigit(*cmd))
				i = '+';				/* "number" is same as "+number" */
			else
				i = *cmd++;
			if (!isdigit(*cmd))			/* '+' is '+1', but '+0' is not '+1' */
				n = 1;
			else 
				n = getdigits(&cmd);
			if (i == '-')
				lnum -= n;
			else
				lnum += n;
		}
		cursor_lnum = lnum;
	} while (*cmd == '/' || *cmd == '?');

error:
	*ptr = cmd;
	return lnum;
}


/*
 * Must parse the command line so far to work out what context we are in.
 * Completion can then be done based on that context.
 * This routine sets two global variables:
 *	char_u *expand_pattern --- The start of the pattern to be expanded within
 *								the command line (ends at the cursor).
 *	int expand_context --- The type of thing to expand.  Will be one of:
 *	  EXPAND_UNSUCCESSFUL --- Used somtimes when there is something illegal on
 *			the command line, like an unknown command.  Caller should beep.
 *	  EXPAND_NOTHING --- Unrecognised context for completion, use char like a
 *			normal char, rather than for completion.  eg :s/^I/
 *	  EXPAND_COMMANDS --- Cursor is still touching the command, so complete it.
 *	  EXPAND_FILES --- After command with XFILE set, or after setting with
 *			P_EXPAND set.  eg :e ^I, :w>>^I
 *	  EXPAND_DIRECTORIES --- In some cases this is used instead of the latter
 *			when we know only directories are of interest.  eg :set dir=^I
 *	  EXPAND_SETTINGS --- Complete variable names.  eg :set d^I
 *	  EXPAND_BOOL_SETTINGS --- Complete bollean variables only,  eg :set no^I
 *	  EXPAND_TAGS --- Complete tags from the files in p_tags.  eg :ta a^I
 *
 * -- webb.
 */
	static void
set_expand_context(firstc, buff)
	int			firstc; 	/* either ':', '/', or '?' */
	char_u		*buff;	 	/* buffer for command string */
{
	char_u		*nextcomm;
	char_u		old_char;

	old_char = cmdbuff[cmdpos];
	cmdbuff[cmdpos] = NUL;
	nextcomm = buff;
	while (nextcomm != NULL)
		nextcomm = set_one_cmd_context(firstc, nextcomm);
	cmdbuff[cmdpos] = old_char;
}

/*
 * This is all pretty much copied from DoOneCmd(), with all the extra stuff we
 * don't need/want deleted.  Maybe this could be done better if we didn't
 * repeat all this stuff.  The only problem is that they may not stay perfectly
 * compatible with each other, but then the command line syntax probably won't
 * change that much -- webb.
 */
	static char_u *
set_one_cmd_context(firstc, buff)
	int			firstc; 	/* either ':', '/', or '?' */
	char_u		*buff;	 	/* buffer for command string */
{
	register char_u		*p;
	char_u				*cmd, *arg;
	int 				i;
	int					cmdidx;
	int					argt;
	char_u				delim;
	int					forced = FALSE;
	int					usefilter = FALSE;	/* filter instead of file name */

	expand_pattern = buff;
	if (firstc != ':')
	{
		expand_context = EXPAND_NOTHING;
		return NULL;
	}
	expand_context = EXPAND_COMMANDS;	/* Default until we get past command */

/*
 * 2. skip comment lines and leading space, colons or bars
 */
	for (cmd = buff; *cmd && strchr(" \t:|", *cmd) != NULL; cmd++)
		;
	expand_pattern = cmd;

	if (*cmd == NUL)
		return NULL;
	if (*cmd == '"')		/* ignore comment lines */
	{
		expand_context = EXPAND_NOTHING;
		return NULL;
	}

/*
 * 3. parse a range specifier of the form: addr [,addr] [;addr] ..
 */
	/* 
	 * Backslashed delimiters after / or ? will be skipped, and commands will
	 * not be expanded between /'s and ?'s or after "'". -- webb
	 */
	while (*cmd != NUL && (isspace(*cmd) || isdigit(*cmd) ||
										STRCHR(".$%'/?-+,;", *cmd) != NULL))
	{
		if (*cmd == '\'')
		{
			if (*++cmd == NUL)
				expand_context = EXPAND_NOTHING;
		}
		else if (*cmd == '/' || *cmd == '?')
		{
			delim = *cmd++;
			while (*cmd != NUL && *cmd != delim)
				if (*cmd++ == '\\' && *cmd != NUL)
					++cmd;
			if (*cmd == NUL)
				expand_context = EXPAND_NOTHING;
		}
		if (*cmd != NUL)
			++cmd;
	}

/*
 * 4. parse command
 */

	skipwhite(&cmd);
	expand_pattern = cmd;
	if (*cmd == NUL)
		return NULL;
	if (*cmd == '"')
	{
		expand_context = EXPAND_NOTHING;
		return NULL;
	}

	if (*cmd == '|' || *cmd == '\n')
		return cmd + 1;					/* There's another command */

	/*
	 * Isolate the command and search for it in the command table.
	 * Exeptions:
	 * - the 'k' command can directly be followed by any character.
	 * - the 's' command can be followed directly by 'c', 'g' or 'r'
	 */
	if (*cmd == 'k')
	{
		cmdidx = CMD_k;
		p = cmd + 1;
	}
	else
	{
		p = cmd;
		while (isalpha(*p) || *p == '*')	/* Allow * wild card */
			++p;
		if (p == cmd && strchr("@!=><&~#", *p) != NULL)	/* non-alpha command */
			++p;
		i = (int)(p - cmd);

		if (i == 0)
		{
			expand_context = EXPAND_UNSUCCESSFUL;
			return NULL;
		}
		for (cmdidx = 0; cmdidx < CMD_SIZE; ++cmdidx)
			if (STRNCMP(cmdnames[cmdidx].cmd_name, cmd, (size_t)i) == 0)
				break;
	}
	if (p == cmdbuff + cmdpos)		/* We are still touching the command */
		return NULL;				/* So complete it */

	if (cmdidx == CMD_SIZE)
	{
		if (*cmd == 's' && strchr("cgr", cmd[1]) != NULL)
		{
			cmdidx = CMD_substitute;
			p = cmd + 1;
		}
		else
		{
			/* Not still touching the command and it was an illegal command */
			expand_context = EXPAND_UNSUCCESSFUL;
			return NULL;
		}
	}

	expand_context = EXPAND_NOTHING; /* Default now that we're past command */

	if (*p == '!')					/* forced commands */
	{
		forced = TRUE;
		++p;
	}

/*
 * 5. parse arguments
 */
	argt = cmdnames[cmdidx].cmd_argt;

	arg = p;						/* remember start of argument */
	skipwhite(&arg);

	if (cmdidx == CMD_write)
	{
		if (*arg == '>')						/* append */
		{
			if (*++arg == '>')				/* It should be */
				++arg;
			skipwhite(&arg);
		}
		else if (*arg == '!')					/* :w !filter */
		{
			++arg;
			usefilter = TRUE;
		}
	}

	if (cmdidx == CMD_read)
	{
		usefilter = forced;					/* :r! filter if forced */
		if (*arg == '!')						/* :r !filter */
		{
			++arg;
			usefilter = TRUE;
		}
	}

	if (cmdidx == CMD_lshift || cmdidx == CMD_rshift)
	{
		while (*arg == *cmd)		/* allow any number of '>' or '<' */
			++arg;
		skipwhite(&arg);
	}

	/*
	 * Check for '|' to separate commands and '"' to start comments.
	 * Don't do this for ":read !cmd" and ":write !cmd".
	 */
	if ((argt & TRLBAR) && !usefilter)
	{
		p = arg;
		while (*p)
		{
			if (*p == Ctrl('V'))
			{
				if (p[1] != NUL)
					++p;
			}
			else if ((*p == '"' && !(argt & NOTRLCOM)) || *p == '|' || *p == '\n')
			{
				if (*(p - 1) != '\\')
				{
					if (*p == '|' || *p == '\n')
						return p + 1;
					return NULL;	/* It's a comment */
				}
			}
			++p;
		}
	}

	if (!(argt & EXTRA) && strchr("|\"", *arg) == NULL)	/* no arguments allowed */
		return NULL;

	/* Find start of last argument (argument just before cursor): */
	p = cmdbuff + cmdpos;
	while (p != arg && *p != ' ' && *p != TAB)
		p--;
	if (*p == ' ' || *p == TAB)
		p++;
	expand_pattern = p;

	if (argt & XFILE)
		expand_context = EXPAND_FILES;

/*
 * 6. switch on command name
 */
	switch (cmdidx)
	{
		case CMD_cd:
		case CMD_chdir:
			expand_context = EXPAND_DIRECTORIES;
			break;
		case CMD_args:			/* args now takes arguments like :next */
		case CMD_edit:
		case CMD_ex:
		case CMD_cfile:
		case CMD_buffer:
		case CMD_wnext:
		case CMD_next:
		case CMD_snext:
		case CMD_split:
		case CMD_new:
		case CMD_visual:
			for (p = arg; *p; ++p)
			{
				if (*p == '\\' && p[1])
					++p;
				else if (*p == '|' || *p == '\n')
					return p + 1;
			}
			break;
		case CMD_global:
		case CMD_vglobal:
			delim = *arg; 			/* get the delimiter */
			if (delim)
				++arg;				/* skip delimiter if there is one */

			while (arg[0] != NUL && arg[0] != delim)
			{
				if (arg[0] == '\\' && arg[1] != NUL)
					++arg;
				++arg;
			}
			if (arg[0] != NUL)
				return arg + 1;
			break;
		case CMD_and:
		case CMD_substitute:
			delim = *arg;
			if (delim)
				++arg;
			for (i = 0; i < 2; i++, arg++)
				while (arg[0] != NUL && arg[0] != delim)
				{
					if (arg[0] == '\\' && arg[1] != NUL)
						++arg;
					++arg;
				}
			while (arg[0] != NUL && strchr("|\"#", arg[0]) == NULL)
				++arg;
			if (arg[0] != NUL)
				return arg;
			break;
		case CMD_autocmd:
			while (*arg && (!iswhite(*arg) || arg[-1] == '\\'))
				arg++;
			if (*arg)
				return arg;
			break;
		case CMD_set:
			set_context_in_set_cmd(arg);
			break;
		case CMD_stag:
		case CMD_tag:
			expand_context = EXPAND_TAGS;
			expand_pattern = arg;
			break;
		default:
			break;
	}
	return NULL;
}

/*
 * Do the expansion based on the global variables expand_context and
 * expand_pattern -- webb.
 */
	static int
ExpandFromContext(pat, num_file, file, files_only, list_notfound)
	char_u *pat;
	int *num_file;
	char_u ***file;
	int files_only;
	int list_notfound;
{
	regexp	*prog;
	int		cmdidx;
	int		count;
	int		ret;
	int		i;

	if (!expand_interactively || expand_context == EXPAND_FILES)
		return ExpandWildCards(1, &pat, num_file, file, files_only, list_notfound);
	else if (expand_context == EXPAND_DIRECTORIES)
	{
		if (ExpandWildCards(1, &pat, num_file, file, files_only, list_notfound)
																	== FAIL)
			return FAIL;
		count = 0;
		for (i = 0; i < *num_file; i++)
			if (isdir((*file)[i]))
				(*file)[count++] = (*file)[i];
			else
				free((*file)[i]);
		if (count == 0)
		{
			free(*file);
			*file = (char_u **)"";
			*num_file = -1;
			return FAIL;
		}
		*num_file = count;
		return OK;
	}
	*file = (char_u **)"";
	*num_file = 0;
	if (expand_context == EXPAND_OLD_SETTING)
		return ExpandOldSetting(num_file, file);
	ret = OK;
	reg_ic = p_ic;
	reg_magic = p_magic;
	prog = regcomp(pat);
	if (prog == NULL)
		return FAIL;
	if (expand_context == EXPAND_COMMANDS)
	{
		/* Count the matches: */
		count = 0;
		for (cmdidx = 0; cmdidx < CMD_SIZE; cmdidx++)
			if (regexec(prog, cmdnames[cmdidx].cmd_name, TRUE))
				count++;
		if (count == 0 || (*file = (char_u **)
							alloc((int)(count * sizeof(char_u *)))) == NULL)
			ret = FAIL;
		else
		{
			*num_file = count;
			count = 0;
			for (cmdidx = 0; cmdidx < CMD_SIZE; cmdidx++)
				if (regexec(prog, cmdnames[cmdidx].cmd_name, TRUE))
					(*file)[count++] = strsave(cmdnames[cmdidx].cmd_name);
		}
	}
	else if (expand_context == EXPAND_SETTINGS
	  || expand_context == EXPAND_BOOL_SETTINGS)
		ret = ExpandSettings(prog, num_file, file);
	else if (expand_context == EXPAND_TAGS)
		ret = ExpandTags(prog, num_file, file);
	else
		ret = FAIL;
	
	free(prog);
	return ret;
}

#ifdef VIMINFO
static char_u **viminfo_history[2] = {NULL, NULL};
static int		viminfo_hisidx[2] = {0, 0};
static int		viminfo_hislen = 0;
static int		viminfo_add_at_front = FALSE;

	void
prepare_viminfo_history(len)
	int len;
{
	int i;
	int num;
	int	type;

	init_history();
	viminfo_add_at_front = (len != 0);
	if (len > hislen)
		len = hislen;

	for (type = 0; type <= 1; ++type)
	{
		/* If there are more spaces available than we request, then fill them up */
		for (i = 0, num = 0; i < hislen; i++)
			if (history[type][i] == NULL)
				num++;
		if (num > len)
			len = num;
		viminfo_hisidx[type] = 0;
		viminfo_history[type] = (char_u **)lalloc(len * sizeof(char_u *), FALSE);
	}
	viminfo_hislen = len;
	if (viminfo_history[0] == NULL || viminfo_history[1] == NULL)
		viminfo_hislen = 0;
}

	int
read_viminfo_history(line, lnum, fp, force)
	char_u	*line;
	linenr_t *lnum;
	FILE	*fp;
	int		force;
{
	int		type;

	type = (line[0] == ':' ? 0 : 1);
	if (viminfo_hisidx[type] != viminfo_hislen)
	{
		viminfo_readstring(line);
		if (!is_in_history(type, line + 1, viminfo_add_at_front))
			viminfo_history[type][viminfo_hisidx[type]++] = strsave(line + 1);
	}
	return vim_fgets(line, LSIZE, fp, lnum);
}

	void
finish_viminfo_history()
{
	int idx;
	int i;
	int	type;

	for (type = 0; type <= 1; ++type)
	{
		if (history[type] == NULL)
			return;
		idx = hisidx[type] + viminfo_hisidx[type];
		if (idx >= hislen)
			idx -= hislen;
		if (viminfo_add_at_front)
			hisidx[type] = idx;
		else
		{
			if (hisidx[type] == -1)
				hisidx[type] = hislen - 1;
			do
			{
				if (history[type][idx] != NULL)
					break;
				if (++idx == hislen)
					idx = 0;
			} while (idx != hisidx[type]);
			if (idx != hisidx[type] && --idx < 0)
				idx = hislen - 1;
		}
		for (i = 0; i < viminfo_hisidx[type]; i++)
		{
			history[type][idx] = viminfo_history[type][i];
			if (--idx < 0)
				idx = hislen - 1;
		}
		free(viminfo_history[type]);
		viminfo_history[type] = NULL;
	}
}

	void
write_viminfo_history(fp)
	FILE	*fp;
{
	int		i;
	int		type;

	init_history();
	if (hislen == 0)
		return;
	for (type = 0; type <= 1; ++type)
	{
		fprintf(fp, "\n# %s History (newest to oldest):\n",
							type == 0 ? "Command Line" : "Search string");
		i = hisidx[type];
		if (i >= 0)
			do
			{
				if (history[type][i] != NULL)
				{
					putc(type == 0 ? ':' : '?', fp);
					viminfo_writestring(fp, history[type][i]);
				}
				if (--i < 0)
					i = hislen - 1;
			} while (i != hisidx[type]);
	}
}
#endif /* VIMINFO */
