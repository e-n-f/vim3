/* tag.c */
void dotag __PARMS((char_u *tag, int type, int count));
void dotags __PARMS((void));
int ExpandTags __PARMS((regexp *prog, int *num_file, char_u ***file, int help_only));
