/* edit.c */
int edit __PARMS((long count));
int is_ctrl_x_key __PARMS((int c));
int add_completion_and_infercase __PARMS((char_u *str, int len, int dir));
int get_literal __PARMS((void));
void insertchar __PARMS((unsigned c, int force_formatting));
void set_last_insert __PARMS((int c));
void beginline __PARMS((int flag));
int oneright __PARMS((void));
int oneleft __PARMS((void));
int oneup __PARMS((long n));
int onedown __PARMS((long n));
int onepage __PARMS((int dir, long count));
void stuff_inserted __PARMS((int c, long count, int no_esc));
char_u *get_last_insert __PARMS((void));
void replace_push __PARMS((int c));
int replace_pop __PARMS((void));
void replace_flush __PARMS((void));
