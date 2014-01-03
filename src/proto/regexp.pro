/* regexp.c */
char_u *skip_regexp __PARMS((char_u *p, int dirc));
regexp *regcomp __PARMS((char_u *exp));
int regexec __PARMS((register regexp *prog, register char_u *string, int at_bol));
int cstrncmp __PARMS((char_u *s1, char_u *s2, int n));
char_u *cstrchr __PARMS((char_u *s, register int c));
