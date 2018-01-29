#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "panel.h"

#define MAX(a, b) (a>b ? a : b)
#define MIN(a, b) (a<b ? a : b)
#define LIM(a, x, b) MAX(MIN(x, b), a)

#define LOG(form, ...) fprintf(stderr, "%s, line %d: " form "\n", __FILE__, __LINE__, __VA_ARGS__)

#define PNLSIZ 1024

extern Panel panels[PNLSIZ];
extern int selection;
extern int panel_head;
extern int interactive;
