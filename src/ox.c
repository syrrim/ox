#include "ox.h"

#include "parse.h"

#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

Panel panels[PNLSIZ];
int panel_head = 0;
int selection = -1;

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

