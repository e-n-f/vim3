/* alloc.c */
char_u *alloc __PARMS((unsigned size));
char_u *alloc_check __PARMS((unsigned size));
char_u *lalloc __PARMS((long_u size, int message));
void do_outofmem_msg __PARMS((void));
char_u *strsave __PARMS((char_u *string));
char_u *strnsave __PARMS((char_u *string, int len));
void copy_spaces __PARMS((char_u *ptr, size_t count));
void del_trailing_spaces __PARMS((char_u *ptr));
void nofreeNULL __PARMS((void *x));
char *bsdmemset __PARMS((char *ptr, int c, long size));
int vim_strnicmp __PARMS((char_u *s1, char_u *s2, size_t len));
