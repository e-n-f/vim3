/* csearch.c */
void dosub __PARMS((linenr_t lp, linenr_t up, char_u *cmd, char_u **nextcommand, int use_old));
void doglob __PARMS((int type, linenr_t lp, linenr_t up, char_u *cmd));
int read_viminfo_sub_string __PARMS((char_u *line, linenr_t *lnum, FILE *fp, int force));
void write_viminfo_sub_string __PARMS((FILE *fp));
