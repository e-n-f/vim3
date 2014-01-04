#define CLEAREOL "\001"
#define INSERTLN "\002"
#define DELETELN "\003"
#define CLEARSCR "\004"
#define STANDOUT "\005"
#define STANDEND "\006"
#define INITCAP "\013"
#define EXITCAP "\014"

#define MOVETO "yes, we can move the cursor"
#define SCROLLSET "yes, we have scroll regions"

#define CCLEAREOL 1
#define CINSERTLN 2
#define CDELETELN 3
#define CCLEARSCR 4
#define CSTANDOUT 5
#define CSTANDEND 6
#define CINIT 11
#define CEXIT 12

extern char *upkey;
extern char *downkey;
extern char *leftkey;
extern char *rightkey;
