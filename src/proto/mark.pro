/* mark.c */
int setmark __PARMS((int c));
void setpcmark __PARMS((void));
void checkpcmark __PARMS((void));
FPOS *movemark __PARMS((int count));
FPOS *getmark __PARMS((int c, int changefile));
void clrallmarks __PARMS((BUF *buf));
char_u *fm_getname __PARMS((struct filemark *fmark));
void domarks __PARMS((void));
void dojumps __PARMS((void));
void mark_adjust __PARMS((linenr_t line1, linenr_t line2, long amount));
