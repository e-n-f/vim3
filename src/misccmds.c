/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * misccmds.c: functions that didn't seem to fit elsewhere
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"

static int prefix_in_list __ARGS((char_u *, char_u *));
static void check_status __ARGS((BUF *));

static char_u *(si_tab[]) = {(char_u *)"if", (char_u *)"else", (char_u *)"while", (char_u *)"for", (char_u *)"do"};

/*
 * count the size of the indent in the current line
 */
	int
get_indent()
{
	register char_u *ptr;
	register int count = 0;

	for (ptr = ml_get(curwin->w_cursor.lnum); *ptr; ++ptr)
	{
		if (*ptr == TAB)	/* count a tab for what it is worth */
			count += (int)curbuf->b_p_ts - (count % (int)curbuf->b_p_ts);
		else if (*ptr == ' ')
			++count;			/* count a space for one */
		else
			break;
	}
	return (count);
}

/*
 * set the indent of the current line
 * leaves the cursor on the first non-blank in the line
 */
	void
set_indent(size, delete)
	register int size;
	int delete;
{
	int				oldstate = State;
	register int	c;

	State = INSERT;		/* don't want REPLACE for State */
	curwin->w_cursor.col = 0;
	if (delete)							/* delete old indent */
	{
		while ((c = gchar_cursor()), iswhite(c))
			(void)delchar(FALSE);
	}
	if (!curbuf->b_p_et)			/* if 'expandtab' is set, don't use TABs */
		while (size >= (int)curbuf->b_p_ts)
		{
			inschar(TAB);
			size -= (int)curbuf->b_p_ts;
		}
	while (size)
	{
		inschar(' ');
		--size;
	}
	State = oldstate;
}

/*
 * Opencmd
 *
 * Add a new line below or above the current line.
 * Caller must take care of undo.
 *
 * Return TRUE for success, FALSE for failure
 */

	int
Opencmd(dir, redraw)
	int 		dir;			/* FORWARD or BACKWARD */
	int			redraw;			/* redraw afterwards */
{
	char_u  *saved_line;		/* copy of the original line */
	char_u	*p_extra;
	FPOS	old_cursor; 		/* old cursor position */
	int		newcol = 0;			/* new cursor column */
	int 	newindent = 0;		/* auto-indent of the new line */
	int		n = 0;				/* init for gcc */
	int		trunc_line = FALSE;	/* truncate current line afterwards */
	int		no_si = FALSE;		/* reset did_si afterwards */
	int		retval = FALSE;		/* return value, default is FAIL */
	int		lead_len;
	char_u	*leader = NULL;
	char_u	*allocated = NULL;
	char_u	*p;
	int		saved_char;
	int		temp;
	FPOS	*pos;

	/*
	 * make a copy of the current line so we can mess with it
	 */
	saved_line = strsave(ml_get(curwin->w_cursor.lnum));
	if (saved_line == NULL)			/* out of memory! */
		return FALSE;

	saved_char = saved_line[curwin->w_cursor.col];
	if (State == INSERT || State == REPLACE)
		saved_line[curwin->w_cursor.col] = NUL;

	u_clearline();				/* cannot do "U" command when adding lines */
	did_si = FALSE;
	if (curbuf->b_p_ai || curbuf->b_p_si)
	{
		/*
		 * count white space on current line
		 */
		newindent = get_indent();
		if (newindent == 0)
			newindent = old_indent;		/* for ^^D command in insert mode */
		old_indent = 0;

			/*
			 * If we just did an auto-indent, then we didn't type anything on
			 * the prior line, and it should be truncated.
			 */
		if (dir == FORWARD && did_ai)
			trunc_line = TRUE;
		else if (curbuf->b_p_si && *saved_line != NUL)
		{
#if 1		/* code from Robert */
			char_u	*ptr;
			char_u	last_char;
			char_u	*pp;
			int		i;

			old_cursor = curwin->w_cursor;
			ptr = saved_line;
			lead_len = get_leader_len(ptr);
			if (dir == FORWARD)
			{
				/*
				 * Skip preprocessor directives, unless they are recognised as
				 * comments.
				 */
				if (lead_len == 0 && ptr[0] == '#')
				{
					while (ptr[0] == '#' && curwin->w_cursor.lnum > 1)
						ptr = ml_get(--curwin->w_cursor.lnum);
					newindent = get_indent();
				}
				lead_len = get_leader_len(ptr);
				if (lead_len > 0)
				{
					/*
					 * This case gets the following right:
					 *		\*
					 *		 * A comment (read "\" as "/").
					 *		 *\
					 * #define IN_THE_WAY
					 *		This should line up here;
					 */
					p = ptr;
					skipwhite(&p);
					if (p[0] == '/' && p[1] == '*')
						p++;
					if (p[0] == '*')
					{
						for (p++; *p; p++)
						{
							if (p[0] == '/' && p[-1] == '*')
							{
								/*
								 * End of C comment, indent should line up with
								 * the line containing the start of the comment
								 */
								curwin->w_cursor.col = p - ptr;
								if ((pos = findmatch(NUL)) != NULL)
								{
									curwin->w_cursor.lnum = pos->lnum;
									newindent = get_indent();
								}
							}
						}
					}
				}
				else	/* Not a comment line */
				{
					/* Find last non-blank in line */
					p = ptr + STRLEN(ptr) - 1;
					while (p > ptr && iswhite(*p))
						--p;
					last_char = *p;

					/*
					 * find the character just before the '{' or ';'
					 */
					if (last_char == '{' || last_char == ';')
					{
						if (p > ptr)
							--p;
						while (p > ptr && iswhite(*p))
							--p;
					}
					/*
					 * Try to catch lines that are split over multiple
					 * lines.  eg:
					 *		if (condition &&
					 *					condition) {
					 *			Should line up here!
					 *		}
					 */
					if (*p == ')')
					{
						curwin->w_cursor.col = p - ptr;
						if ((pos = findmatch('(')) != NULL)
						{
							curwin->w_cursor.lnum = pos->lnum;
							newindent = get_indent();
							ptr = ml_get(curwin->w_cursor.lnum);
						}
					}
					/*
					 * If last character is '{' do indent, without checking
					 * for "if" and the like.
					 */
					if (last_char == '{')
					{
						did_si = TRUE;	/* do indent */
						no_si = TRUE;	/* don't delete it when '{' typed */
					}
					/*
					 * Look for "if" and the like.
					 * Don't do this if the previous line ended in ';' or '}'.
					 */
					else if (last_char != ';' && last_char != '}')
					{
						p = ptr;
						skipwhite(&p);
						for (pp = p; islower(*pp); ++pp)
							;
						/* Careful for vars starting with "if" */
						if (!isidchar(*pp))
						{
							temp = *pp;
							*pp = NUL;
							for (i = sizeof(si_tab)/sizeof(char_u *); --i >= 0;)
								if (STRCMP(p, si_tab[i]) == 0)
								{
									did_si = TRUE;
									break;
								}
							*pp = temp;
						}
					}
				}
			}
			else /* dir == BACKWARD */
			{
				/*
				 * Skip preprocessor directives, unless they are recognised as
				 * comments.
				 */
				if (lead_len == 0 && ptr[0] == '#')
				{
					int was_backslashed = FALSE;

					while ((ptr[0] == '#' || was_backslashed) &&
							curwin->w_cursor.lnum < curbuf->b_ml.ml_line_count)
					{
						if (*ptr && ptr[STRLEN(ptr) - 1] == '\\')
							was_backslashed = TRUE;
						else
							was_backslashed = FALSE;
						ptr = ml_get(++curwin->w_cursor.lnum);
					}
					if (was_backslashed)
						newindent = 0;		/* Got to end of file */
					else
						newindent = get_indent();
				}
				p = ptr;
				skipwhite(&p);
				if (*p == '}')			/* if line starts with '}': do indent */
					did_si = TRUE;
				else
					can_si_back = TRUE;	/* can delete indent when '{' typed */
			}
			curwin->w_cursor = old_cursor;

#else		/* old code */

			char_u	*pp;
			int		i;

			if (dir == FORWARD)
			{
				p = saved_line + STRLEN(saved_line) - 1;
											/* find last non-blank in line */
				while (p > saved_line && isspace(*p))
					--p;
				if (*p == '{')				/* line ends in '{': do indent */
				{
					did_si = TRUE;			/* do indent */
					no_si = TRUE;			/* don't delete it when '{' typed */
				}
				else						/* look for "if" and the like */
				{
					p = saved_line;
					skipwhite(&p);
					for (pp = p; islower(*pp); ++pp)
						;
									/* careful for vars starting with "if" */
					if (!isidchar_id(*pp))
					{
						temp = *pp;
						*pp = NUL;
						for (i = sizeof(si_tab)/sizeof(char_u *); --i >= 0; )
							if (STRCMP(p, si_tab[i]) == 0)
							{
								did_si = TRUE;
								break;
							}
						*pp = temp;
					}
				}
			}
			else
			{
				p = saved_line;
				skipwhite(&p);
				if (*p == '}')			/* if line starts with '}': do indent */
					did_si = TRUE;
			}
#endif
		}
		did_ai = TRUE;
		if (curbuf->b_p_si)
			can_si = TRUE;
	}
	p_extra = NULL;
	lead_len = get_leader_len(saved_line);
	if (lead_len > 0)
	{
		for (n = 0; iswhite(saved_line[n]); n++)
			;
		if (saved_line[n] == '/' && saved_line[n + 1] == '*')
		{
			if (dir == FORWARD)
				n++;
			else
				lead_len = 0;
		}
		if (saved_line[n] == '*' && dir == FORWARD)
		{
			for (p = saved_line + n + 1; *p; p++)
			{
				if (*p == '/' && p[-1] == '*')
				{
					/* We have finished a C comment, so do a normal indent to
					 * align with the line containing the start of the
					 * comment -- webb.
					 */
					lead_len = 0;
					old_cursor = curwin->w_cursor;
					curwin->w_cursor.col = p - saved_line;
					if ((pos = findmatch(NUL)) != NULL)
					{
						curwin->w_cursor.lnum = pos->lnum;
						newindent = get_indent();
					}
					curwin->w_cursor = old_cursor;
					break;
				}
			}
		}
	}
	saved_line[curwin->w_cursor.col] = saved_char;

	if (State == INSERT || State == REPLACE)	/* only when dir == FORWARD */
	{
		p_extra = saved_line + curwin->w_cursor.col;
		if (curwin->w_cursor.col < lead_len)
			lead_len = curwin->w_cursor.col;
		if (lead_len > 0)
		{
				/* save comment leader in leader[] */
			leader = allocated = alloc(lead_len + STRLEN(p_extra) + 2);
			if (leader != NULL)
			{
				STRNCPY(leader, saved_line, lead_len);
				leader[lead_len] = NUL;
				did_si = can_si = FALSE;
			}
		}
		/*
		 * When 'ai' set, skip to the first non-blank.
		 *
		 * When in REPLACE mode, put the deleted blanks on the replace
		 * stack, followed by a NUL, so they can be put back when
		 * a BS is entered.
		 */
		if (State == REPLACE)
			replace_push(NUL);		/* end of extra blanks */
		if (curbuf->b_p_ai)
		{
			while (*p_extra == ' ' || *p_extra == '\t')
			{
				if (State == REPLACE)
					replace_push(*p_extra);
				++p_extra;
			}
		}
		if (*p_extra != NUL)
			did_ai = FALSE; 		/* append some text, don't trucate now */
	}
	else if (lead_len > 0)
	{
		leader = allocated = alloc(lead_len + 2);
		if (leader != NULL)
		{
			STRNCPY(leader, saved_line, lead_len);
			leader[lead_len] = NUL;
			did_si = can_si = FALSE;
		}
	}

	if (leader != NULL)
	{
		if (n > 0 && !iswhite(leader[n - 1]))
		{
			leader[n - 1] = ' ';		/* replace "/" before "*" with " " */
			newindent++;
		}
		/*
		 * When doing 'O' on the end of a comment, replace the '/' after the
		 * '*' with a space.
		 */
		if (leader[n] == '*' && leader[n + 1] == '/')
			leader[n + 1] = ' ';

		/*
		 * if the leader ends in '*' make sure there is a space after it
		 */
		if (lead_len > 1 && leader[lead_len - 1] == '*')
		{
			leader[lead_len++] = ' ';
			leader[lead_len] = NUL;
		}
		newcol = lead_len - n;
		/*
		 * if a new indent will be set below, remove the indent that is in
		 * the comment leader
		 */
		if (newindent || did_si)
		{
			while (lead_len && iswhite(*leader))
			{
				--lead_len;
				++leader;
			}
		}
	}

	if (p_extra == NULL)
		p_extra = (char_u *)"";				/* append empty line */

		/* concatenate leader and p_extra, if there is a leader */
	if (leader)
	{
		STRCAT(leader, p_extra);
		p_extra = leader;
	}

	old_cursor = curwin->w_cursor;
	if (dir == BACKWARD)
		--curwin->w_cursor.lnum;
	if (ml_append(curwin->w_cursor.lnum, p_extra, (colnr_t)0, FALSE) == FAIL)
		goto theend;
	mark_adjust(curwin->w_cursor.lnum + 1, MAXLNUM, 1L);
	if (newindent || did_si)
	{
		++curwin->w_cursor.lnum;
		if (did_si)
		{
			if (p_sr)
				newindent -= newindent % (int)curbuf->b_p_sw;
			newindent += (int)curbuf->b_p_sw;
		}
		set_indent(newindent, FALSE);
		/*
		 * In REPLACE mode the new indent must be put on
		 * the replace stack for when it is deleted with BS
		 */
		if (State == REPLACE)
			for (n = 0; n < curwin->w_cursor.col; ++n)
				replace_push(NUL);
		newcol += curwin->w_cursor.col;
		if (no_si)
			did_si = FALSE;
	}
	/*
	 * In REPLACE mode the extra leader must be put on the replace stack for
	 * when it is deleted with BS.
	 */
	if (State == REPLACE)
		while (lead_len-- > 0)
			replace_push(NUL);

	curwin->w_cursor = old_cursor;

	if (dir == FORWARD)
	{
		if (trunc_line || State == INSERT || State == REPLACE)
		{
			if (trunc_line)
			{
					/* find start of trailing white space */
				for (n = STRLEN(saved_line); n > 0 && iswhite(saved_line[n - 1]); --n)
					;
				saved_line[n] = NUL;
			}
			else
				*(saved_line + curwin->w_cursor.col) = NUL;	/* truncate current line at cursor */
			ml_replace(curwin->w_cursor.lnum, saved_line, FALSE);
			saved_line = NULL;
		}

		/*
		 * Get the cursor to the start of the line, so that 'curwin->w_row' gets
		 * set to the right physical line number for the stuff that
		 * follows...
		 */
		curwin->w_cursor.col = 0;

		if (redraw)
		{
			n = RedrawingDisabled;
			RedrawingDisabled = TRUE;
			cursupdate();				/* don't want it to update srceen */
			RedrawingDisabled = n;

			/*
			 * If we're doing an open on the last logical line, then go ahead and
			 * scroll the screen up. Otherwise, just insert a blank line at the
			 * right place. We use calls to plines() in case the cursor is
			 * resting on a long line.
			 */
			n = curwin->w_row + plines(curwin->w_cursor.lnum);
			if (n == curwin->w_height)
				scrollup(1L);
			else
				win_ins_lines(curwin, n, 1, TRUE, TRUE);
		}
		++curwin->w_cursor.lnum;	/* cursor moves down */
	}
	else if (redraw) 				/* insert physical line above current line */
		win_ins_lines(curwin, curwin->w_row, 1, TRUE, TRUE);

	curwin->w_cursor.col = newcol;
	if (redraw)
	{
		updateScreen(VALID_TO_CURSCHAR);
		cursupdate();			/* update curwin->w_row */
	}
	CHANGED;

	retval = TRUE;				/* success! */
theend:
	free(saved_line);
	free(allocated);
	return retval;
}

/*
 * get_leader_len() returns the length of the prefix of the given string
 * which introduces a comment.  If this string is not a comment then 0 is
 * returned.  If the FO_COMS_PADDED character in p_fo is present, then
 * comments must be followed by at least one space or tab.
 */
	int
get_leader_len(str)
	char_u	*str;
{
	int		len;
	int		i;
	int		got_com = FALSE;

	if (!fo_do_comments)		/* don't format comments at all */
		return 0;
	for (i = 0; str[i]; )
	{
		if (iswhite(str[i]))
		{
			++i;
			continue;
		}
		if (!got_com && (len = prefix_in_list(str + i, curbuf->b_p_com)) > 0)
		{
			got_com = TRUE;
			i += len;
			break;
		}
		else if ((len = prefix_in_list(str + i, curbuf->b_p_ncom)) > 0)
		{
			got_com = TRUE;
			i += len;
		}
		else
			break;
	}
	return (got_com ? i : 0);
}

/*
 * Check whether any word in 'list' is a prefix for 'str'.  Return 0 if no
 * word is a prefix, or the length of the prefix otherwise.
 * 'list' must be a comma separated list of strings. The strings cannot
 * contain a comma.
 */
	static int
prefix_in_list(str, list)
	char_u	*str;
	char_u	*list;
{
	int		len;
	int		found;

	if (list == NULL)		/* can happen if option is not set */
		return 0;
	while (*list)
	{
		found = TRUE;
		for (len = 0; *list && *list != ','; ++len, ++list)
		{
		    /*
		     * If string contains a ' ', accept any non-empty sequence of
		     * blanks, and end-of-line.
		     */
		    if (*list == ' ' && (iswhite(str[len]) || str[len] == NUL))
		    {
		  	    while (iswhite(str[len]))
			  	    ++len;
				while (*list == ' ')
					++list;
			    if (*list == NUL || *list == ',')
				    break;
		    }
		    if (str[len] != *list)
			    found = FALSE;
	    }
	    if (found)
	    {
		    /* Match with any trailing blanks */
		    while (iswhite(str[len]))
			    ++len;
		    return len;
	    }
		if (*list == ',')
			++list;
	}
	return 0;
}

/*
 * plines(p) - return the number of physical screen lines taken by line 'p'
 */
	int
plines(p)
	linenr_t	p;
{
	return plines_win(curwin, p);
}
	
	int
plines_win(wp, p)
	WIN			*wp;
	linenr_t	p;
{
	register long		col;
	register char_u		*s;
	register int		lines;

	if (!wp->w_p_wrap)
		return 1;

	s = ml_get_buf(wp->w_buffer, p, FALSE);
	if (*s == NUL)				/* empty line */
		return 1;

	col = linetabsize(s);

	/*
	 * If list mode is on, then the '$' at the end of the line takes up one
	 * extra column.
	 */
	if (wp->w_p_list)
		col += 1;

	/*
	 * If 'number' mode is on, add another 8.
	 */
	if (wp->w_p_nu)
		col += 8;

	lines = (col + (Columns - 1)) / Columns;
	if (lines <= wp->w_height)
		return lines;
	return (int)(wp->w_height);		/* maximum length */
}

/*
 * Count the physical lines (rows) for the lines "first" to "last" inclusive.
 */
	int
plines_m(first, last)
	linenr_t		first, last;
{
	return plines_m_win(curwin, first, last);
}

	int
plines_m_win(wp, first, last)
	WIN				*wp;
	linenr_t		first, last;
{
	int count = 0;

	while (first <= last)
		count += plines_win(wp, first++);
	return (count);
}

/*
 * Insert or replace a single character at the cursor position.
 * When in REPLACE mode, replace any existing character.
 */
	void
inschar(c)
	int			c;
{
	register char_u  *p;
	char_u			*new;
	char_u			*old;
	int				oldlen;
	int				extra;
	colnr_t			col = curwin->w_cursor.col;
	linenr_t		lnum = curwin->w_cursor.lnum;

	old = ml_get(lnum);
	oldlen = STRLEN(old) + 1;

	if (State != REPLACE || *(old + col) == NUL)
		extra = 1;
	else
		extra = 0;

	/*
	 * a character has to be put on the replace stack if there is a
	 * character that is replaced, so it can be put back when BS is used.
	 * Otherwise a 0 is put on the stack, indicating that a new character
	 * was inserted, which can be deleted when BS is used.
	 */
	if (State == REPLACE)
		replace_push(!extra ? *(old + col) : 0);
	new = alloc_check((unsigned)(oldlen + extra));
	if (new == NULL)
		return;
	memmove((char *)new, (char *)old, (size_t)col);
	p = new + col;
	memmove((char *)p + extra, (char *)old + col, (size_t)(oldlen - col));
	*p = c;
	ml_replace(lnum, new, FALSE);

	/*
	 * If we're in insert or replace mode and 'showmatch' is set, then check for
	 * right parens and braces. If there isn't a match, then beep. If there
	 * is a match AND it's on the screen, then flash to it briefly. If it
	 * isn't on the screen, don't do anything.
	 */
	if (p_sm && (State & INSERT) && (c == ')' || c == '}' || c == ']'))
		showmatch();
	if (!p_ri || State == REPLACE)		/* normal insert: cursor right */
		++curwin->w_cursor.col;
	CHANGED;
}

/*
 * Insert a string at the cursor position.
 * Note: Nothing special for replace mode.
 */
	void
insstr(s)
	register char_u  *s;
{
	register char_u		*old, *new;
	register int		newlen = STRLEN(s);
	int					oldlen;
	colnr_t				col = curwin->w_cursor.col;
	linenr_t			lnum = curwin->w_cursor.lnum;

	old = ml_get(lnum);
	oldlen = STRLEN(old);
	new = alloc_check((unsigned)(oldlen + newlen + 1));
	if (new == NULL)
		return;
	memmove((char *)new, (char *)old, (size_t)col);
	memmove((char *)new + col, (char *)s, (size_t)newlen);
	memmove((char *)new + col + newlen, (char *)old + col, (size_t)(oldlen - col + 1));
	ml_replace(lnum, new, FALSE);
	curwin->w_cursor.col += newlen;
	CHANGED;
}

/*
 * delete one character under the cursor
 *
 * return FAIL for failure, OK otherwise
 */
	int
delchar(fixpos)
	int			fixpos; 	/* if TRUE fix the cursor position when done */
{
	char_u		*old, *new;
	int			oldlen;
	linenr_t	lnum = curwin->w_cursor.lnum;
	colnr_t		col = curwin->w_cursor.col;
	int			was_alloced;

	old = ml_get(lnum);
	oldlen = STRLEN(old);

	if (col >= oldlen)	/* can't do anything (happens with replace mode) */
		return FAIL;

/*
 * If the old line has been allocated the deletion can be done in the
 * existing line. Otherwise a new line has to be allocated
 */
	was_alloced = ml_line_alloced();		/* check if old was allocated */
	if (was_alloced)
		new = old;							/* use same allocated memory */
	else
	{
		new = alloc((unsigned)oldlen);		/* need to allocated a new line */
		if (new == NULL)
			return FAIL;
		memmove((char *)new, (char *)old, (size_t)col);
	}
	memmove((char *)new + col, (char *)old + col + 1, (size_t)(oldlen - col));
	if (!was_alloced)
		ml_replace(lnum, new, FALSE);

	/*
	 * If we just took off the last character of a non-blank line, we don't
	 * want to end up positioned at the NUL.
	 */
	if (fixpos && curwin->w_cursor.col > 0 && col == oldlen - 1)
		--curwin->w_cursor.col;

	CHANGED;
	return OK;
}

	void
dellines(nlines, dowindow, undo)
	long 			nlines;			/* number of lines to delete */
	int 			dowindow;		/* if true, update the window */
	int				undo;			/* if true, prepare for undo */
{
	int 			num_plines = 0;

	if (nlines <= 0)
		return;
	/*
	 * There's no point in keeping the window updated if we're deleting more
	 * than a window's worth of lines.
	 */
	if (nlines > (curwin->w_height - curwin->w_row) && dowindow)
	{
		dowindow = FALSE;
		/* flaky way to clear rest of window */
		win_del_lines(curwin, curwin->w_row, curwin->w_height, TRUE, TRUE);
	}
	if (undo && u_savedel(curwin->w_cursor.lnum, nlines) == FAIL)
		return;

	mark_adjust(curwin->w_cursor.lnum, curwin->w_cursor.lnum + nlines - 1, MAXLNUM);
	mark_adjust(curwin->w_cursor.lnum + nlines, MAXLNUM, -nlines);

	while (nlines-- > 0)
	{
		if (bufempty()) 		/* nothing to delete */
			break;

		/*
		 * Set up to delete the correct number of physical lines on the
		 * window
		 */
		if (dowindow)
			num_plines += plines(curwin->w_cursor.lnum);

		ml_delete(curwin->w_cursor.lnum, TRUE);

		CHANGED;

		/* If we delete the last line in the file, stop */
		if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
		{
			curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
			break;
		}
	}
	curwin->w_cursor.col = 0;
	/*
	 * Delete the correct number of physical lines on the window
	 */
	if (dowindow && num_plines > 0)
		win_del_lines(curwin, curwin->w_row, num_plines, TRUE, TRUE);
}

	int
gchar(pos)
	FPOS *pos;
{
	return (int)(*(ml_get_pos(pos)));
}

	int
gchar_cursor()
{
	return (int)(*(ml_get_cursor()));
}

/*
 * Write a character at the current cursor position.
 * It is directly written into the block.
 */
	void
pchar_cursor(c)
	int c;
{
	*(ml_get_buf(curbuf, curwin->w_cursor.lnum, TRUE) + curwin->w_cursor.col) = c;
}

/*
 * return TRUE if the cursor is before or on the first non-blank in the line
 */
	int
inindent()
{
	register char_u *ptr;
	register int col;

	for (col = 0, ptr = ml_get(curwin->w_cursor.lnum); iswhite(*ptr); ++col)
		++ptr;
	if (col >= curwin->w_cursor.col)
		return TRUE;
	else
		return FALSE;
}

/*
 * skipwhite: skip over ' ' and '\t'.
 *
 * note: you must give a pointer to a char_u pointer!
 */
	void
skipwhite(pp)
	char_u **pp;
{
    register char_u *p;
    
    for (p = *pp; *p == ' ' || *p == '\t'; ++p)	/* skip to next non-white */
    	;
    *pp = p;
}

/*
 * skiptowhite: skip over text until ' ' or '\t'.
 *
 * note: you must give a pointer to a char_u pointer!
 */
	void
skiptowhite(pp)
	char_u **pp;
{
	register char_u *p;

	for (p = *pp; *p != ' ' && *p != '\t' && *p != NUL; ++p)
		;
	*pp = p;
}

/*
 * skiptodigit: skip over text until digit found
 *
 * note: you must give a pointer to a char_u pointer!
 */
	void
skiptodigit(pp)
	char_u **pp;
{
	register char_u *p;

	for (p = *pp; !isdigit(*p) && *p != NUL; ++p)
		;
	*pp = p;
}

/*
 * getdigits: get a number from a string and skip over it
 *
 * note: you must give a pointer to a char_u pointer!
 */

	long
getdigits(pp)
	char_u **pp;
{
    register char_u *p;
	long retval;
    
	p = *pp;
	retval = atol((char *)p);
    while (isdigit(*p))	/* skip to next non-digit */
    	++p;
    *pp = p;
	return retval;
}

	char_u *
plural(n)
	long n;
{
	static char_u buf[2] = "s";

	if (n == 1)
		return &(buf[1]);
	return &(buf[0]);
}

/*
 * set_Changed is called when something in the current buffer is changed
 */
	void
set_Changed()
{
	if (!curbuf->b_changed)
	{
		change_warning();
		curbuf->b_changed = TRUE;
		check_status(curbuf);
	}
}

/*
 * unset_Changed is called when the changed flag must be reset for buffer 'buf'
 */
	void
unset_Changed(buf)
	BUF		*buf;
{
	if (buf->b_changed)
	{
		buf->b_changed = 0;
		check_status(buf);
	}
}

/*
 * check_status: called when the status bars for the buffer 'buf'
 *				 need to be updated
 */
	static void
check_status(buf)
	BUF		*buf;
{
	WIN		*wp;
	int		i;

	i = 0;
	for (wp = firstwin; wp != NULL; wp = wp->w_next)
		if (wp->w_buffer == buf && wp->w_status_height)
		{
			wp->w_redr_status = TRUE;
			++i;
		}
	if (i)
		redraw_later(NOT_VALID);
}

/*
 * If the file is readonly, give a warning message with the first change.
 * Don't use emsg(), because it flushes the macro buffer.
 * If we have undone all changes b_changed will be FALSE, but b_did_warn
 * will be TRUE.
 */
	void
change_warning()
{
	if (curbuf->b_did_warn == FALSE && curbuf->b_changed == 0 && curbuf->b_p_ro)
	{
		curbuf->b_did_warn = TRUE;
		MSG("Warning: Changing a readonly file");
		sleep(1);			/* give him some time to think about it */
	}
}

/*
 * Ask for a reply from the user, a 'y' or a 'n'.
 * No other characters are accepted, the message is repeated until a valid
 * reply is entered or CTRL-C is hit.
 * If direct is TRUE, don't use vgetc but GetChars, don't get characters from
 * any buffers but directly from the user.
 *
 * return the 'y' or 'n'
 */
	int
ask_yesno(str, direct)
	char_u	*str;
	int		direct;
{
	int		r = ' ';
	char_u	buf[20];
	int		len = 0;
	int		idx = 0;

	while (r != 'y' && r != 'n')
	{
		(void)set_highlight('r');	/* same highlighting as for wait_return */
		msg_highlight = TRUE;
		smsg((char_u *)"%s (y/n)?", str);
		if (direct)
		{
			if (idx >= len)
			{
				len = GetChars(buf, 20, -1);
				idx = 0;
			}
			r = buf[idx++];
		}
		else
			r = vgetc();
		if (r == Ctrl('C') || r == ESC)
			r = 'n';
		msg_outchar(r);		/* show what you typed */
		flushbuf();
	}
	return r;
}

/*
 * get a number from the user
 */
	int
get_number()
{
	int		n = 0;
	int		c;

	for (;;)
	{
		windgoto(msg_row, msg_col);
		screen_start();
		c = vgetc();
		if (isdigit(c))
		{
			n = n * 10 + c - '0';
			msg_outchar(c);
		}
		else if (c == K_DEL || c == K_BS)
		{
			n /= 10;
			MSG_OUTSTR("\b \b");
		}
		else if (c == CR || c == NL || c == Ctrl('C'))
			break;
	}
	return n;
}

	void
msgmore(n)
	long n;
{
	long pn;

	if (global_busy ||		/* no messages now, wait until global is finished */
			keep_msg)		/* there is a message already, skip this one */
		return;

	if (n > 0)
		pn = n;
	else
		pn = -n;

	if (pn > p_report)
	{
#ifdef ADDED_BY_WEBB_COMPILE
		sprintf((char *)msg_buf, "%ld %s line%s %s",
#else
		sprintf((char *)msg_buf, (char_u *)"%ld %s line%s %s",
#endif /* ADDED_BY_WEBB_COMPILE */
				pn, n > 0 ? "more" : "fewer", plural(pn),
				got_int ? "(Interrupted)" : "");
		if (msg(msg_buf) && !msg_scroll)
			keep_msg = msg_buf;
	}
}

/*
 * flush map and typeahead buffers and give a warning for an error
 */
	void
beep_flush()
{
	flush_buffers(FALSE);
	beep();
}

/*
 * give a warning for an error
 */
	void
beep()
{
	if (p_vb)
	{
		if (T_VB && *T_VB)
		    outstr(T_VB);
#if 0		/* this mostly goes too fast to be seen */
		else
		{						/* very primitive visual bell */
	        MSG("    ^G");
	        MSG("     ^G");
	        MSG("    ^G ");
	        MSG("     ^G");
	        MSG("       ");
			showmode();			/* may have deleted the mode message */
		}
#endif
	}
	else
	    outchar('\007');
}

/* 
 * Expand environment variable with path name.
 * "~/" is also expanded, like $HOME.
 * If anything fails no expansion is done and dst equals src.
 * Note that IObuff must NOT be used as either src or dst!  This is because
 * vimgetenv() may use IObuff to do its expansion.
 */
	void
expand_env(src, dst, dstlen)
	char_u	*src;			/* input string e.g. "$HOME/vim.hlp" */
	char_u	*dst;			/* where to put the result */
	int		dstlen;			/* maximum length of the result */
{
	char_u	*tail;
	int		c;
	char_u	*var;

	skipwhite(&src);
	while (*src && dstlen > 0)
	{
		if (*src == '$' || (*src == '~' && STRCHR("/ \t\n", src[1]) != NULL))
		{
			/*
			 * The variable name is copied into dst temporarily, because it may
			 * be a string in read-only memory.
			 */
			if (*src == '$')
			{
				tail = src + 1;
				var = dst;
				c = dstlen - 1;
				while (c-- > 0 && *tail && isidchar(*tail))
					*var++ = *tail++;
				*var = NUL;
				var = vimgetenv(dst);
			}
			else
			{
				var = vimgetenv((char_u *)"HOME");
				tail = src + 1;
			}
			if (var && (STRLEN(var) + STRLEN(tail) + 1 < (unsigned)dstlen))
			{
				STRCPY(dst, var);
				dstlen -= STRLEN(var);
				dst += STRLEN(var);
				src = tail;
			}
		}
		while (*src && *src != ' ' && --dstlen > 0)
			*dst++ = *src++;
		while (*src == ' ' && --dstlen > 0)
			*dst++ = *src++;
	}
	*dst = NUL;
}

/* 
 * Replace home directory by "~/"
 * If anything fails dst equals src.
 */
	void
home_replace(src, dst, dstlen)
	char_u	*src;			/* input file name */
	char_u	*dst;			/* where to put the result */
	int		dstlen;			/* maximum length of the result */
{
	char_u	*home;
	size_t	len;

	/*
	 * If there is no "HOME" environment variable, or when it
	 * is very short, don't replace anything.
	 */
	if ((home = vimgetenv((char_u *)"HOME")) == NULL || (len = STRLEN(home)) <= 1)
		STRNCPY(dst, src, (size_t)dstlen);
	else
	{
		skipwhite(&src);
		while (*src && dstlen > 0)
		{
			if (STRNCMP(src, home, len) == 0)
			{
				src += len;
				if (--dstlen > 0)
					*dst++ = '~';
			}
			while (*src && *src != ' ' && --dstlen > 0)
				*dst++ = *src++;
			while (*src == ' ' && --dstlen > 0)
				*dst++ = *src++;
		}
		*dst = NUL;
	}
}

/*
 * Compare two file names and return TRUE if they are different files.
 * For the first name environment variables are expanded
 */
	int
fullpathcmp(s1, s2)
	char_u *s1, *s2;
{
#ifdef UNIX
	struct stat		st1, st2;
	char_u			buf1[MAXPATHL];

	expand_env(s1, buf1, MAXPATHL);
	if (stat((char *)buf1, &st1) == 0 && stat((char *)s2, &st2) == 0 &&
				st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino)
		return FALSE;
	return TRUE;
#else
	char_u	*buf1 = NULL;
	char_u	*buf2 = NULL;
	int		retval = TRUE;
	
	if ((buf1 = alloc(MAXPATHL)) != NULL && (buf2 = alloc(MAXPATHL)) != NULL)
	{
		expand_env(s1, buf2, MAXPATHL);
		/*
		 * If one of the FullNames() failed, the file probably doesn't exist,
		 * this is counted as a different file name.
		 */
		if (FullName(buf2, buf1, MAXPATHL) == OK &&
								FullName(s2, buf2, MAXPATHL) == OK)
			retval = STRCMP(buf1, buf2);
	}
	free(buf1);
	free(buf2);
	return retval;
#endif
}

/*
 * get the tail of a path: the file name.
 */
	char_u *
gettail(fname)
	char_u *fname;
{
	register char_u *p1, *p2;

	if (fname == NULL)
		return (char_u *)"";
	for (p1 = p2 = fname; *p2; ++p2)	/* find last part of path */
	{
		if (ispathsep(*p2))
			p1 = p2 + 1;
	}
	return p1;
}

/*
 * return TRUE if 'c' is a path separator.
 */
	int
ispathsep(c)
	int c;
{
#ifdef UNIX
	return (c == PATHSEP);		/* UNIX has ':' inside file names */
#else
# ifdef MSDOS
	return (c == ':' || c == PATHSEP || c == '/');
# else
	return (c == ':' || c == PATHSEP);
# endif
#endif
}

/*
 * Concatenate filenames fname1 and fname2 into allocated memory.
 * Only add a '/' when 'sep' is TRUE and it is neccesary.
 */
	char_u	*
concat_fnames(fname1, fname2, sep)
	char_u	*fname1;
	char_u	*fname2;
	int		sep;
{
	char_u	*dest;

	dest = alloc((unsigned)(STRLEN(fname1) + STRLEN(fname2) + 2));
	if (dest != NULL)
	{
		STRCPY(dest, fname1);
		if (sep && *dest && !ispathsep(*(dest + STRLEN(dest) - 1)))
			STRCAT(dest, PATHSEPSTR);
		STRCAT(dest, fname2);
	}
	return dest;
}
