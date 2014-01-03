/* fileio.c */
void filemess __PARMS((char_u *name, char_u *s));
int readfile __PARMS((char_u *fname, char_u *sfname, linenr_t from, int newfile, linenr_t skip_lnum, linenr_t nlines));
int buf_write __PARMS((BUF *buf, char_u *fname, char_u *sfname, linenr_t start, linenr_t end, int append, int forceit, int reset_changed));
char_u *modname __PARMS((char_u *fname, char_u *ext));
char_u *buf_modname __PARMS((BUF *buf, char_u *fname, char_u *ext));
int vim_fgets __PARMS((char_u *buf, int size, FILE *fp, linenr_t *lnum));
int vim_rename __PARMS((char_u *from, char_u *to));
