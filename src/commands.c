#include <stdlib.h>
#include <stdio.h>

#include "ox.h"
#include "panel.h"
#include "commands.h"

#include "lodepng.h"

#include "tiny_jpeg.h"

#include "stb_image.h"


int import(char * fn){
    int w, h, chan;

    uint8_t * data = stbi_load(fn, &w, &h, &chan, 4);
    if(!data || !w || !h){
        //fprintf(stderr, "failed to import (%s)\n", stbi__g_failure_reason);
        ERR("failed to import\n");
    }

    Panel * pnl = malloc(sizeof(Panel));
    *pnl = (Panel){.w=w, .h=h, .pixels=(Pixel *)data};
    selection = ins(selection, pnl);
    return 0;
}

int export(char * fn){
    char ext[10];
    int f=1, e=-1;
    Panel pnl = {.pixels=NULL};

    while(fn[f]){
        if(fn[f-1] == '.') e = 0;
        if(e>-1 && e<10)
            ext[e++] = fn[f];
        f++;
    }
    ext[e] = '\0';
    Frame * frm = selection;
    while(frm->prev) 
        frm = frm->prev;
    while(frm){
        combine(&pnl, frm->pnl);
        frm = frm->next;
    }
    int w = pnl.w;
    int h = pnl.h;
    printf("%dx%d\n", w, h);
    Pixel * data = pnl.pixels;
    //Pixel * d = data;
    /*while(++d<data+w)
        printf("%d %d %d\n", d->r, d->g, d->b);*/
    if(strcmp(ext, "png") == 0){
        int err;
        if((err = lodepng_encode32_file(fn, (uint8_t * )data, w, h))){
            ERR("error encoding\n");
        };
    }else if(strcmp(ext, "jpg") == 0){
        tje_encode_to_file(fn, w, h, 4, (uint8_t *)data);
    }else if(strcmp(ext, "ff") == 0){
        FILE * f = fopen(fn, "wb");
        fwrite("farbfeld", 1, 8, f);
        printf("farbfeld %x, %x\n", w, h);
        putc(w>>24, f); putc(w>>16, f); putc(w>>8, f); putc(w, f);
        putc(h>>24, f); putc(h>>16, f); putc(h>>8, f); putc(h, f);
        for(int y=0; y<h; y++)for(int x=0; x<w; x++){
            Pixel p = data[y*w + x];
            putc(p.r,f); putc(p.r,f);
            putc(p.g,f); putc(p.g,f);
            putc(p.b,f); putc(p.b,f);
            putc(p.a,f); putc(p.a,f);
        }
    }else{
        free(data);
        ERR("unknown ext: %s\n", ext);
    }
    free(data);
    return 0;
}

int rotate(double degs){
    double rads = degs*M_PI / 180;
    double mat[4] = {cos(rads), sin(rads), -sin(rads), cos(rads)};
    return transform(selection->pnl, 0, 0, mat);
}

int scale(double factor){
    double mat[4] = {factor, 0, 0, factor};
    return transform(selection->pnl, 0, 0, mat);
}

int blur(int r, int passes){
    int d = (2*r+1);
    double * kernel = malloc(d * sizeof(double));
    for(int i = 0; i < d; i++){
        kernel[i] = 1.0/d;
    }
    for(int i=0; i<passes; i++)
        if(apply_kernel(selection->pnl, kernel, r, 0) ||
           apply_kernel(selection->pnl, kernel, 0, r) ) {
            free(kernel);
            return 1;
        }
    free(kernel);
    return 0;
}

int sharpen(){
    double kernel[9] = {2.0, 2.0, 0.0,
                        2.0, 1.0, -1.,
                        0.0, -1., -1.};
    return apply_kernel(selection->pnl, kernel, 1, 1);
}

int gaussian_blur(int r){
    int d = (2*r+1);
    double * kernel = calloc(d, sizeof(double));
    if(!kernel) return 1;
    double sum = 0;
    for(int i=0; i<d; i++){
        double val = exp(-pow(i-r, 2)/(2*r*r) * 16);
        sum += val;
        kernel[i] = val;
    }
    for(int i=0; i<d; i++){
        kernel[i] /= sum;
    }
    
    apply_kernel(selection->pnl, kernel, 0, r);
    apply_kernel(selection->pnl, kernel, r, 0);
    free(kernel);
    return 0;
}

int join(){
    if(!selection || !selection->prev) ERR("nothing to join\n");
    if(combine(selection->prev->pnl, selection->pnl)){
        ERR("could not combine\n"); 
    }
    selection = rem(selection);
    return 0;
}

int collate(){
    if(!selection)return 0;
    while(selection->prev){
        if(combine(selection->prev->pnl, selection->pnl)) return 1;
        selection = rem(selection);
    }
    while(selection->next){
        if(combine(selection->pnl, selection->next->pnl)) return 1;
        rem(selection->next);
    }
    printf("new w/h: %d/%d\n", selection->pnl->w, selection->pnl->h);
    return 0;
}

int prev(){
    if(!selection) ERR("no item\n");
    if(!selection->prev) ERR("already on first item\n"); 
    selection = selection->prev;
    return 0;
}

int next(){
    if(!selection) ERR("no item\n");
    if(!selection->next) ERR("already on last item\n"); 
    selection = selection->next;
    return 0;
}

int dup(){
    Panel * pnl = malloc(sizeof(Panel));
    *pnl = (Panel){.pixels=NULL,.x=selection->pnl->x,.y=selection->pnl->y};
    if(combine(selection->pnl, pnl))return 1;
    selection = ins(selection, pnl);
    return 0;
}

int swap(){
    if(!selection || !selection->prev) ERR("nothing to swap with\n");
    Frame * p = selection->prev->prev;
    Frame * n = selection->next;
    selection->next = selection->prev;
    selection->prev = p;
    selection->next->next = n;
    selection->next->prev = selection;
    if(p) p->next = selection;
    if(n) n->prev = selection->next;
    selection = selection->prev;
    return 0;
}

int crop(Mark tl, Mark br){
    int w = br.x - tl.x,
        h = br.y - tl.y;
    if(w < 0 || h < 0){
        ERR("br above or to the left of tl \n");
    }
    Pixel * old = selection->pnl->pixels;
    Pixel * new = malloc(w * h * sizeof(Pixel));        // TODO: could probably be done in place 
    for(int y=0; y<h; y++) for(int x=0; x<w; x++)
        new[y*w + x] = old[(y+tl.y)*selection->pnl->w + x+tl.x];
    free(old);
    selection->pnl->pixels = new;
    selection->pnl->w = w;
    selection->pnl->h = h;
    selection->pnl->x += tl.x;
    selection->pnl->y += tl.y;
    return 0;
}

int line(Mark srt, Mark end, Pixel p){
    int w = ABS(end.x - srt.x);
    int h = ABS(end.y - srt.y);
    int x0 = MIN(srt.x, end.x);
    int y0 = MIN(srt.y, end.y);
    Panel * pnl = malloc(sizeof(Panel));
    *pnl = (Panel){.w=w+1, .h=h+1, .x=x0, .y=y0, 
                   .pixels=calloc((w+1)*(h+1), sizeof(Pixel))};
    selection = ins(selection, pnl);
    int pix;// the total number of pixels we should place
    //pix = ABS(w) + ABS(h); 
    //pix = sqrt(w*w + h*h);
    pix = MAX(w, h);
    for(int i=0; i<=pix; i++){
        int x = w * i / pix;
        int y = h * i / pix;
        overlay_pixel(selection->pnl, x, y, p);
    }
    return 0;
}

int pix_cmp(Pixel a, Pixel b){
    if(a.a==0||b.a==0)return a.a==b.a;
    else return (a.r==b.r && a.g==b.g && a.b==b.b);
}

int fill(Mark cnt, Pixel c){
    Panel * cur = selection->pnl;
    Pixel rpl = get_pixel(cur, cnt.x, cnt.y);
    if(pix_cmp(c, rpl))return 0;
#define SIZE 4096
    Mark todo[SIZE] = {cnt};
    int head = 0;
    int tail = 1;
#define PUSH(...) do{todo[tail++]=__VA_ARGS__; tail%=SIZE; if(tail==head) goto unwind;}while(0)
    while(head != tail){
        Mark val = todo[head++];
        head %= SIZE;
        if(pix_cmp(get_pixel(cur, val.x, val.y), c))
            continue;
        //printf("%d/%d: %dx%d\n", tail, head, val.x, val.y);
        set_pixel(cur, val.x, val.y, c);
        if(val.x>0 && pix_cmp(get_pixel(cur, val.x-1, val.y), rpl))
            PUSH((Mark){val.x-1, val.y});
        if(val.x<get_width(cur)-1 && pix_cmp(get_pixel(cur, val.x+1, val.y), rpl))
            PUSH((Mark){val.x+1, val.y});
        if(val.y>0 && pix_cmp(get_pixel(cur, val.x, val.y-1), rpl))
            PUSH((Mark){val.x, val.y-1});
        if(val.y<get_height(cur)-1 && pix_cmp(get_pixel(cur, val.x, val.y+1), rpl))
            PUSH((Mark){val.x, val.y+1});
    }
    return 0;
unwind:
    for(int i=0; i<SIZE; i++){
        Mark val = todo[i];
        if(pix_cmp(get_pixel(cur, val.x, val.y), rpl))
            fill(val, c);
    }
    return 0;
#undef PUSH
#undef SIZE
}

char * randbuf(int n){
    FILE * f;
    char * buf;
    f = fopen("/dev/urandom", "r");
    buf = malloc(n * sizeof(char));
    if(!buf)return NULL;
    fread(buf, sizeof(char), n, f);
    fclose(f);
    return buf;
}
float _qcos(uint8_t ang){
    return cos(ang * M_PI/128);
}
float qcos(uint8_t ang){
    static float cache[256];
    if(!cache[ang])
        cache[ang] = _qcos(ang); // _qcos(64) will be slightly different from zero
    return cache[ang];
}
float qsin(uint8_t ang){
    return qcos(ang-64);
}
float tripol(float l, float r, float dist){
    return LERP(l, r, dist * dist * (3 - 2*dist));
}

int noise(char * type, Mark size){
    int w = size.x,
        h = size.y;
    Pixel * p; 
    if(strcmp("perlin", type)==0){
        int r = MAX(w, h)/50;
        uint8_t * vecs = (uint8_t * )randbuf((w/r+1)*(h/r+1));
        p = malloc(w*h*sizeof(Pixel));
        if(!vecs || !p) ERR("OOM\n");
        for(int y=0; y<h; y++) for(int x=0; x<w; x++){
#define GET(x, y) vecs[(y)*(w/r+1)+(x)]
            CHN a;
            float rx = (float)(x%r+1)/(r+1);
            float ry = (float)(y%r+1)/(r+1);
            float b =   rx  *qcos(GET(x/r,   y/r))   +   ry  *qsin(GET(x/r,   y/r));
            float c = (rx-1)*qcos(GET(x/r+1, y/r))   +   ry  *qsin(GET(x/r+1, y/r));
            float d =   rx  *qcos(GET(x/r,   y/r+1)) + (ry-1)*qsin(GET(x/r,   y/r+1));
            float e = (rx-1)*qcos(GET(x/r+1, y/r+1)) + (ry-1)*qsin(GET(x/r+1, y/r+1));
            a = tripol(tripol(b, c, rx), tripol(d, e, rx), ry)*MAXCHN/2 + MAXCHN/2;
#undef GET
            p[y*w+x].r = a;
            p[y*w+x].g = a;
            p[y*w+x].b = a;
            p[y*w+x].a = MAXCHN;
        }
        printf("\n");
        free(vecs);
    }else if(strcmp("color", type)==0){
        p = (Pixel *)randbuf(w*h * sizeof(Pixel));
        if(!p) ERR("OOM\n");
        for(int i=0; i<w*h; i++){
            p[i].a = MAXCHN;
        }
    }else if(strcmp("gray", type)==0){
        CHN * d = (CHN*)randbuf(w*h*sizeof(CHN));
        p = malloc(w*h*sizeof(Pixel));
        if(!d || !p) ERR("OOM\n");
        for(int i=0; i<w*h; i++){
            p[i].r = d[i];
            p[i].g = d[i];
            p[i].b = d[i];
            p[i].a = 255;
        }
        free(d);
    }
    Panel * pnl = malloc(sizeof(Panel));
    *pnl = (Panel){.w=w, .h=h, .pixels=p};
    selection = ins(selection, pnl);
    return 0;
}
