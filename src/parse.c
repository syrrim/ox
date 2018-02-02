#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ox.h"
#include "parse.h"
#include "commands.h"
#include "text.h"

#define EAT_SPC(str) while(*str == ' ')str++
char * next_word(char ** buf){
    char * start = *buf;
    if(**buf == '"'){
        start++;
        (*buf)++;
        while(**buf != '"' && **buf != '\0')(*buf)++;
    }else{
        while(**buf != ' ' && **buf != '\0')(*buf)++;
    }
    if(**buf)*((*buf)++) = '\0';
    EAT_SPC((*buf));
    return start;
}

int next_text(char ** buf, char ** text){
    *text = next_word(buf);
    return 0;
}

int next_fn(char ** buf, char ** fn){
    *fn = next_word(buf);
    return 0;
}

int next_identifier(char ** buf, char ** id){
    *id = next_word(buf);

    if(!ISAL(**id))
        ERR("invalid identifier start %c\n", **id);
    for(char *c = *id; *c; c++){
        if(!ISAL(*c) && !ISNUM(*c))
            ERR("invalid identifier char %c\n", *c);
    }
    return 0;
}

int next_int(char ** buf, int * val){
    char * end;
    char * n = next_word(buf);
    if(!*n) ERR("no int\n");
    *val = strtol(n, &end, 0);
    if(*end) ERR("trailing int: %s\n", end);
    return 0;
}

int next_float(char ** buf, double * val){
    char * end;
    char * n = next_word(buf);
    if(!*n) ERR("no float\n");
    *val = strtod(n, &end);
    printf("'%s':%f\n", n, *val);
    if(*end) ERR("trailing float: %s\n", end);
    return 0;
}

int next_color(char ** buf, Pixel * p){
#define DIG(ch, val)\
    if('0'<=ch&&ch<='9')val|=ch-'0';\
    else if('A'<=ch&&ch<='F')val|=ch-'A'+10;\
    else if('a'<=ch&&ch<='f')val|=ch-'a'+10;\
    else ERR("invalid color char %c\n", ch)
    char * n = next_word(buf);
    int l = strlen(n);
#define CPY(c, i1, i2)\
    p->c=0;\
    DIG(n[i1], p->c);\
    p->c<<=4;\
    DIG(n[i2], p->c)
#define DUB(c, i) CPY(c, i, i)
#define SIG(c, i) CPY(c, i, i+1)
    if(l == 3){
        DUB(r, 0);
        DUB(g, 1);
        DUB(b, 2);
        p->a = 255;
    }else if(l == 4){
        DUB(r, 0);
        DUB(g, 1);
        DUB(b, 2);
        DUB(a, 3);
    }else if(l==6){
        SIG(r, 0);
        SIG(g, 2);
        SIG(b, 4);
        p->a = 255;
    }else if(l==8){
        SIG(r, 0);
        SIG(g, 2);
        SIG(b, 4);
        SIG(a, 6);
    }else{
        ERR("invalid color %s\n", n);
    }
    return 0;
#undef SIG
#undef DUB
#undef CPY
#undef DIG
}

int run_comm(char * inp){
#define ARG(type, var)\
    if(next_ ## type (&inp, &var))\
        return 1
    EAT_SPC(inp);
    char * comm = next_word(&inp);
    if(strcmp("import", comm) == 0){
        char * fn;
        ARG(fn, fn);
        if(*inp)goto trail;
        return import(fn);
    }else if(strcmp("export", comm) == 0){
        char * fn;
        ARG(fn, fn);
        if(*inp)goto trail;
        return export(fn);
    }else if(strcmp("rotate", comm) == 0){
        double angle;
        ARG(float, angle);
        if(*inp)goto trail;
        return rotate(angle);
    }else if(strcmp("scale", comm) == 0){
        double factor;
        ARG(float, factor);
        if(*inp)goto trail;
        return scale(factor);
    }else if(strcmp("blur", comm) == 0){
        int rad;
        int passes;
        ARG(int, rad);
        if(*inp){
            ARG(int, passes);
        }else
            passes = 1;
        if(*inp)goto trail;
        return blur(rad, passes);
    }else if(strcmp("gaussian_blur", comm) == 0){
        int rad;
        ARG(int, rad);
        if(*inp)goto trail;
        return gaussian_blur(rad);
    }else if(strcmp("sharpen", comm) == 0){
        if(*inp)goto trail;
        return sharpen();
    }else if(strcmp("collate", comm) == 0){
        if(*inp)goto trail;
        return collate();
    }else if(strcmp("prev", comm) == 0){
        if(*inp)goto trail;
        return prev();
    }else if(strcmp("next", comm) == 0){
        if(*inp)goto trail;
        return next();
    }else if(strcmp("dup", comm) == 0){
        if(*inp)goto trail;
        return dup();
    }else if(strcmp("swap", comm) == 0){
        if(*inp)goto trail;
        return swap();
    }else if(strcmp("crop", comm) == 0){
        Mark tl, br;
        if(*inp){
            ARG(mark, tl);
            if(*inp){
                ARG(mark, br);
            }else
                br = (Mark){.x=get_width(&panels[selection]), 
                            .y=get_height(&panels[selection])};
        }else{
            printf("no mark\n");
            tl = (Mark){.x=0, .y=0};
            br = (Mark){.x=get_width(&panels[selection]), 
                        .y=get_height(&panels[selection])};
        }
        if(*inp)goto trail;
        return crop(tl, br);
    }else if(strcmp("mark", comm) == 0){
        Mark mark;
        ARG(mark, mark);
        if(*inp){
            char * name;
            ARG(identifier, name);
            if(*inp)goto trail;
            mark_set(name, mark);
            return 0;
        }else{
            printf("%d,%d\n", mark.x, mark.y);
            return 0;
        }
    }else if(strcmp("line", comm) == 0){
        Mark s, e;
        Pixel p;
        ARG(mark, s);
        ARG(mark, e);
        if(*inp){
            ARG(color, p);
        }else
            p = (Pixel){.a=255};
        if(*inp)goto trail;
        return line(s, e, p);
    }else if(strcmp("font", comm) == 0){
        char * id;
        char * fn;
        ARG(identifier, id);
        ARG(fn, fn);
        if(*inp)goto trail;
        return load_font(id, fn);
    }else if(strcmp("text", comm) == 0){
        char * text;
        char * font;
        Mark mark;
        ARG(text, text);
        ARG(identifier, font);
        ARG(mark, mark);
        if(*inp)goto trail;
        return draw_text(text, font, mark, 50, (Pixel){.r=255,.g=255,.b=255,.a=255});
    }
    ERR("unrecognized command %s\n", comm);

trail:
    ERR("trailing input %s\n", inp);
}

