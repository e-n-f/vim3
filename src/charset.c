/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"


	char_u *
transchar(c)
	int	 c;
{
	static char_u	buf[5];
	int				i;

	i = 0;
	if (c >= 0x100)		/* special key code, display as ~@ char */
	{
		buf[0] = '~';
		buf[1] = '@';
		i = 2;
		c = K_SECOND(c);
	}
	if (c < ' ' || c == DEL)
	{
		if (c == NL)
			c = NUL;			/* we use newline in place of a NUL */
		buf[i] = '^';
		buf[i + 1] = c ^ 0x40;		/* DEL displayed as ^? */
		buf[i + 2] = NUL;
	}
	else if (c <= '~' || c > 0xa0 || p_gr)
	{
		buf[i] = c;
		buf[i + 1] = NUL;
	}
	else
	{
		buf[i] = '~';
		buf[i + 1] = c - 0x80 + '@';
		buf[i + 2] = NUL;
	}
	return buf;
}

/*
 * return the number of characters 'c' will take on the screen
 */
	int
charsize(c)
	int c;
{
	int		len = 0;

	if (c >= 0x100)
	{
		len = 2;
		c = K_SECOND(c);
	}
	len += ((c >= ' ' && (p_gr || c <= '~')) || c > 0xa0 ? 1 : 2);
	return len;
}

/*
 * return the number of characters string 's' will take on the screen
 */
	int
strsize(s)
	char_u *s;
{
	int	len = 0;

	while (*s)
		len += charsize(*s++);
	return len;
}

/*
 * return the number of characters 'c' will take on the screen, taking
 * into account the size of a tab
 */
	int
chartabsize(c, col)
	register int	c;
	long			col;
{
	if ((c >= ' ' && (c <= '~' || p_gr)) || c > 0xa0)
   		return 1;
   	else if (c == TAB && !curwin->w_p_list)
   		return (int)(curbuf->b_p_ts - (col % curbuf->b_p_ts));
   	else
		return 2;
}

/*
 * return the number of characters the string 's' will take on the screen,
 * taking into account the size of a tab
 */
	int
linetabsize(s)
	char_u		*s;
{
	int		col = 0;

	while (*s != NUL)
		col += chartabsize(*s++, col);
	return col;
}

/*
 * return TRUE if 'c' is an identifier character
 */
	int
isidchar(c)
	int c;
{
		if (c > 0x100 || c == NUL)
			return FALSE;
		return (
#ifdef __STDC__
				isalnum(c)
#else
				isalpha(c) || isdigit(c)
#endif
				|| (curbuf->b_p_id != NULL && STRCHR(curbuf->b_p_id, c) != NULL)
	/*
	 * we also accept alpha's with accents
	 */
#ifdef MSDOS
				|| (c >= 0x80 && c <= 0xa7) || (c >= 0xe0 && c <= 0xeb)
#else
				|| (c >= 0xc0 && c <= 0xff)
#endif
				);
}
