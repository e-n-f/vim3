/* cmdcmds.c */
void do_align __PARMS((linenr_t start, linenr_t end, int width, int type));
void do_retab __PARMS((linenr_t start, linenr_t end, int new_ts, int force));
int do_move __PARMS((linenr_t line1, linenr_t line2, linenr_t n));
void do_copy __PARMS((linenr_t line1, linenr_t line2, linenr_t n));
void dobang __PARMS((int addr_count, linenr_t line1, linenr_t line2, int forceit, char_u *arg));
void doshell __PARMS((char_u *cmd));
void dofilter __PARMS((linenr_t line1, linenr_t line2, char_u *buff, int do_in, int do_out));
int read_viminfo __PARMS((char_u *file, int want_info, int want_marks, int force));
void write_viminfo __PARMS((char_u *file, int force));
void viminfo_readstring __PARMS((char_u *p));
void viminfo_writestring __PARMS((FILE *fd, char_u *p));
