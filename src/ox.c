#include "ox.h"

#include "parse.h"

#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

Frame * selection = NULL;

void free_panel(Panel * pnl){
    free(pnl->pixels);
    free(pnl);
}

void free_frame(Frame * frame){
    free_panel(frame->pnl);
    free(frame);
}

Frame * ins(Frame * prev, Panel * pnl){
    Frame * cur = malloc(sizeof(Frame));
    cur->pnl = pnl;
    cur->prev = prev;
    if(prev){
        Frame * n = prev->next;
        prev->next = cur;
        cur->next = n;
    }else{
        cur->next = NULL;
    }
    return cur;
}

Frame * rem(Frame * frame){
    Frame * p = frame->prev;
    Frame * n = frame->next;
    if(p) p->next = n;
    if(n) n->prev = p;
    free_frame(frame);
    return p;
}

int interactive = 1;

int main(int argc, char ** argv){
    FILE * f = stdin;
    char * input;
    if(argc == 2){
        f = fopen(argv[1], "r");
        interactive = 0;
        dup2(fileno(f), 0);
    }
    if(interactive) read_history(".ox_history");
    while(1){
        input = readline(">");
        if (!input) break;

        if(interactive) add_history(input);
        if(run_comm(input) && !interactive)
            return 1;
        free(input);
    }
    if(interactive) write_history(".ox_history");
    return 0;
}

