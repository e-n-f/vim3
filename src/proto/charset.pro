/* charset.c */
char_u *transchar __PARMS((int c));
int charsize __PARMS((int c));
int strsize __PARMS((char_u *s));
int chartabsize __PARMS((register int c, long col));
int linetabsize __PARMS((char_u *s));
int isidchar __PARMS((int c));
int isidchar_id __PARMS((int c));
