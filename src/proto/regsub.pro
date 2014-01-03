/* regsub.c */
char_u *regtilde __PARMS((char_u *source, int magic));
int regsub __PARMS((regexp *prog, char_u *source, char_u *dest, int copy, int magic));
