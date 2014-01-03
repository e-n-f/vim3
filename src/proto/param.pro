/* param.c */
void set_init __PARMS((void));
void set_init_shell __PARMS((void));
int doset __PARMS((char_u *arg));
void paramchanged __PARMS((char_u *arg));
int makeset __PARMS((FILE *fd));
void clear_termparam __PARMS((void));
void comp_col __PARMS((void));
void win_copy_options __PARMS((WIN *wp_from, WIN *wp_to));
void buf_copy_options __PARMS((BUF *bp_from, BUF *bp_to));
void set_context_in_set_cmd __PARMS((char_u *arg));
int ExpandSettings __PARMS((regexp *prog, int *num_file, char_u ***file));
int ExpandOldSetting __PARMS((int *num_file, char_u ***file));
char_u *file_pat_to_reg_pat __PARMS((char_u *pat));
void do_autocmd __PARMS((char_u *arg, int force));
void apply_autocmds __PARMS((char_u *fname));
