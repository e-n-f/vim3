/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * This file contains the machine dependent escape sequences that the editor
 * needs to perform various operations. Some of the sequences here are
 * optional. Anything not available should be indicated by a null string. In
 * the case of insert/delete line sequences, the editor checks the capability
 * and works around the deficiency, if necessary.
 */

#ifdef SASC
/*
 * the SAS C compiler has a bug that makes typedefs being forget sometimes
 */
typedef unsigned char char_u;
#endif

/*
 * the terminal capabilities are stored in this structure
 * IMPORTANT: When making changes, note the following:
 * - keep the arrays in term.c and get_key_names() in sync
 * - adjust the definitions in keymap.h
 * - avoid changing the order of the key codes, add new ones at the end
 *   (Otherwise special keys in script files will break)
 */
typedef struct _tcarr
{
  char_u *t_name;	/* name of this terminal entry */

/* key codes */
#define T_FIRST_KEY	t_ku
  char_u *t_ku;		/* kcuu1    ku	arrow up */
  char_u *t_kd;		/* kcud1    kd	arrow down */
  char_u *t_kl;		/* kcub1    kl	arrow left */
  char_u *t_kr;		/* kcuf1    kr	arrow right */
  char_u *t_sku;	/*				shift arrow up */
  char_u *t_skd;	/*				shift arrow down */
  char_u *t_skl;	/* kLFT     #4	shift arrow left */
  char_u *t_skr;	/* kRIT     %	shift arrow right */

  char_u *t_f1;		/* kf1      k1	function key 1 */
  char_u *t_f2;		/* kf2      k2	function key 2 */
  char_u *t_f3;		/* kf3      k3	function key 3 */
  char_u *t_f4;		/* kf4      k4	function key 4 */
  char_u *t_f5;		/* kf5      k5	function key 5 */
  char_u *t_f6;		/* kf6      k6	function key 6 */
  char_u *t_f7;		/* kf7      k7	function key 7 */
  char_u *t_f8;		/* kf8      k8	function key 8 */
  char_u *t_f9;		/* kf9      k9	function key 9 */
  char_u *t_f10;	/* kf10     k;	function key 10 */

  char_u *t_sf1;	/* kf11     F1	shifted function key 1 */
  char_u *t_sf2;	/* kf12     F2	shifted function key 2 */
  char_u *t_sf3;	/* kf13     F3	shifted function key 3 */
  char_u *t_sf4;	/* kf14     F4	shifted function key 4 */
  char_u *t_sf5;	/* kf15     F5	shifted function key 5 */
  char_u *t_sf6;	/* kf16     F6	shifted function key 6 */
  char_u *t_sf7;	/* kf17     F7	shifted function key 7 */
  char_u *t_sf8;	/* kf18     F8	shifted function key 8 */
  char_u *t_sf9;	/* kf19     F9	shifted function key 9 */
  char_u *t_sf10;	/* kf20     FA	shifted function key 10 */

  char_u *t_help;	/* khlp     %1	help key */
  char_u *t_undo;	/* kund     &8	undo key */

  char_u *t_ins;	/* kins		kI  insert key */
  char_u *t_del;	/* kdel		kD  delete key */
  char_u *t_home;	/* khome    kh	home key */
  char_u *t_end;	/* kend     @7	end key */
  char_u *t_pu;		/* kpp      kP	page up key */
  char_u *t_pd;		/* knp      kN	page down key */

  char_u *t_mouse;	/*				start of mouse click */
#define T_LAST_KEY t_mouse

/* output codes */
  char_u *t_el;		/* el       ce	clear to end of line */
  char_u *t_il;		/* il1      al	add new blank line */
  char_u *t_cil;	/* il       AL	add number of blank lines */
  char_u *t_dl;		/* dl1      dl	delete line */
  char_u *t_cdl;	/* dl       DL	delete number of lines */
  char_u *t_cs;		/*          cs	scroll region */
  char_u *t_ed;		/* clear    cl	clear screen */
  char_u *t_ci;		/* civis    vi	cursor invisible */
  char_u *t_cv;		/* cnorm    ve	cursor visible */
  char_u *t_cvv;	/* cvvis    vs  cursor very visible */
  char_u *t_tp;		/* sgr0     me	normal mode */
  char_u *t_ti;		/* rev      mr	reverse mode */
  char_u *t_tb;		/* bold     md	bold mode */
  char_u *t_se;		/* rmso     se	normal mode */
  char_u *t_so;		/* smso     so	standout mode */
  char_u *t_ue;		/* rmul     ue	exit underscore mode */
  char_u *t_us;		/* smul     us	underscore mode */
  char_u *t_ms;		/* msgr     ms	save to move cursor in reverse mode */
  char_u *t_cm;		/* cup      cm	cursor motion */
  char_u *t_sr;		/* ri       sr	scroll reverse (backward) */
  char_u *t_cri;	/* cuf      RI	cursor number of chars right */
  char_u *t_vb;		/* flash    vb	visual bell */
  char_u *t_ks;		/* smkx     ks	put terminal in "keypad transmit" mode */
  char_u *t_ke;		/* rmkx     ke	out of "keypad transmit" mode */
  char_u *t_ts;		/*          ti	put terminal in termcap mode */
  char_u *t_te;		/*          te	out of termcap mode */

  char_u *t_csc;	/* -		-	cursor relative to scrolling region */
} Tcarr;

extern Tcarr term_strings;	/* currently used terminal strings */

/*
 * strings used for terminal
 */
#define T_EL	(term_strings.t_el)
#define T_IL	(term_strings.t_il)
#define T_CIL	(term_strings.t_cil)
#define T_DL	(term_strings.t_dl)
#define T_CDL	(term_strings.t_cdl)
#define T_CS	(term_strings.t_cs)
#define T_ED	(term_strings.t_ed)
#define T_CI	(term_strings.t_ci)
#define T_CV	(term_strings.t_cv)
#define T_CVV	(term_strings.t_cvv)
#define T_TP	(term_strings.t_tp)
#define T_TI	(term_strings.t_ti)
#define T_TB	(term_strings.t_tb)
#define T_SE	(term_strings.t_se)
#define T_SO	(term_strings.t_so)
#define T_UE	(term_strings.t_ue)
#define T_US	(term_strings.t_us)
#define T_MS	(term_strings.t_ms)
#define T_CM	(term_strings.t_cm)
#define T_SR	(term_strings.t_sr)
#define T_CRI	(term_strings.t_cri)
#define T_VB	(term_strings.t_vb)
#define T_KS	(term_strings.t_ks)
#define T_KE	(term_strings.t_ke)
#define T_TS	(term_strings.t_ts)
#define T_TE	(term_strings.t_te)
#define T_CSC	(term_strings.t_csc)
