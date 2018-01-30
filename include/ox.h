#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "panel.h"

#define MAX(a, b) (a>b ? a : b)
#define MIN(a, b) (a<b ? a : b)
#define LIM(a, x, b) MAX(MIN(x, b), a)

#define ISAL(c) (('A'<=c&&c<='Z') || ('a'<=c&&c<='z') || '_'==c) 
#define ISNUM(c) ('0'<=c&&c<='9')
#define ISALNUM(c) (ISAL(c) || ISNUM(c))

#define LOG(form, ...) fprintf(stderr, "%s, line %d: " form "\n", __FILE__, __LINE__, __VA_ARGS__)
#define ERR(...)do{fprintf(stderr, __VA_ARGS__);return 1;}while(1)

#define PNLSIZ 1024

extern Panel panels[PNLSIZ];
extern int selection;
extern int panel_head;
extern int interactive;
