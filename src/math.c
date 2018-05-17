#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "new.h"

enum ops {ADD, SUB, MUL, DIV, MOD, APP, EQ, LT, GT, LPAR, RPAR};
typedef struct {
    enum {KNUM, KSYM, KVAR, KEND, KERR} type;
    union {double f; int i;};
} Tok;
typedef struct{ 
    char *buf; 
    char c; 
    Tok k; 
    char ** dict;
} Parser;
typedef struct Expr Expr;
struct Expr {
    enum {BINOP, VAR, NUM} type;
    union{
        struct {
            enum ops op;
            Expr * arg1;
            Expr * arg2;
        };
        int i; 
        double f;
    };
};

void free_expr(Expr * expr){
    if(expr->type==BINOP){
        free_expr(expr->arg1);
        free_expr(expr->arg2);
    }
    free(expr);
}

void free_dict(char ** dict){
    while(*dict) free(*(dict++));
}

char adv_char(Parser * p){
    char old = p->c;
    if(old)
        p->c = *(++p->buf);
    return old;
}

#define OPSYM(c) (c=='+'?ADD: c=='-'?SUB: \
                  c=='*'?MUL: c=='/'?DIV: c=='%'?MOD: \
                  c=='='?EQ: c=='<'?LT: c=='>'?GT: \
                  c=='('?LPAR: c==')'?RPAR: -1)

#define ISSPC(c) (c==' ')
#define ISLET(c) (('a'<=c && c<='z') || ('A'<=c && c<='Z'))
#define ISDIG(c) ('0'<=c && c<='9')
#define ISWRD(c) (ISLET(c) || ISDIG(c) || c=='_')
#define ISFLT(c) (ISWRD(c) || c=='.')
#define EAT(p, pred) while(pred(p->c)) adv_char(p)
#define EATSPC(p) EAT(p, ISSPC)
static char lexbuf[1024];
#define GET(p, pred)                    \
        ({                              \
            int i = 0;                  \
            while(pred(p->c)&&i<1023)   \
                lexbuf[i++] = adv_char(p);   \
            lexbuf[i] = '\0';           \
            lexbuf;                     \
        })


Tok adv_tok(Parser * p){
    Tok old = p->k;
    int op;
    EATSPC(p);
    if(!p->c)
        p->k = (Tok){.type=KEND};
    else if(ISLET(p->c)){
        char * wrd = GET(p, ISWRD);
        int i;
        for(i=0; p->dict[i]; i++)
            if(strcmp(wrd, p->dict[i])==0)
                break;
        if(!p->dict[i])
            p->dict[i] = strdup(wrd);
        p->k = (Tok){.type=KVAR, .i=i};
    }else if(ISFLT(p->c))
        p->k = (Tok){.type=KNUM, .f=atof(GET(p, ISFLT))};
    else if((op=OPSYM(p->c)) != -1){
        adv_char(p);
        p->k = (Tok){.type=KSYM, .i=op};
    }else
        p->k = (Tok){.type=KERR};
    return old;
}

Expr * next_sum(Parser * p);

Expr * next_atom(Parser * p){
    Expr * expr;
    if(p->k.type == KNUM) 
        return NEW((Expr){.type=NUM, .f=adv_tok(p).f});
    if(p->k.type == KVAR){
        return NEW((Expr){.type=VAR, .i=adv_tok(p).i});
    }
    if(p->k.type == KSYM && p->k.i == LPAR){
        adv_tok(p);
        expr = next_sum(p);
        if(p->k.type != KSYM || p->k.i != RPAR){
            p->k.type = KERR;
            return NULL;
        }
        adv_tok(p);
        return expr;
    }
    return NULL;
}
Expr * next_appl(Parser * p){
    Expr * expr = next_atom(p);
    Expr * call = next_atom(p);
    if(call)
        expr = NEW((Expr){.type=BINOP, .op=APP, .arg1=expr, .arg2=call});
    return NULL;
}
Expr * next_neg(Parser * p){
    int neg = 0;
    while(p->k.type == KSYM && p->k.i==SUB){
        neg ^= 1;
        adv_tok(p);
    }
    Expr * expr = next_atom(p);
    if(neg)
        expr = NEW((Expr){.type=BINOP, .op=MUL, 
                .arg1=NEW((Expr){.type=NUM, .f=-1}),
                .arg2=expr});
    return expr;
}
Expr * next_prod(Parser * p){
    Expr * expr = next_neg(p);
    while(p->k.type == KSYM && (p->k.i==MUL || p->k.i==DIV || p->k.i==MOD)){
        expr = NEW((Expr){.type=BINOP, .op=adv_tok(p).i, .arg1=expr});
        expr->arg2=next_neg(p);
    }
    return expr;
}
#define NEGATE(expr) NEW((Expr){.type=BINOP, .op=MUL, \
                    .arg1=NEW((Expr){.type=NUM, .f=-1}), \
                    .arg2=expr})
#define RECIPR(expr) NEW((Expr){.type=BINOP, .op=DIV, \
                    .arg1=NEW((Expr){.type=NUM, .f=1}), \
                    .arg2=expr})
Expr * next_sum(Parser * p){
    Expr * expr = next_prod(p);
    while(p->k.type == KSYM && (p->k.i==ADD || p->k.i==SUB)){
        expr = NEW((Expr){.type=BINOP, .op=ADD, .arg1=expr});
        if(adv_tok(p).i==SUB)
            expr->arg2=NEGATE(next_prod(p));
        else
            expr->arg2=next_prod(p);
    }
    return expr;
}
Expr * next_cmp(Parser * p){
    Expr * expr = next_sum(p);
    while(p->k.type == KSYM && (p->k.i==EQ || p->k.i==LT || p->k.i == GT)){
        expr = NEW((Expr){.type=BINOP, .op=adv_tok(p).i, .arg1=expr});
        expr->arg2=next_sum(p);
    }
    return expr;
}
Expr * parse(char * txt, char ** dict){
    Parser p = {.buf=txt, .dict=dict};
    p.c = *txt;
    adv_tok(&p);
    return next_cmp(&p);
}

int sprint_expr(char * buf, Expr * expr, char ** dict){
    switch(expr->type){
        case NUM: return sprintf(buf, "%g", expr->f);
        case VAR: return sprintf(buf, "%s", dict[expr->i]);
        case BINOP:{
            *buf++ = '(';
            int a = sprint_expr(buf, expr->arg1, dict);
            buf += a;
            char c;
            for(c=0; OPSYM(c)!=expr->op && c<256; c++);
            *buf++ = c;
            int b = sprint_expr(buf, expr->arg2, dict);
            buf += b;
            *buf++ = ')';
            *buf = '\0';
            return 3+a+b;
        }
    }
    return 0;
}

void print_expr(Expr * expr, char ** dict){
    char buf[128];
    sprint_expr(buf, expr, dict);
    printf("%s\n", buf);
}

Expr * deep_copy(Expr * expr){
    Expr * new = malloc(sizeof(Expr));
    switch(expr->type){
        case NUM: 
        case VAR: 
            memcpy(new, expr, sizeof(Expr)); 
            break;
        case BINOP:
            *new = (Expr){.type=BINOP, .op=expr->op, 
                        .arg1=deep_copy(expr->arg1), 
                        .arg2=deep_copy(expr->arg2)};
            break;
    }
    return new;
}

#ifndef EPS
# define EPS 1e-12
#endif
#ifndef ABS
# define ABS(a) ((a)>0?(a):-(a))
#endif
int eq(Expr * a, Expr * b){
    if(a->type != b->type) return 0;
    switch(a->type){
        case NUM: return ABS(a->f - b->f) <= EPS;
        case VAR: return a->i == b->i;
        case BINOP: return a->op == b->op && eq(a->arg1, b->arg1) && eq(a->arg2, b->arg2);
    }
    return 0;
}

unsigned int hash(Expr * expr){
    switch(expr->type){
        case NUM: return expr->f*1000;
        case VAR: return expr->i+1;
        case BINOP: {
            return expr->op*1298472149u + hash(expr->arg1)*874212412 + hash(expr->arg2)*32478187;
        }
    }
}

double eval(Expr * expr, double * subs){
    switch(expr->type){
        case NUM: return expr->f;
        case VAR: return subs[expr->i];
        case BINOP: switch(expr->op){
            case ADD: return eval(expr->arg1, subs) + eval(expr->arg2, subs);
            //case SUB: return eval(expr->arg1, subs) - eval(expr->arg2, subs);
            case MUL: return eval(expr->arg1, subs) * eval(expr->arg2, subs);
            case DIV: return eval(expr->arg1, subs) / eval(expr->arg2, subs);
            case MOD:{
                double a = eval(expr->arg1, subs);
                double b = eval(expr->arg2, subs);
                int n = a/b;
                return a - b*n;
            }
        }
    }
    return -1;
}

#define ISOP(expr, oper) (expr->type==BINOP && expr->op==oper)

#define SWAP(x, y) do{typeof(x) z = x; x=y; y=z;}while(0)
#define SWAP3(x, y, z) do{typeof(x) a = x; x=y; y=z; z=a;}while(0)

#define UNMUL(expr, newexpr, coeff)                     \
        if(ISOP(expr, MUL) && expr->arg1->type==NUM){   \
            coeff = expr->arg1->f;                      \
            free(expr->arg1);                           \
            newexpr = expr->arg2;                       \
            free(expr);                                 \
        }else{                                          \
            coeff = 1;                                  \
            newexpr = expr;                             \
        }

#define REMUL(expr, coeff) (coeff==1 ? expr : NEW((Expr){.type=BINOP, .op=MUL,\
                            .arg1=NEW((Expr){.type=NUM,.f=coeff}), .arg2=expr}))

void print(Expr * expr);

void simplify(Expr * expr){
start:
    if(expr->type != BINOP)
        return;
recurse1:
    simplify(expr->arg1);
recurse2:
    simplify(expr->arg2);
    if(expr->arg1->type == NUM && expr->arg2->type == NUM){
        double res = eval(expr, NULL);
        //printf("%f %d %f: %f\n", expr->arg1->f, expr->op, expr->arg2->f, res);
        free_expr(expr->arg1), free_expr(expr->arg2);
        *expr = (Expr){.type=NUM, .f=res};
        return;
    }
    if((expr->op==ADD || expr->op==MUL) && expr->arg2->type == NUM){
        SWAP(expr->arg1, expr->arg2);
    }
prop_zero:
    if((expr->op==MUL) && expr->arg1->type == NUM){
        if(ABS(expr->arg1->f) <= EPS){
            free_expr(expr->arg2);
            free(expr->arg1);
            *expr = (Expr){.type=NUM, .f=0};
            return;
        }else if(ABS(expr->arg1->f - 1) <= EPS){
            free(expr->arg1);
            Expr * r = expr->arg2;
            *expr = *expr->arg2;
            free(r);
            return;
        }
    }
    if((expr->op==ADD) && expr->arg1->type == NUM){
        if(ABS(expr->arg1->f) <= EPS){
            free(expr->arg1);
            Expr * r = expr->arg2;
            *expr = *expr->arg2;
            free(r);
            print(expr);
            return;
        }
    }
    if(expr->op == ADD && ISOP(expr->arg1, ADD)){
        if(ISOP(expr->arg2, ADD)){
            SWAP(expr->arg1, expr->arg2->arg1);
            goto recurse2;
        }else{
            SWAP(expr->arg1, expr->arg2);
        }
    }
add_merge: 
    if(expr->op == ADD && ISOP(expr->arg2, ADD)){
        Expr * a, * b, * res;
        double af, bf;
        UNMUL(expr->arg1, a, af);
        UNMUL(expr->arg2->arg1, b, bf);
        if(eq(a, b)){
            expr->arg1 = REMUL(a, af+bf);
            free_expr(b);
            Expr * r = expr->arg2->arg2;
            free(expr->arg2);
            expr->arg2 = r;
            simplify(expr->arg1);
            goto prop_zero;
        }else if(hash(a) < hash(b)){
            expr->arg1 = REMUL(b, bf);
            expr->arg2->arg1 = REMUL(a, af);
            goto recurse2;
        }else{
            expr->arg1 = REMUL(a, af);
            expr->arg2->arg1 = REMUL(b, bf);
        }
    }else if(expr->op == ADD){
        Expr * a, * b, * res;
        double af, bf;
        UNMUL(expr->arg1, a, af);
        UNMUL(expr->arg2, b, bf);
        if(eq(a, b)){
            Expr * remul = REMUL(a, af+bf);
            *expr = *remul;
            free(remul);
            free_expr(b);
            if(expr->type != BINOP)
                return;
            goto prop_zero;
        }else if(hash(a) < hash(b)){
            expr->arg1 = REMUL(b, bf);
            expr->arg2 = REMUL(a, af);
        }else{
            expr->arg1 = REMUL(a, af);
            expr->arg2 = REMUL(b, bf);
        }
    }
mul_merge:
    if(expr->op == MUL && ISOP(expr->arg2, MUL) &&
            expr->arg1->type == NUM && expr->arg2->arg1->type == NUM){
        expr->arg1->f *= expr->arg2->arg1->f;
        free(expr->arg2->arg1);
        Expr * r = expr->arg2->arg2;
        free(expr->arg2);
        expr->arg2 = r;
        goto mul_merge;
    }
distribute:
    if(expr->op == MUL && ISOP(expr->arg2, ADD)){
        expr->arg1 = NEW((Expr){.type=BINOP, .op=MUL, .arg1=expr->arg1, .arg2=expr->arg2->arg1});
        expr->arg2->arg1 = deep_copy(expr->arg1->arg1);
        expr->op = ADD;
        expr->arg2->op = MUL;
        goto recurse1;
    }
}


Expr * solve_for(Expr * eq, int v){
    Expr *l, *r;
    if(eq->type != BINOP){fprintf(stderr, "need an equation\n");return NULL;}
    if(eq->op != EQ){ fprintf(stderr, "need an equation\n"); return NULL;}
    eq->arg2 = NEW((Expr){.type=BINOP, .op=ADD, .arg1=eq->arg2, .arg2=NEGATE(eq->arg1)});
    eq->arg1 = NEW((Expr){.type=NUM, .f=0});
    simplify(eq->arg2);
    Expr * stack[64] = {eq};
    int dirs[64] = {0};
    int n = 0;
    while(1){
        switch(stack[n]->type){
            case NUM: n--; break;
            case VAR: 
                if(stack[n]->i == v) 
                    goto solve; 
                n--; break;
            case BINOP:
                if(dirs[n]==2){n--; break;}
                dirs[n] ++;
                if(dirs[n]==1)
                    stack[n+1] = stack[n]->arg1;
                else
                    stack[n+1] = stack[n]->arg2;
                n++;
                dirs[n] = 0;
                break;
        }
        if(n==63){fprintf(stderr, "stack out of range\n");return NULL;}
        if(n<0)  {fprintf(stderr, "var %d unfound\n", v); return NULL;}
    }
solve:
    if(dirs[0]==1){
        l = eq->arg1;
        r = eq->arg2;
    }else{
        l = eq->arg2;
        r = eq->arg1;
    }
    int i = 1;
    while(1){
        while(l->type==BINOP){
            Expr *a, *b;
            if(dirs[i]==1){
                a = l->arg1;
                b = l->arg2;
            }else{
                a = l->arg2;
                b = l->arg1;
            }
            switch(l->op){
                case ADD: 
                    r = NEW((Expr){.type=BINOP, .op=ADD, .arg1=r, .arg2=NEGATE(b)});
                    break;
                //case SUB: 
                //    if(dirs[i]==1)
                //        r = NEW((Expr){.type=BINOP, .op=ADD, .arg1=r, .arg2=b});
                //    else
                //        r = NEW((Expr){.type=BINOP, .op=SUB, .arg1=b, .arg2=r});
                //    break;
                case MUL: 
                    r = NEW((Expr){.type=BINOP, .op=MUL, .arg1=r, .arg2=RECIPR(b)});
                    break;
                case DIV: 
                    if(dirs[i]==1)
                        r = NEW((Expr){.type=BINOP, .op=MUL, .arg1=r, .arg2=b});
                    else
                        r = NEW((Expr){.type=BINOP, .op=DIV, .arg1=b, .arg2=r});
                    break;
                default:
                    fprintf(stderr, "illegal operator: %d\n", l->op);
                    return NULL;
            }
            free(l);
            l = a;
            i++;
        }
        if(l->type == VAR)
            return r;
    }
}

double * sys_solve(Expr ** expr, int n, char ** dict){
    Expr * solved[n];
    double * subs = malloc(sizeof(double)*n);
    int i, j;
    for(i=n-1; i>=0; i--){
        solved[i] = solve_for(expr[i], i);
        for(j=0; j<i; j++){
            expr[j] = NEW((Expr){.type=BINOP, .op=EQ, 
                        .arg1=deep_copy(solved[i]), 
                        .arg2=solve_for(expr[j], i)});
        }
    }
    for(i=0; i<n; i++)
        subs[i] = eval(solved[i], subs);
    return subs;
}

char * dict[1024] = {[1023]=0};
void print(Expr * expr){
    print_expr(expr, dict);
}

int main(int argc, char ** argv){
    //Expr * e = parse("x-y-z-x", dict);
    //simplify(e);
    //print_expr(e, dict);
    //return 0;
    Expr * expr[] = {
        parse("x+y+z=5", dict),
        parse("2*x+2*y+z=6", dict),
        parse("3*x+4*y+z=7", dict),
    };
    double * sol = sys_solve(expr, 3, dict);
    printf("%f, %f, %f\n", sol[0], sol[1], sol[2]);
    free_dict(dict);
    //for(int i=0; i<3; i++)
    //    free_expr(expr[i]);
    return 0;
}
