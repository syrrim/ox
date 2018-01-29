#include "ox.h"

void panel_free(Panel * panel){
    switch(panel->type){
    case RGBA8:{
        S_RGBA8 * s = panel->data;
        free(s->pixels);
        free(s);
        break;
    }case GREYA:{
        S_GREYA * s = panel->data;
        free(s->pixels);
        free(s);
    }case MATX:{
        S_MATX * s = panel->data;
        panel_free(s->panel);
        free(s);
    }
    }
}

/*
uint32_t get32(Panel * panel, int x, int y){
    switch(panel->type){
    case RGBA8:{
        S_RGBA8 * s = panel->data;
        return *(int *)(s->pixels + y*s->w + x); 
    }case GREYA:{
        S_GREYA * s = panel->data;
        return *(int *)(s->pixels + y*s->w + x); 
    }case MATX:{
        S_MATX * s = panel->data;
        return get32(s->panel, s->inv[0]*x+s->inv[1]*y, s->inv[2]*x+s->inv[3]*y);
    }}
    return 1;
}
*/

int combine(Panel * panel, P_RGBA8 ** data, int * width, int * height){
    switch(panel->type){
    case RGBA8:{
        S_RGBA8 * s = panel->data;
        P_RGBA8 * buf = *data;
        int w = MAX(*width, s->w),
            h = MAX(*height, s->h);
        if(*width != w || *height != h){
            buf = malloc(w*h * sizeof(P_RGBA8));
            if(!buf)return 1;
        }
        for(int y=0; y<h; y++) for(int x=0; x<w; x++){
            P_RGBA8 p1, p2;
            if(y<s->h && x<s->w)
                p1 = s->pixels[y*s->w + x];
            else
                p1 = (P_RGBA8){.a=0};
            if(y<*width && x<*height)
                p2 = (*data)[y*(*width) + x];
            else
                p2 = (P_RGBA8){.a=0};
            uint8_t a_f = MIN(p1.a + p2.a, 255);
#define CPY(c)\
            buf[y*w+x].c = p1.c * p1.a / (a_f?:1) + p2.c * (a_f - p1.a) / (a_f?:1)
            CPY(r);
            CPY(g);
            CPY(b);
#undef CPY
            buf[y*w+x].a = p1.a + p2.a * (255 - p1.a) / 255;
        }
        if(*width != w || *height != h){
            free(*data);
            *data = buf;
            *width = w;
            *height = h;
        }
        return 0;
    }
    case GREYA:
        return 1;
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
        P_RGBA8 * old = s->pixels;
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
        P_RGBA8 * new = calloc(new_w * new_h, sizeof(P_RGBA8)); 
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
    }case GREYA:{
        
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
        P_RGBA8 * old = s->pixels;
        P_RGBA8 * new = malloc(s->w * s->h * sizeof(P_RGBA8));
        if(!new){fprintf(stderr, "OOM\n");return 1;}                                             //TODO: in place blur 
        int w = s->w, h = s->h;
        for(int y=0; y<h; y++) {
            if(!(y%100))fprintf(stderr,"line %d\n", y);
            for(int x=0; x<w; x++){
            double val[4] = {0.0,0.0,0.0,0.0};
            P_RGBA8 sel;
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
            new[y*w + x] = (P_RGBA8){.r=val[0], .g=val[1], .b=val[2], .a=val[3]};
        }}
        free(old);
        s->pixels = new;
        return 0;
    }case GREYA:{
        return 1;
    }case MATX:
        return 1;
    }
    return 1;
}
