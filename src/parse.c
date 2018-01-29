#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "parse.h"
#include "commands.h"

#define EAT_SPC(str) while(*str == ' ')str++
char * next_word(char ** buf){
    char * start = *buf;
    while(**buf != ' ' && **buf != '\0')(*buf)++;
    if(**buf)*((*buf)++) = '\0';
    EAT_SPC((*buf));
    return start;
}

char * next_ARG_FN(char ** buf){
    return next_word(buf);
}

char * next_ARG_ID(char ** buf){
    return next_word(buf);
}

double next_ARG_INT(char ** buf){
    return atoi(next_word(buf));
}

double next_ARG_FLT(char ** buf){
    return atof(next_word(buf));
}

Mark next_ARG_MRK(char ** buf){
    char * x = next_word(buf);
    char * y = x;
    while(*y){
        if(*y == ','){
            *(y++) = '\0';
            break;
        }
        y++;
    }
    return (Mark){.x=atoi(x), .y=atoi(y)};
}

/** 
 * This method determines the command in `input`, then creates the arguments
 * for that comand from `input`. It generates the argument evaluation code for
 * each method automatically from a definition provided by commands.h. The
 * equivalent code for a command called `command`, with arg `ARG_ARG` is:
 *     typeof(next_ARG_ARG(&input)) aARG_ARG0;
 *     if(strcmp("command", comm)==0){
 *          aARG_ARG0 = next_ARG_ARG(&input);
 *          if(*input)goto trail;
 *          return command(aARG_ARG0);
 *     }
 */
int run_comm(char * input){
    char * comm = next_word(&input);
#pragma GCC diagnostic ignored "-Wunused-variable"
#define DECL(ind, arg)\
    typeof((next_ ## arg)(&input)) a_ ## arg ## ind;
#define LSIND(_,arg) _(0, arg) _(1, arg) _(2, arg)
#define LSARG(_) _(ARG_FN) _(ARG_ID) _(ARG_FLT) _(ARG_INT) _(ARG_MRK)
#define HDECL(arg) LSIND(DECL, arg)
    LSARG(HDECL)

#define GENARG(type, ind) a_ ## type ## ind = next_ ## type(&input)
#define CHECK(name, ...)            \
    if(strcmp(#name, comm) == 0){   \
        __VA_ARGS__;                \
        if(*input) goto trail;      \
        goto label_ ## name;        \
    }else
    COMMANDS(CHECK, GENARG);

#define USEARG(type, ind) (a_ ## type ## ind)
#define CALL(name, ...)         \
label_ ## name:                 \
    return name(__VA_ARGS__);

    COMMANDS(CALL, USEARG)

    fprintf(stderr, "unrecognized command %s\n", comm);
    return 1;

trail:
    fprintf(stderr, "trailing input %s\n", input);
    return 1;
}

