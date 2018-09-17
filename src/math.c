#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "new.h"

#ifndef EPS
# define EPS 1e-12
#endif

#ifndef ABS
# define ABS(a) ((a)>0?(a):-(a))
#endif

#define LEN(a) (sizeof(a)/sizeof(*(a)))
#define SWAP(x, y) do{typeof(x) z = x; x=y; y=z;}while(0)

enum ops {ADD, SUB, MUL, DIV, MOD, POW, APP, EQ, LT, GT, LPAR, RPAR};
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
        if(expr->arg1) free_expr(expr->arg1);
        if(expr->arg2) free_expr(expr->arg2);
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
                  c=='^'?POW: \
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

// PARSER
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
Expr * next_pow(Parser * p){
    Expr * expr = next_atom(p);
    if(!expr) return NULL;
    if(p->k.type == KSYM && p->k.i==POW){
        adv_tok(p);
        expr = NEW((Expr){.type=BINOP, .op=POW, .arg1=expr, .arg2=next_pow(p)});
    }
    return expr;
}
Expr * next_appl(Parser * p){
    Expr * expr = next_pow(p);
    Expr * call = next_pow(p);
    if(call)
        expr = NEW((Expr){.type=BINOP, .op=APP, .arg1=expr, .arg2=call});
    return expr;
}
Expr * next_neg(Parser * p){
    int neg = 0;
    while(p->k.type == KSYM && p->k.i==SUB){
        neg ^= 1;
        adv_tok(p);
    }
    Expr * expr = next_appl(p);
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
void print(Expr * expr);

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
    
// FUNCTIONS
#define N 100
double taylor(double x, double * c, int n){
    double y = 0;
    for(int i=n-1; i>=0; i--){
        y *= x;
        y += c[i];
    }
    return y;
}
#define LONG(v) (*(uint64_t*)&v)
#define DOUB(v) (*(double*)&v)
double sqrt__(double a){
    if(a<-0){LONG(a)=(~(uint64_t)0)<<51;return*(double*)&LONG(a);}
    a = ABS(a);
    int64_t e = (LONG(a)>>52) - 0x3ff;
    uint64_t m = LONG(a) &~(~(uint64_t)0<<52);
    uint64_t u;
    if(e&1){
        u = (uint64_t)0x3fe<<52 | m;
        if(e<0)
            e = e/2 - 1;
        else
            e = e/2;
    }else{
        u = (uint64_t)0x3ff<<52 | m;
        e = e/2;
    }
    static double c[N];
    if(!c[0]){
        c[0] = 1;
        c[1] = .5;
        for(int i=1; i<N-1; i++){
            c[i] = c[i-1] * (1.5/i-1);
        }
    }
    double mag = taylor(DOUB(u) - 1, c, N);
    uint64_t ret = (e+0x3ff)<<52 | (LONG(mag) & ~(~(uint64_t)0<<52));
    return DOUB(ret);
}
#undef LONG
#undef DOUB
double ln(double b){
    int coeff = 1;
    while(b>=2){
        b = sqrt__(b);
        coeff *= 2;
    }
    while(b<=.5){
        b = sqrt__(b);
        coeff *= 2;
    }
    static double c[N];
    if(!c[1])
        for(int i=1; i<N; i++)
            c[i] = (i&1?1.:-1.) / i;
    return coeff * taylor(b-1, c, N);
}
double exp(double a){
    double res;
    if(a>1){
        res = exp(a/2);
        return res*res;
    }
    if(a<-1){
        return 1/exp(-a);
    }
    static double c[N];
    if(!c[0]){
        c[0] = 1;
        for(int i=1; i<=N; i++)
            c[i] = c[i-1]/i;
    }
    return taylor(a, c, N);
}
double fast_pow(double b, int e){
    if(!e)return 1;
    if(e==1)return b;
    int c = fast_pow(b, e/2);
    if(e%2){
        return c*c*b;
    }else{
        return c*c;
    }
}
double pow(double b, double e){
    if(ABS(e-(int)e)<EPS)
        return fast_pow(b, e);
    return exp(ln(b)*e);
}
#undef N

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
            case POW: return pow(eval(expr->arg1, subs), eval(expr->arg2, subs));
        }
    }
    return -1;
}

int eq(Expr * a, Expr * b){
    if(a->type != b->type) return 0;
    switch(a->type){
        case NUM: return ABS(a->f - b->f) <= EPS;
        case VAR: return a->i == b->i;
        case BINOP: return a->op == b->op && eq(a->arg1, b->arg1) && eq(a->arg2, b->arg2);
    }
    return 0;
}

#define ISOP(expr, oper) (expr->type==BINOP && expr->op==oper)

struct Matches {int n; struct Match {char name; Expr ** expr;} * l;};
int match(char * pattern, Expr ** expr_pt, struct Matches * m);
Expr * get_match(struct Matches * m, char name);

unsigned int ord(Expr * expr){
    struct Match l[64];
    struct Matches m = (struct Matches){0, l};
    if(match("*#ax", &expr, &m)){
        return ord(get_match(&m, 'x'));
    }
    m = (struct Matches){0, l};
    if(match("^x#b", &expr, &m)){
        return 10*get_match(&m, 'b')->f + ord(get_match(&m, 'x'));
    }
    switch(expr->type){
        case NUM: return 0;
        case VAR: return 567u * (expr->i+1);
        case BINOP: {
            return expr->op*1298472149u + ord(expr->arg1)*874212412 + ord(expr->arg2)*32478187;
        }
    }
}

// MATCHING
int ins_match(struct Matches * m, char name, Expr ** expr){
    for(int i=0; i<m->n; i++)
        if(m->l[i].name == name && !eq(*m->l[i].expr, *expr))
            return 0;
    m->l[m->n++] = (struct Match){name, expr};
    return 1;
}
Expr * get_match(struct Matches * m, char name){
    for(int i=0; i<m->n; i++)
        if(m->l[i].name == name)
            return *m->l[i].expr;
    return NULL;
}
Expr * pop_match(struct Matches * m, char name){
    for(int i=0; i<m->n; i++) if(m->l[i].name == name){
        Expr ** expr_pt = m->l[i].expr;
        Expr * expr = *expr_pt;
        m->l[i] = m->l[--m->n];
        *expr_pt = NULL;
        return expr;
    }
    return NULL;
}
Expr * unmatch(char ** pattern, struct Matches * m){
    int op;
    Expr * a, * b;
    char c = *(*pattern)++;
    if(c=='-'){
        a = unmatch(pattern, m);
        b = unmatch(pattern, m);
        return NEW((Expr){.type=BINOP, .op=ADD, .arg1=a, .arg2=NEGATE(b)});
    }
    if((op=OPSYM(c)) != -1){
        a = unmatch(pattern, m);
        b = unmatch(pattern, m);
        return NEW((Expr){.type=BINOP, .op=op, .arg1=a, .arg2=b});
    }
    if(c == '.'){
        Expr * expr = NEW(*get_match(m, *(*pattern)++));
        expr->arg1 = unmatch(pattern, m);
        expr->arg2 = unmatch(pattern, m);
        return expr;
    }
    if(ISDIG(c))
        return NEW((Expr){.type=NUM, .f=c - '0'});
    if(c == '@'){
        return deep_copy(get_match(m, *(*pattern)++));
    }
    return pop_match(m, c);
}
int match(char * pattern, Expr ** expr_pt, struct Matches * m){
    Expr * expr = *expr_pt;
    int i=0, n;
    char c = pattern[i++];
    int op;
    if((op=OPSYM(c)) != -1)
        if((ISOP(expr, op)) && 
            (n=match(pattern+i, &expr->arg1, m)) && 
            (n=match(pattern+(i+=n), &expr->arg2, m)))
            return i+n;
        else
            return 0;
    if(c=='.'){
        c=pattern[i++];
        if(expr->type==BINOP){
            for(int i=0; i<m->n; i++)
                if(m->l[i].name == c && (*m->l[i].expr)->op != expr->op)
                    return 0;
            m->l[m->n++] = (struct Match){c, expr_pt};
            if((n=match(pattern+i, &expr->arg1, m)) && 
                (n=match(pattern+(i+=n), &expr->arg2, m)))
                return i+n;
        }
        return 0;
    }
    if(c=='#')
        if(expr->type == NUM && ins_match(m, pattern[i++], expr_pt))
            return i;
        else
            return 0;
    if(c=='!')
        if(expr->type != NUM && ins_match(m, pattern[i++], expr_pt))
            return i;
        else
            return 0;
    if(c==','){
        Expr * cmp = get_match(m, pattern[i++]);
        if((ord(cmp) > ord(expr)) && 
            ins_match(m, pattern[i++], expr_pt))
            return i;
        return 0;
    }
    if(ISLET(c))
        if(ins_match(m, c, expr_pt))
            return i;
        else
            return 0;
    if(ISDIG(c))
        if(expr->type==NUM && expr->f==(c-'0'))
            return i;
        else
            return 0;
}

Expr * simplify(Expr * expr){
    if(expr->type != BINOP)
        return expr;
    expr->arg1 = simplify(expr->arg1);
    expr->arg2 = simplify(expr->arg2);
    if(expr->arg1->type==NUM && expr->arg2->type==NUM){
        double f = eval(expr, NULL);
        free(expr->arg1);
        free(expr->arg2);
        *expr = (Expr){.type=NUM, .f=f};
        return expr;
    }
    char * subs[][2] = {
        {"*a+bc", "+*@ab*ac"},      //distributive property
        {"/xx", "1"},               //definition of division
        {"*1x", "x"},               //unit in multiplication
        {"+0x", "x"},               //unit in addition
        {"+xx", "*2x"},             //factoring +definition of 2
        {"+*#axx", "*+a1x"},        //factoring
        {"+x*#bx", "*+1bx"},        // "
        {"+*#ax*#bx", "*+abx"},     // "
        {"*#a*#bx", "**abx"},
        {"+x+xy", "+*2xy"},         //factoring +definition of 2
        {"+*#ax+xy", "+*+a1xy"},    //factoring
        {"+x+*#bxy", "+*+1bxy"},    // "
        {"+*#ax+*#bxy", "+*+abxy"}, // "
        {"=.oax.obx", "=ab"},       //tautology
        {"=.oxa.oxb", "=ab"},       // "
        {"=!x!y", "=0-xy"},         //rearange
        {"++abc", "+a+bc"},         //left precedence preference
        {"+y,yx", "+xy"},           //sorting
        {"+y+,yxz", "+x+yz"},       // "
        {"**abc", "*a*bc"},         //left precedence preference
        {"*y,yx", "*xy"},           //sorting
        {"*y*,yxz", "*x*yz"},       // "
        {"*xx", "^x2"},             //power laws:
        {"*^xax", "^x+a1"},
        {"*x^xb", "^x+b1"},
        {"*^xa^xb", "^x+ab"},
        {"/^xa^xb", "^x-ab"},
        {"+x^x2", "-^+x/122/14"},   //completing the square:
        {"+x*a^x2", "-*@a^+x/1*2@a2/1*4a"},
        {"+*bx^x2", "-^+x/@b22/^b24"},
        {"+*bx*a^x2", "-*@a^+x/@b*2@a2/^b2*4a"},
        {NULL}
    };
    struct Match ms[100];
    struct Matches matches;
    for(int i=0; *subs[i]; i++){
        matches = (struct Matches){0, ms};
        char * patt = subs[i][0];
        if(match(patt, &expr, &matches)){
            patt = subs[i][1];
            Expr * new = unmatch(&patt, &matches);
            free_expr(expr);
            expr = new;
            return simplify(expr);
        }
    }
    return expr;
}

/*
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
*/

Expr * solve_for(Expr * eq, int v){
    Expr *l, *r;
    if(eq->type != BINOP){fprintf(stderr, "need an equation\n");return NULL;}
    if(eq->op != EQ){ fprintf(stderr, "need an equation\n"); return NULL;}
    eq = simplify(eq);
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
                case MUL: 
                    r = NEW((Expr){.type=BINOP, .op=MUL, .arg1=r, .arg2=RECIPR(b)});
                    break;
                case DIV: 
                    if(dirs[i]==1)
                        r = NEW((Expr){.type=BINOP, .op=MUL, .arg1=r, .arg2=b});
                    else
                        r = NEW((Expr){.type=BINOP, .op=DIV, .arg1=b, .arg2=r});
                    break;
                case POW:
                    if(dirs[i]==1)
                        r = NEW((Expr){.type=BINOP, .op=POW, .arg1=r, .arg2=RECIPR(b)});
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

double * sys_solve(Expr ** expr, int n){
    Expr * solved[n];
    double * subs = malloc(sizeof(double)*n);
    int i, j;
    for(i=n-1; i>=0; i--){
        solved[i] = solve_for(expr[i], i);
        if(!solved[i]) return NULL;
        for(j=0; j<i; j++){
            expr[j] = NEW((Expr){.type=BINOP, .op=EQ, 
                        .arg1=deep_copy(solved[i]), 
                        .arg2=solve_for(expr[j], i)});
        }
    }
    for(i=0; i<n; i++)
        subs[i] = eval(solved[i], subs),
        expr[i] = NEW((Expr){.type=BINOP, .op=EQ,
                    .arg1=NEW((Expr){.type=VAR, .i=i}),
                    .arg2=solved[i]});
    return subs;
}

char * dict[1024] = {[1023]=0};
void print(Expr * expr){
    print_expr(expr, dict);
}

int main(int argc, char ** argv){
    Expr * expr[] = {
        parse("x-y=4", dict),
        parse("x+y=3", dict),
    };
    double * sol = sys_solve(expr, LEN(expr));
    if(!sol) return 1;
    for(int i=0; i<LEN(expr); i++)
        printf("%s=%f ", dict[i], sol[i]);
    printf("\n");
    free(sol);
    for(int i=0; i<LEN(expr); i++)
        free_expr(expr[i]);
    free_dict(dict);
    return 0;
}
