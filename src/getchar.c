/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * getchar.c
 *
 * functions related with getting a character from the user/mapping/redo/...
 *
 * manipulations with redo buffer and stuff buffer
 * mappings and abbreviations
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"

/*
 * structure used to store one block of the stuff/redo/macro buffers
 */
struct bufblock
{
		struct bufblock *b_next;		/* pointer to next bufblock */
		char_u			b_str[1];		/* contents (actually longer) */
};

#define MINIMAL_SIZE 20 				/* minimal size for b_str */

/*
 * header used for the stuff buffer and the redo buffer
 */
struct buffheader
{
		struct bufblock bh_first;		/* first (dummy) block of list */
		struct bufblock *bh_curr;		/* bufblock for appending */
		int 			bh_index;		/* index for reading */
		int 			bh_space;		/* space in bh_curr for appending */
};

static struct buffheader stuffbuff = {{NULL, {NUL}}, NULL, 0, 0};
static struct buffheader redobuff = {{NULL, {NUL}}, NULL, 0, 0};
static struct buffheader old_redobuff = {{NULL, {NUL}}, NULL, 0, 0};
static struct buffheader recordbuff = {{NULL, {NUL}}, NULL, 0, 0};

	/*
	 * when block_redo is TRUE redo buffer will not be changed
	 * used by edit() to repeat insertions and 'V' command for redoing
	 */
static int		block_redo = FALSE;

/*
 * structure used for mapping
 */
struct mapblock
{
	struct mapblock *m_next;		/* next mapblock */
	char_u			*m_keys;		/* mapped from */
	int				 m_keylen;		/* strlen(m_keys) */
	char_u			*m_str; 		/* mapped to */
	int 			 m_mode;		/* valid mode */
	int				 m_noremap;		/* if non-zero no re-mapping for m_str */
};

static struct mapblock maplist = {NULL, NULL, 0, NULL, 0, 0};
									/* first dummy entry in maplist */

/*
 * variables used by vgetorpeek() and flush_buffers()
 *
 * typebuf[] contains all characters that are not consumed yet.
 * typebuf[typeoff] is the first valid character in typebuf[].
 * typebuf[typeoff + typelen - 1] is the last valid char.
 * The part in front may contain the result of mappings, abbreviations and
 * @a commands. The lenght of this part is typemaplen.
 * After it are characters that come from the terminal.
 * no_abbr_cnt is the number of characters in typebuf that should not be
 * considered for abbreviations.
 * Some parts of typebuf may not be mapped. These parts are remembered in
 * noremapbuf, which is the same length as typebuf and contains TRUE for the
 * characters that are not to be remapped. noremapbuf[typeoff] is the first
 * valid flag.
 * (typebuf has been put in globals.h, because check_termcode() needs it).
 */
static char_u	*noremapbuf = NULL;       /* flags for typeahead characters */
#define TYPELEN_INIT	(3 * (MAXMAPLEN + 3))
static char_u	typebuf_init[TYPELEN_INIT];			/* initial typebuf */
static char_u	noremapbuf_init[TYPELEN_INIT];		/* initial noremapbuf */
static int		real_State;							/* State for gotchars() */

static int		typemaplen = 0;		/* nr of mapped characters in typebuf */
static int		no_abbr_cnt = 0;	/* nr of chars without abbrev. in typebuf */

static void		free_buff __ARGS((struct buffheader *));
static char_u	*get_bufcont __ARGS((struct buffheader *, int));
static void		add_buff __ARGS((struct buffheader *, char_u *));
static void		add_num_buff __ARGS((struct buffheader *, long));
static void		add_char_buff __ARGS((struct buffheader *, int));
static int		read_stuff __ARGS((int));
static void		start_stuff __ARGS((void));
static int		read_redo __ARGS((int, int));
static void		copy_redo __ARGS((int));
static void		init_typebuf __ARGS((void));
static void		gotchars __ARGS((char_u *, int));
static int		vgetorpeek __ARGS((int));
static void		showmap __ARGS((struct mapblock *));

/*
 * free and clear a buffer
 */
	static void
free_buff(buf)
	struct buffheader *buf;
{
		register struct bufblock *p, *np;

		for (p = buf->bh_first.b_next; p != NULL; p = np)
		{
				np = p->b_next;
				free(p);
		}
		buf->bh_first.b_next = NULL;
}

/*
 * return the contents of a buffer as a single string
 */
	static char_u *
get_bufcont(buffer, dozero)
	struct buffheader	*buffer;
	int					dozero;		/* count == zero is not an error */
{
	long_u			count = 0;
	char_u			*p = NULL;
	char_u			*p2;
	char_u			*str;
	struct bufblock	*bp;

/* compute the total length of the string */
	for (bp = buffer->bh_first.b_next; bp != NULL; bp = bp->b_next)
		count += STRLEN(bp->b_str);

	if ((count || dozero) && (p = lalloc(count + 1, TRUE)) != NULL)
	{
		p2 = p;
		for (bp = buffer->bh_first.b_next; bp != NULL; bp = bp->b_next)
			for (str = bp->b_str; *str; )
				*p2++ = *str++;
		*p2 = NUL;
	}
	return (p);
}

/*
 * return the contents of the record buffer as a single string
 *	and clear the record buffer
 */
	char_u *
get_recorded()
{
	char_u *p;

	p = get_bufcont(&recordbuff, TRUE);
	free_buff(&recordbuff);
	return (p);
}

/*
 * return the contents of the redo buffer as a single string
 */
	char_u *
get_inserted()
{
		return(get_bufcont(&redobuff, FALSE));
}

/*
 * add string "s" after the current block of buffer "buf"
 */
	static void
add_buff(buf, s)
	register struct buffheader	*buf;
	char_u						*s;
{
	struct bufblock *p;
	long_u 			n;
	long_u 			len;

	if ((n = STRLEN(s)) == 0)				/* don't add empty strings */
		return;

	if (buf->bh_first.b_next == NULL)		/* first add to list */
	{
		buf->bh_space = 0;
		buf->bh_curr = &(buf->bh_first);
	}
	else if (buf->bh_curr == NULL)			/* buffer has already been read */
	{
		EMSG("Add to read buffer");
		return;
	}
	else if (buf->bh_index != 0)
		STRCPY(buf->bh_first.b_next->b_str, buf->bh_first.b_next->b_str + buf->bh_index);
	buf->bh_index = 0;

	if (buf->bh_space >= n)
	{
		strcat((char *)buf->bh_curr->b_str, (char *)s);
		buf->bh_space -= n;
	}
	else
	{
		if (n < MINIMAL_SIZE)
			len = MINIMAL_SIZE;
		else
			len = n;
		p = (struct bufblock *)lalloc((long_u)(sizeof(struct bufblock) + len), TRUE);
		if (p == NULL)
			return; /* no space, just forget it */
		buf->bh_space = len - n;
		STRCPY(p->b_str, s);

		p->b_next = buf->bh_curr->b_next;
		buf->bh_curr->b_next = p;
		buf->bh_curr = p;
	}
	return;
}

	static void
add_num_buff(buf, n)
	struct buffheader *buf;
	long 			  n;
{
		char_u	number[32];

		sprintf((char *)number, "%ld", n);
		add_buff(buf, number);
}

	static void
add_char_buff(buf, c)
	struct buffheader *buf;
	int 			  c;
{
	char_u	temp[3];

	/*
	 * translate special key code into two byte sequence
	 */
	if (c >= 0x100)
	{
		temp[0] = K_SPECIAL;
		temp[1] = K_SECOND(c);
		temp[2] = NUL;
	}
	else
	{
		temp[0] = c;
		temp[1] = NUL;
	}
	add_buff(buf, temp);
}

/*
 * get one character from the stuff buffer
 * If advance == TRUE go to the next char.
 */
	static int
read_stuff(advance)
	int			advance;
{
	register char_u c;
	register struct bufblock *curr;


	if (stuffbuff.bh_first.b_next == NULL)	/* buffer is empty */
		return NUL;

	curr = stuffbuff.bh_first.b_next;
	c = curr->b_str[stuffbuff.bh_index];

	if (advance)
	{
		if (curr->b_str[++stuffbuff.bh_index] == NUL)
		{
			stuffbuff.bh_first.b_next = curr->b_next;
			free(curr);
			stuffbuff.bh_index = 0;
		}
	}
	return c;
}

/*
 * prepare stuff buffer for reading (if it contains something)
 */
	static void
start_stuff()
{
	if (stuffbuff.bh_first.b_next != NULL)
	{
		stuffbuff.bh_curr = &(stuffbuff.bh_first);
		stuffbuff.bh_space = 0;
	}
}

/*
 * check if the stuff buffer is empty
 */
	int
stuff_empty()
{
	return (stuffbuff.bh_first.b_next == NULL);
}

/*
 * Remove the contents of the stuff buffer and the mapped characters in the
 * typeahead buffer (used in case of an error). If 'typeahead' is true,
 * flush all typeahead characters (used when interrupted by a CTRL-C).
 */
	void
flush_buffers(typeahead)
	int typeahead;
{
	init_typebuf();

	start_stuff();
	while (read_stuff(TRUE) != NUL)
		;

	if (typeahead)			/* remove all typeahead */
	{
			/*
			 * We have to get all characters, because we may delete the first
			 * part of an escape sequence.
			 * In an xterm we get one char at a time and we have to get them
			 * all.
			 */
		while (inchar(typebuf, MAXMAPLEN, 10))	
			;
		typeoff = MAXMAPLEN;
		typelen = 0;
	}
	else					/* remove mapped characters only */
	{
		typeoff += typemaplen;
		typelen -= typemaplen;
	}
	typemaplen = 0;
	no_abbr_cnt = 0;
}

/*
 * The previous contents of the redo buffer is kept in old_redobuffer.
 * This is used for the CTRL-O <.> command in insert mode.
 */
	void
ResetRedobuff()
{
	if (!block_redo)
	{
		free_buff(&old_redobuff);
		old_redobuff = redobuff;
		redobuff.bh_first.b_next = NULL;
	}
}

	void
AppendToRedobuff(s)
	char_u		   *s;
{
	if (!block_redo)
		add_buff(&redobuff, s);
}

	void
AppendCharToRedobuff(c)
	int			   c;
{
	if (!block_redo)
		add_char_buff(&redobuff, c);
}

	void
AppendNumberToRedobuff(n)
	long 			n;
{
	if (!block_redo)
		add_num_buff(&redobuff, n);
}

	void
stuffReadbuff(s)
	char_u		   *s;
{
	add_buff(&stuffbuff, s);
}

	void
stuffcharReadbuff(c)
	int			   c;
{
	add_char_buff(&stuffbuff, c);
}

	void
stuffnumReadbuff(n)
	long	n;
{
	add_num_buff(&stuffbuff, n);
}

/*
 * Read a character from the redo buffer.
 * The redo buffer is left as it is.
 * if init is TRUE, prepare for redo, return FAIL if nothing to redo, OK otherwise
 * if old is TRUE, use old_redobuff instead of redobuff
 */
	static int
read_redo(init, old)
	int			init;
	int			old;
{
	static struct bufblock	*bp;
	static char_u			*p;
	int						c;

	if (init)
	{
		if (old)
			bp = old_redobuff.bh_first.b_next;
		else
			bp = redobuff.bh_first.b_next;
		if (bp == NULL)
			return FAIL;
		p = bp->b_str;
		return OK;
	}
	if ((c = *p) != NUL)
	{
		if (*++p == NUL && bp->b_next != NULL)
		{
			bp = bp->b_next;
			p = bp->b_str;
		}
	}
	return c;
}

/*
 * copy the rest of the redo buffer into the stuff buffer (could be done faster)
 * if old is TRUE, use old_redobuff instead of redobuff
 */
	static void
copy_redo(old)
	int		old;
{
	register int c;

	while ((c = read_redo(FALSE, old)) != NUL)
		stuffcharReadbuff(c);
}

/*
 * Stuff the redo buffer into the stuffbuff.
 * Insert the redo count into the command.
 * If 'old' is TRUE, the last but one command is repeated
 * instead of the last command (inserting text). This is used for
 * CTRL-O <.> in insert mode
 *
 * return FAIL for failure, OK otherwise
 */
	int
start_redo(count, old)
	long	count;
	int		old;
{
	register int c;

	if (read_redo(TRUE, old) == FAIL)	/* init the pointers; return if nothing to redo */
		return FAIL;

	c = read_redo(FALSE, old);

/* copy the buffer name, if present */
	if (c == '"')
	{
		add_buff(&stuffbuff, (char_u *)"\"");
		c = read_redo(FALSE, old);

/* if a numbered buffer is used, increment the number */
		if (c >= '1' && c < '9')
			++c;
		add_char_buff(&stuffbuff, c);
		c = read_redo(FALSE, old);
	}

	if (c == 'v')	/* redo Visual */
	{
		VIsual = curwin->w_cursor;
		VIsual_active = TRUE;
		redo_Visual_busy = TRUE;
		c = read_redo(FALSE, old);
	}

/* try to enter the count (in place of a previous count) */
	if (count)
	{
		while (isdigit(c))		/* skip "old" count */
			c = read_redo(FALSE, old);
		add_num_buff(&stuffbuff, count);
	}

/* copy from the redo buffer into the stuff buffer */
	add_char_buff(&stuffbuff, c);
	copy_redo(old);
	return OK;
}

/*
 * Repeat the last insert (R, o, O, a, A, i or I command) by stuffing
 * the redo buffer into the stuffbuff.
 * return FAIL for failure, OK otherwise
 */
	int
start_redo_ins()
{
	register int c;

	if (read_redo(TRUE, FALSE) == FAIL)
		return FAIL;
	start_stuff();

/* skip the count and the command character */
	while ((c = read_redo(FALSE, FALSE)) != NUL)
	{
		c = TO_UPPER(c);
		if (strchr("AIRO", c) != NULL)
		{
			if (c == 'O')
				stuffReadbuff(NL_STR);
			break;
		}
	}

/* copy the typed text from the redo buffer into the stuff buffer */
	copy_redo(FALSE);
	block_redo = TRUE;
	return OK;
}

	void
set_redo_ins()
{
	block_redo = TRUE;
}

	void
stop_redo_ins()
{
	block_redo = FALSE;
}

/*
 * Initialize typebuf to point to typebuf_init.
 * Alloc() cannot be used here: In out-of-memory situations it would
 * be impossible to type anything.
 */
	static void
init_typebuf()
{
	if (typebuf == NULL)
	{
		typebuf = typebuf_init;
		noremapbuf = noremapbuf_init;
		typebuflen = TYPELEN_INIT;
		typelen = 0;
		typeoff = 0;
	}
}

/*
 * insert a string in position 'offset' in the typeahead buffer (for "@r"
 * command, vgetorpeek() and check_termcode())
 *
 * If noremap is 0, new string can be mapped again.
 * If noremap is -1, new string cannot be mapped again.
 * If noremap is >0, that many characters of the new string cannot be mapped.
 *
 * If nottyped is TRUE, the string does not return KeyTyped (don't use when
 * offset is non-zero!).
 *
 * return FAIL for failure, OK otherwise
 */
	int
ins_typebuf(str, noremap, offset, nottyped)
	char_u	*str;
	int		noremap;
	int		offset;
	int		nottyped;
{
	register char_u	*s1, *s2;
	register int	newlen;
	register int	addlen;
	register int	i;
	register int	newoff;

	init_typebuf();

	addlen = STRLEN(str);
	/*
	 * Easy case: there is room in front of typebuf[typeoff]
	 */
	if (offset == 0 && addlen <= typeoff)
	{
		typeoff -= addlen;
		memmove(typebuf + typeoff, str, addlen);
	}
	/*
	 * Need to allocate new buffer.
	 * In typebuf there must always be room for MAXMAPLEN + 3 characters.
	 * We add some extra room to avoid having to allocate too often.
	 */
	else
	{
		newoff = MAXMAPLEN + 3;
		newlen = typelen + addlen + newoff + 2 * (MAXMAPLEN + 3);
		if (newlen < 0)				/* string is getting too long */
		{
			emsg(e_toocompl);		/* also calls flush_buffers */
			setcursor();
			return FAIL;
		}
		s1 = alloc(newlen);
		if (s1 == NULL)				/* out of memory */
			return FAIL;
		s2 = alloc(newlen);
		if (s2 == NULL)				/* out of memory */
		{
			free(s1);
			return FAIL;
		}
		typebuflen = newlen;

		memmove(s1 + newoff, typebuf + typeoff, offset);
		memmove(s1 + newoff + offset, str, addlen);
		memmove(s1 + newoff + offset + addlen, typebuf + typeoff + offset,
														typelen - offset);
		if (typebuf != typebuf_init)
			free(typebuf);
		typebuf = s1;

		memmove(s2 + newoff, noremapbuf + typeoff, offset);
		memmove(s2 + newoff + offset + addlen, noremapbuf + typeoff + offset,
														typelen - offset);
		if (noremapbuf != noremapbuf_init)
			free(noremapbuf);
		noremapbuf = s2;

		typeoff = newoff;
	}
	typelen += addlen;

	/*
	 * Adjust noremapbuf[] for the new characters:
	 * If noremap  < 0: all the new characters are flagged not remappable
	 * If noremap == 0: all the new characters are flagged mappable
	 * If noremap  > 0: 'noremap' characters are flagged not remappable, the
	 *					rest mappable
	 */
	if (noremap < 0)		/* length not specified */
		noremap = addlen;
	for (i = 0; i < addlen; ++i)
		noremapbuf[typeoff + i + offset] = (noremap-- > 0);

					/* this is only correct for offset == 0! */
	if (nottyped)						/* the inserted string is not typed */
		typemaplen += addlen;
	if (no_abbr_cnt && offset == 0)		/* and not used for abbreviations */
		no_abbr_cnt += addlen;

	return OK;
}

/*
 * remove "len" characters from typebuf[typeoff + offset]
 */
	void
del_typebuf(len, offset)
	int	len;
	int	offset;
{
	int		i;

	typelen -= len;
	/*
	 * Easy case: Just increase typeoff.
	 */
	if (offset == 0 && typebuflen - (typeoff + len) >= MAXMAPLEN + 3)
		typeoff += len;
	/*
	 * Have to move the characters in typebuf[] and noremapbuf[]
	 */
	else
	{
		i = typeoff + offset;
		/*
		 * Leave some extra room at the end to avoid reallocation.
		 */
		if (typeoff > MAXMAPLEN)
		{
			memmove(typebuf + MAXMAPLEN, typebuf + typeoff, offset);
			memmove(noremapbuf + MAXMAPLEN, noremapbuf + typeoff, offset);
			typeoff = MAXMAPLEN;
		}
			/* adjust typebuf */
		memmove(typebuf + typeoff + offset, typebuf + i + len,
														typelen - offset);
			/* adjust noremapbuf[] */
		memmove(noremapbuf + typeoff + offset, noremapbuf + i + len,
														typelen - offset);
	}

	if (typemaplen > offset)			/* adjust typemaplen */
	{
		if (typemaplen < offset + len)
			typemaplen = offset;
		else
			typemaplen -= len;
	}
	if (no_abbr_cnt > offset)			/* adjust no_abbr_cnt */
	{
		if (no_abbr_cnt < offset + len)
			no_abbr_cnt = offset;
		else
			no_abbr_cnt -= len;
	}
}

/*
 * Write typed characters to script file.
 * If recording is on put the character in the recordbuffer.
 */
	static void
gotchars(s, len)
	char_u	*s;
	int		len;
{
	while (len--)
	{
		updatescript(*s & 255);

		if (Recording)
			add_char_buff(&recordbuff, (*s & 255));
		++s;
	}

		/*
		 * Do not sync in insert mode, unless cursor key has been used.
		 * We need to use real_State, because State may be changed to
		 * NOMAPPING for getting the second key of a special key code.
		 */
	if (!(real_State & (INSERT + CMDLINE)) || arrow_used)		
		u_sync();
}

/*
 * open new script file for ":so!" command
 * return OK on success, FAIL on error
 */
	int
openscript(name)
	char_u *name;
{
	int oldcurscript;

	if (curscript + 1 == NSCRIPT)
	{
		emsg(e_nesting);
		return FAIL;
	}
	else
	{
		if (scriptin[curscript] != NULL)	/* already reading script */
			++curscript;
		if ((scriptin[curscript] = fopen((char *)name, READBIN)) == NULL)
		{
			emsg2(e_notopen, name);
			if (curscript)
				--curscript;
			return FAIL;
		}
		/*
		 * With command ":g/pat/so! file" we have to execute the
		 * commands from the file now.
		 */
		if (global_busy)
		{
			State = NORMAL;
			oldcurscript = curscript;
			do
			{
				normal();
				vpeekc();			/* check for end of file */
			}
			while (scriptin[oldcurscript]);
			State = CMDLINE;
		}
	}
	return OK;
}

/*
 * updatescipt() is called when a character can be written into the script file
 * or when we have waited some time for a character (c == 0)
 *
 * All the changed memfiles are synced if c == 0 or when the number of typed
 * characters reaches 'updatecount'.
 */
	void
updatescript(c)
	int c;
{
	static int		count = 0;

	if (c && scriptout)
		putc(c, scriptout);
	if (c == 0 || ++count >= p_uc)
	{
		ml_sync_all(c == 0, TRUE);
		count = 0;
	}
}

#define NEEDMORET -1		/* value for incomplete mapping or key-code */

static int old_char = -1;		/* ungotten character */

	int
vgetc()
{
	int		c;

	real_State = State;
	c = vgetorpeek(TRUE);
/*
 * get extra byte for special keys
 */
	if (c == K_SPECIAL)
	{
		State = NOMAPPING;
		c = vgetorpeek(TRUE);		/* no mapping for second char */
		State = real_State;
		if (c == KS_SPECIAL)
			c = K_SPECIAL;
		else
			c += KS_OFF;
	}
#ifdef MSDOS
/*
 * If K_NUL was typed, it is replaced by K_NUL, 3 in GetChars().
 * Delete the 3 here.
 */
	else if (c == K_NUL && vpeekc() == 3)
		(void)vgetorpeek(TRUE);
#endif
	return c;
}

	int
vpeekc()
{
	real_State = State;
	return (vgetorpeek(FALSE));
}

/*
 * Call vpeekc() without causing anything to be mapped.
 * Return TRUE if a character is available, FALSE otherwise.
 */
	int
char_avail()
{
	int		save_State;
	int		retval;

	save_State = State;
	State = NOMAPPING;
	retval = vpeekc();
	State = save_State;
	return (retval != NUL);
}

	void
vungetc(c)		/* unget one character (can only be done once!) */
	int		c;
{
	old_char = c;
}

/*
 * get a character: 1. from a previously ungotten character
 *					2. from the stuffbuffer
 *					3. from the typeahead buffer
 *					4. from the user
 *
 * KeyTyped is set to TRUE in the case the user typed the key.
 * advance is TRUE (vgetc()): really get the character.
 * advance is FALSE (vpeekc()): just look whether there is a character
 * available.
 */

	static int
vgetorpeek(advance)
	int		advance;
{
	register int	c;
#ifdef ADDED_BY_WEBB_COMPILE
	int				keylen = 0;				/* init for gcc */
#else
	int				keylen;
#endif /* ADDED_BY_WEBB_COMPILE */
#ifdef AMIGA
	char_u			*s;
#endif
	register struct mapblock *mp;
	int				timedout = FALSE;		/* waited for more than 1 second
												for mapping to complete */
	int				mapdepth = 0;			/* check for recursive mapping */
	int				mode_deleted = FALSE;	/* set when mode has been deleted */
	int				local_state;
	register int	mlen;
	int				max_mlen;
	int				i;

	/*
	 * VISUAL state is never set, it is used only here, therefore a check is
	 * made if NORMAL state is actually VISUAL state.
	 */
	local_state = State;
	if ((State & NORMAL) && VIsual_active)
		local_state = VISUAL;

/*
 * get a character: 1. from a previously ungotten character
 */
	if (old_char >= 0)
	{
		c = old_char;
		if (advance)
			old_char = -1;
		return c;
	}

	init_typebuf();
	start_stuff();
	if (advance && typemaplen == 0)
		Exec_reg = FALSE;
	do
	{
/*
 * get a character: 2. from the stuffbuffer
 */
		c = read_stuff(advance);
		if (c != NUL && !got_int)
			KeyTyped = FALSE;
		else
		{
			/*
			 * Loop until we either find a matching mapped key, or we
			 * are sure that it is not a mapped key.
			 * If a mapped key sequence is found we go back to the start to
			 * try re-mapping.
			 */

			for (;;)
			{
				breakcheck();				/* check for CTRL-C */
				if (got_int)
				{
					c = inchar(typebuf, MAXMAPLEN, 0);	/* flush all input */
					/*
					 * If inchar returns TRUE (script file was active) or we are
					 * inside a mapping, get out of insert mode.
					 * Otherwise we behave like having gotten a CTRL-C.
					 * As a result typing CTRL-C in insert mode will
					 * really insert a CTRL-C.
					 */
					if ((c || typemaplen) && (State & (INSERT + CMDLINE)))
						c = ESC;
					else
						c = Ctrl('C');
					flush_buffers(TRUE);		/* flush all typeahead */
					break;
				}
				else if (typelen > 0)	/* check for a mappable key sequence */
				{
					/*
					 * walk through the maplist until we find an
					 * entry that matches.
					 *
					 * Don't look for mappings if:
					 * - timed out
					 * - typebuf[typeoff] should not be remapped
					 * - in insert or cmdline mode and 'paste' option set
					 * - waiting for "hit return to continue" and CR or SPACE
					 *   typed
					 * - waiting for a char with --more--
					 * - in Ctrl-X mode, and we get a valid char for that mode
					 */
					mp = NULL;
					max_mlen = 0;
					if (!timedout && (typemaplen == 0 ||
								(p_remap && noremapbuf[typeoff] == FALSE))
							&& !((State & (INSERT + CMDLINE)) && p_paste)
							&& !(State == HITRETURN && (typebuf[typeoff] == CR
								|| typebuf[typeoff] == ' '))
							&& State != ASKMORE
							&& !is_ctrl_x_key(typebuf[typeoff]))
					{
						for (mp = maplist.m_next; mp; mp = mp->m_next)
						{
							/*
							 * Only consider an entry if
							 * - it is not an abbreviation and
							 * - the first character matches and
							 * - it is for the current state
							 */
							if (!(mp->m_mode & ABBREV) && 
										mp->m_keys[0] == typebuf[typeoff] &&
										(mp->m_mode & local_state))
							{
								int		n;

									/* find the match length of this mapping */
								for (mlen = 1; mlen < typelen; ++mlen)
									if (mp->m_keys[mlen] !=
													typebuf[typeoff + mlen])
										break;

									/* if one of the typed keys cannot be
									 * remapped, skip the entry */
								for (n = 0; n < mlen; ++n)
									if (noremapbuf[typeoff + n] == TRUE)
										break;
								if (n != mlen)
									continue;

									/* (partly) match found */
								keylen = mp->m_keylen;
								if (mlen == (keylen > typelen ?
													typelen : keylen))
								{
										/* partial match, need more chars */
									if (keylen > typelen)
										keylen = NEEDMORET;
									break;	
								}
									/* no match, may have to check for
									 * termcode at next character */
								if (max_mlen < mlen)
									max_mlen = mlen;
							}
						}
					}
					if (mp == NULL)					/* no match found */
					{
							/*
							 * check if we have a terminal code, when
							 *	mapping is allowed,
							 *  keys have not been mapped,
							 *	and not an ESC sequence, not in insert mode or
							 *		p_ek is on,
							 *	and when not timed out,
							 */
						if (State != NOMAPPING && (typemaplen == 0 ||
								(p_remap && noremapbuf[typeoff] == FALSE)) &&
											!timedout)
							keylen = check_termcode(max_mlen + 1);
						else
							keylen = 0;
						if (keylen == 0)		/* no matching terminal code */
						{
#ifdef AMIGA					/* check for window bounds report */
							if (typemaplen == 0 &&
											(typebuf[typeoff] & 0xff) == CSI)
							{
								for (s = typebuf + typeoff + 1;
										s < typebuf + typeoff + typelen &&
										(isdigit(*s) || *s == ';' || *s == ' ');
										++s)
									;
								if (*s == 'r' || *s == '|')	/* found one */
								{
									del_typebuf(s + 1 - (typebuf + typeoff), 0);
										/* get size and redraw screen */
									set_winsize(0, 0, FALSE);
									continue;
								}
								if (*s == NUL)		/* need more characters */
									keylen = -1;
							}
							if (keylen != -1)		/* got a single character */
#endif
							{
/*
 * get a character: 3. from the typeahead buffer
 */
								c = typebuf[typeoff] & 255;
								if (typemaplen)
									KeyTyped = FALSE;
								else
								{
									KeyTyped = TRUE;
										/* write char to script file(s) */
									if (advance)
										gotchars(typebuf + typeoff, 1);
								}
								if (advance)	/* remove chars from typebuf */
									del_typebuf(1, 0);
								break;		/* got character, break for loop */
							}
						}
						if (keylen > 0)		/* full matching terminal code */
							continue;	/* try mapping again */

						/* partial match: get some more characters */
						keylen = NEEDMORET;
					}
						/* complete match */
					if (keylen != NEEDMORET && keylen <= typelen)
					{
										/* write chars to script file(s) */
						if (keylen > typemaplen)
							gotchars(typebuf + typeoff + typemaplen,
														keylen - typemaplen);

						del_typebuf(keylen, 0);	/* remove the mapped keys */

						/*
						 * Put the replacement string in front of mapstr.
						 * The depth check catches ":map x y" and ":map y x".
						 */
						if (++mapdepth >= p_mmd)
						{
							EMSG("recursive mapping");
							if (State == CMDLINE)
								redrawcmdline();
							else
								setcursor();
							flush_buffers(FALSE);
							mapdepth = 0;		/* for next one */
							c = -1;
							break;
						}
						/*
						 * Insert the 'to' part in the typebuf.
						 * If 'from' field is the same as the start of the
						 * 'to' field, don't remap this part.
						 * If m_noremap is set, don't remap the whole 'to'
						 * part.
						 */
						if (ins_typebuf(mp->m_str, mp->m_noremap ? -1 :
							STRNCMP(mp->m_str, mp->m_keys, keylen) ? 0 : keylen,
															0, TRUE) == FAIL)
						{
							c = -1;
							break;
						}
						continue;
					}
				}
				/*
				 * special case: if we get an <ESC> in insert mode and there
				 * are no more characters at once, we pretend to go out of
				 * insert mode.  This prevents the one second delay after
				 * typing an <ESC>.  If we get something after all, we may
				 * have to redisplay the mode. That the cursor is in the wrong
				 * place does not matter.
				 */
				c = 0;
				if (advance && typelen == 1 && typebuf[typeoff] == ESC &&
						typemaplen == 0 && (State & INSERT) &&
						(p_timeout || (keylen == NEEDMORET && p_ttimeout)) &&
						(c = inchar(typebuf + typeoff + typelen, 2, 0)) == 0)
				{
					if (p_smd)
					{
						delmode();
						mode_deleted = TRUE;
					}
					if (curwin->w_cursor.col != 0)	/* move cursor one left if possible */
					{
						if (curwin->w_col)
						{
							if (did_ai)
							{
								/*
								 * We are expecting to truncate the trailing
								 * white-space, so find the last non-white
								 * character -- webb
								 */
								int		col, vcol;
								char_u	*ptr;

								col = vcol = curwin->w_col = 0;
								ptr = ml_get(curwin->w_cursor.lnum);
								while (col < curwin->w_cursor.col)
								{
									if (!iswhite(ptr[col]))
										curwin->w_col = vcol;
									vcol += chartabsize(ptr[col++], vcol);
								}
  								if (curwin->w_p_nu)
									curwin->w_col += 8;
							}
							else
								--curwin->w_col;
						}
						else if (curwin->w_p_wrap && curwin->w_row)
						{
								--curwin->w_row;
								curwin->w_col = Columns - 1;
						}
					}
					setcursor();
					flushbuf();
				}
				typelen += c;
													/* buffer full, don't map */
				if (typelen >= typemaplen + MAXMAPLEN)
				{
					timedout = TRUE;
					continue;
				}
/*
 * get a character: 4. from the user
 */
				/*
				 * If we have a partial match (and are going to wait for more
				 * input from the user), show the partially matched characters
				 * to the user with showcmd -- webb.
				 */
				i = 0;
				if (keylen == NEEDMORET &&
									(State & (NORMAL | INSERT)) && advance)
				{
					push_showcmd();
					while (i < typelen)
						(void)add_to_showcmd(typebuf[typeoff + i++]);
				}

				c = inchar(typebuf + typeoff + typelen,
							typemaplen + MAXMAPLEN - typelen,
							!advance ? 0 : ((typelen == 0 ||
								!(p_timeout || (p_ttimeout &&
								keylen == NEEDMORET))) ? -1 : (int)p_tm));

				if (i)
					pop_showcmd();

				if (c <= NUL)		/* no character available */
				{
					if (!advance)
						break;
					if (typelen)				/* timed out */
					{
						timedout = TRUE;
						continue;
					}
				}
				else
				{			/* allow mapping for just typed characters */
					while (typebuf[typeoff + typelen] != NUL)
						noremapbuf[typeoff + typelen++] = FALSE;
				}
			}		/* for (;;) */
		}		/* if (!character from stuffbuf) */

						/* if advance is FALSE don't loop on NULs */
	} while (c < 0 || (advance && c == NUL));

	/*
	 * The "INSERT" message is taken care of here:
	 *   if we return an ESC the message is deleted
	 *   if we don't return an ESC but deleted the message before, redisplay it
	 */
	if (p_smd && (State & INSERT))
	{
		if (c == ESC && !mode_deleted)
			delmode();
		else if (c != ESC && mode_deleted)
			showmode();
	}

	return c;
}

/*
 * map[!]					: show all key mappings
 * map[!] {lhs}				: show key mapping for {lhs}
 * map[!] {lhs} {rhs}		: set key mapping for {lhs} to {rhs}
 * noremap[!] {lhs} {rhs}	: same, but no remapping for {rhs}
 * unmap[!] {lhs}			: remove key mapping for {lhs}
 * abbr						: show all abbreviations
 * abbr {lhs}				: show abbreviations for {lhs}
 * abbr {lhs} {rhs}			: set abbreviation for {lhs} to {rhs}
 * noreabbr {lhs} {rhs}		: same, but no remapping for {rhs}
 * unabbr {lhs}				: remove abbreviation for {lhs}
 *
 * maptype == 1 for unmap command, 2 for noremap command.
 *
 * keys is pointer to any arguments.
 *
 * for :map	  mode is NORMAL + VISUAL
 * for :map!  mode is INSERT + CMDLINE
 * for :cmap  mode is CMDLINE
 * for :imap  mode is INSERT 
 * for :nmap  mode is NORMAL
 * for :vmap  mode is VISUAL
 * for :abbr  mode is INSERT + CMDLINE + ABBREV
 * for :iabbr mode is INSERT + ABBREV
 * for :cabbr mode is CMDLINE + ABBREV
 * 
 * Return 0 for success
 *		  1 for invalid arguments
 *		  2 for no match
 *		  3 for ambiguety
 *		  4 for out of mem
 */
	int
domap(maptype, keys, mode)
	int		maptype;
	char_u	*keys;
	int		mode;
{
	struct mapblock		*mp, *mprev;
	char_u				*arg;
	char_u				*p;
	int					n;
	int					len = 0;		/* init for GCC */
	char_u				*newstr;
	int					hasarg;
	int					haskey;
	int					did_it = FALSE;
	int					abbrev = 0;
	int					round;
	char_u				*keys_buf = NULL;
	char_u				*arg_buf = NULL;
	int					retval = 0;

	if (mode & ABBREV)		/* not a mapping but an abbreviation */
	{
		abbrev = ABBREV;
		mode &= ~ABBREV;
	}
/*
 * find end of keys and remove CTRL-Vs in it
 * with :unmap white space is included in the keys, no argument possible
 */
	p = keys;
	while (*p && (maptype == 1 || !iswhite(*p)))
	{
		if (*p == Ctrl('V') && p[1] != NUL)
			STRCPY(p, p + 1);			/* remove CTRL-V */
		++p;
	}
	if (*p != NUL)
		*p++ = NUL;
	skipwhite(&p);
	arg = p;
	hasarg = (*arg != NUL);
	haskey = (*keys != NUL);

		/* check for :unmap without argument */
	if (maptype == 1 && !haskey)	
	{
		retval = 1;
		goto theend;
	}

	/*
	 * If mapping has been given as ^V<C_UP> say, then replace the term codes
	 * with the appropriate two bytes.
	 * The length may be doubled, we need to allocate some memory.
	 */
	if (haskey)
	{
		keys_buf = alloc(STRLEN(keys) * 2 + 1);
		if (keys_buf != NULL)
		{
			STRCPY(keys_buf, keys);
			keys = keys_buf;
			replace_termcodes(keys);
		}
	}
	if (hasarg)
	{
		arg_buf = alloc(STRLEN(arg) * 2 + 1);
		if (arg_buf != NULL)
		{
			STRCPY(arg_buf, arg);
			arg = arg_buf;
			replace_termcodes(arg);
		}
	}

/*
 * remove CTRL-Vs from argument
 */
	for (p = arg; *p; ++p)
		if (*p == Ctrl('V') && p[1] != NUL)
			STRCPY(p, p + 1);			/* remove CTRL-V */

/*
 * check arguments and translate function keys
 */
	if (haskey)
	{
		if (keys[0] == '#' && isdigit(keys[1]))		/* function key */
		{
			keys[0] = K_SPECIAL;
			if (keys[1] == '0')
				keys[1] = KS_F10;
			else
				keys[1] = keys[1] - '1' + KS_F1;
		}
		len = STRLEN(keys);
		if (len > MAXMAPLEN)			/* maximum lenght of MAXMAPLEN chars */
		{
			retval = 1;
			goto theend;
		}

		/*
		 * abbreviation must end in id-char
		 * rest must be all id-char or all non-id-char
		 */
		if (abbrev)
		{
			if (!isidchar_id(*(keys + len - 1)))  /* does not end in id char */
				{
					retval = 1;
					goto theend;
				}
			for (n = 0; n < len - 2; ++n)
				if (isidchar_id(*(keys + n)) != isidchar_id(*(keys + len - 2)))
				{
					retval = 1;
					goto theend;
				}
		}
	}

	if (haskey && hasarg && abbrev)		/* if we will add an abbreviation */
		no_abbr = FALSE;				/* reset flag that indicates there are
															no abbreviations */

	if (!haskey || (maptype != 1 && !hasarg))
		msg_start();
/*
 * Find an entry in the maplist that matches.
 * For :unmap we may loop two times: once to try to unmap an entry with a
 * matching 'from' part, a second time, if the first fails, to unmap an
 * entry with a matching 'to' part. This was done to allow ":ab foo bar" to be
 * unmapped by typing ":unab foo", where "foo" will be replaced by "bar" because
 * of the abbreviation.
 */
	for (round = 0; (round == 0 || maptype == 1) && round <= 1 && !did_it && !got_int; ++round)
	{
		for (mp = maplist.m_next, mprev = &maplist; mp && !got_int; mprev = mp, mp = mp->m_next)
		{
										/* skip entries with wrong mode */
			if (!(mp->m_mode & mode) || (mp->m_mode & ABBREV) != abbrev)
				continue;
			if (!haskey)						/* show all entries */
			{
				showmap(mp);
				did_it = TRUE;
			}
			else								/* do we have a match? */
			{
				if (round)		/* second round: try 'to' string for unmap */
				{
					n = STRLEN(mp->m_str);
					p = mp->m_str;
				}
				else
				{
					n = mp->m_keylen;
					p = mp->m_keys;
				}
				if (!STRNCMP(p, keys, (size_t)(n < len ? n : len)))
				{
					if (maptype == 1)			/* delete entry */
					{
						if (n != len)			/* not a full match */
							continue;
						/*
						 * We reset the indicated mode bits. If nothing is left the
						 * entry is deleted below.
						 */
						mp->m_mode &= (~mode | ABBREV);
						did_it = TRUE;			/* remember that we did something */
					}
					else if (!hasarg)			/* show matching entry */
					{
						showmap(mp);
						did_it = TRUE;
					}
					else if (n != len)			/* new entry is ambigious */
					{
						if (abbrev)				/* for abbreviations that's ok */
							continue;
						retval = 3;
						goto theend;
					}
					else
					{
						mp->m_mode &= (~mode | ABBREV);		/* remove mode bits */
						if (!(mp->m_mode & ~ABBREV) && !did_it)	/* reuse existing entry */
						{
							newstr = strsave(arg);
							if (newstr == NULL)
							{
								retval = 4;			/* no mem */
								goto theend;
							}
							free(mp->m_str);
							mp->m_str = newstr;
							mp->m_noremap = maptype;
							mp->m_mode = mode + abbrev;
							did_it = TRUE;
						}
					}
					if (!(mp->m_mode & ~ABBREV))		/* entry can be deleted */
					{
						free(mp->m_keys);
						free(mp->m_str);
						mprev->m_next = mp->m_next;
						free(mp);
						mp = mprev;					/* continue with next entry */
					}
				}
			}
		}
	}

	if (maptype == 1)						/* delete entry */
	{
		if (!did_it)
			retval = 2;						/* no match */
		goto theend;
	}

	if (!haskey || !hasarg)					/* print entries */
	{
		if (!did_it)
		{
			if (abbrev)
				MSG("No abbreviation found");
			else
				MSG("No mapping found");
		}
		goto theend;						/* listing finished */
	}

	if (did_it)					/* have added the new entry already */
		goto theend;
/*
 * get here when we have to add a new entry
 */
		/* allocate a new entry for the maplist */
	mp = (struct mapblock *)alloc((unsigned)sizeof(struct mapblock));
	if (mp == NULL)
	{
		retval = 4;			/* no mem */
		goto theend;
	}
	mp->m_keys = strsave(keys);
	mp->m_str = strsave(arg);
	if (mp->m_keys == NULL || mp->m_str == NULL)
	{
		free(mp->m_keys);
		free(mp->m_str);
		free(mp);
		retval = 4;		/* no mem */
		goto theend;
	}
	mp->m_keylen = STRLEN(mp->m_keys);
	mp->m_noremap = maptype;
	mp->m_mode = mode + abbrev;

	/* add the new entry in front of the maplist */
	mp->m_next = maplist.m_next;
	maplist.m_next = mp;

theend:
	free(keys_buf);
	free(arg_buf);
	return retval;
}

	static void
showmap(mp)
	struct mapblock *mp;
{
	int len;

	if (msg_didout)
		msg_outchar('\n');
	if ((mp->m_mode & (INSERT + CMDLINE)) == INSERT + CMDLINE)
		MSG_OUTSTR("! ");
	else if (mp->m_mode & INSERT)
		MSG_OUTSTR("i ");
	else if (mp->m_mode & CMDLINE)
		MSG_OUTSTR("c ");
	else if (!(mp->m_mode & VISUAL))
		MSG_OUTSTR("n ");
	else if (!(mp->m_mode & NORMAL))
		MSG_OUTSTR("v ");
	else
		MSG_OUTSTR("  ");
	len = msg_outtrans_meta(mp->m_keys, TRUE);	/* get length of what we write */
	do
	{
		msg_outchar(' ');				/* padd with blanks */
		++len;
	} while (len < 12);
	if (mp->m_noremap)
		msg_outchar('*');
	else
		msg_outchar(' ');
	/* Use FALSE below if we only want things like <C_UP> to show up as such on
	 * the rhs, and not M-x etc, TRUE gets both -- webb
	 */
	msg_outtrans_meta(mp->m_str, TRUE);
	flushbuf();							/* show one line at a time */
}

/*
 * Check for an abbreviation.
 * Cursor is at ptr[col]. When inserting, mincol is where insert started.
 * "c" is the character typed before check_abbr was called.
 *
 * Historic vi practice: The last character of an abbreviation must be an id
 * character ([a-zA-Z0-9_]). The characters in front of it must be all id
 * characters or all non-id characters. This allows for abbr. "#i" to
 * "#include".
 *
 * return TRUE if there is an abbreviation, FALSE if not
 */
	int
check_abbr(c, ptr, col, mincol)
	int		c;
	char_u	*ptr;
	int		col;
	int		mincol;
{
	int				len;
	int				j;
	char_u			tb[3];
	struct mapblock *mp;
	int				is_id = TRUE;

	if (no_abbr_cnt)		/* abbrev. are not recursive */
		return FALSE;

	if (col == 0 || !isidchar_id(ptr[col - 1]))	/* cannot be an abbr. */
		return FALSE;

	if (col > 1)
		is_id = isidchar_id(ptr[col - 2]);
	for (len = col - 1; len > 0 && !isspace(ptr[len - 1]) &&
								is_id == isidchar_id(ptr[len - 1]); --len)
		;

	if (len < mincol)
		len = mincol;
	if (len < col)				/* there is a word in front of the cursor */
	{
		ptr += len;
		len = col - len;
		for (mp = maplist.m_next; mp; mp = mp->m_next)
		{
					/* find entries with right mode and keys */
			if ((mp->m_mode & ABBREV) == ABBREV &&
						(mp->m_mode & State) &&
						mp->m_keylen == len &&
						!STRNCMP(mp->m_keys, ptr, (size_t)len))
				break;
		}
		if (mp)
		{
			/*
			 * Found a match:
			 * Insert the rest of the abbreviation in typebuf[].
			 * This goes from end to start.
			 *
			 * Characters 0x000 - 0x100: normal chars, may need CTRL-V.
			 * Characters 0x100 - 0x200: key codes, need K_SPECIAL.
			 * Characters   above 0x200: don't use CTRL-V
			 */
			j = 0;
			if (c < 0x100)
			{
				if (c < ' ' || c > '~')
					tb[j++] = Ctrl('V');	/* special char needs CTRL-V */
			}
			else if (c < 0x200)				/* special key code, split up */
			{
				tb[j++] = K_SPECIAL;
				c = K_SECOND(c);
			}
			tb[j++] = c;
			tb[j] = NUL;
												/* insert the last typed char */
			(void)ins_typebuf(tb, TRUE, 0, TRUE);
												/* insert the to string */
			(void)ins_typebuf(mp->m_str, mp->m_noremap, 0, TRUE);
												/* no abbrev. for these chars */
			no_abbr_cnt += STRLEN(mp->m_str) + j;
			while (len--)
												/* delete the from string */
				(void)ins_typebuf((char_u *)"\b", TRUE, 0, TRUE);
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * Write map commands for the current mappings to an .exrc file.
 * Return FAIL on error, OK otherwise.
 */
	int
makemap(fd)
	FILE *fd;
{
	struct mapblock *mp;
	char_u			c1;
	char_u 			*p;

	for (mp = maplist.m_next; mp; mp = mp->m_next)
	{
		c1 = NUL;
		p = (char_u *)"map";
		switch (mp->m_mode)
		{
		case NORMAL + VISUAL:
			break;
		case NORMAL:
			c1 = 'n';
			break;
		case VISUAL:
			c1 = 'v';
			break;
		case CMDLINE + INSERT:
			p = (char_u *)"map!";
			break;
		case CMDLINE:
			c1 = 'c';
			break;
		case INSERT:
			c1 = 'i';
			break;
		case INSERT + CMDLINE + ABBREV:
			p = (char_u *)"abbr";
			break;
		case CMDLINE + ABBREV:
			c1 = 'c';
			p = (char_u *)"abbr";
			break;
		case INSERT + ABBREV:
			c1 = 'i';
			p = (char_u *)"abbr";
			break;
		default:
			EMSG("makemap: Illegal mode");
			return FAIL;
		}
		if (c1 && putc(c1, fd) < 0)
			return FAIL;
		if (mp->m_noremap && fprintf(fd, "nore") < 0)
			return FAIL;
		if (fprintf(fd, (char *)p) < 0)
			return FAIL;

		if (	putc(' ', fd) < 0 || putescstr(fd, mp->m_keys, FALSE) == FAIL ||
				putc(' ', fd) < 0 || putescstr(fd, mp->m_str, FALSE) == FAIL ||
#ifdef MSDOS
				putc('\r', fd) < 0 ||
#endif
				putc('\n', fd) < 0)
			return FAIL;
	}
	return OK;
}

/*
 * write escape string to file
 *
 * return FAIL for failure, OK otherwise
 */
	int
putescstr(fd, str, set)
	FILE		*fd;
	char_u		*str;
	int			set;		/* TRUE for makeset, FALSE for makemap */
{
	int		c;

	for ( ; *str; ++str)
	{
		c = *str;
		/*
		 * Special key codes have to be translated to be able to make sense
		 * when they are read back.
		 */
		if (c == K_SPECIAL)
		{
			c = *++str;
			if (!set && c >= KS_UARROW && c <= KS_MAXKEY)
			{
					/* sorry, no check for write error on this one */
				fprintf(fd, "<%s>", (char *)get_key_names()[c - KS_UARROW]);
				continue;
			}
			else if (c == KS_SPECIAL)
				c = K_SPECIAL;
			else if (c == KS_ZERO)
				c = NUL;
			/* else: illegal key code !? */
		}
		/*
		 * some characters have to be escaped with CTRL-V to
		 * prevent them from misinterpreted in DoOneCmd().
		 * A space has to be escaped with a backslash to
		 * prevent it to be misinterpreted in doset().
		 */
		if (c < ' ' || c > '~' || (c == ' ' && !set))
		{
			if (putc(Ctrl('V'), fd) < 0)
				return FAIL;
		}
		else if ((set && c == ' ') || c == '|')
		{
			if (putc('\\', fd) < 0)
				return FAIL;
		}
		if (putc(c, fd) < 0)
			return FAIL;
	}
	return OK;
}
