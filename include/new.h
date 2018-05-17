#include <stdlib.h>

#define NEW(...) ({\
    typeof(__VA_ARGS__) * _pt = malloc(sizeof(__VA_ARGS__));\
    *_pt = __VA_ARGS__;\
    _pt;\
})

#define FREE free

#if 0
#include <string.h>
#include <stdio.h>

typedef struct node * node;

struct node{
    int data;
    node prev;
    node next;
};

typedef struct list * list;

struct list{
    node head;
    node tail;
};

#define NEW_NODE(...) NEW((struct node){__VA_ARGS__})

#define FOR_NODE(head, iter) for(node iter = head; iter; iter=iter->next)
#define REV_NODE(head, iter) for(node iter = head; iter; iter=iter->prev)

node splice(node bef, node aft, node ins){
    ins->prev = bef;
    ins->next = aft;
    if(bef) bef->next = ins;
    if(aft) aft->prev = ins;
    return ins;
}

node ins(node ins, node aft){
    return splice(aft->prev, aft, ins);
}

node app(node bef, node ins){
    return splice(bef, bef->next, ins);
}

#define LI(dat, next) ins(NEW_NODE(.data=dat), next)
#define LE(dat) NEW_NODE(.data=dat)

#define LS(dat) NEW_NODE(.data=dat)
#define LA(next, data) ins(next, NEW_NODE(.data=dat))

node unsplice(node n){
    node bef = n->prev;
    node aft = n->next;
    if(bef) bef->next = aft;
    if(aft) aft->prev = bef;
    return n;
}

int main(void){
    node head = LI(2, LE(3));
    FOR_NODE(head, i){
        printf("d: %d\n", i->data);
    }
    node tail = LA(LS(1), 2);
    REV_NODE(tail, i){
        printf("d: %d\n", i->data);
    }
}
#endif
