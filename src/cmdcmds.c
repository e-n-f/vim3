/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * cmdcmds.c: functions for command line commands
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"

#if defined(LATTICE) || defined(NT)
# define mktemp(a)	tmpnam(a)
#endif

extern char		*mktemp __ARGS((char *));

#ifdef VIMINFO
static char_u *viminfo_filename __ARGS((char_u 	*));
static void do_viminfo __ARGS((FILE *fp_in, FILE *fp_out, int want_info, int want_marks, int force_read));
static int read_viminfo_up_to_marks __ARGS((char_u *line, linenr_t *lnum, FILE *fp, int force));
#endif /* VIMINFO */

/*
 * align text:
 * type = -1  left aligned
 * type = 0   centered
 * type = 1   right aligned
 */
	void
do_align(start, end, width, type)
	linenr_t	start;
	linenr_t	end;
	int			width;
	int			type;
{
	FPOS	pos;
	int		len;
	int		indent = 0;
	int		new = 0;			/* init for GCC */
	char_u	*first;
	char_u	*last;
	int		save;

	pos = curwin->w_cursor;
	if (type == -1)		/* left align: width is used for new indent */
	{
		if (width >= 0)
			indent = width;
	}
	else
	{
		/*
		 * if 'textwidth' set, use it
		 * else if 'wrapmargin' set, use it
		 * if invalid value, use 80
		 */
		if (width <= 0)
			width = curbuf->b_p_tw;
		if (width == 0 && curbuf->b_p_wm > 0)
			width = Columns - curbuf->b_p_wm;
		if (width <= 0)
			width = 80;
	}

	if (u_save((linenr_t)(start - 1), (linenr_t)(end + 1)) == FAIL)
		return;
	for (curwin->w_cursor.lnum = start;
						curwin->w_cursor.lnum <= end; ++curwin->w_cursor.lnum)
	{
			/* find the first non-blank character */
		first = ml_get(curwin->w_cursor.lnum);
		skipwhite(&first);
			/* find the character after the last non-blank character */
		for (last = first + STRLEN(first);
								last > first && iswhite(last[-1]); --last)
			;
		save = *last;
		*last = NUL;
		len = linetabsize(first);					/* get line lenght */
		*last = save;
		if (len == 0)								/* skip blank lines */
			continue;
		switch (type)
		{
			case -1:	new = indent;				/* left align */
						break;
			case 0:		new = (width - len) / 2;	/* center */
						break;
			case 1:		new = width - len;			/* right align */
						break;
		}
		if (new < 0)
			new = 0;
		set_indent(new, TRUE);			/* set indent */
	}
	curwin->w_cursor = pos;
	beginline(TRUE);
	updateScreen(NOT_VALID);
}

	void
do_retab(start, end, new_ts, force)
	linenr_t	start;
	linenr_t	end;
	int			new_ts;
	int			force;
{
	linenr_t	lnum;
	int			got_tab = FALSE;
	long		num_spaces = 0;
	long		num_tabs = 0;
	long		len;
	long		col;
	long		vcol;
	long		start_col = 0;			/* For start of white-space string */
	long		start_vcol = 0;			/* For start of white-space string */
	int			temp;
	long		old_len;
	char_u		*ptr;
	char_u		*new_line = (char_u *)1;	/* init to non-NULL */
	int			did_something = FALSE;
	int			did_undo;				/* called u_save for current line */

	if (new_ts == 0)
		new_ts = curbuf->b_p_ts;
	for (lnum = start; !got_int && lnum <= end; ++lnum)
	{
		ptr = ml_get(lnum);
		col = 0;
		vcol = 0;
		did_undo = FALSE;
		for(;;)
		{
			if (iswhite(ptr[col]))
			{
				if (!got_tab && num_spaces == 0)
				{
					/* First consecutive white-space */
					start_vcol = vcol;
					start_col = col;
				}
				if (ptr[col] == ' ')
					num_spaces++;
				else
					got_tab = TRUE;
			}
			else
			{
				if (got_tab || (force && num_spaces > 1))
				{
					/* Retabulate this string of white-space */

					/* len is virtual length of white string */
					len = num_spaces = vcol - start_vcol;
					num_tabs = 0;
					if (!curbuf->b_p_et)
					{
						temp = new_ts - (start_vcol % new_ts);
						if (num_spaces >= temp)
						{
							num_spaces -= temp;
							num_tabs++;
						}
						num_tabs += num_spaces / new_ts;
						num_spaces -= (num_spaces / new_ts) * new_ts;
					}
					if (curbuf->b_p_et || got_tab ||
										(num_spaces + num_tabs < len))
					{
						if (did_undo == FALSE)
						{
							did_undo = TRUE;
							if (u_save((linenr_t)(lnum - 1),
												(linenr_t)(lnum + 1)) == FAIL)
							{
								new_line = NULL;		/* flag out-of-memory */
								break;
							}
						}

						/* len is actual number of white characters used */
						len = num_spaces + num_tabs;
						old_len = STRLEN(ptr);
						new_line = lalloc(old_len - col + start_col + len + 1,
																		TRUE);
						if (new_line == NULL)
							break;
						if (start_col > 0)
							memmove((char *)new_line, (char *)ptr, start_col);
						memmove((char *)new_line + start_col + len,
										(char *)ptr + col, old_len - col + 1);
						ptr = new_line + start_col;
						for (col = 0; col < len; col++)
							ptr[col] = (col < num_tabs) ? '\t' : ' ';
						ml_replace(lnum, new_line, FALSE);
						did_something = TRUE;
						ptr = new_line;
						col = start_col + len;
					}
				}
				got_tab = FALSE;
				num_spaces = 0;
			}
			if (ptr[col] == NUL)
				break;
			vcol += chartabsize(ptr[col++], vcol);
		}
		if (new_line == NULL)				/* out of memory */
			break;
		breakcheck();
	}
	if (got_int)
		emsg(e_interr);
	if (did_something)
		CHANGED;
	curbuf->b_p_ts = new_ts;
	coladvance(curwin->w_curswant);
}

/*
 * :move command - move lines line1-line2 to line n
 *
 * return FAIL for failure, OK otherwise
 */
	int
do_move(line1, line2, n)
	linenr_t	line1;
	linenr_t	line2;
	linenr_t	n;
{
	char_u		*str;
	linenr_t	l;
	linenr_t	extra;		/* Num lines added before line1 */
	linenr_t	num_lines;	/* Num lines moved */
	linenr_t	last_line;	/* Last line in file after adding new text */
	int			has_mark;

	if (n >= line1 && n < line2)
	{
		EMSG("Move lines into themselves");
		return FAIL;
	}

	num_lines = line2 - line1 + 1;

	/*
	 * First we copy the old text to its new location -- webb
	 */
	if (u_save(n, n + 1) == FAIL)
		return FAIL;
	for (extra = 0, l = line1; l <= line2; l++)
	{
		str = strsave(ml_get(l + extra));
		if (str != NULL)
		{
			has_mark = ml_has_mark(l + extra);
			ml_append(n + l - line1, str, (colnr_t)0, FALSE);
			free(str);
			if (has_mark)
				ml_setmarked(n + l - line1 + 1);
			if (n < line1)
				extra++;
		}
	}

	/*
	 * Now we must be careful adjusting our marks so that we don't overlap our
	 * mark_adjust() calls.
	 *
	 * We adjust the marks within the old text so that they refer to the
	 * last lines of the file (temporarily), because we know no other marks
	 * will be set there since these line numbers did not exist until we added
	 * our new lines.
	 *
	 * Then we adjust the marks on lines between the old and new text positions
	 * (either forwards or backwards).
	 *
	 * And Finally we adjust the marks we put at the end of the file back to
	 * their final destination at the new text position -- webb
	 */
	last_line = curbuf->b_ml.ml_line_count;
	mark_adjust(line1, line2, last_line - line2);
	if (n >= line2)
		mark_adjust(line2 + 1, n, -num_lines);
	else
		mark_adjust(n + 1, line1 - 1, num_lines);
	mark_adjust(last_line - num_lines + 1, last_line,
												-(last_line - n - extra));
	
	/*
	 * Now we delete the original text -- webb
	 */
	if (u_save(line1 + extra - 1, line2 + extra + 1) == FAIL)
		return FAIL;

	for (l = line1; l <= line2; l++)
		ml_delete(line1 + extra, TRUE);

	CHANGED;
	if (!global_busy && num_lines > p_report)
		smsg((char_u *)"%ld line%s moved", num_lines, plural(num_lines));
	return OK;
}

/*
 * :copy command - copy lines line1-line2 to line n
 */
	void
do_copy(line1, line2, n)
	linenr_t	line1;
	linenr_t	line2;
	linenr_t	n;
{
	linenr_t		lnum;
	char_u			*p;

	mark_adjust(n + 1, MAXLNUM, line2 - line1 + 1);

	/*
	 * there are three situations:
	 * 1. destination is above line1
	 * 2. destination is between line1 and line2
	 * 3. destination is below line2
	 *
	 * n = destination (when starting)
	 * curwin->w_cursor.lnum = destination (while copying)
	 * line1 = start of source (while copying)
	 * line2 = end of source (while copying)
	 */
	if (u_save(n, n + 1) == FAIL)
		return;
	curwin->w_cursor.lnum = n;
	lnum = line2 - line1 + 1;
	while (line1 <= line2)
	{
		/* need to use strsave() because the line will be unlocked
			within ml_append */
		p = strsave(ml_get(line1));
		if (p != NULL)
		{
			ml_append(curwin->w_cursor.lnum, p, (colnr_t)0, FALSE);
			free(p);
		}
				/* situation 2: skip already copied lines */
		if (line1 == n)
			line1 = curwin->w_cursor.lnum;
		++line1;
		if (curwin->w_cursor.lnum < line1)
			++line1;
		if (curwin->w_cursor.lnum < line2)
			++line2;
		++curwin->w_cursor.lnum;
	}
	CHANGED;
	msgmore((long)lnum);
}

/*
 * handle the :! command.
 * We replace the extra bangs by the previously entered command and remember
 * the command.
 */
	void
dobang(addr_count, line1, line2, forceit, arg)
	int			addr_count;
	linenr_t	line1, line2;
	int			forceit;
	char_u		*arg;
{
	static	char_u	*prevcmd = NULL;		/* the previous command */
	char_u			*t;
	char_u			*trailarg;
	int 			len;

	/*
	 * Disallow shell commands from .exrc and .vimrc in current directory for
	 * security reasons.
	 */
	if (secure)
	{
		secure = 2;
		emsg(e_curdir);
		return;
	}
	len = STRLEN(arg) + 1;

	autowrite_all();
	/*
	 * try to find an embedded bang, like in :!<cmd> ! [args]
	 * (:!! is indicated by the 'forceit' variable)
	 */
	trailarg = arg;
	skiptowhite(&trailarg);
	skipwhite(&trailarg);
	if (*trailarg == '!')
		*trailarg++ = NUL;
	else
		trailarg = NULL;

	if (forceit || trailarg != NULL)			/* use the previous command */
	{
		if (prevcmd == NULL)
		{
			emsg(e_noprev);
			return;
		}
		len += STRLEN(prevcmd) * (trailarg != NULL && forceit ? 2 : 1);
	}

	if (len > CMDBUFFSIZE)
	{
		emsg(e_toolong);
		return;
	}
	if ((t = alloc(len)) == NULL)
		return;
	*t = NUL;
	if (forceit)
		STRCPY(t, prevcmd);
	STRCAT(t, arg);
	if (trailarg != NULL)
	{
		STRCAT(t, prevcmd);
		STRCAT(t, trailarg);
	}
	free(prevcmd);
	prevcmd = t;

	if (bangredo)			/* put cmd in redo buffer for ! command */
	{
		AppendToRedobuff(prevcmd);
		AppendToRedobuff((char_u *)"\n");
		bangredo = FALSE;
	}
		/* echo the command */
	msg_start();
	msg_outchar(':');
	if (addr_count)						/* :range! */
	{
		msg_outnum((long)line1);
		msg_outchar(',');
		msg_outnum((long)line2);
	}
	msg_outchar('!');
	msg_outtrans(prevcmd, -1);
	msg_clr_eos();

	if (addr_count == 0)				/* :! */
		doshell(prevcmd); 
	else								/* :range! */
		dofilter(line1, line2, prevcmd, TRUE, TRUE);
}

/*
 * call a shell to execute a command
 */
	void
doshell(cmd)
	char_u	*cmd;
{
	BUF		*buf;

	/*
	 * Disallow shell commands from .exrc and .vimrc in current directory for
	 * security reasons.
	 */
	if (secure)
	{
		secure = 2;
		emsg(e_curdir);
		msg_end();
		return;
	}
	stoptermcap();
	msg_outchar('\n');					/* may shift screen one line up */

		/* warning message before calling the shell */
	if (p_warn)
		for (buf = firstbuf; buf; buf = buf->b_next)
			if (buf->b_changed)
			{
				msg_outstr((char_u *)"[No write since last change]\n");
				break;
			}

	windgoto((int)Rows - 1, 0);
	cursor_on();
	(void)call_shell(cmd, 0, TRUE);

#ifdef AMIGA
	wait_return(term_console ? -1 : TRUE);		/* see below */
#else
	wait_return(TRUE);				/* includes starttermcap() */
#endif

	/*
	 * In an Amiga window redrawing is caused by asking the window size.
	 * If we got an interrupt this will not work. The chance that the window
	 * size is wrong is very small, but we need to redraw the screen.
	 * Don't do this if ':' hit in wait_return().
	 * THIS IS UGLY but it save an extra redraw.
	 */
#ifdef AMIGA
	if (skip_redraw)				/* ':' hit in wait_return() */
		must_redraw = CLEAR;
	else if (term_console)
	{
		OUTSTR("\033[0 q"); 		/* get window size */
		if (got_int)
			must_redraw = CLEAR;	/* if got_int is TRUE we have to redraw */
		else
			must_redraw = 0;		/* no extra redraw needed */
	}
#endif /* AMIGA */
}

/*
 * dofilter: filter lines through a command given by the user
 *
 * We use temp files and the call_shell() routine here. This would normally
 * be done using pipes on a UNIX machine, but this is more portable to
 * the machines we usually run on. The call_shell() routine needs to be able
 * to deal with redirection somehow, and should handle things like looking
 * at the PATH env. variable, and adding reasonable extensions to the
 * command name given by the user. All reasonable versions of call_shell()
 * do this.
 * We use input redirection if do_in is TRUE.
 * We use output redirection if do_out is TRUE.
 */
	void
dofilter(line1, line2, buff, do_in, do_out)
	linenr_t	line1, line2;
	char_u		*buff;
	int			do_in, do_out;
{
#ifdef LATTICE
	char_u		itmp[L_tmpnam];		/* use tmpnam() */
	char_u		otmp[L_tmpnam];
#else
	char_u		itmp[TMPNAMELEN];
	char_u		otmp[TMPNAMELEN];
#endif
	linenr_t 	linecount;
	int			msg_save;

	/*
	 * Disallow shell commands from .exrc and .vimrc in current directory for
	 * security reasons.
	 */
	if (secure)
	{
		secure = 2;
		emsg(e_curdir);
		return;
	}
	if (*buff == NUL)		/* no filter command */
		return;
	linecount = line2 - line1 + 1;
	curwin->w_cursor.lnum = line1;
	curwin->w_cursor.col = 0;

	/*
	 * 1. Form temp file names
	 * 2. Write the lines to a temp file
	 * 3. Run the filter command on the temp file
	 * 4. Read the output of the command into the buffer
	 * 5. Delete the original lines to be filtered
	 * 6. Remove the temp files
	 */

#ifndef LATTICE
	/* for lattice we use tmpnam(), which will make its own name */
	STRCPY(itmp, TMPNAME1);
	STRCPY(otmp, TMPNAME2);
#endif

	if ((do_in && *mktemp((char *)itmp) == NUL) || (do_out && *mktemp((char *)otmp) == NUL))
	{
		emsg(e_notmp);
		return;
	}

/*
 * ! command will be overwritten by next mesages
 * This is a trade off between showing the command and not scrolling the
 * text one line up (problem on slow terminals).
 */
	must_redraw = CLEAR;		/* screen has been shifted up one line */
	msg_save = msg_scroll;
	msg_scroll = FALSE;
	++no_wait_return;			/* don't call wait_return() while busy */
	if (do_in && buf_write(curbuf, itmp, NULL, line1, line2, FALSE, 0, FALSE) == FAIL)
	{
		msg_scroll = msg_save;
		msg_outchar('\n');					/* keep message from writeit() */
		--no_wait_return;
		(void)emsg2(e_notcreate, itmp);		/* will call wait_return */
		return;
	}
	if (!do_out)
		msg_outchar('\n');

#if defined(UNIX) && !defined(ARCHIE)
/*
 * put braces around the command (for concatenated commands)
 */
 	sprintf((char *)IObuff, "(%s)", (char *)buff);
	if (do_in)
	{
		STRCAT(IObuff, " < ");
		STRCAT(IObuff, itmp);
	}
#else
/*
 * for shells that don't understand braces around commands, at least allow
 * the use of commands in a pipe.
 */
	STRCPY(IObuff, buff);
	if (do_in)
	{
		char_u		*p;
	/*
	 * If there is a pipe, we have to put the '<' in front of it
	 */
		p = STRCHR(IObuff, '|');
		if (p)
			*p = NUL;
		STRCAT(IObuff, " < ");
		STRCAT(IObuff, itmp);
		p = STRCHR(buff, '|');
		if (p)
			STRCAT(IObuff, p);
	}
#endif
	if (do_out)
		sprintf((char *)IObuff + STRLEN(IObuff), " %s %s", p_srr, otmp);

	windgoto((int)Rows - 1, 0);
	cursor_on();
	/*
	 * When call_shell() fails wait_return() is called to give the user a
	 * chance to read the error messages. Otherwise errors are ignored, so you
	 * can see the error messages from the command that appear on stdout; use
	 * 'u' to fix the text
	 */
	if (call_shell(IObuff, 1, FALSE) == FAIL)
		wait_return(FALSE);

	if (do_out)
	{
		if (u_save((linenr_t)(line2), (linenr_t)(line2 + 1)) == FAIL)
		{
			linecount = 0;
			goto error;
		}
		if (readfile(otmp, NULL, line2, FALSE, (linenr_t)0, MAXLNUM) == FAIL)
		{
			msg_outchar('\n');
			emsg2(e_notread, otmp);
			linecount = 0;
			goto error;
		}

		if (do_in)
		{
			curwin->w_cursor.lnum = line1;
			dellines(linecount, TRUE, TRUE);
		}
		--no_wait_return;
	}
	else
	{
error:
		--no_wait_return;
		wait_return(FALSE);
	}

	if (linecount > p_report)
	{
		if (!do_in && do_out)
			msgmore(linecount);
		else
		{
			sprintf((char *)msg_buf, "%ld lines filtered", (long)linecount);
			if (msg(msg_buf) && !msg_scroll)
				keep_msg = msg_buf;		/* display message after redraw */
		}
	}
	msg_scroll = msg_save;
	remove((char *)itmp);
	remove((char *)otmp);
}

#ifdef VIMINFO
/*
 * read_viminfo() -- Read the viminfo file.  Registers etc. which are already
 * set are not over-written unless force is TRUE. -- webb
 */
	int
read_viminfo(file, want_info, want_marks, force)
	char_u	*file;
	int		want_info;
	int		want_marks;
	int		force;
{
	FILE	*fp;

	file = viminfo_filename(file);			/* may set to default if NULL */
	if ((fp = fopen((char *)file, "r")) == NULL)
		return FAIL;

	do_viminfo(fp, NULL, want_info, want_marks, force);

	fclose(fp);

	return OK;
}

/*
 * write_viminfo() -- Write the viminfo file.  The old one is read in first so
 * that effectively a merge of current info and old info is done.  This allows
 * multiple vims to run simultaneously, without losing any marks etc.  If
 * force is TRUE, then the old file is not read in, and only internal info is
 * written to the file. -- webb
 */
	void
write_viminfo(file, force)
	char_u	*file;
	int		force;
{
	FILE	*fp_in = NULL;
	FILE	*fp_out = NULL;
	char_u	tmpname[TMPNAMELEN];

	STRCPY(tmpname, TMPNAME2);
	file = viminfo_filename(file);			/* may set to default if NULL */
	fp_in = fopen((char *)file, "r");
	if (fp_in == NULL)
		fp_out = fopen((char *)file, "w");
	else if (*mktemp((char *)tmpname) != NUL)
		fp_out = fopen((char *)tmpname, "w");
	if (fp_out == NULL)
	{
		EMSG("Can't write viminfo file!");
		if (fp_in != NULL)
			fclose(fp_in);
		return;
	}

	do_viminfo(fp_in, fp_out, !force, !force, FALSE);

	fclose(fp_out);			/* errors are ignored !? */
	if (fp_in != NULL)
	{
		fclose(fp_in);
		if (vim_rename(tmpname, file) == -1)
			unlink((char *)tmpname);
	}
}

	static char_u *
viminfo_filename(file)
	char_u		*file;
{
	if (file == NULL || *file == NUL)
	{
		expand_env((char_u *)VIMINFO_FILE, NameBuff, MAXPATHL);
		return NameBuff;
	}
	return file;
}

/*
 * do_viminfo() -- Should only be called from read_viminfo() & write_viminfo().
 */
	static void
do_viminfo(fp_in, fp_out, want_info, want_marks, force_read)
	FILE	*fp_in;
	FILE	*fp_out;
	int		want_info;
	int		want_marks;
	int		force_read;
{
	BUF		*buf;
	int		count = 0;
	int		eof = FALSE;
	int		load_marks;
	int		copy_marks_out;
	int		is_mark_set;
	char_u	line[LSIZE];
	linenr_t lnum = 0;
	char_u	*str;
	int		i;

	if (fp_in != NULL)
	{
		if (want_info)
			eof = read_viminfo_up_to_marks(line, &lnum, fp_in, force_read);
		else
			/* Skip info, find start of marks */
			while (!(eof = vim_fgets(line, LSIZE, fp_in, &lnum)) &&
															line[0] != '>')
				;
	}
	if (fp_out != NULL)
	{
		/* Write the info: */
		fprintf(fp_out, "# This viminfo file was generated by vim\n");
		fprintf(fp_out, "# You may edit it if you're careful!\n\n");
		write_viminfo_search_pattern(fp_out);
		write_viminfo_sub_string(fp_out);
		write_viminfo_history(fp_out);
		write_viminfo_registers(fp_out);
		fprintf(fp_out, "\n# History of marks within files (newest to oldest):\n");
		count = 0;
		for (buf = firstbuf; buf; buf = buf->b_next)
		{
			/* Check that at least one mark is set: */
			if (buf->b_startop.lnum != 0 || buf->b_endop.lnum != 0)
				is_mark_set = TRUE;
			else
			{
				is_mark_set = FALSE;
				for (i = 0; i < NMARKS; i++)
					if (buf->b_namedm[i].lnum != 0)
					{
						is_mark_set = TRUE;
						break;
					}
			}
			if (is_mark_set && buf->b_filename != NULL && buf->b_filename[0] != NUL)
			{
				fprintf(fp_out, "\n> %s\n", buf->b_filename);
				if (buf->b_startop.lnum != 0)
					fprintf(fp_out, "\t[\t%ld\t%d\n", buf->b_startop.lnum,
						buf->b_startop.col);
				if (buf->b_endop.lnum != 0)
					fprintf(fp_out, "\t]\t%ld\t%d\n", buf->b_endop.lnum,
						buf->b_endop.col);
				for (i = 0; i < NMARKS; i++)
					if (buf->b_namedm[i].lnum != 0)
						fprintf(fp_out, "\t%c\t%ld\t%d\n", 'a' + i,
							buf->b_namedm[i].lnum, buf->b_namedm[i].col);
				count++;
			}
		}
	}
	if (fp_in != NULL && want_marks)
	{
		while (!eof && (count < p_viminfo || fp_out == NULL))
		{
			if (line[0] != '>')
			{
				if (line[0] != '\n' && line[0] != '\r' && line[0] != '#')
				{
					sprintf((char *)IObuff, "viminfo, %ld: Illegal starting char '%c'",
						lnum, line[0]);
					emsg(IObuff);
				}
				eof = vim_fgets(line, LSIZE, fp_in, &lnum);
				continue;			/* Skip this dud line */
			}
			str = line + 1;
			skipwhite(&str);
			buf = buflist_findname(str);
			load_marks = copy_marks_out = FALSE;
			if (fp_out == NULL && buf == curbuf)
				load_marks = TRUE;
			else if (fp_out != NULL && buf == NULL)
			{
				copy_marks_out = TRUE;
				fputs("\n", fp_out);
				fputs((char *)line, fp_out);
				count++;
			}
			while (!(eof = vim_fgets(line, LSIZE, fp_in, &lnum)) && line[0] == TAB)
			{
				if (load_marks)
				{
					if (line[1] == '[')
						sscanf((char *)line + 2, "%ld %d",
							&buf->b_startop.lnum, &buf->b_startop.col);
					else if (line[1] == ']')
						sscanf((char *)line + 2, "%ld %d",
							&buf->b_endop.lnum, &buf->b_endop.col);
					else if ((i = line[1] - 'a') >= 0 && i < NMARKS)
						sscanf((char *)line + 2, "%ld %d",
							&buf->b_namedm[i].lnum, &buf->b_namedm[i].col);
				}
				else if (copy_marks_out)
					fputs((char *)line, fp_out);
			}
			if (load_marks)
				return;
		}
	}
}

/*
 * read_viminfo_up_to_marks() -- Only called from do_viminfo().  Reads in the
 * first part of the viminfo file which contains everything but the marks that
 * are local to a file.  Returns TRUE when end-of-file is reached. -- webb
 */
	static int
read_viminfo_up_to_marks(line, lnum, fp, force)
	char_u	*line;
	linenr_t *lnum;
	FILE	*fp;
	int		force;
{
	int		eof = FALSE;

	prepare_viminfo_history(force ? 9999 : 0);
	eof = vim_fgets(line, LSIZE, fp, lnum);
	while (!eof && line[0] != '>')
	{
		switch (line[0])
		{
			case NUL:
			case '\r':
			case '\n':
			case '#':		/* A comment */
				eof = vim_fgets(line, LSIZE, fp, lnum);
				break;
			case '"':
				eof = read_viminfo_register(line, lnum, fp, force);
				break;
			case '/':		/* Search string */
			case '&':		/* Substitute search string */
			case '~':		/* Last search string, followed by '/' or '&' */
				eof = read_viminfo_search_pattern(line, lnum, fp, force);
				break;
			case '$':
				eof = read_viminfo_sub_string(line, lnum, fp, force);
				break;
			case ':':
			case '?':
				eof = read_viminfo_history(line, lnum, fp, force);
				break;
#if 0
			case '\'':
				/* How do we have a file mark when the file is not in the
				 * buffer list?
				 */
				eof = read_viminfo_filemark(line, lnum, fp, force);
				break;
			case '+':
				/* eg: "+40 /path/dir file", for running vim with no args */
				eof = vim_fgets(line, LSIZE, fp, lnum);
				break;
#endif
			default:
				sprintf((char *)IObuff, "viminfo, %ld: Illegal starting char '%c'",
					*lnum, line[0]);
				emsg(IObuff);
				eof = vim_fgets(line, LSIZE, fp, lnum);
				break;
		}
	}
	finish_viminfo_history();
	return eof;
}

/*
 * check string read from viminfo file
 * remove '\n' at the end of the line
 * - replace CTRL-V CTRL-V by CTRL-V
 * - replace CTRL-V 'n'    by '\n'
 */
	void
viminfo_readstring(p)
	char_u		*p;
{
	while (*p != NUL && *p != '\n')
	{
		if (*p == Ctrl('V'))
		{
			if (p[1] == 'n')
				p[0] = '\n';
			memmove(p + 1, p + 2, STRLEN(p));
		}
		++p;
	}
	*p = NUL;
}

/*
 * write string to viminfo file
 * - replace CTRL-V by CTRL-V CTRL-V
 * - replace '\n'   by CTRL-V 'n'
 * - add a '\n' at the end
 */
	void
viminfo_writestring(fd, p)
	FILE	*fd;
	char_u	*p;
{
	register int	c;

	while ((c = *p++) != NUL)
	{
		if (c == Ctrl('V') || c == '\n')
		{
			putc(Ctrl('V'), fd);
			if (c == '\n')
				c = 'n';
		}
		putc(c, fd);
	}
	putc('\n', fd);
}
#endif /* VIMINFO */
