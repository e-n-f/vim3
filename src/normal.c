/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * Contains the main routine for processing characters in command mode.
 * Communicates closely with the code in ops.c to handle the operators.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"

#undef EXTERN
#undef INIT
#define EXTERN
#define INIT(x) x
#include "ops.h"

/*
 * Generally speaking, every command in normal() should either clear any
 * pending operator (with CLEAROP), or set the motion type variable.
 */

#define CLEAROP 		clearop()		/* clear any pending operator */
#define CLEAROPBEEP 	clearopbeep()	/* CLEAROP plus a beep() */
#define CHECKCLEAROP	if (checkclearop()) break;
#define CHECKCLEAROPQ	if (checkclearopq()) break;

/*
 * If a count is given before the operator, it is saved in opnum.
 */
static linenr_t	opnum = 0;
static linenr_t	Prenum; 		/* The (optional) number before a command. */
#ifdef ADDED_BY_WEBB_COMPILE
static int		p_char = NUL;	/* prepended command char */
#else
static int		pchar = NUL;	/* prepended command char */
#endif /* ADDED_BY_WEBB_COMPILE */

static void		prep_redo __ARGS((long, int, int, int, int));
static int		checkclearop __ARGS((void));
static int		checkclearopq __ARGS((void));
static void		clearop __ARGS((void));
static void		clearopbeep __ARGS((void));

/*
 * normal
 *
 * Execute a command in normal mode.
 *
 * This is basically a big switch with the cases arranged in rough categories
 * in the following order:
 *
 *	  0. Macros (q, @)
 *	  1. Screen positioning commands (^U, ^D, ^F, ^B, ^E, ^Y, z)
 *	  2. Control commands (:, <help>, ^L, ^G, ^^, ZZ, *, ^], ^T)
 *	  3. Cursor motions (G, H, M, L, l, K_RARROW,  , h, K_LARROW, ^H, k, K_UARROW, ^P, +, CR, LF, j, K_DARROW, ^N, _, |, B, b, W, w, E, e, $, ^, 0)
 *	  4. Searches (?, /, n, N, T, t, F, f, ,, ;, ], [, %, (, ), {, })
 *	  5. Edits (., u, K_UNDO, ^R, U, r, J, p, P, ^A, ^S)
 *	  6. Inserts (A, a, I, i, o, O, R)
 *	  7. Operators (~, d, c, y, >, <, !, =, Q)
 *	  8. Abbreviations (x, X, D, C, s, S, Y, &)
 *	  9. Marks (m, ', `, ^O, ^I)
 *	 10. Buffer setting (")
 *	 11. Visual (v, V, ^V)
 *   12. Suspend (^Z)
 *   13. Window commands (^W)
 *   14. extended commands (starting with 'g')
 *   15. mouse click
 *   16. The end (ESC)
 */

	void
normal()
{
	register int	c;
	long 			n = 0;					/* init for GCC */
	int				flag = FALSE;
	int				flag2 = FALSE;
	int 			type = 0;				/* type of operation */
	int 			dir = FORWARD;			/* search direction */
	int				nchar = NUL;			/* next command char */
	int				finish_op;
	linenr_t		Prenum1;
	char_u			*searchbuff = NULL;		/* buffer for search string */
	FPOS			*pos = NULL;			/* init for gcc */
	char_u			*ptr = NULL;
	int				command_busy = FALSE;
	int				modified = FALSE;		/* changed current buffer */
	int				ctrl_w = FALSE;			/* got CTRL-W command */
	int				old_col = 0;

		/* the visual area is remembered for reselection */
	static int		resel_Visual_mode = NUL;
	static linenr_t	resel_Visual_nlines;	/* number of lines */
	static colnr_t	resel_Visual_col;		/* number of cols or end column */
		/* the visual area is remembered for redo */
	static int		redo_Visual_mode = NUL;
	static linenr_t	redo_Visual_nlines;		/* number of lines */
	static colnr_t	redo_Visual_col;		/* number of cols or end column */
	static long		redo_Visual_Prenum;		/* Prenum for operator */

	Prenum = 0;
	/*
	 * If there is an operator pending, then the command we take this time
	 * will terminate it. Finish_op tells us to finish the operation before
	 * returning this time (unless the operation was cancelled).
	 */
	finish_op = (operator != NOP);

	if (!finish_op && !yankbuffer)
		opnum = 0;

	State = NORMAL_BUSY;
	c = vgetc();
	(void)add_to_showcmd(c);

getcount:
	/* Pick up any leading digits and compute 'Prenum' */
	while ((c >= '1' && c <= '9') || (Prenum != 0 && (c == K_DEL || c == '0')))
	{
		if (c == K_DEL)
			Prenum /= 10;
		else
			Prenum = Prenum * 10 + (c - '0');
		if (Prenum < 0)			/* got too large! */
			Prenum = 999999999;
		c = vgetc();
		(void)add_to_showcmd(c);
	}

/*
 * If we got CTRL-W there may be a/another count
 */
	if (c == Ctrl('W') && !ctrl_w)
	{
		ctrl_w = TRUE;
		opnum = Prenum;						/* remember first count */
		Prenum = 0;
		State = ONLYKEY;					/* no mapping for nchar, but keys */
		c = vgetc();						/* get next character */
		(void)add_to_showcmd(c);
		goto getcount;						/* jump back */
	}

	/*
	 * If we're in the middle of an operator (including after entering a yank
	 * buffer with ") AND we had a count before the
	 * operator, then that count overrides the current value of Prenum. What
	 * this means effectively, is that commands like "3dw" get turned into
	 * "d3w" which makes things fall into place pretty neatly.
	 * If you give a count before AND after the operator, they are multiplied.
	 */
	if (opnum != 0)
	{
			if (Prenum)
				Prenum *= opnum;
			else
				Prenum = opnum;
			opnum = 0;
	}

	Prenum1 = (Prenum == 0 ? 1 : Prenum);		/* Prenum often defaults to 1 */

	/*
	 * get an additional character if we need one
	 * for CTRL-W we already got it when looking for a count
	 */
	if (ctrl_w)
	{
		nchar = c;
		c = Ctrl('W');
	}
	else if ((c < 0x100 && strchr("@zZtTfF[]mg'`\"", c) != NULL) ||
					(c == 'q' && !Recording && !Exec_reg) ||
					(c == 'r' && !VIsual_active)
#if defined(UNIX) || defined(MSDOS)
					|| c == K_MOUSE
#endif
					)
	{
		State = ONLYKEY;
		nchar = vgetc();		/* no macro mapping for this char, but allow
								 * key codes */
		(void)add_to_showcmd(nchar);
	}
	if (p_sc)
		flushbuf();				/* flush the showcmd characters onto the
								 * screen so we can see them while the command
								 * is being executed
								 */

/*
 * For commands that don't get another character we can put the State back to
 * NORMAL and check for a window size change.
 */
	if (c >= 0x100 || STRCHR("z:/?", c) == NULL)
		State = NORMAL;
	if (nchar == ESC)
	{
		CLEAROP;
		goto normal_end;
	}
	switch (c)
	{

/*
 * 0: Macros
 */
	  case 'q': 		/* (stop) recording into a named register */
		CHECKCLEAROP;
						/* command is ignored while executing a register */
		if (!Exec_reg && dorecord(nchar) == FAIL)
			CLEAROPBEEP;
		break;

	 case '@':			/* execute a named buffer */
		CHECKCLEAROP;
		while (Prenum1--)
		{
			if (doexecbuf(nchar) == FAIL)
			{
				CLEAROPBEEP;
				break;
			}
		}
		break;

/*
 * 1: Screen positioning commands
 */
	  case Ctrl('D'):
		flag = TRUE;

	  case Ctrl('U'):
		CHECKCLEAROP;
		if (Prenum)
			curwin->w_p_scroll = (Prenum > curwin->w_height) ? curwin->w_height : Prenum;
		n = (curwin->w_p_scroll <= curwin->w_height) ? curwin->w_p_scroll : curwin->w_height;
		if (flag)
		{
				curwin->w_topline += n;
				if (curwin->w_topline > curbuf->b_ml.ml_line_count)
					curwin->w_topline = curbuf->b_ml.ml_line_count;
				comp_Botline(curwin);		/* compute curwin->w_botline */
				(void)onedown(n);
		}
		else
		{
				if (n >= curwin->w_cursor.lnum)
					n = curwin->w_cursor.lnum - 1;
				Prenum1 = curwin->w_cursor.lnum - n;
				scrolldown(n);
				if (Prenum1 < curwin->w_cursor.lnum)
					curwin->w_cursor.lnum = Prenum1;
		}
		beginline(MAYBE);
		updateScreen(VALID);
		break;

	  case Ctrl('B'):
	  case K_SUARROW:
	  case K_PAGEUP:
		dir = BACKWARD;

	  case Ctrl('F'):
	  case K_SDARROW:
	  case K_PAGEDOWN:
		CHECKCLEAROP;
		(void)onepage(dir, Prenum1);
		break;

	  case Ctrl('E'):
		CHECKCLEAROP;
		scrollup(Prenum1);
				/* We may have moved to another line -- webb */
		coladvance(curwin->w_curswant);
		updateScreen(VALID);
		break;

	  case Ctrl('Y'):
		CHECKCLEAROP;
		scrolldown(Prenum1);
				/* We may have moved to another line -- webb */
		coladvance(curwin->w_curswant);
		updateScreen(VALID);
		break;

	  case 'z':
		CHECKCLEAROP;
		if (nchar < 0x100 && isdigit(nchar))
		{
			Prenum = nchar - '0';
			for (;;)
			{
				nchar = vgetc();
				(void)add_to_showcmd(nchar);
				State = NORMAL;
				if (c == K_DEL)
					Prenum /= 10;
				else if (nchar < 0x100 && isdigit(nchar))
					Prenum = Prenum * 10 + (nchar - '0');
				else if (nchar == CR)
				{
					win_setheight((int)Prenum);
					break;
				}
				else
				{
					CLEAROPBEEP;
					break;
				}
			}
			operator = NOP;
			break;
		}

		if (Prenum && Prenum != curwin->w_cursor.lnum)	/* line number given */
		{
			setpcmark();
			if (Prenum > curbuf->b_ml.ml_line_count)
				curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
			else
				curwin->w_cursor.lnum = Prenum;
		}
		State = NORMAL;			/* for updateScreen() */
		switch (nchar)
		{
		  case NL:				/* put curwin->w_cursor at top of screen */
		  case CR:
			beginline(TRUE);
		  case 't':
			curwin->w_topline = curwin->w_cursor.lnum;
			break;

		  case '.': 			/* put curwin->w_cursor in middle of screen */
		  case 'z':
			n = (curwin->w_height + plines(curwin->w_cursor.lnum)) / 2 - 1;
			goto dozcmd;

		  case '-': 			/* put curwin->w_cursor at bottom of screen */
		  case 'b':
			n = curwin->w_height;
			/* FALLTHROUGH */

	dozcmd:
			{
				register linenr_t	lp = curwin->w_cursor.lnum;
				register long		l = plines(lp);

				do
				{
					curwin->w_topline = lp;
					if (--lp == 0)
						break;
					l += plines(lp);
				} while (l <= n);
			}
			if (nchar != 'z' && nchar != 'b')
				beginline(TRUE);
			break;

		  case Ctrl('S'):	/* ignore CTRL-S and CTRL-Q to avoid problems */
		  case Ctrl('Q'):	/* with terminals that use xon/xoff */
		  	break;

		  default:
			CLEAROPBEEP;
		}
		updateScreen(VALID);
		break;

/*
 *	  2: Control commands
 */
	  case ':':
	    if (VIsual_active)
			goto dooperator;
		CHECKCLEAROP;
		/*
		 * translate "count:" into ":.,.+(count - 1)"
		 */
		if (Prenum)
		{
			stuffReadbuff((char_u *)".");
			if (Prenum > 1)
			{
				stuffReadbuff((char_u *)",.+");
				stuffnumReadbuff((long)Prenum - 1L);
			}
		}
		docmdline(NULL, FALSE, FALSE);
		modified = TRUE;
		break;

	  case K_HELP:
		CHECKCLEAROP;
#ifdef ADDED_BY_WEBB_COMPILE
		dohelp((char_u *)"");
#else
		dohelp("");
#endif /* ADDED_BY_WEBB_COMPILE */
		break;

	  case Ctrl('L'):
		CHECKCLEAROP;
		updateScreen(CLEAR);
		break;

	  case Ctrl('G'):
		CHECKCLEAROP;
			/* print full name if count given or :cd used */
		fileinfo(did_cd || Prenum);
		break;

	  case K_CCIRCM:			/* CTRL-^, short for ":e #" */
		CHECKCLEAROPQ;
		(void)buflist_getfile((int)Prenum, (linenr_t)0, TRUE);
		break;

	  case 'Z': 		/* write, if changed, and exit */
		CHECKCLEAROPQ;
		if (nchar != 'Z')
		{
			CLEAROPBEEP;
			break;
		}
		stuffReadbuff((char_u *)":x\n");
		break;

	  case Ctrl(']'):			/* :ta to current identifier */
		if (VIsual_active)		/* :ta to visual highlighted text */
		{
			if (VIsual.lnum != curwin->w_cursor.lnum)
			{
				CLEAROPBEEP;
				break;
			}
			if (lt(curwin->w_cursor, VIsual))
			{
				ptr = ml_get_pos(&curwin->w_cursor);
				n = VIsual.col - curwin->w_cursor.col + 1;
			}
			else
			{
				ptr = ml_get_pos(&VIsual);
				n = curwin->w_cursor.col - VIsual.col + 1;
			}
			VIsual_active = FALSE;
			++RedrawingDisabled;
			update_curbuf(NOT_VALID);		/* update the inversion later */
			--RedrawingDisabled;
		}
		CHECKCLEAROPQ;
	  case '*': 				/* / to current identifier or string */
	  case '#': 				/* ? to current identifier or string */
	  case 'K':					/* run program for current identifier */
		if (ptr == NULL &&
				(n = find_ident_under_cursor(&ptr, c == '*' || c == '#')) == 0)
		{
			CLEAROP;
			break;
		}

		if (Prenum)
			stuffnumReadbuff(Prenum);
		switch (c)
		{
			case '*':
				stuffReadbuff((char_u *)"/");
				goto sow;

			case '#':
				stuffReadbuff((char_u *)"?");
sow:			if (isidchar_id(*ptr))
					stuffReadbuff((char_u *)"\\<");
				break;

			case 'K':
				stuffReadbuff((char_u *)":! ");
				stuffReadbuff(p_kp);
				stuffReadbuff((char_u *)" ");
				break;
			default:
				stuffReadbuff((char_u *)":ta ");
		}

		/*
		 * Now grab the chars in the identifier
		 */
		while (n--)
		{
			if (STRCHR(escape_chars, *ptr) != NULL)
				stuffcharReadbuff('\\');
			stuffcharReadbuff(*ptr++);
		}

		if ((c == '*' || c == '#') && isidchar_id(ptr[-1]))
			stuffReadbuff((char_u *)"\\>");
		stuffReadbuff((char_u *)"\n");
		break;

	  case Ctrl('T'):		/* backwards in tag stack */
			CHECKCLEAROPQ;
	  		dotag((char_u *)"", 2, (int)Prenum1);
			break;

/*
 * Cursor motions
 */
	  case 'G':
		mtype = MLINE;
		setpcmark();
		if (Prenum == 0 || Prenum > curbuf->b_ml.ml_line_count)
				curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
		else
				curwin->w_cursor.lnum = Prenum;
		beginline(MAYBE);
		break;

	  case 'H':
	  case 'M':
		if (c == 'M')
				n = (curwin->w_height - curwin->w_empty_rows) / 2;
		else
				n = Prenum;
		mtype = MLINE;
		setpcmark();
		curwin->w_cursor.lnum = curwin->w_topline;
		while (n && onedown((long)1) == OK)
				--n;
		beginline(MAYBE);
		break;

	  case 'L':
		mtype = MLINE;
		setpcmark();
		curwin->w_cursor.lnum = curwin->w_botline - 1;
		for (n = Prenum; n && oneup((long)1) == OK; n--)
				;
		beginline(MAYBE);
		break;

	  case 'l':
	  case K_RARROW:
	  case ' ':
		mtype = MCHAR;
		mincl = FALSE;
		n = Prenum1;
		while (n--)
		{
			if (oneright() == FAIL)
			{
					/* space wraps to next line if 'whichwrap' bit 1 set */
					/* 'l' wraps to next line if 'whichwrap' bit 2 set */
					/* CURS_RIGHT wraps to next line if 'whichwrap' bit 3 set */
				if (((c == ' ' && (p_ww & 2)) ||
					 (c == 'l' && (p_ww & 4)) ||
					 (c == K_RARROW && (p_ww & 8))) &&
					 	curwin->w_cursor.lnum < curbuf->b_ml.ml_line_count)
				{
					++curwin->w_cursor.lnum;
					curwin->w_cursor.col = 0;
					curwin->w_set_curswant = TRUE;
					continue;
				}
				if (operator == NOP)
					beep_flush();
				else
				{
					if (lineempty(curwin->w_cursor.lnum))
						CLEAROPBEEP;
					else
					{
						mincl = TRUE;
						if (n)
							beep_flush();
					}
				}
				break;
			}
		}
		break;

	  case Ctrl('H'):
	  case 'h':
	  case K_LARROW:

		mtype = MCHAR;
		mincl = FALSE;
		n = Prenum1;
		while (n--)
		{
			if (oneleft() == FAIL)
			{
					/* backspace and del wrap to previous line if 'whichwrap'
					 *											bit 0 set */
					/* 'h' wraps to previous line if 'whichwrap' bit 2 set */
					/* CURS_LEFT wraps to previous line if 'whichwrap' bit 3 set */
				if ((((c == Ctrl('H') || c == K_DEL) &&
							(p_ww & 1)) || (c == 'h' && (p_ww & 4)) ||
					 (c == K_LARROW && (p_ww & 8))) &&
							curwin->w_cursor.lnum > 1)
				{
					--(curwin->w_cursor.lnum);
					coladvance(MAXCOL);
					curwin->w_set_curswant = TRUE;
					continue;
				}
				else if (operator != DELETE && operator != CHANGE)
					beep_flush();
				else if (Prenum1 == 1)
					CLEAROPBEEP;
				break;
			}
		}
		break;

	  case '-':
		flag = TRUE;
		/* FALLTHROUGH */

	  case 'k':
	  case K_UARROW:
	  case Ctrl('P'):
		mtype = MLINE;
		if (oneup(Prenum1) == FAIL)
			CLEAROPBEEP;
		else if (flag)
			beginline(TRUE);
		break;

	  case '+':
	  case CR:
		flag = TRUE;
		/* FALLTHROUGH */

	  case 'j':
	  case K_DARROW:
	  case Ctrl('N'):
	  case NL:
		mtype = MLINE;
		if (onedown(Prenum1) == FAIL)
			CLEAROPBEEP;
		else if (flag)
			beginline(TRUE);
		break;

		/*
		 * This is a strange motion command that helps make operators more
		 * logical. It is actually implemented, but not documented in the
		 * real 'vi'. This motion command actually refers to "the current
		 * line". Commands like "dd" and "yy" are really an alternate form of
		 * "d_" and "y_". It does accept a count, so "d3_" works to delete 3
		 * lines.
		 */
	  case '_':
lineop:
		old_col = curwin->w_curswant;
		mtype = MLINE;
		if (onedown((long)(Prenum1 - 1)) == FAIL)
			CLEAROPBEEP;
		if (operator == DELETE || operator == LSHIFT || operator == RSHIFT)
			beginline(MAYBE);
		else if (operator != YANK)			/* 'Y' does not move cursor */
			beginline(TRUE);
		break;

	  case '|':
		mtype = MCHAR;
		mincl = FALSE;
		beginline(FALSE);
		if (Prenum > 0)
			coladvance((colnr_t)(Prenum - 1));
		curwin->w_curswant = (colnr_t)(Prenum - 1);
			/* keep curswant at the column where we wanted to go, not where
				we ended; differs is line is too short */
		curwin->w_set_curswant = FALSE;
		break;

		/*
		 * Word Motions
		 */

	  case 'B':
		type = 1;
		/* FALLTHROUGH */

	  case 'b':
	  case K_SLARROW:
		mtype = MCHAR;
		mincl = FALSE;
		curwin->w_set_curswant = TRUE;
		if (bck_word(Prenum1, type))
			CLEAROPBEEP;
		break;

	  case 'E':
		type = 1;
		/* FALLTHROUGH */

	  case 'e':
		mincl = TRUE;
		goto dowrdcmd;

	  case 'W':
		type = 1;
		/* FALLTHROUGH */

	  case 'w':
	  case K_SRARROW:
		mincl = FALSE;
		flag = TRUE;
		/*
		 * This is a little strange. To match what the real vi does, we
		 * effectively map 'cw' to 'ce', and 'cW' to 'cE', provided that we
		 * are not on a space or a TAB. This seems impolite at first, but it's
		 * really more what we mean when we say 'cw'.
		 * Another strangeness: When standing on the end of a word "ce" will
		 * change until the end of the next wordt, but "cw" will change only
		 * one character! This is done by setting type to 2.
		 */
		if (operator == CHANGE && (n = gchar_cursor()) != ' ' && n != TAB &&
																n != NUL)
		{
			mincl = TRUE;
			flag = FALSE;
			flag2 = TRUE;
		}

dowrdcmd:
		mtype = MCHAR;
		curwin->w_set_curswant = TRUE;
		if (flag)
			n = fwd_word(Prenum1, type, operator != NOP);
		else
			n = end_word(Prenum1, type, flag2);
		if (n)
		{
			CLEAROPBEEP;
			break;
		}
#if 0
		/*
		 * If we do a 'dw' for the last word in a line, we only delete the rest
		 * of the line, not joining the two lines, unless the current line is empty.
		 */
		if (operator == DELETE && Prenum1 == 1 &&
				curbuf->b_startop.lnum != curwin->w_cursor.lnum && !lineempty(startop.lnum))
		{
				curwin->w_cursor = curbuf->b_startop;
				while (oneright() == OK)
					;
				mincl = TRUE;
		}
#endif
		break;

	  case '$':
	  case K_END:
		mtype = MCHAR;
		mincl = TRUE;
		curwin->w_curswant = MAXCOL;				/* so we stay at the end */
		if (onedown((long)(Prenum1 - 1)) == FAIL)
		{
			CLEAROPBEEP;
			break;
		}
		break;

	  case '^':
		flag = TRUE;
		/* FALLTHROUGH */

	  case '0':
	  case K_HOME:
		mtype = MCHAR;
		mincl = FALSE;
		beginline(flag);
		break;

/*
 * 4: Searches
 */
	  case '?':
	  case '/':
		if ((searchbuff = getcmdline(c)) == NULL)
		{
			CLEAROP;
			break;
		}
		mtype = MCHAR;
		mincl = FALSE;
		curwin->w_set_curswant = TRUE;

		n = dosearch(c, searchbuff, FALSE, Prenum1, TRUE, TRUE);
		if (n == 0)
			CLEAROP;
		else if (n == 2)
			mtype = MLINE;
		break;

	  case 'N':
		flag = 1;

	  case 'n':
		mtype = MCHAR;
		mincl = FALSE;
		curwin->w_set_curswant = TRUE;
		if (!dosearch(0, NULL, flag, Prenum1, TRUE, TRUE))
			CLEAROP;
		break;

		/*
		 * Character searches
		 */
	  case 'T':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case 't':
		type = 1;
		goto docsearch;

	  case 'F':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case 'f':
docsearch:
		mtype = MCHAR;
		if (dir == BACKWARD)
			mincl = FALSE;
		else
			mincl = TRUE;
		curwin->w_set_curswant = TRUE;
		if (nchar >= 0x100 || !searchc(nchar, dir, type, Prenum1))
			CLEAROPBEEP;
		break;

	  case ',':
		flag = 1;
		/* FALLTHROUGH */

	  case ';':
	    dir = flag;
	    goto docsearch;		/* nchar == NUL, thus repeat previous search */

		/*
		 * section or C function searches
		 */
	  case '[':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case ']':
		mtype = MCHAR;
		mincl = FALSE;

		/*
		 * "[f" or "]f" : Edit file under the cursor (same as "gf")
		 */
		if (nchar == 'f')
			goto gotofile;

		/*
		 * Find the occurence(s) of the identifier or define under cursor
		 * in current and included files or jump to the first occurence.
		 *
		 * 					search 		 list		    jump 
		 * 				  fwd   bwd    fwd   bwd     fwd    bwd
		 * identifier     "]i"  "[i"   "]I"  "[I"   "]^I"  "[^I"
		 * define		  "]d"  "[d"   "]D"  "[D"   "]^D"  "[^D"
		 */
		if (nchar == 'i' || nchar == 'I' || nchar == Ctrl('I') ||
			nchar == 'd' || nchar == 'D' || nchar == Ctrl('D'))
		{
			int			len;

			if ((len = find_ident_under_cursor(&ptr, FALSE)) == 0)
			{
				CLEAROP;
				break;
			}
			find_pattern_in_path(ptr, len, TRUE, !isupper(nchar),
				((nchar & 0xf) == ('d' & 0xf)) ?  FIND_DEFINE : FIND_ANY,
				Prenum1,
				isupper(nchar) ? ACTION_SHOW_ALL :
							islower(nchar) ? ACTION_SHOW : ACTION_GOTO,
				c == ']' ? curwin->w_cursor.lnum : (linenr_t)1,
				(linenr_t)MAXLNUM);
			curwin->w_set_curswant = TRUE;
			break;
		}

		/*
		 * "[{", "[(", "]}" or "])": go to Nth unclosed '{', '(', '}' or ')'
		 */
		if ((c == '[' && (nchar == '{' || nchar == '(')) ||
		   ((c == ']' && (nchar == '}' || nchar == ')'))))
		{
			FPOS old_pos;

			old_pos = curwin->w_cursor;
			while (Prenum1--)
			{
				if ((pos = findmatch(nchar)) == NULL)
				{
					CLEAROPBEEP;
					break;
				}
				curwin->w_cursor = *pos;
			}
			curwin->w_cursor = old_pos;
			if (pos != NULL)
			{
				setpcmark();
				curwin->w_cursor = *pos;
				curwin->w_set_curswant = TRUE;
			}
			break;
		}

		/*
		 * "[[", "[]", "]]" and "][": move to start or end of function
		 */
		if (nchar == '[' || nchar == ']')
		{
			if (nchar == c)				/* "]]" or "[[" */
				flag = '{';
			else
				flag = '}';				/* "][" or "[]" */

			curwin->w_set_curswant = TRUE;
			/*
			 * Imitate strange vi behaviour: When using "]]" with an operator
			 * we also stop at '}'.
			 */
			if (!findpar(dir, Prenum1, flag,
							(operator != NOP && dir == FORWARD && flag == '{')))
				CLEAROPBEEP;
			else if (operator == NOP)
				beginline(TRUE);
			break;
		}

		/*
		 * "[p", "[P", "]P" and "]p": put with indent adjustment
		 */
		if (nchar == 'p' || nchar == 'P')
		{
			CHECKCLEAROPQ;
			prep_redo(Prenum, NUL, c, nchar, NUL);
			doput((c == ']' && nchar == 'p') ? FORWARD : BACKWARD,
															Prenum1, TRUE);
			modified = TRUE;
			break;
		}

		/*
		 * end of '[' and ']': not a valid nchar
		 */
		CLEAROPBEEP;
		break;

	  case '%':
		mincl = TRUE;
	    if (Prenum)		/* {cnt}% : goto {cnt} percentage in file */
		{
			if (Prenum > 100)
				CLEAROPBEEP;
			else
			{
				mtype = MLINE;
				setpcmark();
						/* round up, so CTRL-G will give same value */
				curwin->w_cursor.lnum = (curbuf->b_ml.ml_line_count * Prenum + 99) / 100;
				beginline(MAYBE);
			}
		}
		else			/* % : go to matching paren */
		{
			mtype = MCHAR;
			if ((pos = findmatch(NUL)) == NULL)
				CLEAROPBEEP;
			else
			{
				setpcmark();
				curwin->w_cursor = *pos;
				curwin->w_set_curswant = TRUE;
			}
		}
		break;

	  case '(':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case ')':
		mtype = MCHAR;
		if (c == ')')
			mincl = FALSE;
		else
			mincl = TRUE;
		curwin->w_set_curswant = TRUE;

		if (!findsent(dir, Prenum1))
			CLEAROPBEEP;
		break;

	  case '{':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case '}':
		mtype = MCHAR;
		mincl = FALSE;
		curwin->w_set_curswant = TRUE;
		if (!findpar(dir, Prenum1, NUL, FALSE))
			CLEAROPBEEP;
		break;

/*
 * 5: Edits
 */
	  case '.':
		CHECKCLEAROPQ;
		/*
		 * if restart_edit is TRUE, the last but one command is repeated
		 * instead of the last command (inserting text). This is used for
		 * CTRL-O <.> in insert mode
		 */
		if (start_redo(Prenum, restart_edit && !arrow_used) == FAIL)
			CLEAROPBEEP;
		modified = TRUE;
		break;

	  case 'u':
	    if (VIsual_active || operator == STRCHR(opchars, 'u') - opchars + 1)
			goto dooperator;
	  case K_UNDO:
		CHECKCLEAROPQ;
		u_undo((int)Prenum1);
		curwin->w_set_curswant = TRUE;
		modified = TRUE;
		break;

	  case Ctrl('R'):
		CHECKCLEAROPQ;
	  	u_redo((int)Prenum1);
		curwin->w_set_curswant = TRUE;
		modified = TRUE;
		break;

	  case 'U':
	    if (VIsual_active || operator == STRCHR(opchars, 'U') - opchars + 1)
			goto dooperator;
		CHECKCLEAROPQ;
		u_undoline();
		curwin->w_set_curswant = TRUE;
		modified = TRUE;
		break;

	  case 'r':
	    if (VIsual_active)
		{
			c = 'c';
			goto dooperator;
		}
		CHECKCLEAROPQ;
		ptr = ml_get_cursor();
			/* special key or not enough characters to replace */
		if (nchar >= 0x100 || STRLEN(ptr) < (unsigned)Prenum1)
		{
			CLEAROPBEEP;
			break;
		}
		/*
		 * Replacing with a line break or tab is done by edit(), because it
         * is complicated.
		 * Other characters are done below to avoid problems with things like
		 * CTRL-V 048 (for edit() this would be R CTRL-V 0 ESC).
		 */
		if (nchar == '\r' || nchar == '\n' || nchar == '\t')
		{
			prep_redo(Prenum1, NUL, 'r', nchar, NUL);
			stuffnumReadbuff(Prenum1);
			stuffcharReadbuff('R');
			stuffcharReadbuff(nchar);
			stuffcharReadbuff(ESC);
			break;
		}

		if (nchar == Ctrl('V'))				/* get another character */
		{
			c = Ctrl('V');
			nchar = get_literal();
		}
		else
			c = NUL;
		prep_redo(Prenum1, NUL, 'r', c, nchar);
		if (u_save_cursor() == FAIL)		/* save line for undo */
			break;
		curwin->w_cursor.col--;
		while (Prenum1--)					/* replace the characters */
		{
			/*
			 * Get ptr again, because u_save and/or showmatch() will have
			 * released the line.  At the same time we let know that the line
			 * will be changed.
			 */
			ptr = ml_get_buf(curbuf, curwin->w_cursor.lnum, TRUE);
			ptr[++curwin->w_cursor.col] = nchar;
			if (p_sm && (nchar == ')' || nchar == '}' || nchar == ']'))
				showmatch();
		}
		curwin->w_set_curswant = TRUE;
		CHANGED;
		updateline();
		modified = TRUE;
		set_last_insert(nchar);
		break;

	  case 'J':
	    if (VIsual_active)		/* join the visual lines */
			goto dooperator;
		CHECKCLEAROP;
		if (Prenum <= 1)
			Prenum = 2; 			/* default for join is two lines! */
		if (curwin->w_cursor.lnum + Prenum - 1 > curbuf->b_ml.ml_line_count)
		{
			CLEAROPBEEP;			/* beyond last line */
			break;
		}

		prep_redo(Prenum, NUL, 'J', NUL, NUL);
		dodojoin(Prenum, TRUE, TRUE);
		modified = TRUE;
		break;

	  case 'P':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case 'p':
		CHECKCLEAROPQ;
		prep_redo(Prenum, NUL, c, NUL, NUL);
		doput(dir, Prenum1, FALSE);
		modified = TRUE;
		break;

	  case Ctrl('A'):			/* add to number */
	  case Ctrl('X'):			/* subtract from number */
		CHECKCLEAROPQ;
		if (doaddsub((int)c, Prenum1) == OK)
			prep_redo(Prenum1, NUL, c, NUL, NUL);
		modified = TRUE;
		break;

/*
 * 6: Inserts
 */
	  case 'A':
		curwin->w_set_curswant = TRUE;
		while (oneright() == OK)
				;
		/* FALLTHROUGH */

	  case 'a':
		CHECKCLEAROPQ;
		/* Works just like an 'i'nsert on the next character. */
		if (u_save_cursor() == OK)
		{
			if (!lineempty(curwin->w_cursor.lnum))
				inc_cursor();
			command_busy = startinsert(c, FALSE, Prenum1);
			modified = TRUE;
		}
		break;

	  case 'I':
		beginline(TRUE);
		/* FALLTHROUGH */

	  case 'i':
	  case K_INS:
		CHECKCLEAROPQ;
		if (u_save_cursor() == OK)
		{
			command_busy = startinsert(c, FALSE, Prenum1);
			modified = TRUE;
		}
		break;

	  case 'o':
	  	if (VIsual_active)	/* switch start and end of visual */
		{
			Prenum = VIsual.lnum;
			VIsual.lnum = curwin->w_cursor.lnum;
			curwin->w_cursor.lnum = Prenum;
			n = VIsual.col;
			VIsual.col = curwin->w_cursor.col;
			curwin->w_cursor.col = (int)n;
			curwin->w_set_curswant = TRUE;
			break;
		}
		CHECKCLEAROP;
		if (curbuf->b_p_fo != NULL &&
						STRCHR(curbuf->b_p_fo, FO_OPEN_COMS) != NULL)
			fo_do_comments = TRUE;
		if (u_save(curwin->w_cursor.lnum,
								(linenr_t)(curwin->w_cursor.lnum + 1)) == OK &&
						Opencmd(FORWARD, TRUE))
		{
			command_busy = startinsert('o', TRUE, Prenum1);
			modified = TRUE;
		}
		fo_do_comments = FALSE;
		break;

	  case 'O':
		CHECKCLEAROPQ;
		if (curbuf->b_p_fo != NULL &&
						STRCHR(curbuf->b_p_fo, FO_OPEN_COMS) != NULL)
			fo_do_comments = TRUE;
		if (u_save((linenr_t)(curwin->w_cursor.lnum - 1),
						curwin->w_cursor.lnum) == OK && Opencmd(BACKWARD, TRUE))
		{
			command_busy = startinsert('O', TRUE, Prenum1);
			modified = TRUE;
		}
		fo_do_comments = FALSE;
		break;

	  case 'R':
	    if (VIsual_active)
		{
			c = 'c';
			Visual_mode = 'V';
			goto dooperator;
		}
		CHECKCLEAROPQ;
		if (u_save_cursor() == OK)
		{
			command_busy = startinsert('R', FALSE, Prenum1);
			modified = TRUE;
		}
		break;

/*
 * 7: Operators
 */
	  case '~': 		/* swap case */
	  /*
	   * if tilde is not an operator and Visual is off: swap case
	   * of a single character
	   */
		if (!p_to && !VIsual_active &&
					operator != STRCHR(opchars, '~') - opchars + 1)
		{
			CHECKCLEAROPQ;
			if (lineempty(curwin->w_cursor.lnum))
			{
				CLEAROPBEEP;
				break;
			}
			prep_redo(Prenum, NUL, '~', NUL, NUL);

			if (u_save_cursor() == FAIL)
				break;

			for (; Prenum1 > 0; --Prenum1)
			{
				if (gchar_cursor() == NUL)
					break;
				swapchar(&curwin->w_cursor);
				inc_cursor();
			}

			curwin->w_set_curswant = TRUE;
			CHANGED;
			updateline();
			modified = TRUE;
			break;
		}
		/*FALLTHROUGH*/

	  case 'd':
	  case 'c':
	  case 'y':
	  case '>':
	  case '<':
	  case '!':
	  case '=':
	  case 'Q':
dooperator:
		n = STRCHR(opchars, c) - opchars + 1;
		if (n == operator)		/* double operator works on lines */
			goto lineop;
		CHECKCLEAROP;
		if (Prenum != 0)
			opnum = Prenum;
		curbuf->b_startop = curwin->w_cursor;
		operator = (int)n;
		break;

/*
 * 8: Abbreviations
 */

	 /* when Visual the next commands are operators */
	  case K_DEL:
	  		c = 'x';			/* DEL key behaves like 'x' */
	  case 'S':
	  case 'Y':
	  case 'D':
	  case 'C':
	  case 'x':
	  case 'X':
	  case 's':
	  	if (VIsual_active)
		{
			static char_u trans[] = "ScYyDdCcxdXdsc";

											/* uppercase means linewise */
			if (isupper(c) && Visual_mode != Ctrl('V'))
				Visual_mode = 'V';
			c = *(STRCHR(trans, c) + 1);
			goto dooperator;
		}

	  case '&':
		CHECKCLEAROPQ;
		if (Prenum)
			stuffnumReadbuff(Prenum);

		{
				static char_u *(ar[8]) = {(char_u *)"dl", (char_u *)"dh",
										  (char_u *)"d$", (char_u *)"c$",
										  (char_u *)"cl", (char_u *)"cc",
										  (char_u *)"yy", (char_u *)":s\r"};
				static char_u *str = (char_u *)"xXDCsSY&";

				stuffReadbuff(ar[(int)(STRCHR(str, c) - str)]);
		}
		break;

/*
 * 9: Marks
 */

	  case 'm':
		CHECKCLEAROP;
		if (setmark(nchar) == FAIL)
			CLEAROPBEEP;
		break;

	  case '\'':
		flag = TRUE;
		/* FALLTHROUGH */

	  case '`':
		pos = getmark(nchar, (operator == NOP));
		if (pos == (FPOS *)-1)	/* jumped to other file */
		{
			if (flag)
				beginline(TRUE);
			break;
		}

		if (pos != NULL)
			setpcmark();

cursormark:
		if (pos == NULL || pos->lnum == 0)
			CLEAROPBEEP;
		else
		{
			curwin->w_cursor = *pos;
			if (flag)
				beginline(TRUE);
		}
		mtype = flag ? MLINE : MCHAR;
		mincl = FALSE;		/* ignored if not MCHAR */
		curwin->w_set_curswant = TRUE;
		break;

	case Ctrl('O'):			/* goto older pcmark */
		Prenum1 = -Prenum1;
		/* FALLTHROUGH */

	case Ctrl('I'):			/* goto newer pcmark */
		CHECKCLEAROPQ;
		pos = movemark((int)Prenum1);
		if (pos == (FPOS *)-1)	/* jump to other file */
		{
			curwin->w_set_curswant = TRUE;
			break;
		}
		goto cursormark;

/*
 * 10. Buffer setting
 */
	  case '"':
		CHECKCLEAROP;
		if (nchar != NUL && is_yank_buffer(nchar, FALSE))
		{
			yankbuffer = nchar;
			opnum = Prenum;		/* remember count before '"' */
		}
		else
			CLEAROPBEEP;
		break;

/*
 * 11. Visual
 */
 	  case 'v':
	  case 'V':
	  case Ctrl('V'):
		CHECKCLEAROP;

			/* change Visual mode */
		if (VIsual_active)
		{
			if (c == 'v')					/* stop visual mode */
				VIsual_active = FALSE;
			else							/* toggle char/block mode */
			{								/*     or char/line mode */
				if (Visual_mode == c)
					Visual_mode = 'v';
				else
					Visual_mode = c;
			}
			update_curbuf(NOT_VALID);		/* update the inversion */
		}
			/* start Visual mode */
		else
		{
			start_visual_highlight();
			if (Prenum)						/* use previously selected part */
			{
				if (resel_Visual_mode == NUL)	/* there is none */
				{
					beep_flush();
					break;
				}
				VIsual = curwin->w_cursor;
				VIsual_active = TRUE;
				/*
				 * For V and ^V, we multiply the number of lines even if there
				 * was only one -- webb
				 */
				if (resel_Visual_mode != 'v' || resel_Visual_nlines > 1)
				{
					curwin->w_cursor.lnum += resel_Visual_nlines * Prenum - 1;
					if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
						curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
				}
				Visual_mode = resel_Visual_mode;
				if (Visual_mode == 'v')
				{
					if (resel_Visual_nlines <= 1)
						curwin->w_cursor.col += resel_Visual_col * Prenum - 1;
					else
						curwin->w_cursor.col = resel_Visual_col;
				}
				if (resel_Visual_col == MAXCOL)
				{
					curwin->w_curswant = MAXCOL;
					coladvance(MAXCOL);
				}
				else if (Visual_mode == Ctrl('V'))
				{
					curwin->w_curswant = curwin->w_virtcol +
											resel_Visual_col * Prenum - 1;
					coladvance((colnr_t)curwin->w_curswant);
				}
				else
					curwin->w_set_curswant = TRUE;
				curs_columns(TRUE);			/* recompute w_virtcol */
				update_curbuf(NOT_VALID);	/* show the inversion */
			}
			else
			{
				VIsual = curwin->w_cursor;
				VIsual_active = TRUE;
				Visual_mode = c;
				updateline();				/* start the inversion */
			}
		}
		break;

/*
 * 12. Suspend
 */

 	case Ctrl('Z'):
		CLEAROP;
		VIsual_active = FALSE;				/* stop Visual */
		stuffReadbuff((char_u *)":st\r");	/* with autowrite */
		break;

/*
 * 13. Window commands
 */

 	case Ctrl('W'):
		CHECKCLEAROP;
		do_window(nchar, Prenum);			/* everything is in window.c */
		break;

/*
 *   14. extended commands (starting with 'g')
 */
 	case 'g':
		switch (nchar)
		{
			/*
			 * "gf": goto file, edit file under cursor
			 * "]f" and "[f": can also be used.
			 */
			case 'f':
gotofile:
				ptr = file_name_at_cursor();
					/* do autowrite if necessary */
				if (curbuf->b_changed && curbuf->b_nwindows <= 1 && !p_hid)
					autowrite(curbuf);
				if (ptr != NULL)
				{
					setpcmark();
					stuffReadbuff((char_u *) ":e ");
					stuffReadbuff(ptr);
					stuffReadbuff((char_u *) "\n");
					free(ptr);
				}
				else
					CLEAROP;
				break;

			/*
			 * "gs": goto sleep
			 */
			case 's':
				while (Prenum1-- && !got_int)
				{
					sleep(1);
					breakcheck();
				}
				break;

			/*
			 * Display the ascii value of the character under the cursor.
			 * It is displayed in decimal, hex, and octal. -- webb
			 */
			case 'a':
				do_ascii();
				break;

			/*
			 * Goto the first line in file.  With a count it goes to that line
			 * number like for G. -- webb
			 */
			case 'g':
				mtype = MLINE;
				setpcmark();
				if (Prenum > curbuf->b_ml.ml_line_count)
						curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
				else
						curwin->w_cursor.lnum = Prenum1;
				beginline(MAYBE);
				break;

			/*
			 * Operators to change the case of text:
			 *   g~		Toggle the case of the text.
			 *   gu		Change text to lower case.
			 *   gU		Change text to upper case.
			 *									--webb
			 */
			case '~':
			case 'u':
			case 'U':
#ifdef ADDED_BY_WEBB_COMPILE
				p_char = c;
#else
				pchar = c;
#endif /* ADDED_BY_WEBB_COMPILE */
				c = nchar;
				goto dooperator;

		/*
		 * "gd": Find first occurence of pattern under the cursor in the
		 *       current function
		 * "gD": idem, but in the current file.
		 */
			case 'd':
			case 'D':
				{
					int			len;
					char_u		*pat;
					FPOS		old_pos;
					int			t;

					if ((len = find_ident_under_cursor(&ptr, FALSE)) == 0 ||
										(pat = alloc(len + 5)) == NULL)
					{
						CLEAROPBEEP;
						break;
					}
					sprintf((char *)pat, isidchar_id(*ptr) ? "\\<%.*s\\>" :
															"%.*s", len, ptr);
					old_pos = curwin->w_cursor;
					n = p_ws;
					p_ws = FALSE;		/* don't wrap around end of file now */
					fo_do_comments = TRUE;
					/*
					 * Search back for the end of the previous function.
					 * If this fails, and with "gD", go to line 1.
					 * Search forward for the identifier, ignore comment lines.
					 */
					if (nchar == 'D' || !findpar(BACKWARD, 1, '}', FALSE))
					{
						setpcmark();		/* Set in findpar() otherwise */
						curwin->w_cursor.lnum = 1;
					}

					while ((t = searchit(&curwin->w_cursor, FORWARD, pat,
												1, FALSE, FALSE, 2)) == OK &&
							get_leader_len(ml_get(curwin->w_cursor.lnum)) &&
							old_pos.lnum > curwin->w_cursor.lnum)
						++curwin->w_cursor.lnum;
					if (t == FAIL || old_pos.lnum <= curwin->w_cursor.lnum)
					{
						CLEAROPBEEP;
						curwin->w_cursor = old_pos;
					}
					else
						curwin->w_set_curswant = TRUE;

					free(pat);
					p_ws = n;
					fo_do_comments = FALSE;
					break;
				}

			default:	CLEAROPBEEP;
						break;
		}
		break;

/*
 * 15. mouse click
 */
#if defined(UNIX) || defined(MSDOS)
	  case K_MOUSE:
		mouse_code = 0;					/* reset for next click */
			/* any button up and middle button down are ignored */
		if (nchar != 'L' && nchar != 'R' && nchar != 'P')
			break;
		{
			FPOS	start_visual;
			FPOS	end_visual;
			BUF		*save_buffer;
			int		diff;

			mtype = MCHAR;
			mincl = TRUE;
			/*
			 * in visual mode: left click = set end of visual
			 *                 right click = change start or end of visual
			 *                 position = change start or end of visual
			 * in normal mode: left click = set cursor position
			 *                 right click = start visual mode
			 *                 position = set cursor position
			 */
			start_visual.lnum = 0;
			if (VIsual_active && !curbuf->b_help)
			{
				if (nchar == 'L')
				{
					VIsual_active = FALSE;
					update_curbuf(NOT_VALID);
				}
				else if (nchar == 'R' || nchar == 'P')
				{
					/* remember the start and end of visual before moving the
					 * cursor */
					if (lt(curwin->w_cursor, VIsual))
					{
						start_visual = curwin->w_cursor;
						end_visual = VIsual;
					}
					else
					{
						start_visual = VIsual;
						end_visual = curwin->w_cursor;
					}
				}
			}
			else if (nchar == 'R' && !curbuf->b_help)
			{
				start_visual_highlight();
				VIsual = curwin->w_cursor;
				VIsual_active = TRUE;
			}
			save_buffer = curbuf;
			if (jumpto(mouse_row, mouse_col, nchar == 'P', nchar != 'P')
																	== FAIL)
			{
				CLEAROPBEEP;
				break;
			}
				/* In help file: Right button is jump to tag */
			if (nchar == 'R' && curbuf->b_help)
				stuffcharReadbuff(Ctrl(']'));

				/* when jumping to another buffer, stop visual mode */
			if (curbuf != save_buffer && VIsual_active)
			{
				VIsual_active = FALSE;
				update_curbuf(NOT_VALID);	/* delete the inversion */
			}
			else if (start_visual.lnum)		/* right click in visual mode */
			{
				/*
				 * If the click is before the start of visual, change the
				 * start.  If the click is after the end of visual, change the
				 * end.  If the click is insed the visual, change the the
				 * closest side.
				 */
				if (lt(curwin->w_cursor, start_visual))
					VIsual = end_visual;
				else if (lt(end_visual, curwin->w_cursor))
					VIsual = start_visual;
				else
				{
						/* in the same line, compare column number */
					if (end_visual.lnum == start_visual.lnum)
					{
						if (curwin->w_cursor.col - start_visual.col >
										end_visual.col - curwin->w_cursor.col)
							VIsual = start_visual;
						else
							VIsual = end_visual;
					}
						/* in different lines, compare line number */
					else
					{
						diff = (curwin->w_cursor.lnum - start_visual.lnum) -
									(end_visual.lnum - curwin->w_cursor.lnum);
						if (diff > 0)			/* closest to end */
							VIsual = start_visual;
						else if (diff < 0)		/* closest to start */
							VIsual = end_visual;
						else					/* in the middle line */
						{
							if (curwin->w_cursor.col < (start_visual.col +
														end_visual.col) / 2)
								VIsual = end_visual;
							else
								VIsual = start_visual;
						}
					}
				}
			}
		}
		break;
#endif

/*
 * 16. The end
 */
	  case ESC:
		/* Don't drop through and beep if we are canceling a command: */
		if (!VIsual_active && (operator != NOP ||
									opnum || Prenum || yankbuffer))
		{
			CLEAROP;					/* don't beep */
			break;
		}
	    if (VIsual_active)
		{
			VIsual_active = FALSE;		/* stop Visual */
			update_curbuf(NOT_VALID);
			CLEAROP;					/* don't beep */
			break;
		}
		/* ESC in normal mode: beep, but don't flush buffers */
		CLEAROP;
		beep();
		break;

	  default:					/* not a known command */
		CLEAROPBEEP;
		break;

	}	/* end of switch on command character */

/*
 * if we didn't start or finish an operator, reset yankbuffer, unless we
 * need it later.
 */
	if (!finish_op && !operator && (c >= 0x100 || strchr("\"DCYSsXx.", c) == NULL))
		yankbuffer = 0;

	/*
	 * If an operation is pending, handle it...
	 */
	if ((VIsual_active || finish_op) && operator != NOP)
	{
		if (operator != YANK && !VIsual_active)		/* can't redo yank */
		{
#ifdef ADDED_BY_WEBB_COMPILE
			prep_redo(Prenum, p_char, opchars[operator - 1], c, nchar);
#else
			prep_redo(Prenum, pchar, opchars[operator - 1], c, nchar);
#endif /* ADDED_BY_WEBB_COMPILE */
			if (c == '/' || c == '?')				/* was a search */
			{
				AppendToRedobuff(searchbuff);
				AppendToRedobuff(NL_STR);
			}
		}

		if (redo_Visual_busy)
		{
			curbuf->b_startop = curwin->w_cursor;
			curwin->w_cursor.lnum += redo_Visual_nlines - 1;
			Visual_mode = redo_Visual_mode;
			if (Visual_mode == 'v')
			{
				if (redo_Visual_nlines <= 1)
					curwin->w_cursor.col += redo_Visual_col - 1;
				else
					curwin->w_cursor.col = redo_Visual_col;
			}
			if (redo_Visual_col == MAXCOL)
			{
				curwin->w_curswant = MAXCOL;
				coladvance(MAXCOL);
			}
			Prenum = redo_Visual_Prenum;
			if (Prenum == 0)
				Prenum1 = 1L;
			else
				Prenum1 = Prenum;
		}
		else if (VIsual_active)
		{
			curbuf->b_startop = VIsual;
			if (Visual_mode == 'V')
				curbuf->b_startop.col = 0;
		}

		if (lt(curbuf->b_startop, curwin->w_cursor))
		{
			curbuf->b_endop = curwin->w_cursor;
			curwin->w_cursor = curbuf->b_startop;
		}
		else
		{
			curbuf->b_endop = curbuf->b_startop;
			curbuf->b_startop = curwin->w_cursor;
		}
		nlines = curbuf->b_endop.lnum - curbuf->b_startop.lnum + 1;

		if (VIsual_active || redo_Visual_busy)
		{
			if (Visual_mode == Ctrl('V'))		/* block mode */
			{
				colnr_t		start, end;

				block_mode = TRUE;
				getvcol(curwin, &(curbuf->b_startop),
												&startvcol, NULL, &endvcol);
				getvcol(curwin, &(curbuf->b_endop), &start, NULL, &end);
				if (start < startvcol)
					startvcol = start;
				if (end > endvcol)
					endvcol = end;

			/* if '$' was used, get endvcol from longest line */
				if (curwin->w_curswant == MAXCOL)
				{
					curwin->w_cursor.col = MAXCOL;
					endvcol = 0;
					for (curwin->w_cursor.lnum = curbuf->b_startop.lnum;
							curwin->w_cursor.lnum <= curbuf->b_endop.lnum;
													++curwin->w_cursor.lnum)
					{
						getvcol(curwin, &curwin->w_cursor, NULL, NULL, &end);
						if (end > endvcol)
							endvcol = end;
					}
					curwin->w_cursor = curbuf->b_startop;
				}
				else if (redo_Visual_busy)
					endvcol = startvcol + redo_Visual_col - 1;
				coladvance(startvcol);
			}

	/*
	 * prepare to reselect and redo Visual: this is based on the size
	 * of the Visual text
	 */
			resel_Visual_mode = Visual_mode;
			if (curwin->w_curswant == MAXCOL)
				resel_Visual_col = MAXCOL;
			else if (Visual_mode == Ctrl('V'))
				resel_Visual_col = endvcol - startvcol + 1;
			else if (nlines > 1)
				resel_Visual_col = curbuf->b_endop.col;
			else
				resel_Visual_col = curbuf->b_endop.col -
											curbuf->b_startop.col + 1;
			resel_Visual_nlines = nlines;
												/* can't redo yank and : */
			if (operator != YANK && operator != COLON)
			{
#ifdef ADDED_BY_WEBB_COMPILE
				prep_redo(0L, p_char, 'v', opchars[operator - 1], NUL);
#else
				prep_redo(0L, pchar, 'v', opchars[operator - 1], NUL);
#endif /* ADDED_BY_WEBB_COMPILE */
				redo_Visual_mode = resel_Visual_mode;
				redo_Visual_col = resel_Visual_col;
				redo_Visual_nlines = resel_Visual_nlines;
				redo_Visual_Prenum = Prenum;
			}

			/*
			 * Mincl defaults to TRUE.
			 * If endop is on a NUL (empty line) mincl becomes FALSE
			 * This makes "d}P" and "v}dP" work the same.
			 */
			mincl = TRUE;
			if (Visual_mode == 'V')
				mtype = MLINE;
			else
			{
				mtype = MCHAR;
				if (*ml_get_pos(&(curbuf->b_endop)) == NUL)
					mincl = FALSE;
			}

			redo_Visual_busy = FALSE;
			/*
			 * Switch Visual off now, so screen updating does
			 * not show inverted text when the screen is redrawn.
			 * With YANK and sometimes with COLON and FILTER there is no screen
			 * redraw, so it is done here to remove the inverted part.
			 */
			VIsual_active = FALSE;
			if (operator == YANK || operator == COLON || operator == FILTER)
				updateScreen(NOT_VALID);
		}
		else if (operator == LSHIFT || operator == RSHIFT)
			Prenum1 = 1L;		/* if not visual mode: shift one indent */

		curwin->w_set_curswant = 1;

			/* no_op is set when start and end are the same */
		no_op = (mtype == MCHAR && !mincl &&
								equal(curbuf->b_startop, curbuf->b_endop));

	/*
	 * If the end of an operator is in column one while mtype is MCHAR and mincl
	 * is FALSE, we put endop after the last character in the previous line.
	 * If startop is on or before the first non-blank in the line, the operator
	 * becomes linewise (strange, but that's the way vi does it).
	 */
		if (mtype == MCHAR && mincl == FALSE &&
										curbuf->b_endop.col == 0 && nlines > 1)
		{
			--nlines;
			--curbuf->b_endop.lnum;
			if (inindent())
				mtype = MLINE;
			else
			{
				curbuf->b_endop.col = STRLEN(ml_get(curbuf->b_endop.lnum));
				if (curbuf->b_endop.col)
				{
					--curbuf->b_endop.col;
					mincl = TRUE;
				}
			}
		}
		switch (operator)
		{
		  case LSHIFT:
		  case RSHIFT:
			doshift(operator, TRUE, (int)Prenum1);
			modified = TRUE;
			break;

		  case JOIN:
			if (nlines < 2)
				nlines = 2;
			if (curwin->w_cursor.lnum + nlines - 1 > curbuf->b_ml.ml_line_count)
				beep_flush();
			else
			{
				dodojoin(nlines, TRUE, TRUE);
				modified = TRUE;
			}
			break;

		  case DELETE:
			if (!no_op)
			{
				dodelete();
				modified = TRUE;
			}
			break;

		  case YANK:
			if (!no_op)
				(void)doyank(FALSE);
			break;

		  case CHANGE:
			command_busy = dochange();
			modified = TRUE;
			break;

		  case FILTER:
			bangredo = TRUE;		/* dobang() will put cmd in redo buffer */

		  case INDENT:
		  case COLON:
dofilter:
			sprintf((char *)IObuff, ":%ld,%ld",
						(long)curbuf->b_startop.lnum,
						(long)curbuf->b_endop.lnum);
			stuffReadbuff(IObuff);
			if (operator != COLON)
				stuffReadbuff((char_u *)"!");
			if (operator == INDENT)
			{
				stuffReadbuff(p_ep);
				stuffReadbuff((char_u *)"\n");
			}
			else if (operator == FORMAT)
			{
				stuffReadbuff(p_fp);
				stuffReadbuff((char_u *)"\n");
			}
				/*	docmdline() does the rest */
			break;

		  case TILDE:
		  case UPPER:
		  case LOWER:
			if (!no_op)
			{
				dotilde();
				modified = TRUE;
			}
			break;

		  case FORMAT:
			if (*p_fp != NUL)
				goto dofilter;		/* use external command */
			doformat();				/* use internal function */
			modified = TRUE;
			break;

		  default:
			CLEAROPBEEP;
		}
		/*
		 * if 'sol' not set, go back to old column for some commands
		 */
		if (!p_sol && mtype == MLINE && (operator == LSHIFT ||
								operator == RSHIFT || operator == DELETE))
			coladvance(curwin->w_curswant = old_col);
		operator = NOP;
#ifdef ADDED_BY_WEBB_COMPILE
		p_char = NUL;
#else
		pchar = NUL;
#endif /* ADDED_BY_WEBB_COMPILE */
		VIsual_active = FALSE;
		block_mode = FALSE;
		yankbuffer = 0;
	}

normal_end:
	if (operator == NOP && yankbuffer == 0)
		clear_showcmd();

	if (restart_edit && operator == NOP && !VIsual_active
					&& !command_busy && stuff_empty() && yankbuffer == 0)
	{
		(void)startinsert(restart_edit, FALSE, 1L);
		modified = TRUE;
	}

	checkpcmark();			/* check if we moved since setting pcmark */
	free(searchbuff);

/*
 * TEMPORARY: update the other windows for the current buffer if modified
 */
	if (modified)
	{
		WIN		*wp;

        for (wp = firstwin; wp; wp = wp->w_next)
			if (wp != curwin && wp->w_buffer == curbuf)
			{
				cursor_off();
				wp->w_redr_type = NOT_VALID;
				win_update(wp);
			}
	}
}

/*
 * start highlighting for visual mode
 */
	void
start_visual_highlight()
{
	static int		didwarn = FALSE;		/* warned for broken inversion */

	if (!didwarn && set_highlight('v') == FAIL)/* cannot highlight */
	{
		EMSG("Warning: terminal cannot highlight");
		didwarn = TRUE;
	}
}

/*
 * Find the identifier under or to the right of the cursor.  If none is
 * found and try_string is TRUE, then find any non-white string.  The length
 * of the string is returned, or zero if no string is found.  If a string is
 * found, a pointer to the string is put in *string, but note that the caller
 * must use the length returned as this string may not be NUL terminated.
 */
	int
find_ident_under_cursor(string, try_string)
	char_u	**string;
	int		try_string;
{
	char_u	*ptr;
	int		col;
	int		i;

	/*
	 * if i == 0: try to find an identifier
	 * if i == 1: try to find any string
	 */
	ptr = ml_get(curwin->w_cursor.lnum);
	for (i = 0;	i < 2; ++i)
	{
		/*
		 * skip to start of identifier/string
		 */
		col = curwin->w_cursor.col;
		while (ptr[col] != NUL &&
					(i == 0 ? !isidchar_id(ptr[col]) : iswhite(ptr[col])))
			++col;

		/*
		 * Back up to start of identifier/string. This doesn't match the
		 * real vi but I like it a little better and it shouldn't bother
		 * anyone.
		 */
		while (col > 0 && (i == 0 ? isidchar_id(ptr[col - 1]) :
					(!iswhite(ptr[col - 1]) && !isidchar_id(ptr[col - 1]))))
			--col;

		/*
		 * if we don't want just any old string, or we've found an identifier,
		 * stop searching.
		 */
		if (!try_string || isidchar_id(ptr[col]))
			break;
	}
	/*
	 * didn't find an identifier or string
	 */
	if (ptr[col] == NUL || (!isidchar_id(ptr[col]) && i == 0))
	{
		if (try_string)
			EMSG("No identifier or string under cursor");
		else
			EMSG("No identifier under cursor");
		return 0;
	}
	ptr += col;
	*string = ptr;
	col = 0;
	while (i == 0 ? isidchar_id(*ptr) : (*ptr != NUL && !iswhite(*ptr)))
	{
		++ptr;
		++col;
	}
	return col;
}

	static void
prep_redo(num, prechar, cmd, c, nchar)
	long 	num;
	int		prechar;
	int		cmd;
	int		c;
	int		nchar;
{
	ResetRedobuff();
	if (yankbuffer != 0)	/* yank from specified buffer */
	{
		AppendCharToRedobuff('\"');
		AppendCharToRedobuff(yankbuffer);
	}
	if (num)
		AppendNumberToRedobuff(num);
	if (prechar != NUL)
		AppendCharToRedobuff(prechar);
	AppendCharToRedobuff(cmd);
	if (c != NUL)
		AppendCharToRedobuff(c);
	if (nchar != NUL)
		AppendCharToRedobuff(nchar);
}

/*
 * check for operator active
 *
 * return TRUE if operator was active
 */
	static int
checkclearop()
{
	if (operator == NOP)
		return (FALSE);
	clearopbeep();
	return (TRUE);
}

/*
 * check for operator or Visual active
 *
 * return TRUE if operator was active
 */
	static int
checkclearopq()
{
	if (operator == NOP && !VIsual_active)
		return (FALSE);
	clearopbeep();
	return (TRUE);
}

	static void
clearop()
{
	operator = NOP;
	yankbuffer = 0;
#ifdef ADDED_BY_WEBB_COMPILE
	p_char = NUL;
#else
	pchar = NUL;
#endif /* ADDED_BY_WEBB_COMPILE */
}

	static void
clearopbeep()
{
	CLEAROP;
	beep_flush();
}

/*
 * Routines for displaying a partly typed command
 */
static char_u	showcmd_buf[SHOWCMD_COLS + 1];
static char_u	old_showcmd_buf[SHOWCMD_COLS + 1];	/* For push_showcmd() */
static int		is_showcmd_clear = TRUE;

	void
clear_showcmd()
{
	if (!p_sc)
		return;

	showcmd_buf[0] = NUL;

	/*
	 * Use is_showcmd_clear here instead of char_avail(), because after ^R in
	 * insert mode, there are always chars available, but we still want to
	 * clear the "^R" from the showcmd.
	 */
	if (!(!is_showcmd_clear || KeyTyped))
		return;

	cursor_off();
	msg_pos((int)Rows - 1, sc_col);
	msg_outstr((char_u *)"          ");		/* SHOWCMD_COLS spaces */
	is_showcmd_clear = TRUE;
	setcursor();
}

/*
 * Add 'c' to string of shown command chars.
 * Return TRUE if setcursor() has been called.
 */
	int
add_to_showcmd(c)
	int c;
{
	char_u	*p;
	int		old_len;
	int		extra_len;
	int		overflow;

	if (!p_sc)
		return FALSE;

	p = transchar(c);
	old_len = STRLEN(showcmd_buf);
	extra_len = STRLEN(p);
	overflow = old_len + extra_len - SHOWCMD_COLS;
	if (overflow > 0)
		STRCPY(showcmd_buf, showcmd_buf + overflow);
	STRCAT(showcmd_buf, p);

	if (!(!char_avail() || KeyTyped))
		return FALSE;

	cursor_off();
	msg_pos((int)Rows - 1, sc_col);
	msg_outstr(showcmd_buf);
	is_showcmd_clear = FALSE;

	setcursor();			/* put cursor back where it belongs */

	return TRUE;
}

	void
push_showcmd()
{
	if (p_sc)
		STRCPY(old_showcmd_buf, showcmd_buf);
}

	void
pop_showcmd()
{
	if (!p_sc)
		return;

	STRCPY(showcmd_buf, old_showcmd_buf);

	if (!(!char_avail() || KeyTyped))
		return;

	cursor_off();
	msg_pos((int)Rows - 1, sc_col);
	msg_outstr((char_u *)"          ");		/* SHOWCMD_COLS spaces */
	msg_pos((int)Rows - 1, sc_col);
	msg_outstr(showcmd_buf);

	setcursor();
}
