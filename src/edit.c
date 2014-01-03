/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * edit.c: functions for insert mode
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"
#include "ops.h"	/* for operator */

#define CTRL_X_WANT_IDENT		0x100

#define CTRL_X_NOT_DEFINED_YET	(1)
#define CTRL_X_SCROLL			(2)
#define CTRL_X_WHOLE_LINE		(3)
#define CTRL_X_FILES			(4)
#define CTRL_X_TAGS				(5 + CTRL_X_WANT_IDENT)
#define CTRL_X_PATH_PATTERNS	(6 + CTRL_X_WANT_IDENT)
#define CTRL_X_PATH_DEFINES		(7 + CTRL_X_WANT_IDENT)
#define CTRL_X_FINISHED			(8)

struct Completion
{
	char_u *str;
	struct Completion *next;
	struct Completion *prev;
};

struct Completion *first_match = NULL;
struct Completion *curr_match = NULL;

static int add_new_completion __ARGS((char_u *str, int len, int dir));
static void make_cyclic __ARGS((void));
static void complete_dictionaries __ARGS((char_u *, int));
static void free_completions __ARGS((void));
static int count_completions __ARGS((void));

static void insert_special __ARGS((int));
static void start_arrow __ARGS((void));
static void stop_arrow __ARGS((void));
static void stop_insert __ARGS((void));
static int echeck_abbr __ARGS((int));
static char_u *get_id_option __ARGS((void));

static char_u	*last_insert = NULL;
							/* the text of the previous insert */
static int		last_insert_skip;
							/* number of chars in front of the previous insert */
static int		new_insert_skip;
							/* number of chars in front of the current insert */
static int		ctrl_x_mode = 0;	/* Which Ctrl-X mode are we in? */
static char_u	*original_text = NULL;
							/* Original text typed before completion */

/*
 * edit() returns TRUE if it returns because of a CTRL-O command
 */
	int
edit(count)
	long count;
{
	int			 c;
	int			 cc;
	char_u		*ptr;
	linenr_t	 lnum;
	int			 vcol;
	int			 last_vcol;
	int 		 temp = 0;
	int			 mode;
	int			 lastc = 0;
	int			 revins;					/* reverse insert mode */
	colnr_t		 mincol;
	static linenr_t o_lnum = 0;
	static int	 o_eol = FALSE;
	FPOS		 first_match_pos;
	FPOS		 last_match_pos;
	FPOS		*complete_pos;
	char_u		*complete_pat = NULL;
	char_u		*tmp_ptr;
	char_u		*m = NULL;					/* Message about completion */
	char_u		*quick_m;					/* Message without sleep */
	char_u		*idp;
	int			 started_completion = FALSE;
	int			 complete_col = 0;			/* init for gcc */
	int			 complete_direction;
	int			 done_dir = 0;				/* Found all matches in this
											 * direction */
	int			 i;
	int			 num_matches;
	char_u		**matches;
	regexp		*prog;
	int			 save_sm = -1;				/* init for gcc */

	clear_showcmd();
	revins = (State == INSERT && p_ri);	/* there is no reverse replace mode */

	/*
	 * When CTRL-O . is used to repeat an insert, we get here with
	 * restart_edit TRUE, but something in the stuff buffer
	 */
	if (restart_edit && stuff_empty())
	{
		arrow_used = TRUE;
		restart_edit = 0;
		/*
		 * If the cursor was after the end-of-line before the CTRL-O
		 * and it is now at the end-of-line, put it after the end-of-line
		 * (this is not correct in very rare cases).
		 * Also do this if curswant is set to the end of the line.
		 */
		if (((o_eol && curwin->w_cursor.lnum == o_lnum) ||
									(curwin->w_curswant == MAXCOL)) &&
				*((ptr = ml_get(curwin->w_cursor.lnum)) + curwin->w_cursor.col) != NUL &&
				*(ptr + curwin->w_cursor.col + 1) == NUL)
			++curwin->w_cursor.col;
	}
	else
	{
		arrow_used = FALSE;
		o_eol = FALSE;
	}

#ifdef DIGRAPHS
	dodigraph(-1);					/* clear digraphs */
#endif

/*
 * Get the current length of the redo buffer, those characters have to be
 * skipped if we want to get to the inserted characters.
 */
	ptr = get_inserted();
	new_insert_skip = STRLEN(ptr);
	free(ptr);

	old_indent = 0;

	for (;;)
	{
		if (arrow_used)		/* don't repeat insert when arrow key used */
			count = 0;

		if (!arrow_used)
			curwin->w_set_curswant = TRUE;	/* set curwin->w_curswant for next K_DARROW or K_UARROW */
		cursupdate();		/* Figure out where the cursor is based on curwin->w_cursor. */
		showruler(0);
		setcursor();

		c = vgetc();
		if (c == Ctrl('C'))
			got_int = FALSE;

		if (ctrl_x_mode == CTRL_X_NOT_DEFINED_YET)
		{
			/* We have just entered ctrl-x mode and aren't quite sure which
			 * ctrl-x mode it will be yet.  Now we decide -- webb
			 */
			switch (c)
			{
				case Ctrl('E'):
				case Ctrl('Y'):
					ctrl_x_mode = CTRL_X_SCROLL;
					edit_submode = (char_u *)"Scroll (^E/^Y)";
					break;
				case Ctrl('L'):
					ctrl_x_mode = CTRL_X_WHOLE_LINE;
					edit_submode = (char_u *)"Whole line completion (^L/^N/^P)";
					break;
				case Ctrl('F'):
					ctrl_x_mode = CTRL_X_FILES;
					edit_submode = (char_u *)"File name completion (^F/^N/^P)";
					break;
				case Ctrl(']'):
					ctrl_x_mode = CTRL_X_TAGS;
					edit_submode = (char_u *)"Tag completion (^]/^N/^P)";
					break;
				case Ctrl('I'):
					ctrl_x_mode = CTRL_X_PATH_PATTERNS;
					edit_submode = (char_u *)"Path pattern completion (^N/^P)";
					break;
				case Ctrl('D'):
					ctrl_x_mode = CTRL_X_PATH_DEFINES;
					edit_submode = (char_u *)"Definition completion (^D/^N/^P)";
					break;
				default:
					ctrl_x_mode = 0;
					break;
			}
			showmode();
		}
		else if (ctrl_x_mode)
		{
			/* We we're already in ctrl-x mode, do we stay in it? */
			if (!is_ctrl_x_key(c))
			{
				if (ctrl_x_mode == CTRL_X_SCROLL)
					ctrl_x_mode = 0;
				else
					ctrl_x_mode = CTRL_X_FINISHED;
				edit_submode = NULL;
			}
			showmode();
		}
		if (started_completion || ctrl_x_mode == CTRL_X_FINISHED)
		{
			/* Show error message from attempted keyword completion (probably
			 * 'Pattern not found') until another key is hit, then go back to
			 * showing what mode we are in.
			 */
			showmode();
			if ((ctrl_x_mode == 0 && c != Ctrl('N') && c != Ctrl('P')) ||
												ctrl_x_mode == CTRL_X_FINISHED)
			{
				/* Get here when we have finished typing a sequence of ^N and
				 * ^P or other completion characters in CTRL-X mode. Free up
				 * memory that was used, and make sure we can redo the insert
				 * -- webb.
				 */
				if (curr_match != NULL)
				{
					/*
					 * If any of the original typed text has been changed,
					 * eg when ignorecase is set, we must add back-spaces to
					 * the redo buffer.  We add as few as necessary to delete
					 * just the part of the original text that has changed
					 * -- webb
					 */
					ptr = curr_match->str;
					tmp_ptr = original_text;
					while (*tmp_ptr && *tmp_ptr == *ptr)
					{
						++tmp_ptr;
						++ptr;
					}
					for (temp = 0; tmp_ptr[temp]; ++temp)
						AppendCharToRedobuff(K_BS);
					if (*ptr)
						AppendToRedobuff(ptr);
				}
				/* Break line if it's too long */
				lnum = curwin->w_cursor.lnum;
				insertchar(NUL, FALSE, -1);
				if (lnum != curwin->w_cursor.lnum)
					updateScreen(CURSUPD);

				free(complete_pat);
				free(original_text);
				complete_pat = NULL;
				free_completions();
				started_completion = FALSE;
				ctrl_x_mode = 0;
				p_sm = save_sm;
				if (edit_submode != NULL)
				{
					edit_submode = NULL;
					showmode();
				}
			}
		}
		if (c != Ctrl('D'))			/* remember to detect ^^D and 0^D */
			lastc = c;

#ifdef DIGRAPHS
		c = dodigraph(c);
#endif /* DIGRAPHS */

		if (c == Ctrl('V'))
		{
			screen_start();
			screen_outchar('^', curwin->w_winpos + curwin->w_row, curwin->w_col);
			AppendToRedobuff((char_u *)"\026");	/* CTRL-V */
			cursupdate();

			if (!add_to_showcmd(c))
				setcursor();

			c = get_literal();
			clear_showcmd();
			insert_special(c);
			continue;
		}
		switch (c)		/* handle character in insert mode */
		{
			  case K_INS:			/* toggle insert/replace mode */
			    if (State == REPLACE)
					State = INSERT;
				else
					State = REPLACE;
				AppendCharToRedobuff(K_INS);
				showmode();
				break;

			  case Ctrl('X'):		/* Enter ctrl-x mode */
				/* We're not sure which ctrl-x mode it will be yet */
				ctrl_x_mode = CTRL_X_NOT_DEFINED_YET;
				MSG("^X mode (^E/^Y/^L/^]/^F/^I/^D)");
				break;

			  case Ctrl('O'):		/* execute one command */
			    if (echeck_abbr(Ctrl('O') + 0x200))
					break;
			  	count = 0;
				if (State == INSERT)
					restart_edit = 'I';
				else
					restart_edit = 'R';
				o_lnum = curwin->w_cursor.lnum;
				o_eol = (gchar_cursor() == NUL);
				goto doESCkey;

			  case ESC: 			/* an escape ends input mode */
			    if (echeck_abbr(ESC + 0x200))
					break;
				/*FALLTHROUGH*/

			  case Ctrl('C'):
doESCkey:
				temp = curwin->w_cursor.col;
				if (!arrow_used)
				{
					AppendToRedobuff(ESC_STR);

					if (--count > 0)		/* repeat what was typed */
					{
							(void)start_redo_ins();
							continue;
					}
					stop_insert();
				}
				/* When an autoindent was removed, curswant stays after the indent */
				if (!restart_edit && temp == curwin->w_cursor.col)
					curwin->w_set_curswant = TRUE;

				/*
				 * The cursor should end up on the last inserted character.
				 */
				if (curwin->w_cursor.col != 0 && (!restart_edit || gchar_cursor() == NUL) && !revins)
					--curwin->w_cursor.col;
				if (State == REPLACE)
					replace_flush();	/* free replace stack */
				State = NORMAL;
					/* inchar() may have deleted the "INSERT" message */
				if (Recording)
					showmode();
				else if (p_smd)
					MSG("");
				old_indent = 0;
				return (c == Ctrl('O'));

			  	/*
				 * Insert the previously inserted text.
				 * Last_insert actually is a copy of the redo buffer, so we
				 * first have to remove the command.
				 * For ^@ the trailing ESC will end the insert.
				 */
			  case K_ZERO:
			  case Ctrl('A'):
				stuff_inserted(NUL, 1L, (c == Ctrl('A')));
				break;

			  	/*
				 * insert the contents of a register
				 */
			  case Ctrl('R'):
				add_to_showcmd(c);
			  	if (insertbuf(vgetc()) == FAIL)
					beep_flush();
				clear_showcmd();
				break;

			  case Ctrl('B'):			/* toggle reverse insert mode */
			  	p_ri = !p_ri;
				revins = (State == INSERT && p_ri);
				showmode();
				break;

				/*
				 * If the cursor is on an indent, ^T/^D insert/delete one
				 * shiftwidth. Otherwise ^T/^D behave like a TAB/backspace.
				 * This isn't completely compatible with vi, but the
				 * difference isn't very noticeable and now you can
				 * mix ^D/backspace and ^T/TAB without thinking about which one
				 * must be used.
				 */
			  case Ctrl('D'): 		/* make indent one shiftwidth smaller */
				if (ctrl_x_mode == CTRL_X_PATH_DEFINES)
					goto docomplete;
				/* FALLTHROUGH */
			  case Ctrl('T'):		/* make indent one shiftwidth greater */
				stop_arrow();
				AppendCharToRedobuff(c);
				if ((lastc == '0' || lastc == '^') && curwin->w_cursor.col)
				{
					--curwin->w_cursor.col;
					(void)delchar(FALSE);			/* delete the '^' or '0' */
					if (lastc == '^')
						old_indent = get_indent();	/* remember current indent */

						/* determine offset from first non-blank */
					temp = curwin->w_cursor.col;
					beginline(TRUE);
					i = curwin->w_cursor.col;
					temp -= curwin->w_cursor.col;
					set_indent(0, TRUE);	/* remove all indent */
				}
				else
				{
ins_indent:
						/* determine offset from first non-blank */
					temp = curwin->w_cursor.col;
					vcol = curwin->w_virtcol;
					beginline(TRUE);
					i = curwin->w_cursor.col;
					temp -= curwin->w_cursor.col;
					if (temp < 0)		/* Cursor in indented part */
						vcol = get_indent() - vcol;

					shift_line(c == Ctrl('D'), TRUE, 1);

						/* try to put cursor on same character */
					if (temp < 0)		/* Cursor in indented part */
					{
						temp = get_indent() - vcol;
						curwin->w_virtcol = (temp < 0) ? 0 : temp;
						vcol = last_vcol = 0;
						temp = -1;
						ptr = ml_get(curwin->w_cursor.lnum);
						while (vcol <= curwin->w_virtcol)
						{
							last_vcol = vcol;
							vcol += chartabsize(ptr[++temp], vcol);
						}
						vcol = last_vcol;
						if (vcol != curwin->w_virtcol)
						{
							curwin->w_cursor.col = temp;
							i = curwin->w_virtcol - vcol;
							ptr = alloc(i + 1);
							if (ptr != NULL)
							{
								temp += i;
								ptr[i] = NUL;
								while (--i >= 0)
									ptr[i] = ' ';
								insstr(ptr);
								free(ptr);
							}
						}
					}
					else
						temp += curwin->w_cursor.col;
				}
				/* May have to adjust the start of the insert. -- webb */
				if (curwin->w_cursor.lnum == Insstart.lnum && Insstart.col != 0)
					Insstart.col += curwin->w_cursor.col - i;
				if (temp <= 0)
					curwin->w_cursor.col = 0;
				else
					curwin->w_cursor.col = temp;
				did_ai = FALSE;
				did_si = FALSE;
				can_si = FALSE;
				can_si_back = FALSE;
		  		goto redraw;

			  case K_DEL:
				stop_arrow();
			  	if (gchar_cursor() == NUL)		/* delete newline */
				{
					temp = curwin->w_cursor.col;
					if (!p_bs ||				/* only if 'bs' set */
						u_save((linenr_t)(curwin->w_cursor.lnum - 1),
							(linenr_t)(curwin->w_cursor.lnum + 2)) == FAIL ||
								dojoin(FALSE, TRUE) == FAIL)
						beep_flush();
					else
						curwin->w_cursor.col = temp;
				}
				else if (delchar(FALSE) == FAIL)/* delete char under cursor */
					beep_flush();
				did_ai = FALSE;
				did_si = FALSE;
				can_si = FALSE;
				can_si_back = FALSE;
				AppendCharToRedobuff(c);
				goto redraw;

			  case K_BS:
				mode = 0;
dodel:
				/* can't delete anything in an empty file */
				/* can't backup past first character in buffer */
				/* can't backup past starting point unless 'backspace' > 1 */
				/* can backup to a previous line if 'backspace' == 0 */
				if (bufempty() || (!revins &&
						((curwin->w_cursor.lnum == 1 &&
									curwin->w_cursor.col <= 0) ||
						(p_bs < 2 && (arrow_used ||
							(curwin->w_cursor.lnum == Insstart.lnum &&
							curwin->w_cursor.col <= Insstart.col) ||
							(curwin->w_cursor.col <= 0 && p_bs == 0))))))
				{
					beep_flush();
					goto redraw;
				}

				stop_arrow();
				if (revins)			/* put cursor after last inserted char */
					inc_cursor();
				if (curwin->w_cursor.col <= 0)		/* delete newline! */
				{
					lnum = Insstart.lnum;
					if (curwin->w_cursor.lnum == Insstart.lnum || revins)
					{
						if (u_save((linenr_t)(curwin->w_cursor.lnum - 2),
								(linenr_t)(curwin->w_cursor.lnum + 1)) == FAIL)
							goto redraw;
						--Insstart.lnum;
						Insstart.col = 0;
					}
					/*
					 * In replace mode:
					 * cc < 0: NL was inserted, delete it
					 * cc >= 0: NL was replaced, put original characters back
					 */
					cc = -1;
					if (State == REPLACE)
						cc = replace_pop();
				/* in replace mode, in the line we started replacing, we
														only move the cursor */
					if (State != REPLACE || curwin->w_cursor.lnum > lnum)
					{
						temp = gchar_cursor();		/* remember current char */
						--curwin->w_cursor.lnum;
						(void)dojoin(FALSE, TRUE);
						if (temp == NUL && gchar_cursor() != NUL)
							++curwin->w_cursor.col;
						/*
						 * in REPLACE mode we have to put back the text that
						 * was replace by the NL. On the replace stack is
						 * first a NUL-terminated sequence of characters that
						 * were deleted and then the character that NL
						 * replaced.
						 */
						if (State == REPLACE)
						{
							/*
							 * Do the next inschar() in NORMAL state, to
							 * prevent inschar() from replacing characters and
							 * avoiding showmatch().
							 */
							State = NORMAL;
							/*
							 * restore blanks deleted after cursor
							 */
							while (cc > 0)
							{
								temp = curwin->w_cursor.col;
								inschar(cc);
								curwin->w_cursor.col = temp;
								cc = replace_pop();
							}
							cc = replace_pop();
							if (cc > 0)
							{
								inschar(cc);
								dec_cursor();
							}
							State = REPLACE;
						}
					}
					else
						dec_cursor();
					did_ai = FALSE;
				}
				else
				{
					if (revins)			/* put cursor on last inserted char */
						dec_cursor();
					mincol = 0;
					if (mode == 3 && !revins && curbuf->b_p_ai)	/* keep indent */
					{
						temp = curwin->w_cursor.col;
						beginline(TRUE);
						if (curwin->w_cursor.col < temp)
							mincol = curwin->w_cursor.col;
						curwin->w_cursor.col = temp;
					}

					/* delete upto starting point, start of line or previous
					 * word */
					do
					{
						if (!revins)	/* put cursor on char to be deleted */
							dec_cursor();

								/* start of word? */
						if (mode == 1 && !isspace(gchar_cursor()))
						{
							mode = 2;
							temp = isidchar_id(gchar_cursor());
						}
								/* end of word? */
						else if (mode == 2 && (isspace(cc = gchar_cursor()) ||
												isidchar_id(cc) != temp))
						{
							if (!revins)
								inc_cursor();
							else if (State == REPLACE)
								dec_cursor();
							break;
						}
						if (State == REPLACE)
						{
							/*
							 * cc < 0: replace stack empty, just move cursor
							 * cc == 0: character was inserted, delete it
							 * cc > 0: character was replace, put original back
							 */
							cc = replace_pop();
							if (cc > 0)
								pchar_cursor(cc);
							else if (cc == 0)
								(void)delchar(FALSE);
						}
						else  /* State != REPLACE */
						{
							(void)delchar(FALSE);
							if (revins && gchar_cursor() == NUL)
								break;
						}
						if (mode == 0)		/* just a single backspace */
							break;
					} while (revins || (curwin->w_cursor.col > mincol &&
							(curwin->w_cursor.lnum != Insstart.lnum ||
							curwin->w_cursor.col != Insstart.col)));
				}
				did_si = FALSE;
				can_si = FALSE;
				can_si_back = FALSE;
				if (curwin->w_cursor.col <= 1)
					did_ai = FALSE;
				/*
				 * It's a little strange to put backspaces into the redo
				 * buffer, but it makes auto-indent a lot easier to deal
				 * with.
				 */
				AppendCharToRedobuff(c);
redraw:
				cursupdate();
				updateline();
				break;

			  case Ctrl('W'):		/* delete word before cursor */
			  	mode = 1;
			  	goto dodel;

			  case Ctrl('U'):		/* delete inserted text in current line */
				mode = 3;
			  	goto dodel;

#if defined(UNIX) || defined(MSDOS)	/* only for xterm and msdos at the moment */
			  case K_MOUSE:
				(void)vgetc();		/* get and ignore button specifier */
				mouse_code = 0;		/* reset for next click */
				if (jumpto(mouse_row, mouse_col, FALSE, TRUE) == OK)
					start_arrow();
				break;
#endif

			  case K_LARROW:
			  	if (oneleft() == OK)
					start_arrow();
				/*
				 * if 'whichwrap' set for cursor in insert mode may go to
				 * previous line
				 */
				else if ((p_ww & 16) && curwin->w_cursor.lnum > 1)
				{
					start_arrow();
					--(curwin->w_cursor.lnum);
					coladvance(MAXCOL);
					curwin->w_set_curswant = TRUE;	/* so we stay at the end */
				}
				else
					beep_flush();
				break;

			  case K_HOME:
			  	curwin->w_cursor.col = 0;
				start_arrow();
				break;

			  case K_END:
				coladvance(MAXCOL);
				start_arrow();
				break;

			  case K_SLARROW:
			  	if (curwin->w_cursor.lnum > 1 || curwin->w_cursor.col > 0)
				{
					bck_word(1L, 0);
					start_arrow();
				}
				else
					beep_flush();
				break;

			  case K_RARROW:
				if (gchar_cursor() != NUL)
				{
					curwin->w_set_curswant = TRUE;
					start_arrow();
					++curwin->w_cursor.col;
				}
					/* if 'whichwrap' set for cursor in insert mode may go
					 * to next line */
				else if ((p_ww & 16) && curwin->w_cursor.lnum < curbuf->b_ml.ml_line_count)
				{
					curwin->w_set_curswant = TRUE;
					start_arrow();
					++curwin->w_cursor.lnum;
					curwin->w_cursor.col = 0;
				}
				else
					beep_flush();
				break;

			  case K_SRARROW:
			  	if (curwin->w_cursor.lnum < curbuf->b_ml.ml_line_count || gchar_cursor() != NUL)
				{
					fwd_word(1L, 0, 0);
					start_arrow();
				}
				else
					beep_flush();
				break;

			  case K_UARROW:
			  	if (oneup(1L))
					start_arrow();
				else
					beep_flush();
				break;

			  case K_SUARROW:
			  case K_PAGEUP:
			  	if (onepage(BACKWARD, 1L))
					start_arrow();
				else
					beep_flush();
				break;

			  case K_DARROW:
			  	if (onedown(1L))
					start_arrow();
				else
					beep_flush();
				break;

			  case K_SDARROW:
			  case K_PAGEDOWN:
			  	if (onepage(FORWARD, 1L))
					start_arrow();
				else
					beep_flush();
				break;

			  case TAB:				/* TAB or Complete patterns along path */
				if (ctrl_x_mode == CTRL_X_PATH_PATTERNS)
					goto docomplete;

			    if (echeck_abbr(TAB + 0x200))
					break;
			  	if (!curbuf->b_p_et && !(p_sta && inindent()))
					goto normalchar;

				AppendToRedobuff((char_u *)"\t");

				if (!curbuf->b_p_et)
					goto ins_indent;

										/* p_te set: expand a tab into spaces */
				stop_arrow();
				did_ai = FALSE;
				did_si = FALSE;
				can_si = FALSE;
				can_si_back = FALSE;
				if (p_sta && inindent())		/* insert smart tab */
					temp = (int)curbuf->b_p_sw;
				else							/* insert normal tab */
					temp = (int)curbuf->b_p_ts;
				temp -= curwin->w_virtcol % temp;
				/*
				 * insert the first space with inschar(); it will delete one
				 * char in replace mode. Insert the rest with insstr(); it
				 * will not delete any chars
				 */
				inschar(' ');
				while (--temp)
				{
					insstr((char_u *)" ");
					if (State == REPLACE)		/* no char replaced */
						replace_push(0);
				}
				goto redraw;

			  case CR:
			  case NL:
			    if (echeck_abbr(c + 0x200))
					break;
				stop_arrow();
				if (State == REPLACE)
					replace_push(NUL);

				AppendToRedobuff(NL_STR);
				if (curbuf->b_p_fo != NULL &&
								STRCHR(curbuf->b_p_fo, FO_RET_COMS) != NULL)
					fo_do_comments = TRUE;
				i = Opencmd(FORWARD, TRUE);
				fo_do_comments = FALSE;
				if (!i)
					goto doESCkey;		/* out of memory */
				if (revins)
					dec_cursor();
				break;

#ifdef DIGRAPHS
			  case Ctrl('K'):
				screen_start();
				screen_outchar('?', curwin->w_winpos + curwin->w_row,
															curwin->w_col);
				if (!add_to_showcmd(c))
					setcursor();
			  	c = vgetc();
				if (c != ESC)
				{
					if (charsize(c) == 1)
					{
						screen_start();
						screen_outchar(c, curwin->w_winpos + curwin->w_row,
																curwin->w_col);
					}
					if (!add_to_showcmd(c))
						setcursor();
					cc = vgetc();
					if (cc != ESC)
					{
						AppendToRedobuff((char_u *)"\026");	/* CTRL-V */
						c = getdigraph(c, cc, TRUE);
						clear_showcmd();
						goto normalchar;
					}
				}
				clear_showcmd();
				updateline();
				goto doESCkey;
#endif /* DIGRAPHS */

			  case Ctrl(']'):			/* Tag name completion after ^X */
				if (ctrl_x_mode != CTRL_X_TAGS)
					goto normalchar;
				goto docomplete;

			  case Ctrl('F'):			/* File name completion after ^X */
				if (ctrl_x_mode != CTRL_X_FILES)
					goto normalchar;
				goto docomplete;

			  case Ctrl('L'):			/* Whole line completion after ^X */
				if (ctrl_x_mode != CTRL_X_WHOLE_LINE)
					goto normalchar;
				/* FALLTHROUGH */

			  case Ctrl('P'):			/* Do previous pattern completion */
			  case Ctrl('N'):			/* Do next pattern completion */
docomplete:
				if (c == Ctrl('P') || c == Ctrl('L'))
					complete_direction = BACKWARD;
				else
					complete_direction = FORWARD;
				quick_m = m = NULL;			/* No message by default */
				if (!started_completion)
				{
					/* First time we hit ^N or ^P (in a row, I mean) */

					/* Turn off 'sm' so we don't show matches with ^X^L */
					save_sm = p_sm;
					p_sm = FALSE;

					if (ctrl_x_mode == 0)
					{
						edit_submode = (char_u *)"Keyword completion (^P/^N)";
						showmode();
					}
					did_ai = FALSE;
					did_si = FALSE;
					can_si = FALSE;
					can_si_back = FALSE;
					stop_arrow();
					done_dir = 0;
					first_match_pos = curwin->w_cursor;
					ptr = tmp_ptr = ml_get(first_match_pos.lnum);
					complete_col = first_match_pos.col;
					temp = complete_col - 1;

					/* Work out completion pattern and original text -- webb */
					if (ctrl_x_mode == 0 || (ctrl_x_mode & CTRL_X_WANT_IDENT))
					{
						if (temp < 0 || !isidchar_id(ptr[temp]))
						{
							/* Match any identifier of at least two chars */
							idp = get_id_option();	/* get value of 'id' */
							sprintf((char *)IObuff,
									"\\<[a-zA-Z%s][a-zA-Z0-9%s]", idp, idp);
							complete_pat = strsave(IObuff);
							if (complete_pat == NULL)
								break;
							tmp_ptr += complete_col;
							temp = 0;
						}
						else
						{
							while (temp >= 0 && isidchar_id(ptr[temp]))
								temp--;
							tmp_ptr += ++temp;
							if ((temp = complete_col - temp) == 1)
							{
								/* Only match identifiers with at least two
								 * chars -- webb
								 */
								idp = get_id_option();	/* get value of 'id' */
								sprintf((char *)IObuff,
										"\\<%c[a-zA-Z0-9%s]", *tmp_ptr, idp);
								complete_pat = strsave(IObuff);
								if (complete_pat == NULL)
									break;
							}
							else
							{
								complete_pat = alloc(temp + 3);
								if (complete_pat == NULL)
									break;
								sprintf((char *)complete_pat, "\\<%.*s", temp,
																	tmp_ptr);
							}
						}
					}
					else if (ctrl_x_mode == CTRL_X_WHOLE_LINE)
					{
						tmp_ptr = ptr;
						skipwhite(&tmp_ptr);
						temp = complete_col - (tmp_ptr - ptr);
						complete_pat = alloc(temp + 1);
						if (complete_pat == NULL)
							break;
						STRNCPY(complete_pat, tmp_ptr, temp);
						complete_pat[temp] = NUL;
					}
					else if (ctrl_x_mode == CTRL_X_FILES)
					{
						while (temp >= 0 && isfilechar(ptr[temp]))
							temp--;
						tmp_ptr += ++temp;
						temp = complete_col - temp;
						complete_pat = addstar(tmp_ptr, temp);
						if (complete_pat == NULL)
							break;
					}
					original_text = alloc(temp + 1);
					if (original_text == NULL)
					{
						free(complete_pat);
						complete_pat = NULL;
						break;
					}
					STRNCPY(original_text, tmp_ptr, temp);
					original_text[temp] = NUL;

					/* Get list of all completions now, if appropriate */
					if (ctrl_x_mode == CTRL_X_PATH_PATTERNS ||
						ctrl_x_mode == CTRL_X_PATH_DEFINES)
					{
						started_completion = TRUE;
						find_pattern_in_path(complete_pat,
								STRLEN(complete_pat), FALSE, FALSE,
							(ctrl_x_mode == CTRL_X_PATH_DEFINES) ? FIND_DEFINE
							: FIND_ANY, 1L, ACTION_EXPAND,
							(linenr_t)1, (linenr_t)MAXLNUM);
							/* eat the ESC to avoid leaving insert mode */
						if (got_int)
						{
							(void)vgetc();
							got_int = FALSE;
						}
						make_cyclic();
						if (first_match && first_match->next != first_match)
						{
							sprintf((char *)IObuff, "There are %d matches",
								count_completions());
							m = IObuff;
						}
					}
					else if (ctrl_x_mode == CTRL_X_TAGS)
					{
						started_completion = TRUE;
						reg_ic = p_ic;
						prog = regcomp(complete_pat);
						if (prog != NULL &&
							ExpandTags(prog, &num_matches, &matches, FALSE)
													== OK && num_matches > 0)
						{
							for (i = 0; i < num_matches; i++)
								if (add_new_completion(matches[i],
								  STRLEN(matches[i]), FORWARD) == RET_ERROR)
									break;
							free(matches);
							make_cyclic();
							free(prog);
							if (first_match && first_match->next != first_match)
							{
								sprintf((char *)IObuff,
									"There are %d matching tags",
									count_completions());
								m = IObuff;
							}
						}
						else
						{
							free(prog);
							free(complete_pat);
							complete_pat = NULL;
						}
					}
					else if (ctrl_x_mode == CTRL_X_FILES)
					{
						started_completion = TRUE;
						expand_interactively = TRUE;
						if (ExpandWildCards(1, &complete_pat, &num_matches,
							&matches, FALSE, FALSE) == OK)
						{
							for (i = 0; i < num_matches; i++)
								if (add_new_completion(matches[i],
								  STRLEN(matches[i]), FORWARD) == RET_ERROR)
									break;
							free(matches);
							make_cyclic();
							if (first_match && first_match->next != first_match)
							{
								sprintf((char *)IObuff,
									"There are %d matching file names",
									count_completions());
								m = IObuff;
							}
						}
						else
						{
							free(complete_pat);
							complete_pat = NULL;
						}
						expand_interactively = FALSE;
					}
					complete_col = tmp_ptr - ptr;
					first_match_pos.col -= temp;

					/* So that ^N can match word immediately after cursor */
					if (ctrl_x_mode == 0)
						dec(&first_match_pos);

					last_match_pos = first_match_pos;
				}
				/*
				 * In insert mode: Delete the typed part.
				 * In replace mode: Put the old characters back, if any.
				 */
				while (curwin->w_cursor.col > complete_col)
				{
					curwin->w_cursor.col--;
					if (State == REPLACE)
					{
						if ((cc = replace_pop()) > 0)
							pchar(curwin->w_cursor, cc);
					}
					else
						delchar(FALSE);
				}
				complete_pos = NULL;
				if (started_completion && curr_match == NULL &&
										(p_ws || done_dir == BOTH_DIRECTIONS))
					quick_m = e_patnotf;
				else if (curr_match != NULL && complete_direction == FORWARD &&
											curr_match->next != NULL)
					curr_match = curr_match->next;
				else if (curr_match != NULL && complete_direction == BACKWARD &&
											curr_match->prev != NULL)
					curr_match = curr_match->prev;
				else
				{
					complete_pos = (complete_direction == FORWARD) ?
										&last_match_pos : &first_match_pos;
					keep_old_search_pattern = TRUE;
					for (;;)
					{
						if (ctrl_x_mode == CTRL_X_WHOLE_LINE)
							temp = search_for_exact_line(complete_pos,
										complete_direction, complete_pat);
						else
							temp = searchit(complete_pos, complete_direction,
										complete_pat, 1L, FALSE, TRUE, 2);
						if (temp == FAIL)
						{
							if (!p_ws && done_dir != -complete_direction)
							{
								/*
								 * With nowrapscan, we haven't finished
								 * looking in the other direction yet -- webb
								 */
								temp = OK;
								done_dir = complete_direction;
							}
							else if (!p_ws)
								done_dir = BOTH_DIRECTIONS;
							break;
						}
						if (!started_completion)
						{
							started_completion = TRUE;
							first_match_pos = *complete_pos;
							last_match_pos = *complete_pos;
						}
						else if (first_match_pos.lnum == last_match_pos.lnum &&
						  first_match_pos.col == last_match_pos.col)
						{
							/* We have found all the matches in this file */
							temp = FAIL;
							break;
						}
						ptr = ml_get_pos(complete_pos);
						if (ctrl_x_mode == CTRL_X_WHOLE_LINE)
							temp = STRLEN(ptr);
						else
						{
							tmp_ptr = ptr;
							temp = 0;
							while (*tmp_ptr != NUL && isidchar_id(*tmp_ptr++))
								temp++;
						}
						if (add_completion_and_infercase(ptr, temp,
								complete_direction) != FAIL)
						{
							temp = OK;
							break;
						}
					}
					keep_old_search_pattern = FALSE;
				}
				i = -1;
				if (complete_pos != NULL && temp == FAIL && ctrl_x_mode == 0)
				{
					i = count_completions();	/* Num matches in this file */
					complete_dictionaries(complete_pat, complete_direction);
				}
				if (complete_pos != NULL && temp == FAIL)
				{
					int tot;

					tot = count_completions();	/* Total num matches */
					if (curr_match != NULL)
					{
						make_cyclic();
						if (complete_direction == FORWARD)
							curr_match = curr_match->next;
						else
							curr_match = curr_match->prev;
					}
					if ((i < 0 && tot > 1) || (tot == i && i > 1))
					{
						sprintf((char *)IObuff,
							"All %d matches have now been found", tot);
						m = IObuff;
					}
					else if (i >= 0 && tot > i)
					{
						sprintf((char *)IObuff,
							"%d matches in file, %d matches in dictionary",
							i, tot - i);
						m = IObuff;
					}
					else if (tot == 0)
						quick_m = e_patnotf;
				}

				/* Why do we still exit insert mode after ^C? -- webb */
				got_int = FALSE;

				/*
				 * Use inschar() to insert the text, it is a bit slower than
				 * insstr(), but it takes care of replace mode.
				 */
				if (curr_match != NULL)
					ptr = curr_match->str;
				else
					ptr = original_text;
				if (ptr != NULL)
					while (*ptr)
						inschar(*ptr++);
				started_completion = TRUE;
				updateline();
				(void)set_highlight('r');
				msg_highlight = TRUE;
				if (m != NULL)
				{
					msg(m);
					sleep(1);
				}
				else if (quick_m != NULL)
					msg(quick_m);
				else if (first_match != NULL &&
											first_match->next == first_match)
					MSG("This is the only match");
				break;

			  case Ctrl('Y'):				/* copy from previous line */
				if (ctrl_x_mode == CTRL_X_SCROLL)
				{
					scrolldown_clamp();
					updateScreen(VALID);
					break;
				}
				lnum = curwin->w_cursor.lnum - 1;
				goto copychar;

			  case Ctrl('E'):				/* copy from next line */
				if (ctrl_x_mode == CTRL_X_SCROLL)
				{
					scrollup_clamp();
					updateScreen(VALID);
					break;
				}
				lnum = curwin->w_cursor.lnum + 1;
copychar:
				if (lnum < 1 || lnum > curbuf->b_ml.ml_line_count)
				{
					beep_flush();
					break;
				}

				/* try to advance to the cursor column */
				temp = 0;
				ptr = ml_get(lnum);
				while (temp < curwin->w_virtcol && *ptr)
						temp += chartabsize(*ptr++, (long)temp);

				if (temp > curwin->w_virtcol)
						--ptr;
				if ((c = *ptr) == NUL)
				{
					beep_flush();
					break;
				}

				/*FALLTHROUGH*/
			  default:
normalchar:
				/*
				 * do some very smart indenting when entering '{' or '}' or '#'
				 */
				if (((did_si || can_si_back) && c == '{') ||
								(can_si && c == '}'))
				{
					FPOS	*pos, old_pos;

						/* for '}' set indent equal to indent of line
						 * containing matching '{'
						 */
					if (c == '}' && (pos = findmatch('{')) != NULL)
					{
						old_pos = curwin->w_cursor;
						/*
						 * If the matching '{' has a ')' immediately before it
						 * (ignoring white-space), then line up with the
						 * matching '(' if there is one.  This handles the case
						 * where an "if (..\n..) {" statement continues over
						 * multiple lines -- webb
						 */
						ptr = ml_get(pos->lnum);
						i = (pos->col == 0) ? pos->col : (pos->col - 1);
						while (i > 0 && iswhite(ptr[i]))
						{
							if (ptr[i] == ')')
								break;
							i--;
						}
						curwin->w_cursor.lnum = pos->lnum;
						curwin->w_cursor.col = i;
						if (ptr[i] == ')' && (pos = findmatch('(')) != NULL)
							curwin->w_cursor = *pos;
						i = get_indent();
						curwin->w_cursor = old_pos;
						set_indent(i, TRUE);
					}
					else if (curwin->w_cursor.col > 0)
						shift_line(TRUE, FALSE, 1);
				}
					/* set indent of '#' always to 0 */
				if (curwin->w_cursor.col > 0 && can_si && c == '#')
				{
								/* remember current indent for next line */
					old_indent = get_indent();
					set_indent(0, TRUE);
				}

				if (isidchar_id(c) || !echeck_abbr(c))
					insert_special(c);
				break;
		}	/* end of switch (c) */
	}
}

/*
 * Is the character 'c' a valid key to keep us in the current ctrl-x mode?
 * -- webb
 */
	int
is_ctrl_x_key(c)
	int		c;
{
	switch (ctrl_x_mode)
	{
		case 0:				/* Not in any ctrl-x mode */
			break;
		case CTRL_X_NOT_DEFINED_YET:
			if (c == Ctrl('X') || c == Ctrl('Y') || c == Ctrl('E') ||
					c == Ctrl('L') || c == Ctrl('F') || c == Ctrl(']') ||
					c == Ctrl('I') || c == Ctrl('D') || c == Ctrl('P') ||
					c == Ctrl('N'))
				return TRUE;
			break;
		case CTRL_X_SCROLL:
			if (c == Ctrl('Y') || c == Ctrl('E'))
				return TRUE;
			break;
		case CTRL_X_WHOLE_LINE:
			if (c == Ctrl('L') || c == Ctrl('P') || c == Ctrl('N'))
				return TRUE;
			break;
		case CTRL_X_FILES:
			if (c == Ctrl('F') || c == Ctrl('P') || c == Ctrl('N'))
				return TRUE;
			break;
		case CTRL_X_TAGS:
			if (c == Ctrl(']') || c == Ctrl('P') || c == Ctrl('N'))
				return TRUE;
			break;
		case CTRL_X_PATH_PATTERNS:
			if (c == Ctrl('P') || c == Ctrl('N'))
				return TRUE;
			break;
		case CTRL_X_PATH_DEFINES:
			if (c == Ctrl('D') || c == Ctrl('P') || c == Ctrl('N'))
				return TRUE;
			break;
		default:
			emsg(e_internal);
			break;
	}
	return FALSE;
}

/*
 * This is like add_new_completion(), but if ic and inf are set, then the
 * case of the originally typed text is used, and the case of the completed
 * text is infered, ie this tries to work out what case you probably wanted
 * the rest of the word to be in -- webb
 */
	int
add_completion_and_infercase(str, len, dir)
	char_u	*str;
	int		len;
	int		dir;
{
	int has_lower = FALSE;
	int was_letter = FALSE;
	int orig_len;
	int idx;

	if (p_ic && p_inf && len < IOSIZE)
	{
		/* Infer case of completed part -- webb */
		orig_len = STRLEN(original_text);

		/* Use IObuff, str would change text in buffer! */
		STRNCPY(IObuff, str, len);
		IObuff[len] = NUL;

		/* Rule 1: Were any chars converted to lower? */
		for (idx = 0; idx < orig_len; ++idx)
		{
			if (islower(original_text[idx]))
			{
				has_lower = TRUE;
				if (isupper(IObuff[idx]))
				{
					/* Rule 1 is satisfied */
					for (idx = orig_len; idx < len; ++idx)
						IObuff[idx] = TO_LOWER(IObuff[idx]);
					break;
				}
			}
		}

		/*
		 * Rule 2: No lower case, 2nd consecutive letter converted to
		 * upper case.
		 */
		if (!has_lower)
		{
			for (idx = 0; idx < orig_len; ++idx)
			{
				if (was_letter && isupper(original_text[idx]) &&
					islower(IObuff[idx]))
				{
					/* Rule 2 is satisfied */
					for (idx = orig_len; idx < len; ++idx)
						IObuff[idx] = TO_UPPER(IObuff[idx]);
					break;
				}
				was_letter = isalpha(original_text[idx]);
			}
		}

		/* Copy the original case of the part we typed */
		STRNCPY(IObuff, original_text, orig_len);

		return add_new_completion(IObuff, len, dir);
	}
	return add_new_completion(str, len, dir);
}

/*
 * If the given string is already in the list of completions, then return
 * FAIL, otherwise add it to the list and return OK.  If there is an error,
 * maybe because alloc returns NULL, then RET_ERROR is returned -- webb.
 */
	static int
add_new_completion(str, len, dir)
	char_u	*str;
	int		len;
	int		dir;
{
	struct Completion *match;
	char_u *new_str;

	breakcheck();
	if (got_int)
		return RET_ERROR;
	if (first_match == NULL)
	{
		if ((new_str = alloc(len + 1)) == NULL)
			return RET_ERROR;
		first_match = curr_match =
			(struct Completion *)alloc(sizeof(struct Completion));
		if (first_match == NULL)
		{
			free(new_str);
			return RET_ERROR;
		}
		curr_match->next = curr_match->prev = NULL;
	}
	else
	{
		match = first_match;
		do
		{
			if (STRNCMP(match->str, str, len) == 0 && match->str[len] == NUL)
				return FAIL;
			match = match->next;
		} while (match != NULL && match != first_match);
		if ((new_str = alloc(len + 1)) == NULL)
			return RET_ERROR;
		match = (struct Completion *)alloc(sizeof(struct Completion));
		if (match == NULL)
		{
			free(new_str);
			return RET_ERROR;
		}
		if (dir == FORWARD)
		{
			match->next = NULL;
			match->prev = curr_match;
			curr_match->next = match;
			curr_match = match;
		}
		else	/* BACKWARD */
		{
			match->prev = NULL;
			match->next = curr_match;
			curr_match->prev = match;
			first_match = curr_match = match;
		}
	}
	STRNCPY(new_str, str, len);
	new_str[len] = NUL;
	curr_match->str = new_str;
	return OK;
}

/*
 * Make the completion list cyclic.  We assume that curr_match is either at
 * the start or the end of the list.
 */
	static void
make_cyclic()
{
	if (curr_match != NULL)
	{
		if (curr_match != first_match)		/* must be last match */
		{
			curr_match->next = first_match;
			first_match->prev = curr_match;
		}
		else
		{
			while (curr_match->next != NULL)
				curr_match = curr_match->next;
			curr_match->next = first_match;
			first_match->prev = curr_match;
			curr_match = first_match;
		}
	}
}

/*
 * Add any identifiers that match the given pattern to the list of
 * completions.
 */
	static void
complete_dictionaries(pat, dir)
	char_u	*pat;
	int		dir;
{
	struct Completion *save_curr_match = curr_match;
	char_u	*dict = p_dict;
	char_u	*ptr;
	char_u	save_char;
	char_u	*buf;
	int		at_start;
	FILE	*fp;
	struct regexp *prog = NULL;

	if ((buf = alloc(LSIZE)) == NULL)
		return;
	if (curr_match != NULL)
	{
		while (curr_match->next != NULL)
			curr_match = curr_match->next;
	}
	if (dict != NULL)
	{
		skipwhite(&dict);
		if (*dict != NUL)
		{
			(void)set_highlight('r');
			msg_highlight = TRUE;
			MSG("Please wait, searching dictionaries");
			prog = regcomp(pat);
		}
		while (*dict != NUL && prog != NULL && !got_int)
		{
			ptr = dict;
			skiptowhite(&ptr);
			save_char = *ptr;
			*ptr = NUL;
			fp = fopen((char *)dict, "r");
			*ptr = save_char;
			dict = ptr;
			if (fp != NULL)
			{
				while (!got_int && vim_fgets(buf, LSIZE, fp, NULL) != VIM_EOF)
				{
					ptr = buf;
					at_start = TRUE;
					while (regexec(prog, ptr, at_start))
					{
						at_start = FALSE;
						ptr = prog->startp[0];
						while (isidchar_id(*ptr))
							++ptr;
						if (add_completion_and_infercase(prog->startp[0],
								ptr - prog->startp[0], FORWARD) == RET_ERROR)
							break;
					}
				}
				fclose(fp);
			}
			skipwhite(&dict);
		}
		free(prog);
	}
	if (save_curr_match != NULL)
		curr_match = save_curr_match;
	else if (dir == BACKWARD)
		curr_match = first_match;
	free(buf);
}

/*
 * Free the list of completions
 */
	static void
free_completions()
{
	struct Completion *match;

	if (first_match == NULL)
		return;
	curr_match = first_match;
	do
	{
		match = curr_match;
		curr_match = curr_match->next;
		free(match);
	} while (curr_match != NULL && curr_match != first_match);
	first_match = curr_match = NULL;
}

/*
 * Return the number of items in the Completion list
 */
	static int
count_completions()
{
	struct Completion *match;
	int num = 0;

	if (first_match == NULL)
		return 0;
	match = first_match;
	do
	{
		num++;
		match = match->next;
	} while (match != NULL && match != first_match);
	return num;
}

/*
 * Next character is interpreted literally.
 * A one, two or three digit decimal number is interpreted as its byte value.
 * If one or two digits are entered, the next character is given to vungetc().
 */
	int
get_literal()
{
	int			 cc;
	int			 nc;
	int			 oldstate;
	int			 i;

	oldstate = State;
	State = NOMAPPING;		/* next characters not mapped */

	if (got_int)
		return Ctrl('C');
	cc = 0;
	for (i = 0; i < 3; ++i)
	{
		nc = vgetc();
		if (!(oldstate & CMDLINE))
			add_to_showcmd(nc);
		if (nc >= 0x100 || !isdigit(nc))
			break;
		cc = cc * 10 + nc - '0';
		nc = 0;
	}
	if (i == 0)		/* no number entered */
	{
		if (nc == K_ZERO)	/* NUL is stored as NL */
		{
			cc = '\n';
			nc = 0;
		}
		else
		{
			cc = nc;
			nc = 0;
		}
	}
	else if (cc == 0)		/* NUL is stored as NL */
		cc = '\n';

	State = oldstate;
	if (nc)
		vungetc(nc);
	got_int = FALSE;		/* CTRL-C typed after CTRL-V is not an interrupt */
	return cc;
}

/*
 * Insert character, taking care of special codes above 0x100
 */
	static void
insert_special(c)
	int		c;
{
	/*
	 * Special function key, translate into two chars: K_SPECIAL KS_...
	 */
	if (c >= 0x100)
	{
		insertchar(K_SPECIAL, FALSE, -1);
		c = K_SECOND(c);
	}
	insertchar(c, FALSE, -1);
}

/*
 * Special characters in this context are those that need processing other
 * than the simple insertion that can be performed here. This includes ESC
 * which terminates the insert, and CR/NL which need special processing to
 * open up a new line. This routine tries to optimize insertions performed by
 * the "redo", "undo" or "put" commands, so it needs to know when it should
 * stop and defer processing to the "normal" mechanism.
 */
#define ISSPECIAL(c)	((c) < ' ' || (c) >= DEL)

	void
insertchar(c, force_formatting, second_indent)
	unsigned	c;
	int			force_formatting;		/* format line regardless of p_fo */
	int			second_indent;			/* indent for second line if >= 0 */
{
	int		haveto_redraw = FALSE;
	int		textwidth;
	int		leader_len;
	int		save;
	int		first_line = TRUE;

	stop_arrow();

	/*
	 * find out textwidth to be used:
	 *	if 'textwidth' option is set, use it
	 *	else if 'wrapmargin' option is set, use Columns - 'wrapmargin'
	 *	if invalid value, use 0.
	 */
	textwidth = curbuf->b_p_tw;
	if (textwidth == 0 && curbuf->b_p_wm)
		textwidth = Columns - curbuf->b_p_wm;
	if (textwidth < 0)
		textwidth = 0;

	/*
	 * If the cursor is past 'textwidth' and we are inserting a non-space,
	 * try to break the line in two or more pieces. If c == NUL then we have
	 * been called to do formatting only. If textwidth == 0 it does nothing.
	 * Don't do this if an existing character is being replaced.
	 */
	if (force_formatting ||
				!(iswhite(c) || (State == REPLACE && *ml_get_cursor() != NUL)))
	{
		while (textwidth && curwin->w_virtcol >= textwidth)
		{
			int		startcol;		/* Cursor column at entry */
			int		wantcol;		/* column at textwidth border */
			int		foundcol;		/* column for start of spaces */
			int		end_foundcol = 0;/* column for start of word */

			if (!force_formatting && curbuf->b_p_fo != NULL &&
								STRCHR(curbuf->b_p_fo, FO_WRAP_COMS) != NULL)
				fo_do_comments = TRUE;

			/* Don't break until after the comment leader */
			leader_len = get_leader_len(ml_get(curwin->w_cursor.lnum));
			if (!force_formatting && leader_len == 0 &&
								curbuf->b_p_fo != NULL &&
								STRCHR(curbuf->b_p_fo, FO_WRAP) == NULL)
			{
				textwidth = 0;
				break;
			}
			if ((startcol = curwin->w_cursor.col) == 0)
				break;
			coladvance(textwidth);		/* find column of textwidth border */
			wantcol = curwin->w_cursor.col;

			curwin->w_cursor.col = startcol - 1;
			foundcol = 0;
			while (curwin->w_cursor.col > 0)	/* find position to break at */
			{
				if (iswhite(gchar_cursor()))
				{
						/* remember position of blank just before text */
					end_foundcol = curwin->w_cursor.col;
					while (curwin->w_cursor.col > 0 && iswhite(gchar_cursor()))
						--curwin->w_cursor.col;
					if (curwin->w_cursor.col == 0)
						break;			/* only spaces in front of text */
					/* Don't break until after the comment leader */
					if (curwin->w_cursor.col < leader_len)
						break;
					foundcol = curwin->w_cursor.col + 1;
					if (curwin->w_cursor.col < wantcol)
						break;
				}
				--curwin->w_cursor.col;
			}

			if (foundcol == 0)			/* no spaces, cannot break line */
			{
				curwin->w_cursor.col = startcol;
				break;
			}
			/*
			 * offset between cursor position and line break is used by
			 * replace stack functions
			 */
			replace_offset = startcol - end_foundcol - 1;

			/*
			 * adjust startcol for spaces that will be deleted and
			 * characters that will remain on top line
			 */
			curwin->w_cursor.col = foundcol;
			while (iswhite(gchar_cursor()))
			{
				++curwin->w_cursor.col;
				--startcol;
			}
			startcol -= foundcol;
			if (startcol < 0)
				startcol = 0;

				/* put cursor after pos. to break line */
			curwin->w_cursor.col = foundcol;

				/* switch 'ai' on to make spaces to be deleted */
			save = curbuf->b_p_ai;
			curbuf->b_p_ai = TRUE;
			Opencmd(FORWARD, FALSE);
			curbuf->b_p_ai = save;
			replace_offset = 0;
			if (second_indent >= 0 && first_line)
				set_indent(second_indent, TRUE);
			first_line = FALSE;
			curwin->w_cursor.col += startcol;
			curs_columns(FALSE);		/* update curwin->w_virtcol */
			haveto_redraw = TRUE;
		}
		if (c == NUL)					/* formatting only */
			return;
		fo_do_comments = FALSE;
		if (haveto_redraw)
		{
			/*
			 * If the cursor ended up just below the screen we scroll up here
			 * to avoid a redraw of the whole screen in the most common cases.
			 */
 			if (curwin->w_cursor.lnum == curwin->w_botline && !curwin->w_empty_rows)
				win_del_lines(curwin, 0, 1, TRUE, TRUE);
			updateScreen(CURSUPD);
		}
	}
	if (c == NUL)			/* only formatting was wanted */
		return;

	did_ai = FALSE;
	did_si = FALSE;
	can_si = FALSE;
	can_si_back = FALSE;

	/*
	 * If there's any pending input, grab up to MAX_COLUMNS at once.
	 * This speeds up normal text input considerably.
	 */
	if (vpeekc() != NUL && State != REPLACE && !p_ri)
	{
		char_u			p[MAX_COLUMNS + 1];
		int 			i;

		p[0] = c;
		i = 1;
		while ((c = vpeekc()) != NUL && !ISSPECIAL(c) && i < MAX_COLUMNS &&
					(textwidth == 0 || (curwin->w_virtcol += charsize(p[i - 1])) < textwidth) &&
					!(!no_abbr && !isidchar_id(c) && isidchar_id(p[i - 1])))
			p[i++] = vgetc();
#ifdef DIGRAPHS
		dodigraph(-1);					/* clear digraphs */
		dodigraph(p[i-1]);				/* may be the start of a digraph */
#endif
		p[i] = '\0';
		insstr(p);
		AppendToRedobuff(p);
	}
	else
	{
		inschar(c);
		AppendCharToRedobuff(c);
	}

	/*
	 * TODO: If the cursor has shifted past the end of the screen, should
	 * adjust the screen display. Avoids extra redraw.
	 */

	updateline();
}

/*
 * start_arrow() is called when an arrow key is used in insert mode.
 * It resembles hitting the <ESC> key.
 */
	static void
start_arrow()
{
	if (!arrow_used)		/* something has been inserted */
	{
		AppendToRedobuff(ESC_STR);
		arrow_used = TRUE;		/* this means we stopped the current insert */
		stop_insert();
	}
}

/*
 * stop_arrow() is called before a change is made in insert mode.
 * If an arrow key has been used, start a new insertion.
 */
	static void
stop_arrow()
{
	if (arrow_used)
	{
		(void)u_save_cursor();				/* errors are ignored! */
		Insstart = curwin->w_cursor;		/* new insertion starts here */
		ResetRedobuff();
		AppendToRedobuff((char_u *)"1i");	/* pretend we start an insertion */
		arrow_used = FALSE;
	}
}

/*
 * do a few things to stop inserting
 */
	static void
stop_insert()
{
	stop_redo_ins();

	/*
	 * save the inserted text for later redo with ^@
	 */
	free(last_insert);
	last_insert = get_inserted();
	last_insert_skip = new_insert_skip;

	/*
	 * If we just did an auto-indent, remove the white space from the end of
	 * the line, and put the cursor back.
	 */
	if (did_ai && !arrow_used)
	{
		if (gchar_cursor() == NUL && curwin->w_cursor.col > 0)
			--curwin->w_cursor.col;
		while (iswhite(gchar_cursor()))
			delchar(TRUE);
		if (gchar_cursor() != NUL)
			++curwin->w_cursor.col;		/* put cursor back on the NUL */
		if (curwin->w_p_list)			/* the deletion is only seen in list mode */
			updateline();
	}
	did_ai = FALSE;
	did_si = FALSE;
	can_si = FALSE;
	can_si_back = FALSE;
}

/*
 * Set the last inserted text to a single character.
 * Used for the replace command.
 */
	void
set_last_insert(c)
	int		c;
{
	free(last_insert);
	last_insert = alloc(4);
	if (last_insert != NULL)
	{
		last_insert[0] = Ctrl('V');
		last_insert[1] = c;
		last_insert[2] = ESC;
		last_insert[3] = NUL;
			/* Use the CTRL-V only when not entering a digit */
		last_insert_skip = isdigit(c) ? 1 : 0;
	}
}

/*
 * move cursor to start of line
 * if flag == TRUE move to first non-white
 * if flag == MAYBE then move to first non-white if startofline is set,
 *		otherwise don't move at all.
 */
	void
beginline(flag)
	int			flag;
{
	if (flag == MAYBE && !p_sol)
		coladvance(curwin->w_curswant);
	else
	{
		curwin->w_cursor.col = 0;
		if (flag)
		{
			register char_u *ptr;

			for (ptr = ml_get(curwin->w_cursor.lnum); iswhite(*ptr); ++ptr)
				++curwin->w_cursor.col;
		}
		curwin->w_set_curswant = TRUE;
	}
}

/*
 * oneright oneleft onedown oneup
 *
 * Move one char {right,left,down,up}.
 * Return OK when sucessful, FAIL when we hit a line of file boundary.
 */

	int
oneright()
{
	char_u *ptr;

	ptr = ml_get_cursor();
	if (*ptr++ == NUL || *ptr == NUL)
		return FAIL;
	curwin->w_set_curswant = TRUE;
	++curwin->w_cursor.col;
	return OK;
}

	int
oneleft()
{
	if (curwin->w_cursor.col == 0)
		return FAIL;
	curwin->w_set_curswant = TRUE;
	--curwin->w_cursor.col;
	return OK;
}

	int
oneup(n)
	long n;
{
	if (n != 0 && curwin->w_cursor.lnum == 1)
		return FAIL;
	if (n >= curwin->w_cursor.lnum)
		curwin->w_cursor.lnum = 1;
	else
		curwin->w_cursor.lnum -= n;

	/* try to advance to the column we want to be at */
	coladvance(curwin->w_curswant);

	if (operator == NOP)
		cursupdate();				/* make sure curwin->w_topline is valid */

	return OK;
}

	int
onedown(n)
	long n;
{
	if (n != 0 && curwin->w_cursor.lnum == curbuf->b_ml.ml_line_count)
		return FAIL;
	curwin->w_cursor.lnum += n;
	if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
		curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;

	/* try to advance to the column we want to be at */
	coladvance(curwin->w_curswant);

	if (operator == NOP)
		cursupdate();				/* make sure curwin->w_topline is valid */

	return OK;
}

/*
 * move screen 'count' pages up or down and update screen
 *
 * return FAIL for failure, OK otherwise
 */
	int
onepage(dir, count)
	int		dir;
	long	count;
{
	linenr_t		lp;
	long			n;

	if (curbuf->b_ml.ml_line_count == 1)	/* nothing to do */
		return FAIL;
	for ( ; count > 0; --count)
	{
		if (dir == FORWARD ? (curwin->w_topline >= curbuf->b_ml.ml_line_count - 1) : (curwin->w_topline == 1))
		{
			beep_flush();
			return FAIL;
		}
		if (dir == FORWARD)
		{
			if (curwin->w_botline > curbuf->b_ml.ml_line_count)				/* at end of file */
				curwin->w_topline = curbuf->b_ml.ml_line_count;
			else if (plines(curwin->w_botline) >= curwin->w_height - 2 ||	/* next line is big */
					curwin->w_botline - curwin->w_topline <= 3)		/* just three lines on screen */
				curwin->w_topline = curwin->w_botline;
			else
				curwin->w_topline = curwin->w_botline - 2;
			curwin->w_cursor.lnum = curwin->w_topline;
			if (count != 1)
				comp_Botline(curwin);
		}
		else	/* dir == BACKWARDS */
		{
			lp = curwin->w_topline;
			/*
			 * If the first two lines on the screen are not too big, we keep
			 * them on the screen.
			 */
			if ((n = plines(lp)) > curwin->w_height / 2)
				--lp;
			else if (lp < curbuf->b_ml.ml_line_count && n + plines(lp + 1) < curwin->w_height / 2)
				++lp;
			curwin->w_cursor.lnum = lp;
			n = 0;
			while (n <= curwin->w_height && lp >= 1)
			{
				n += plines(lp);
				--lp;
			}
			if (n <= curwin->w_height)				/* at begin of file */
				curwin->w_topline = 1;
			else if (lp >= curwin->w_topline - 2)		/* happens with very long lines */
			{
				--curwin->w_topline;
				comp_Botline(curwin);
				curwin->w_cursor.lnum = curwin->w_botline - 1;
			}
			else
				curwin->w_topline = lp + 2;
		}
	}
	beginline(MAYBE);
	updateScreen(VALID);
	return OK;
}

	void
stuff_inserted(c, count, no_esc)
	int		c;
	long	count;
	int		no_esc;
{
	char_u		*esc_ptr = NULL;
	char_u		*ptr;

	if (last_insert == NULL)
	{
		EMSG("No inserted text yet");
		ptr = (char_u *)"";
	}
	else	/* skip the command */
		ptr = last_insert + last_insert_skip;

	if (c)
		stuffcharReadbuff(c);
	if (no_esc && (esc_ptr = (char_u *)STRRCHR(ptr, 27)) != NULL)
		*esc_ptr = NUL;		/* remove the ESC */

	do
		stuffReadbuff(ptr);
	while (--count > 0);

	if (no_esc && esc_ptr)
		*esc_ptr = 27;		/* put the ESC back */
}

	char_u *
get_last_insert()
{
	if (last_insert == NULL)
		return NULL;
	return last_insert + last_insert_skip;
}

/*
 * Check the word in front of the cursor for an abbreviation.
 * Called when the non-id character "c" has been entered.
 * When an abbreviation is recognized it is removed from the text and
 * the replacement string is inserted in typebuf[], followed by "c".
 */
	static int
echeck_abbr(c)
	int c;
{
	if (p_paste || no_abbr)			/* no abbreviations or in paste mode */
		return FALSE;

	return check_abbr(c, ml_get(curwin->w_cursor.lnum), curwin->w_cursor.col,
				curwin->w_cursor.lnum == Insstart.lnum ? Insstart.col : 0);
}

/*
 * replace-stack functions
 *
 * When replacing characters the replaced character is remembered
 * for each new character. This is used to re-insert the old text
 * when backspacing.
 *
 * replace_offset is normally 0, in which case replace_push will add a new
 * character at the end of the stack. If replace_offset is not 0, that many
 * characters will be left on the stack above the newly inserted character.
 */

char_u	*replace_stack = NULL;
long	replace_stack_nr = 0;		/* next entry in replace stack */
long	replace_stack_len = 0;		/* max. number of entries */

	void
replace_push(c)
	int		c;		/* character that is replaced (NUL is none) */
{
	char_u	*p;

	if (replace_stack_nr < replace_offset)		/* nothing to do */
		return;
	if (replace_stack_len <= replace_stack_nr)
	{
		replace_stack_len += 50;
		p = lalloc(sizeof(char_u) * replace_stack_len, TRUE);
		if (p == NULL)		/* out of memory */
		{
			replace_stack_len -= 50;
			return;
		}
		if (replace_stack != NULL)
		{
			memmove(p, replace_stack, replace_stack_nr * sizeof(char_u));
			free(replace_stack);
		}
		replace_stack = p;
	}
	p = replace_stack + replace_stack_nr - replace_offset;
	if (replace_offset)
		memmove(p + 1, p, replace_offset * sizeof(char_u));
	*p = c;
	++replace_stack_nr;
}

/*
 * pop one item from the replace stack
 * return -1 if stack empty
 * return 0 if no character was replaced
 * return replaced character otherwise
 */
	int
replace_pop()
{
	if (replace_stack_nr == 0)
		return -1;
	return (int)replace_stack[--replace_stack_nr];
}

/*
 * make the replace stack empty
 * (called when exiting replace mode)
 */
	void
replace_flush()
{
	free(replace_stack);
	replace_stack = NULL;
	replace_stack_len = 0;
	replace_stack_nr = 0;
}

	static char_u *
get_id_option()
{
	char_u	*idp;

	if (curbuf->b_p_id != NULL)
	{
		/* move '-' to the end of 'id' to avoid
		 * problems with the search pattern */
		idp = STRCHR(curbuf->b_p_id, '-');
		if (idp != NULL)
		{
			STRCPY(idp, idp + 1);
			STRCAT(idp, "-");
		}
		idp = curbuf->b_p_id;
	}
	else
		idp = (char_u *)"";		/* out of memory, just return empty string */
	return idp;
}
