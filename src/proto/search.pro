/* search.c */
regexp *myregcomp __PARMS((char_u *pat, int sub_cmd, int which_pat));
int searchit __PARMS((FPOS *pos, int dir, char_u *str, long count, int end, int message, int which_pat));
int dosearch __PARMS((int dirc, char_u *str, int reverse, long count, int echo, int message));
int search_for_exact_line __PARMS((FPOS *pos, int dir, char_u *pat));
int searchc __PARMS((int c, register int dir, int type, long count));
FPOS *findmatch __PARMS((int initc));
void showmatch __PARMS((void));
int findfunc __PARMS((int dir, int what, long count));
int findsent __PARMS((int dir, long count));
int findpar __PARMS((register int dir, long count, int what, int both));
int startPS __PARMS((linenr_t lnum, int para, int both));
int fwd_word __PARMS((long count, int type, int eol));
int bck_word __PARMS((long count, int type));
int end_word __PARMS((long count, int type, int stop));
int skip_chars __PARMS((int class, int dir));
void find_pattern_in_path __PARMS((char_u *ptr, int len, int whole, int type, int count, int action, linenr_t start_lnum, linenr_t end_lnum));
int read_viminfo_search_pattern __PARMS((char_u *line, linenr_t *lnum, FILE *fp, int force));
void write_viminfo_search_pattern __PARMS((FILE *fp));
