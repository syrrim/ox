#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#define MAX(a, b) (a>b ? a : b)
#define MIN(a, b) (a<b ? a : b)
#define LIM(a, x, b) MAX(MIN(x, b), a)

#define ABS(a) ((a)>0?(a):-(a))

#define LERP(l, u, d) ((l)*(1-d) + (u)*(d))
#define LERP2(tl, tr, bl, br, dx, dy) LERP(LERP(tl, tr, dx), LERP(bl, br, dx), dy)

#define EAT_SPC(s) while(*(s) == ' ') (s)++

#define ISEOL(c) ('\0'==c || '#'==c)
#define ISAL(c) (('A'<=c&&c<='Z') || ('a'<=c&&c<='z') || '_'==c) 
#define ISNUM(c) ('0'<=c&&c<='9')
#define ISALNUM(c) (ISAL(c) || ISNUM(c))

#define LOG(form, ...) fprintf(stderr, "%s, line %d: " form "\n", __FILE__, __LINE__, __VA_ARGS__)
#define ERR(...) do{fprintf(stderr, __VA_ARGS__);return 1;}while(0)

#define CHN uint8_t
#define MAXCHN 255

typedef struct {
    CHN r, g, b, a;
} Pixel;

typedef struct {
    int w, h;
    int x, y;
    Pixel * pixels;
} Panel;

void free_panel(Panel * pnl);

typedef struct Frame {
    Panel * pnl;
    struct Frame * prev;
    struct Frame * next;
} Frame;

typedef struct Mark{
    int x, y;
} Mark;

extern Frame * selection;

void free_frame(Frame * frame);

Frame * ins(Frame * prev, Panel * pnl);

Frame * rem(Frame * frame);

extern int interactive;
