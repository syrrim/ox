
typedef struct Mark{
    int x, y;
} Mark;

void mark_set(char * name, Mark mark);

int mark_get(char * name, Mark * mark);

int next_mark(char ** buf, Mark * mark);
