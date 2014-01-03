/* normal.c */
void normal __PARMS((void));
void start_visual_highlight __PARMS((void));
int find_ident_under_cursor __PARMS((char_u **string, int try_string));
void clear_showcmd __PARMS((void));
int add_to_showcmd __PARMS((int c));
void push_showcmd __PARMS((void));
void pop_showcmd __PARMS((void));
