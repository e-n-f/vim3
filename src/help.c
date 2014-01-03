/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * help.c: open a read-only window on the vim_help.txt file
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"

	void
dohelp(arg)
	char_u		*arg;
{
	char_u	*fnamep;
	FILE	*helpfd;			/* file descriptor of help file */
	int		n;

/*
 * try to open the file specified by the "helpfile" option
 */
	fnamep = p_hf;
	if ((helpfd = fopen((char *)p_hf, READBIN)) == NULL)
	{
#if defined(MSDOS) && !defined(NT)
	/*
	 * for MSDOS: try the DOS search path
     */
		fnamep = searchpath("vim_help.txt");
		if (fnamep == NULL || (helpfd = fopen((char *)fnamep, READBIN)) == NULL)
		{
			smsg((char_u *)"Sorry, help file \"%s\" and \"vim_help.txt\" not found", p_hf);
			return;
		}
#else
		smsg((char_u *)"Sorry, help file \"%s\" not found", p_hf);
		return;
#endif
	}
	fclose(helpfd);

	if (win_split(p_hh, FALSE) == FAIL)
		return;

	/* open help file in readonly mode */
	n = readonlymode;
	readonlymode = TRUE;
	(void)doecmd(0, fnamep, NULL, NULL, TRUE, (linenr_t)0);
	readonlymode = n;
		/* set help flag, use "vim_tags" instead of "tags" file */
	curbuf->b_help = TRUE;
		/* accept many characters for identifier, except white space and '|' */
	free(curbuf->b_p_id);
	curbuf->b_p_id = strsave((char_u *)"!\"#$%&'()+,-./:;<=>?@[\\]^_`{}~");

	if (*arg != NUL)
	{
#ifdef ADDED_BY_WEBB_COMPILE
		stuffReadbuff((char_u *)":ta ");
#else
		stuffReadbuff(":ta ");
#endif /* ADDED_BY_WEBB_COMPILE */
		stuffReadbuff(arg);
		stuffcharReadbuff('\n');
	}
}
