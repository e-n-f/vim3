/* slowunix.c

   stuff to make using vim on a slow terminal less painful.
   revised version, works on loads more terminals.
   even more revised, now has open mode.

   Eric Fischer, etaoin@uchicago.edu
*/

#include <stdio.h>
#include <stdlib.h>
#include <sgtty.h>

#include "vim.h"
#include "unix.h"
#include "proto.h"

#include "terms.h"

/* these are #defined to point to our functions rather than the
   real ones.  we need the real ones to do real output.
*/

#undef tgoto
#undef tgetent
#undef tputs
#undef tgetstr

char *tgetstr ();
char *tgetent ();
char *tgoto ();
void tputs ();
void setphysscroll();

/* arrays to hold real and virtual screens */

char *wantscreen = 0;
char *onscreen = 0;
char *wantattr = 0;
char *onattr = 0;

/* size of the screen */

int scrrows = 0;
int scrcols = 0;

/* cursor position */

int virtx = 0;
int virty = 0;
int physx = 0;
int physy = 0;
int oldvirtx = 0, oldvirty = 0;  /* fake, inverse video, cursor */

/* current inverseness */

int virtattr = 0;
int physattr = 0;

/* termcap strings */

char *clearscreencmd = 0;
char *setscrollcmd = 0;
char *scrolldowncmd = 0;
char *standoutcmd = 0;
char *standendcmd = 0;
char *cleareolcmd = 0;
char *insertcmd = 0;
char *deletecmd = 0;

char *movecurscmd = 0;
char *homecmd = 0;
char *upcmd = 0;
char *downcmd = 0;
char *leftcmd = 0;

char *initterm = 0;
char *exitterm = 0;

/* scroll regions */

int virtscrolltop = 0;
int virtscrollbot = 0;
int physscrolltop = 0;
int physscrollbot = 0;

/* tty size */

extern int Rows;
extern int Columns;

/* flow control */

int stillpending = 0;

/* do we have a really ridiculous terminal? */

int openmode = 0;
int inval = 0;     /* do we need to retype this line? */
int deleted = -1;  /* last line that was deleted */

/* cursor keys */

char *upkey = 0;
char *downkey = 0;
char *leftkey = 0;
char *rightkey = 0;

/* screen size from termcap */

int termcols = 0;
int termrows = 0;


int screendirty = 1;

extern int scroll_region;


/* calculate where in the array corresponds to some screen coordinates.
   should be inline?

   if out of bounds, just return 0, even though it'll toast the top left
   of the screen.
*/

static int
loc (y, x)
	int y;
	int x;
{
	if (y < 0 || y >= scrrows || x < 0 || x >= scrcols) {
		/* fprintf (stderr, "out of bounds: %d %d", y, x); */
		return scrrows * scrcols - 1;
	}

	return y * scrcols + x;
}

#if 0
#define loc(y,x) ((y) * scrcols + (x))
#endif


/* used by termcap routines to actually do output.  no idea what
   tputs() wants it to return, but i don't think it really cares.
*/

static int
putfunc (what)
	int what;
{
	printf ("%c", what);

	return what;  /* is this right? */
}

/* if we don't have standout mode, try to do something vaguely
   highlightish.  with luck, we'll have a reasonable terminal and
   it won't matter...
*/

static int
putfunkyfunc (what)
	int what;
{
	if (islower (what)) what -= 32;
	if (what == ' ') what = '_';

	putfunc (what);
	return what;
}

void
reallyclearscreen()
{
	if (openmode) {
		inval = 1;
		physx = physy = 0;
		return;
	}

	tputs (clearscreencmd, 1, putfunc);
	physx = 0;
	physy = 0;
}

/* fill an array with spaces */

static void
blank (where, len)
	char *where;
	int len;
{
	while (len) {
		*where = ' ';
		where++, len--;
	}
}

/* zero an array of the specified len */

static void
zero (where, len)
	char *where;
	int len;
{
	while (len) {
		*where = 0;
		where++, len--;
	}
}

/* called when the screen gets resized, or when we start up.
   deallocate any previous memory used to save screens and make
   new ones of the specified size.
*/

void
newscreensize (rows, cols)
	int rows;
	int cols;
{
	scrrows = rows;
	scrcols = cols;

	if (wantscreen) free (wantscreen);
	if (onscreen) free (onscreen);
	if (wantattr) free (wantattr);
	if (onattr) free (onattr);

	wantscreen = malloc (rows * cols * sizeof (char));
	onscreen = malloc (rows * cols * sizeof (char));
	wantattr = malloc (rows * cols * sizeof (char));
	onattr = malloc (rows * cols * sizeof (char));

	if (wantscreen == 0 || onscreen == 0 || wantattr == 0 || onattr == 0) {
		fprintf (stderr, "Vim: malloc failure!\n");
		exit (1);
	}

	blank (wantscreen, rows * cols);
	blank (onscreen, rows * cols);
	zero (wantattr, rows * cols);
	zero (onattr, rows * cols);

/*
	if (clearscreencmd) {
		reallyclearscreen();
	} else {
		virtx = 0;
		virty = 0;
		physx = -1;
		physy = -1;

		zero (onscreen, rows * cols);
		blank (onattr, rows * cols);
	}
*/

	physscrolltop = -1;
	physscrollbot = -1;

/*	physscrolltop = 0;
	physscrollbot = Rows - 1;
*/
	virtscrolltop = 0;
	virtscrollbot = Rows - 1;
}

/* get all the termcap commands we need.

   complain if we can't move the cursor, because that's just *too* dumb
   a terminal to deal with...
*/

static char buf1 [2048], buf2[2048], *here = buf2;

/* mini version just to get the terminal size before we do the
   real setup, in case we're on console..
*/

void
gettermsize()
{
	char *TERM = getenv ("TERM");
	
	if (TERM == 0) {
		fprintf (stderr, "vim: TERM isn't set; assuming ``dumb''\n");
		TERM = "dumb";
	}

	tgetent (buf1, TERM);

	termcols = tgetnum ("co");
	termrows = tgetnum ("li");
}

void
reallyinittcap()
{
	char *TERM = getenv ("TERM");
	
	if (TERM == 0) {
		fprintf (stderr, "vim: TERM isn't set; assuming ``dumb''\n");
		TERM = "dumb";
	}

	tgetent (buf1, TERM);

	movecurscmd = tgetstr ("cm", &here); 
	upcmd = tgetstr ("up", &here);
	downcmd = tgetstr ("do", &here);
	leftcmd = tgetstr ("le", &here);
	homecmd = tgetstr ("ho", &here);

	if (movecurscmd == 0
	&& (homecmd == 0 || leftcmd == 0 /* || rightcmd == 0 */
	||  downcmd == 0 || upcmd == 0))  {
		fprintf (stderr, "vim: Your terminal can't move the cursor; using open mode\n");
		openmode = 1;
	} else {
		clearscreencmd = tgetstr ("cl", &here); 
		setscrollcmd = tgetstr ("cs", &here); 
		scrolldowncmd = tgetstr ("sr", &here); 
		standoutcmd = tgetstr ("so", &here);
		standendcmd = tgetstr ("se", &here);
		cleareolcmd = tgetstr ("ce", &here);
		insertcmd = tgetstr ("al", &here);
		deletecmd = tgetstr ("dl", &here);

		initterm = tgetstr ("ti", &here);
		exitterm = tgetstr ("te", &here);

		if (tgetnum ("sg") != -1) {
			standoutcmd = 0;
			standendcmd = 0;
		}
	}

	upkey = tgetstr ("ku", &here);
	downkey = tgetstr ("kd", &here);
	leftkey = tgetstr ("kl", &here);
	rightkey = tgetstr ("kr", &here);

	termcols = tgetnum ("co");
	termrows = tgetnum ("li");

	if (termcols == -1) termcols = 0;
	if (termrows == -1) termrows = 0;

	newscreensize (Rows, Columns);
}

/* called from term.c.  move the virtual cursor somewhere. */

void
windgoto (row, col)
	int row;
	int col;
{
	flushbuf();
	if (row >= 0 && row < scrrows && col >=0 && col < scrcols) {
		virtx = col;
		virty = row;
	}
}

/* set the actual terminal's attribute to whatever.  if it's
   already that, don't do anything.
*/

void
attrto (what)
	int what;
{
	if (physattr == what) return;

	physattr = what;
	if (what) {
		if (standoutcmd) tputs (standoutcmd, 1, putfunc);
	} else {
		if (standendcmd) tputs (standendcmd, 1, putfunc);
	}
}

/* send out a single character from the array.
   used by cursto() in optimizing
*/

void
spewone (x, y)
	int x;
	int y;
{
	int lyx = loc (y, x);

	if (standoutcmd) {
		attrto  (wantattr  [lyx]);
		putfunc (wantscreen[lyx]);
	} else {
		if (wantattr [lyx]) putfunkyfunc (wantscreen[lyx]);
		else                putfunc      (wantscreen[lyx]);
	}
	onscreen[lyx] = wantscreen[lyx];
	onattr  [lyx] = wantattr  [lyx];
}

/* move the cursor to wherever.  we do a little optimizing --
   if it's already there we don't do anything, and if we can
   get there by backspacing or printing a few chars, we do
   that instead of actually cming.

   if we need to move the cursor manually, do that.
*/

void
cursto (row, col)
	int row;
	int col;
{
	if (openmode) return;

	if (row != physy || col != physx) {
		char *go;

		if (row == physy) {
			if (col == 0) {
				putfunc ('\r');
				physx = 0;
				return;
			} else if (col == physx - 1) {
				putfunc ('\b');
				physx = col;
				return;
			} else if (col == physx + 1) {
				spewone (physx, physy);
				physx++;
				return;
			} else if (col == physx + 2) {
				spewone (physx, physy);
				spewone (physx + 1, physy);
				physx += 2;
				return;
			}
		}

		if (movecurscmd) {
			go = tgoto (movecurscmd, col, row);
			tputs (go, 1, putfunc);
		} else {
			if (setscrollcmd) {
				setphysscroll (virtscrollbot, virtscrolltop);
			}

			if (physx ==  -1 || physy == -1
			||  physx == 999 || physy == 999) {
				tputs (homecmd, 1, putfunc);
				physx = 0; physy = 0;
			}

			while (row < physy) {
				tputs (upcmd, 1, putfunc);
				physy--;
			}
	
			while (row > physy) {
				tputs (downcmd, 1, putfunc);
				physy++;
			}

			while (col < physx) {
				tputs (leftcmd, 1, putfunc);
				physx--;
			}

			while (col > physx) {
				spewone (physx, physy);
				/* tputs (rightcmd, 1, putfunc); */
				physx++;
			}
		}
	}

	physy = row;
	physx = col;
}

/* set the virtual scroll region to whatever.  the physical one
   will get set when we actually scroll.

   called from inside term.c
*/

void
setscroll (bot, top)
	int bot;
	int top;
{
	flushbuf();

	virtscrolltop = top;
	virtscrollbot = bot;
}

/* this is what other .c files get when they call tgoto().
   really it should send out a character string that will
   alert spewchar(), but since we're just lazy we hope that
   no one will call it for anything other than an actual
   move and just set the vars ourself.
*/

char *
our_tgoto (how, col, row)
	char *how;
	int col;
	int row;
{
	flushbuf();

	if (col >= 0 && col < scrcols && row >= 0 && row <= scrrows) {
		virtx = col;
		virty = row;
	}

	return how;
}

/* this is supposed to get the terminal size from termcap.
   but since we have ioctls to do it, just forget it.
*/

void
getlinecol()
{
	if (!termcols) gettermsize();

	if (Columns == 0) Columns = termcols;
	if (Rows == 0) Rows = termrows;
}

/* set physical scroll region.  if it's already what we want,
   do nothing.
*/

void
setphysscroll (bot, top)
	int bot;
	int top;
{
	char *s;

	if (physscrollbot == bot && physscrolltop == top) return;

	physscrollbot = bot;
	physscrolltop = top;

	s = tgoto (setscrollcmd, bot, top);
	tputs (s, 1, putfunc);

	/* I have no idea why we need to bounce the cursor around like
	   this.  In fact, it's not necessary on the NeXT console, at
	   least.  But the Mac with ZTerm doesn't work right without
	   it, so here it is.
	*/

/*
	cursto (top, 0);
	cursto (bot, 0);
*/

	/* Actually, I think this should be sufficient.  We'll see,
	   I guess.
	*/

	physx = -1; physy = -1;
/*	cursto (virty, virtx);  */
}

/* clear to end of the line.  if it's supported in hardware, do that;
   otherwise, just put spaces in the buffer and do it by hand.
*/

void
docleareol()
{
	int x;

	/* if we have hardware cleareol, use it; otherwise, fake it */

	if (cleareolcmd) {
		int vloc = loc (virty, 0);

		cursto (virty, virtx);
		tputs (cleareolcmd, 1, putfunc);

		for (x = virtx; x < scrcols; x++) {
			int lyx = vloc + x;

			onscreen[lyx] = ' ';
			wantscreen[lyx] = ' ';

			onattr[lyx] = 0;
			wantattr[lyx] = 0;
		}
	} else {
		int vloc = loc (virty, 0);

		for (x = virtx; x < scrcols; x++) {
			int lyx = vloc + x;

			wantscreen[lyx] = ' ';
			wantattr[lyx] = 0;
		}

		stillpending = 1;
	}
}

/* use scroll regions to insert a blank line before 'virty',
   and set the arrays to include the new line and shove everything
   else down.

   if there are no scroll regions, look for hardware line insert/delete
   support; if there's not that, just shove everything and do it
   by hand later.
*/

void
doinsertln()
{
	int x, y;

	/* Zterm can't cope with a one-line scroll region, so erase
	   the line ourself if we're only doing one.
	   Just checked, and NCSA Telnet can't either. 
	*/

	inval = 1;

	if (virtscrollbot == virty) {
		int oldvirtx;

		oldvirtx = virtx;
		virtx = 0;
		docleareol();
		virtx = oldvirtx;
		return;
	}

	if (scrolldowncmd && setscrollcmd) {
		setphysscroll (virtscrollbot, virty);

		cursto (virty, virtx);
		tputs (scrolldowncmd, 1, putfunc);

		for (y = virtscrollbot - 1; y >= virty; y--) {
			int line1 = loc (y+1, 0);
			int line2 = loc (y, 0);

			for (x = 0; x < scrcols; x++) {
				int ly1x = line1 + x;
				int lyx = line2 + x;

				wantscreen[ly1x] = wantscreen[lyx];
				onscreen[ly1x] = onscreen[lyx];

				wantscreen[lyx] = ' ';
				onscreen[lyx] = ' ';

				wantattr[ly1x] = wantattr[lyx];
				onattr[ly1x] = onattr[lyx];

				wantattr[lyx] = 0;
				onattr[lyx] = 0;
			}
		}

	} else if (insertcmd && deletecmd) {
		cursto (virtscrollbot, virtx);
		tputs (deletecmd, 1, putfunc);
		cursto (virty, virtx);
		tputs (insertcmd, 1, putfunc);

		for (y = virtscrollbot - 1; y >= virty; y--) {
			int theloc = loc (y, 0);
			int theloc1 = loc (y+1, 0);

			for (x = 0; x < scrcols; x++) {
				int ly1x = theloc1 + x;
				int lyx = theloc + x;

				wantscreen[ly1x] = wantscreen[lyx];
				onscreen[ly1x] = onscreen[lyx];

				wantscreen[lyx] = ' ';
				onscreen[lyx] = ' ';

				wantattr[ly1x] = wantattr[lyx];
				onattr[ly1x] = onattr[lyx];

				wantattr[lyx] = 0;
				onattr[lyx] = 0;
			}
		}
	} else {
		for (y = virtscrollbot - 1; y >= virty; y--) {
			int theloc = loc (y, 0);
			int theloc1 = loc (y+1, 0);

			for (x = 0; x < scrcols; x++) {
				int ly1x = theloc1 + x;
				int lyx = theloc + x;

				wantscreen[ly1x] = wantscreen[lyx];
				wantscreen[lyx] = ' ';
				wantattr[ly1x] = wantattr[lyx];
				wantattr[lyx] = 0;
			}
		}
	}

	oldvirty++;
}

/* set the scroll regions up to delete line 'virty',
   and move stuff around in the arrays to reflect that it's gone.
*/

void
dodeleteln()
{
	int x, y;

	inval = 1;
	deleted = virty;

	/* deleting the bottom line is similarly annoying.
	   odd that in this degenerate case, inserting and
	   deleting a line mean exactly the same thing!
	*/

	if (virtscrollbot == virty) {
		int ovirtx;

		ovirtx = virtx;
		virtx = 0;
		docleareol();
		virtx = ovirtx;
		return;
	}

	if (setscrollcmd) {
		int yloc = 0;

		setphysscroll (virtscrollbot, virty);

		physx = -1, physy = -1;

		cursto (virtscrollbot, virtx);
		putfunc ('\n');

		for (y = virty + 1; y <= virtscrollbot; y++) {
			int yloc1 = loc (y - 1, 0);
			yloc = loc (y, 0);

			for (x = 0; x < scrcols; x++) {
				int lyx = yloc + x;
				int ly1x = yloc1 + x;

				wantscreen[ly1x] = wantscreen[lyx];
				onscreen[ly1x] = onscreen[lyx];

				wantattr[ly1x] = wantattr[lyx];
				onattr[ly1x] = onattr[lyx];
			}
		}

		for (x = 0; x < scrcols; x++) {
			int lyx = yloc + x;

			wantscreen[lyx] = ' ';
			onscreen[lyx] = ' ';

			wantattr[lyx] = 0;
			onattr[lyx] = 0;
		}
	} else {
		if (insertcmd && deletecmd) {
			int yloc = 0;

			cursto (virty, virtx);
			tputs (deletecmd, 1, putfunc);
			cursto (virtscrollbot, virtx);
			tputs (insertcmd, 1, putfunc);

			for (y = virty + 1; y <= virtscrollbot; y++) {
				int yloc1 = loc (y - 1, 0);
				yloc = loc (y, 0);

				for (x = 0; x < scrcols; x++) {
					int lyx = yloc + x;
					int ly1x = yloc1 + x;

					wantscreen[ly1x] = wantscreen[lyx];
					onscreen[ly1x] = onscreen[lyx];

					wantattr[ly1x] = wantattr[lyx];
					onattr[ly1x] = onattr[lyx];
				}
			}

			for (x = 0; x < scrcols; x++) {
				int lyx = yloc + x;

				wantscreen[lyx] = ' ';
				onscreen[lyx] = ' ';

				wantattr[lyx] = 0;
				onattr[lyx] = 0;
			}
		} else {
			for (y = virty + 1; y <= virtscrollbot; y++) {
				int yloc = loc (y, 0);
				int yloc1 = loc (y - 1, 0);

				for (x = 0; x < scrcols; x++) {
					int lyx = yloc + x;
					int ly1x = yloc1 + x;

					wantscreen[ly1x] = wantscreen[lyx];
					wantscreen[lyx] = ' ';
					wantattr[ly1x] = wantattr[lyx];
					wantattr[lyx] = 0;
				}
			}

		}
	}

	oldvirty--;
}

/* clear the screen for real, and blank out both physical and
   wanted screen arrays.  perhaps it'd be better not to actually
   clear the screen but just set wantscreen[] to blanks, so we
   could use whatever was there already.

   no, actually, that usually sucks.  we do that if the hardware
   doesn't support clearing the screen, though!
*/

void
doclearscr()
{
	int y, x;

	inval = 1;

	if (clearscreencmd) {
		reallyclearscreen();

		virtx = 0; virty = 0;

		for (x = 0; x < scrcols; x++) {
			for (y = 0; y < scrrows; y++) {
				int lyx = loc (y, x);

				onscreen[lyx] = ' ';
				wantscreen[lyx] = ' ';

				onattr [lyx] = 0;
				wantattr [lyx] = 0;
			}
		}
	} else {
		virtx = 0; virty = 0;

		for (x = 0; x < scrcols; x++) {
			for (y = 0; y < scrrows; y++) {
				int lyx = loc (y, x);

				wantscreen[lyx] = ' ';
				onscreen[lyx] = 0;
				wantattr [lyx] = 0;
				onattr [lyx] = 32;
			}
		}
	}
}

/* turn inverse video on... */

void
dostandout()
{
	virtattr = 1;
}

/* and off.... */

void
dostandend()
{
	virtattr = 0;
}

void
beepbeep()
{
	putfunc (7);
}

/* move everything in the scroll region up a line.

   spewchar() handles actually setting the region and
   printing the \n.
*/

void
doscrollup()
{
	int x, y;

	inval = 1;

	for (y = virtscrolltop + 1; y <= virtscrollbot; y++) {
		int locy1 = loc (y-1, 0);
		int locy = loc (y, 0);

		for (x = 0; x < scrcols; x++) {
			int ly1x = locy1 + x;
			int lyx = locy + x;

			wantscreen[ly1x] = wantscreen[lyx];
			onscreen[ly1x] = onscreen[lyx];
			wantattr[ly1x] = wantattr[lyx];
			onattr[ly1x] = onattr[lyx];

			wantscreen[lyx] = ' ';
			onscreen[lyx] = ' ';
			wantattr[lyx] = 0;
			onattr[lyx] = 0;
		}
	}

	oldvirty--;
}

void
doinitterm()
{
	if (initterm) tputs (initterm, 1, putfunc);
	fflush (stdout);
}

void
doexitterm()
{
	if (exitterm) tputs (exitterm, 1, putfunc);
	if (openmode) printf ("\n\n\r");
	fflush (stdout);
}

/* terminal emulation!

   perform an action for each of the special characters,
   or if it's nothing special at all stuff it in the
   screen array.

   also handle part of the scrolling that takes place
   when we fall off the bottom of the screen.
*/

void
spewchar (what)
	unsigned what;
{
	screendirty = 1;

	if        (what == CCLEAREOL) {
		docleareol();
	} else if (what == CINSERTLN) {
		doinsertln();
	} else if (what == CDELETELN) {
		dodeleteln();
	} else if (what == CCLEARSCR) {
		doclearscr();
	} else if (what == CSTANDOUT) {
		dostandout();
	} else if (what == CSTANDEND) {
		dostandend();
	} else if (what == CINIT) {
		doinitterm();
	} else if (what == CEXIT) {
		doexitterm();
	} else if (what == '\r') {
		virtx = 0;
	} else if (what == '\n') {
		virty++;
	} else if (what == '\t') {
		virtx = ((virtx + 8) % 8) * 8;
	} else if (what == 7) {
		beepbeep();
	} else if (what >= ' ') {
		stillpending = 2;
		wantscreen[loc (virty, virtx)] = what;
		wantattr  [loc (virty, virtx)] = virtattr;
		virtx++;
	} else {
		stillpending = 2;
		wantscreen[loc (virtx, virty)] = (what & 31) + 64;
		wantattr  [loc (virtx, virty)] = virtattr;
		virtx++;
	}

	if (virtx >= scrcols) {
		virty++;
		virtx -= scrcols;
	}

	if (virty > virtscrollbot) {
		while (virty > virtscrollbot) {
			int savex = virtx, savey = virty;

			virty = virtscrolltop;
			dodeleteln();

			deleted = -1; /* so open mode doesn't think a
			                 line really vanished */

			virtx = savex;
			virty = savey;

			virty--;
		}
	}
}

/* update at least one character on the screen, and possibly
   a few more if they need updating and are in the neighborhood.
*/

void
fixchar (y, x)
	int y;
	int x;
{
	int lll = loc (y, 0);

	while (1) {
		int ll = lll + x;

		cursto (y, x);

		if (standoutcmd) {
			attrto (wantattr [ll]);
			putfunc (wantscreen[ll]);
		} else {
			if (wantattr[ll]) putfunkyfunc (wantscreen[ll]);
			else              putfunc      (wantscreen[ll]);
		}

		onscreen[ll] = wantscreen[ll];
		onattr  [ll] = wantattr  [ll];

		x++;
		physx++;
		if (x >= scrcols) return;

		if (wantscreen[lll + x] == onscreen[lll + x] &&
		    wantscreen[lll + x + 1] == onscreen[lll + x + 1] &&
		    wantscreen[lll + x + 2] == onscreen[lll + x + 2] &&

		    wantattr[lll + x] == onattr[lll + x] &&
		    wantattr[lll + x + 1] == onattr[lll + x + 1] &&
		    wantattr[lll + x + 2] == onattr[lll + x + 2])
		{
			return;
		}
	}
}

void
splatonscreen (y)
	int y;
{
	int x;

	for (x = 0; x < scrcols; x++) {
		onattr[loc (y, x)] = 0;
		onscreen[loc (y, x)] = ' ';
	}
}

int
xlocmove (vx, xloc, y)
	int vx;
	int xloc;
	int y;
{
	if ((xloc > vx) && (vx < xloc-vx)) {
		printf ("\r");
		xloc = 0;
	}

	while (xloc > vx) {
		printf ("\b");
		xloc--;
	}

	while (xloc < vx) {
		int ll = loc (y, xloc);

		if (wantattr[ll]) putfunkyfunc (wantscreen[ll]);
		else              putfunc      (wantscreen[ll]);

		xloc++;
	}

	return xloc;
}

int
fixbottomline()
{
	int difference = 0;
	int x;
	int scrloc;

	if (virty == scrrows - 1) return 0;

	scrloc = loc (scrrows - 1, 0);

	for (x = 0; x < scrcols - 1; x++) {
		int ll = scrloc + x;

		if ((onscreen[ll] != wantscreen[ll])
		&&  (wantscreen[ll] != ' ')) difference = 1;
	}

	if (difference) {
		int xloc = 0;

		printf ("\r\n");

		for (x = 0; x < scrcols - 1; x++) {
			int ll = scrloc + x;

			if (onscreen[ll] != wantscreen[ll]
			||  onattr  [ll] != wantattr  [ll]) {
				xloc = xlocmove (x, xloc, scrrows - 1);

				if (wantattr[ll]) putfunkyfunc (wantscreen[ll]);
				else              putfunc      (wantscreen[ll]);

				onscreen[ll] = wantscreen[ll];
				onattr  [ll] = wantattr  [ll];

				xloc++;
			}
		}
		return 1;
	} else {
		return 0;
	}
}

void
openrefresh()
{
	static int oldy = -1;
	static int xloc;
	int x;
	int virtyloc;

	if (deleted == virty) {
		printf ("\r@");
		xloc = 1;
		deleted = -1;
	}

	if (fixbottomline()) inval = 1;

	if (inval || virty != oldy) {
		inval = 0;
		oldy = virty;

		printf ("\n\r");
		splatonscreen (virty);
		xloc = 0;
	}

	virtyloc = loc (virty, 0);

	for (x = 0; x < scrcols - 1; x++) {
		int ll = virtyloc + x;

		if (onscreen[ll] != wantscreen[ll]
		||  onattr  [ll] != wantattr  [ll]) {
			xloc = xlocmove (x, xloc, virty);

			if (wantattr[ll]) putfunkyfunc (wantscreen[ll]);
			else              putfunc      (wantscreen[ll]);

			onscreen[ll] = wantscreen[ll];
			onattr  [ll] = wantattr  [ll];

			xloc++;
		}
	}

	xloc = xlocmove (virtx, xloc, virty);
}

#define UNKNOWN 3
#define EXTRA 7

void
fixscreen ()
{
	register int y, x;

	if (openmode) {
		openrefresh();
		fflush (stdout);
		return;
	}

	if (screendirty) {
		for (y = 0; y < scrrows; y++) {
			int lap = loc (y, 0);

			for (x = 0; x < scrcols; x++) {
				int lyx = lap + x;

				if (onscreen[lyx] != wantscreen[lyx] ||
				    onattr  [lyx] != wantattr  [lyx]) {
					fixchar (y, x);
					x = physx;
				}
			}
		}
	}

/*
	if (setscrollcmd) {
		setphysscroll (virtscrollbot, virtscrolltop);
	}
*/

	cursto (virty, virtx);
	screendirty = 0;

	fflush (stdout);
}
