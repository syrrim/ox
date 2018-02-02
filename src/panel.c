#include "ox.h"

void panel_free(Panel * panel){
    switch(panel->type){
    case RGBA8:{
        S_RGBA8 * s = panel->data;
        free(s->pixels);
        free(s);
        break;
    }case MATX:{
        S_MATX * s = panel->data;
        panel_free(s->panel);
        free(s);
    }
    }
}

Pixel get_pixel(Panel * panel, int x, int y){
    switch(panel->type){
    case RGBA8:{
        S_RGBA8 * s = panel->data;
        return s->pixels[y*s->w + x]; 
    }case MATX:{
        S_MATX * s = panel->data;
        return get_pixel(s->panel, s->inv[0]*x+s->inv[1]*y, s->inv[2]*x+s->inv[3]*y);
    }}
    return (Pixel){.a=0};
}

int get_width(Panel * panel){
    switch(panel->type){
    case RGBA8:{
        S_RGBA8 * s = panel->data;
        return s->w;
    }case MATX:return 0;}
    return -1;
}

int get_height(Panel * panel){
    switch(panel->type){
    case RGBA8:{
        S_RGBA8 * s = panel->data;
        return s->h;
    }case MATX:return 0;}
    return -1;
}

void set_pixel(Panel * panel, int x, int y, Pixel pix){
    switch(panel->type){
    case RGBA8:{
        S_RGBA8 * s = panel->data;
        s->pixels[y*s->w + x] = pix;
        break;
    }case MATX:{
        S_MATX * s = panel->data;
        set_pixel(s->panel, s->inv[0]*x+s->inv[1]*y, s->inv[2]*x+s->inv[3]*y, pix);
        break;
    }}
}

Pixel overlay(Pixel top, Pixel bot){
    if(top.a==255)return top;
    Pixel fin;
    uint8_t a = MIN(top.a + bot.a, 255);
#define CPY(c)\
    fin.c = top.c * top.a / (a?:1) + bot.c * (a - top.a) / (a?:1)
    CPY(r);
    CPY(g);
    CPY(b);
#undef CPY
    fin.a = top.a + bot.a * (255 - top.a) / 255;
    return fin;
}

void overlay_pixel(Panel * panel, int x, int y, Pixel pix){
    Pixel prev = get_pixel(panel, x, y);
    set_pixel(panel, x, y, overlay(pix, prev));
}

int combine(Panel * panel, S_RGBA8 * old){
    switch(panel->type){
    case RGBA8:{
        S_RGBA8 * s = panel->data;
        Pixel * buf = old->pixels;
        int x = MIN(old->x, s->x);
        int y = MIN(old->y, s->y);
        int w = MAX(old->w+old->x-x, s->w+s->x-x);
        int h = MAX(old->h+old->y-y, s->h+s->y-y);
        if(old->w != w || old->h != h){
            buf = malloc(w*h * sizeof(Pixel));
            if(!buf)return 1;
        }
        for(int j=0; j<h; j++) for(int i=0; i<w; i++){
            Pixel p1, p2;
            if((s->y-y)<=j&&j<s->h+(s->y-y) && (s->x-x)<=i&&i<s->w+(s->x-x)){
                p1 = s->pixels[(j-s->y+y)*s->w + (i-s->x+x)];
            }else{
                p1 = (Pixel){.a=0};
            }
            if((old->y-y)<=j&&j<old->h && (old->x-x)<=i&&i<old->w){
                p2 = old->pixels[(j-old->y+y)*old->w + (i-old->x+x)];
            }else{
                p2 = (Pixel){.a=0};
            }
            buf[j*w+i] = overlay(p1, p2);
        }
        if(old->w != w || old->h != h){
            free(old->pixels);
            old->pixels = buf;
            old->w = w;
            old->h = h;
        }
        return 0;
    }
    case MATX:
        return 1;
    }
    return 1;
}

int transform(Panel * p, int x, int y, double * mat){ // mat should have 4 entries
#define APPX(x, y) (int)(mat[0]*(x) + mat[1]*(y))
#define APPY(x, y) (int)(mat[2]*(x) + mat[3]*(y))
    double det = mat[0]*mat[3] - mat[1]*mat[2];
    double inv[4] = {mat[3]/det, -mat[1]/det, -mat[2]/det, mat[0]/det};
#define FINDX(x, y) (int)(inv[0]*(x) + inv[1]*(y))
#define FINDY(x, y) (int)(inv[2]*(x) + inv[3]*(y))
    switch(p->type){
    case RGBA8:{
        S_RGBA8 * s = p->data;
        Pixel * old = s->pixels;
        int w = s->w, h = s->h;
        int blx = APPX(0-x, 0-y),
            bly = APPY(0-x, 0-y),
            tlx = APPX(0-x, h-y),
            tly = APPY(0-x, h-y),
            brx = APPX(w-x, 0-y),
            bry = APPY(w-x, 0-y),
            trx = APPX(w-x, h-y),
            try = APPY(w-x, h-y);
        int lx = MIN(MIN(MIN(blx, tlx), brx), trx),
            rx = MAX(MAX(MAX(brx, trx), blx), tlx),
            by = MIN(MIN(MIN(bly, bry), tly), try),
            ty = MAX(MAX(MAX(bly, bry), tly), try);
        int new_w = rx - lx,
            new_h = ty - by;
        Pixel * new = calloc(new_w * new_h, sizeof(Pixel)); 
        if(!new) return 1;
        for(int j=0; j<new_h; j++) 
            for(int i=0; i<new_w; i++){
                int pos = (FINDY(i+lx, j+by) + y)*s->w + FINDX(i+lx, j+by) + x;
                new[j*new_w + i] = old[LIM(0, pos, s->w*s->h)];
                //new[LIM(0, (APPY(i-x, j-y) - by)*new_w + APPX(i-x, j-y) - rx, new_w*new_h)] = old[j*s->w + i];
            }
        free(old);
        s->pixels = new;
        s->w = new_w;
        s->h = new_h;
        return 0;
    }case MATX:{
        S_MATX * s = p->data;
        double new[4] = {s->inv[0]*inv[0] + s->inv[1]*inv[2], s->inv[0]*inv[1] + s->inv[1]*inv[3], 
                         s->inv[2]*inv[0] + s->inv[3]*inv[2], s->inv[2]*inv[1] + s->inv[3]*inv[3]};
        memcpy(s->inv, new, 4*sizeof(double));
        return 0;
    }
    }
    return 1;
#undef FINDY
#undef FINDX
#undef APPY
#undef APPX
}

int apply_kernel(Panel * p, double * cells, int rw, int rh){ 
    int dw = rw*2+1;
    int dh = rh*2+1;
    switch(p->type){
    case RGBA8:{
        S_RGBA8 * s = p->data;
        Pixel * old = s->pixels;
        Pixel * new = malloc(s->w * s->h * sizeof(Pixel));//TODO: in place blur 
        if(!new)ERR("OOM\n");
        int w = s->w, h = s->h;
        for(int y=0; y<h; y++) {
            fprintf(stderr,"line %d    \r", y);
            fflush(stderr);
            for(int x=0; x<w; x++){
            double val[4] = {0.0,0.0,0.0,0.0};
            Pixel sel;
            for(int j=0; j<dh; j++) for(int i=0; i<dw; i++){
                double mult = cells[j * dw + i];
                if(!mult)continue;
                sel = old[MAX(MIN(y+j-rh, h-1), 0) * w + 
                        MAX(MIN(x+i-rw,  w-1), 0)  ];
                val[0] += sel.r * mult;
                //printf("sel.r: %d, %f, %f\n", sel.r, mult, sel.r * mult);
                val[1] += sel.g * mult;
                val[2] += sel.b * mult;
                val[3] += sel.a * mult;
            }
            //printf("a: %hhu, %f\n", (char)val[3], val[3]);
            new[y*w + x] = (Pixel){.r=val[0], .g=val[1], .b=val[2], .a=val[3]};
        }}
        free(old);
        s->pixels = new;
        return 0;
    }case MATX:
        return 1;
    }
    return 1;
}
