/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * Code to handle tags and the tag stack
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"

static int findtag __ARGS((char_u *));
static char_u *bottommsg = (char_u *)"at bottom of tag stack";
static char_u *topmsg = (char_u *)"at top of tag stack";

/*
 * Jump to tag; handling of tag stack
 *
 * *tag != NUL (:tag): jump to new tag, add to tag stack
 * type == 1 (:pop) || type == 2 (CTRL-T): jump to old position
 * type == 0 (:tag): jump to old tag
 */
	void
dotag(tag, type, count)
	char_u	*tag;
	int		type;
	int		count;
{
	int 			i;
	struct taggy	*tagstack = curwin->w_tagstack;
	int				tagstackidx = curwin->w_tagstackidx;
	int				tagstacklen = curwin->w_tagstacklen;
	int				oldtagstackidx = tagstackidx;

	if (*tag != NUL)						/* new pattern, add to the stack */
	{
		/*
		 * if last used entry is not at the top, delete all tag stack entries
		 * above it.
		 */
		while (tagstackidx < tagstacklen)
			free(tagstack[--tagstacklen].tagname);

				/* if tagstack is full: remove oldest entry */
		if (++tagstacklen > TAGSTACKSIZE)
		{
			tagstacklen = TAGSTACKSIZE;
			free(tagstack[0].tagname);
			for (i = 1; i < tagstacklen; ++i)
				tagstack[i - 1] = tagstack[i];
			--tagstackidx;
			--oldtagstackidx;
		}
	/*
	 * put the tag name in the tag stack
	 * the position is added below
	 */
		tagstack[tagstackidx].tagname = strsave(tag);
	}
	else if (tagstacklen == 0)					/* empty stack */
	{
		EMSG("tag stack empty");
		goto end_dotag;
	}
	else if (type)								/* go to older position */
	{
		if ((tagstackidx -= count) < 0)
		{
			emsg(bottommsg);
			if (tagstackidx + count == 0)
			{
				/* We did ^T (or <num>^T) from the bottom of the stack */
				tagstackidx = 0;
				goto end_dotag;
			}
			/* We weren't at the bottom of the stack, so jump all the way to
			 * the bottom.
			 */
			tagstackidx = 0;
		}
		else if (tagstackidx >= tagstacklen)	/* must have been count == 0 */
		{
			emsg(topmsg);
			goto end_dotag;
		}
		if (tagstack[tagstackidx].fmark.fnum != curbuf->b_fnum)
		{
			/*
			 * Jump to other file. If this fails (e.g. because the file was
			 * changed) keep original position in tag stack.
			 */
			if (buflist_getfile(tagstack[tagstackidx].fmark.fnum,
						tagstack[tagstackidx].fmark.mark.lnum, TRUE) == FAIL)
			{
				tagstackidx = oldtagstackidx;	/* back to old position */
				goto end_dotag;
			}
		}
		else
			curwin->w_cursor.lnum = tagstack[tagstackidx].fmark.mark.lnum;
		curwin->w_cursor.col = tagstack[tagstackidx].fmark.mark.col;
		curwin->w_set_curswant = TRUE;
		goto end_dotag;
	}
	else									/* go to newer pattern */
	{
		if ((tagstackidx += count - 1) >= tagstacklen)
		{
			/*
			 * beyond the last one, just give an error message and go to the
			 * last one
			 */
			tagstackidx = tagstacklen - 1;
			emsg(topmsg);
		}
		else if (tagstackidx < 0)			/* must have been count == 0 */
		{
			emsg(bottommsg);
			tagstackidx = 0;
			goto end_dotag;
		}
	}
	/*
	 * For :tag [arg], remember position before the jump
	 */
	if (type == 0)
	{
		tagstack[tagstackidx].fmark.mark = curwin->w_cursor;
		tagstack[tagstackidx].fmark.fnum = curbuf->b_fnum;
	}
	/* curwin will change in the call to findtag() if ^W^] was used -- webb */
	curwin->w_tagstackidx = tagstackidx;
	curwin->w_tagstacklen = tagstacklen;
	if (findtag(tagstack[tagstackidx].tagname) > 0)
		++tagstackidx;

end_dotag:
	curwin->w_tagstackidx = tagstackidx;
	curwin->w_tagstacklen = tagstacklen;
}

/*
 * Print the tag stack
 */
	void
dotags()
{
	int				i;
	char_u			*name;
	struct taggy	*tagstack = curwin->w_tagstack;
	int				tagstackidx = curwin->w_tagstackidx;
	int				tagstacklen = curwin->w_tagstacklen;

	msg_outstr((char_u *)"\n  # TO tag      FROM line in file");
	for (i = 0; i < tagstacklen; ++i)
	{
		if (tagstack[i].tagname != NULL)
		{
			name = fm_getname(&(tagstack[i].fmark));
			if (name == NULL)		/* file name not available */
				continue;

			sprintf((char *)IObuff, "\n%c%2d %-15s %4ld  %s",
				i == tagstackidx ? '>' : ' ',
				i + 1,
				tagstack[i].tagname,
				tagstack[i].fmark.mark.lnum,
				name);
			msg_outstr(IObuff);
		}
		flushbuf();					/* show one line at a time */
	}
	if (tagstackidx == tagstacklen)		/* idx at top of stack */
		msg_outstr((char_u *)"\n>");
}

/*
 * findtag(tag) - goto tag
 *   return 0 for failure, 1 for success
 */
	static int
findtag(tag)
	char_u		   *tag;
{
	FILE	   *tp;
	char_u		lbuf[LSIZE];
	char_u		pbuf[LSIZE];			/* search pattern buffer */
	char_u	   *fname, *str;
	char_u	   *tagname;
	char_u	   *tail;
	int			cmplen;
	char_u		*m = (char_u *)"No tags file";
	char_u		*marg = NULL;
	register char_u	*p;
	char_u		*p2;
	char_u		*np;					/* pointer into file name string */
	char_u		sbuf[CMDBUFFSIZE + 1];	/* tag file name */
	int			i;
	int			save_secure;
	int			save_p_ws;
	int			tried_local = FALSE;	/* tried tags file local to file */

	if (tag == NULL)		/* out of memory condition */
		return 0;

	tail = (curbuf->b_filename == NULL) ? NULL : gettail(curbuf->b_filename);

	if ((cmplen = p_tl) == 0)
		cmplen = 999;

	/* get stack of tag file names from tags option */
	for (np = p_tags; tried_local == FALSE; )
	{
		/*
		 * after trying all the names in the 'tags' option, try the 'tags' file
		 * in the same directory as the current file (unless it is the same as "./tags").
		 */
		if (*np == NUL)
		{
			if (curbuf->b_filename == NULL)				/* no file name */
				break;
			np = gettail(curbuf->b_filename);
			i = np - curbuf->b_filename;
			if (i + 5 >= CMDBUFFSIZE)
				break;
			STRNCPY(sbuf, curbuf->b_filename, i);

			STRCPY(sbuf + i, "tags");
			if (FullName((char_u *)"tags", lbuf, LSIZE) == OK &&
												STRCMP(sbuf, lbuf) == 0)
				break;									/* same as ./tags */
			tried_local = TRUE;
		}
		else
		{
			for (i = 0; i < CMDBUFFSIZE && *np; ++i)	/* copy next file name into sbuf */
			{
				if (*np == ' ')
				{
					++np;
					break;
				}
				sbuf[i] = *np++;
			}
			sbuf[i] = 0;
		}
		if ((tp = fopen((char *)sbuf, "r")) == NULL)
			continue;
		reg_ic = p_ic;										/* for cstrncmp() */
		while (fgets((char *)lbuf, LSIZE, tp) != NULL)
		{
			m = (char_u *)"Format error in tags file %s";	/* default error message */
			marg = sbuf;

			/* Find start of file name, after first white space */
			fname = tagname = lbuf;
			while (*fname != ' ' && *fname != '\t' && *fname != NUL)
			{
				/*
				 * Static tags produced by elvis's ctags program have the
				 * format: 'file:tag  file  /pattern$'. jw.
				 */
				if (*fname == ':')
				{
					/*
					 * Static tag must be in current file
					 * If it is not, set tagname to NULL
					 */
					*fname = '\0';
					if (tail != NULL && STRCMP(tail, gettail(lbuf)) == 0)
						tagname = fname + 1;
					else
						tagname = NULL;
				}
				++fname;
			}
			if (*fname == NUL)
				goto erret;
			*fname++ = '\0';

			if (tagname != NULL && cstrncmp(tagname, tag, cmplen) == 0)
											/* Tag found */
			{
				fclose(tp);
				skipwhite(&fname);

			/* find start of search command, after second white space */
				str = fname;
				skiptowhite(&str);
				if (*str == NUL)
					goto erret;
				*str++ = '\0';
				skipwhite(&str);

				/*
				 * If the command is a string like "/^function fname"
				 * scan through the search string. If we see a magic
				 * char, we have to quote it. This lets us use "real"
				 * implementations of ctags.
				 */
				if (*str == '/' || *str == '?')
				{
					p = pbuf;
					*p++ = *str++;			/* copy the '/' or '?' */
					if (*str == '^')
						*p++ = *str++;			/* copy the '^' */

					while (*str)
					{
						switch (*str)
						{
						case '\\':	if (str[1] == '(')	/* remove '\' before '(' */
										++str;
									else
										*p++ = *str++;
									break;

						case '\r':
						case '\n':	*str = pbuf[0];	/* copy '/' or '?' */
									str[1] = NUL;	/* delete NL after CR */
									break;

									/*
									 * if string ends in search character: skip it
									 * else escape it with '\'
									 */
						case '/':
						case '?':	if (*str != pbuf[0])	/* not the search char */
										break;
															/* last char */
									if (str[1] == '\n' || str[1] == '\r')
									{
										++str;
										continue;
									}
						case '[':
									if (!p_magic)
										break;
						case '^':
						case '*':
						case '.':	*p++ = '\\';
									break;
						}
						*p++ = *str++;
					}
				}
				else		/* not a search command, just copy it */
					for (p = pbuf; *str && *str != '\n'; )
						*p++ = *str++;
				*p = NUL;

				/*
				 * expand filename (for environment variables)
				 */
				if ((p = ExpandOne((char_u *)fname, 1, -1)) != NULL)
					fname = p;
				/*
				 * if 'tagrelative' option set, may change file name
				 */
				if (p_tr && !isFullName(fname) && (p2 = gettail(sbuf)) != sbuf)
				{
					STRNCPY(p2, fname, CMDBUFFSIZE - (p2 - sbuf));
					fname = sbuf;
				}
				/*
				 * check if file for tag exists before abandoning current file
				 */
				if (getperm(fname) < 0)
				{
					m = (char_u *)"File \"%s\" does not exist";
					marg = fname;
					goto erret;
				}

				++RedrawingDisabled;
				/*
				 * if it was a CTRL-W CTRL-] command split window now
				 */
				if (postponed_split)
					win_split(0L, FALSE);
				i = getfile(0, fname, NULL, TRUE, (linenr_t)0);
				if (p)
					free(p);
				if (i <= 0)
				{
					curwin->w_set_curswant = TRUE;
					postponed_split = FALSE;

					--RedrawingDisabled;
					save_secure = secure;
					secure = 1;
					tag_busy = TRUE;	/* don't set marks for this search */
					keep_old_search_pattern = TRUE;

					/*
					 * if the command is a search, try here
					 *
					 * Rather than starting at line one, just turn wrap-scan
					 * on temporarily, this ensures that tags on line 1 will
					 * be found, and makes sure our guess searches search the
					 * whole file when repeated -- webb.
					 */
					if (pbuf[0] == '/' || pbuf[0] == '?')
					{
						save_p_ws = p_ws;
						p_ws = TRUE;	/* Switch wrap-scan on temporarily */
						if (!dosearch(pbuf[0], pbuf + 1,
												FALSE, (long)1, FALSE, FALSE))
						{
							register int notfound = FALSE;

							/*
							 * Failed to find pattern, take a guess:
							 */
							sprintf((char *)pbuf, "^%s(", tagname);
							if (!dosearch('/', pbuf, FALSE,
												(long)1, FALSE, FALSE))
							{
								/* Guess again: */
								sprintf((char *)pbuf,
												"^[#a-zA-Z_].*%s(", tagname);
								if (!dosearch('/', pbuf,
												FALSE, (long)1, FALSE, FALSE))
									notfound = TRUE;
							}
							if (notfound)
								EMSG("Can't find tag pattern");
							else
							{
								MSG("Couldn't find tag, just guessing!");
								sleep(1);
							}
						}
						p_ws = save_p_ws;
					}
					else
					{							/* start command in line 1 */
						curwin->w_cursor.lnum = 1;
						docmdline(pbuf, TRUE, TRUE);
					}

					tag_busy = FALSE;
					keep_old_search_pattern = FALSE;
									/* done something that is not allowed */
					if (secure == 2)
						wait_return(TRUE);
					secure = save_secure;

						/* print the file message after redraw if jumped to
						 * another file */
					if (i == -1)
						need_fileinfo = TRUE;
					return 1;
				}
				--RedrawingDisabled;
				if (postponed_split)			/* close the window */
				{
					close_window(curwin, FALSE);
					postponed_split = FALSE;
				}
				return 0;
			}
		}
		m = NULL;

erret:
		fclose(tp);
		if (m)
			emsg2(m, marg);
	}
	if (m == NULL)
		EMSG("tag not found");
	else if (marg == NULL)
		emsg(m);
	return 0;
}

	int
ExpandTags(prog, num_file, file)
	regexp *prog;
	int *num_file;
	char_u ***file;
{
	char_u	**matches, **new_matches;
	char_u	tag_file[CMDBUFFSIZE + 1];
	char_u	line[LSIZE];
	char_u	*tagname;
	char_u	*tail;			/* Tail of current file name */
	char_u	*np;
	char_u	*p;
	int		limit = 100;
	int		idx;
	int		i;
	linenr_t lnum;
	FILE	*fp;

	matches = (char_u **) alloc((unsigned)(limit * sizeof(char_u *)));
	if (matches == NULL)
		return FAIL;
	tail = (curbuf->b_filename == NULL) ? NULL : gettail(curbuf->b_filename);
	idx = 0;
	for (np = p_tags; *np; )
	{
		for (i = 0; i < CMDBUFFSIZE && *np && *np != ' '; i++)
			tag_file[i] = *np++;
		tag_file[i] = NUL;
		skipwhite(&np);
		if ((fp = fopen((char *)tag_file, "r")) == NULL)
			continue;
		lnum = 0;
		while (!vim_fgets(line, LSIZE, fp, &lnum))
		{
			p = tagname = line;
			while (*p != ' ' && *p != '\t' && *p != NUL)
			{
				/*
				 * Static tags produced by elvis's ctags program have the
				 * format: 'file:tag  file  /pattern$'. jw.
				 */
				if (*p == ':')
				{
					/* Static tags must be in current file */
					*p = NUL;
					if (tail != NULL && STRCMP(tail, gettail(line)) == 0)
						tagname = p + 1;
					else
						tagname = NULL;
				}
				++p;
			}
			*p = NUL;

			if (tagname != NULL && regexec(prog, tagname, TRUE))
			{
				if (idx == limit)
				{
					limit *= 2;
					new_matches = (char_u **) alloc((unsigned)(limit * sizeof(char_u *)));
					if (new_matches == NULL)
					{
						/* We'll miss some matches, oh well */
						*file = matches;
						*num_file = idx;
						return OK;
					}
					for (i = 0; i < idx; i++)
						new_matches[i] = matches[i];
					free(matches);
					matches = new_matches;
				}
				matches[idx++] = strsave(tagname);
			}
		}
	}
	if (idx > 0)
	{
		new_matches = *file = (char_u **) alloc((unsigned)(idx * sizeof(char_u *)));
		if (new_matches == NULL)
		{
			*file = matches;
			*num_file = idx;
			return OK;
		}
		for (i = 0; i < idx; i++)
			new_matches[i] = matches[i];
	}
	free(matches);
	*num_file = idx;
	return OK;
}
