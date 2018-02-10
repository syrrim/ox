#include "ox.h"
#include "mark.h"
#include "uthash.h"

typedef struct NamedMark {
    char * name;
    Mark mark;
    UT_hash_handle hh;
} NamedMark;

NamedMark * named_marks = NULL;

char * symbolic_marks[] = {"w", "h", NULL};

void mark_set(char * name, Mark mark){
    for(int i=0; symbolic_marks[i]; i++)
        if(strcmp(symbolic_marks[i], name)==0) return;

    int n = strlen(name) + 1; // include the \0
    char * new_name = malloc(n * sizeof(char));
    strncpy(new_name, name, n);
    
    NamedMark * nm = malloc(sizeof(NamedMark));
    *nm = (NamedMark){.name=new_name, .mark=mark};
    NamedMark * old;
    HASH_REPLACE_STR(named_marks, name, nm, old);
    if(old){
        free(old->name);
        free(old);
    }
}

int mark_get(char * name, Mark * mark){
    for(int i=0; symbolic_marks[i]; i++)
        if(strcmp(symbolic_marks[i], name)==0) switch(i){
        case 0://w
            *mark = (Mark){.x=selection->pnl->w};
            return 0;
        case 1://h
            *mark = (Mark){.y=selection->pnl->h};
            return 0;
        }

    NamedMark * nm;
    HASH_FIND_STR(named_marks, name, nm);
    if(nm){
        *mark = nm->mark;
        return 0;
    }
    ERR("unrecognized mark %s\n", name);
}

int sum(char ** buf, Mark * mark);

int prod(char ** buf, Mark * mark){
    *mark = (Mark){1,1};
    Mark next;
    int dir = 0;
    while(1){
        if(ISAL(**buf)){
            char * word = *buf;
            while(ISALNUM(**buf)) (*buf)++;
            char temp = **buf;
            **buf = '\0';
            if(mark_get(word, &next))
                return 1;
            **buf = temp;
        }else if(ISNUM(**buf)){
            char * end;
            int num = strtol(*buf, &end, 0);
            next = (Mark){num,num};
            *buf = end;
        }else if(**buf == '('){
            (*buf)++;
            EAT_SPC(*buf);
            if(next_mark(buf, &next))return 1;
            if(**buf != ')'){
                ERR("unmatched paren: %s\n", *buf);
            }
            (*buf)++;
        }else{
            ERR("invalid mark: %s\n", *buf);
        }
        if(dir>=0){
            mark->x *= next.x;
            mark->y *= next.y;
        }else{
            mark->x /= next.x;
            mark->y /= next.y;
        }
        EAT_SPC(*buf);
        if(**buf == '*'){
            dir = 1;
        }else if(**buf == '/'){
            dir = -1;
        }else{
            return 0;
        }
        (*buf)++;
        EAT_SPC(*buf);
    }
}

int sum(char ** buf, Mark * mark){
    *mark = (Mark){0,0};
    Mark next;
    int dir = 0;
    while(1){
        if(**buf=='-'){
            (*buf)++;
            if(prod(buf, &next))return 1;

            next.x *= -1;
            next.y *= -1;
        }else if(prod(buf, &next))return 1;
        if(dir>=0){
            mark->x += next.x;
            mark->y += next.y;
        }else{
            mark->x -= next.x;
            mark->y -= next.y;
        }
        if(**buf == '+'){
            dir = 1;
        }else if(**buf == '-'){
            dir = -1;
        }else{
            return 0;
        }
        (*buf)++;
        EAT_SPC(*buf);
    }
}

int next_mark(char ** buf, Mark * mark){
    if(sum(buf, mark))return 1;
    if(**buf == ','){
        Mark y;
        (*buf)++;
        EAT_SPC(*buf);
        if(sum(buf, &y))return 1;
        mark->y = y.y;
    }
    return 0;
}
