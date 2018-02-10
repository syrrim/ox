#include "ox.h"
#include "panel.h"

Pixel get_pixel(Panel * pnl, int x, int y){
    if(0>x||x>=get_width(pnl) || 0>y||y>=get_height(pnl))
        return (Pixel){.a=0};
    return pnl->pixels[y*pnl->w + x]; 
}

static int lipol(int l, int r, float d){
    return l*(1-d) + r*d;
}

Pixel get_pixel_float(Panel * p, float x, float y){
    int x0 = x;
    int y0 = y;
    float dx = x-x0,
          dy = y-y0;
    Pixel a = get_pixel(p,x0,  y0),
          b = get_pixel(p,x0+1,y0),
          c = get_pixel(p,x0,  y0+1),
          d = get_pixel(p,x0+1,y0+1);
    Pixel new;
#define CPY(v)\
    new.v = lipol(lipol(a.v,b.v,dx), lipol(c.v,d.v,dx), dy)
    CPY(r);
    CPY(g);
    CPY(b);
    CPY(a);
#undef CPY
    return new;
}

int get_width(Panel * pnl){
    return pnl->w;
}

int get_height(Panel * panel){
    return panel->h;
}

void set_pixel(Panel * pnl, int x, int y, Pixel pix){
    if(0>x||x>=get_width(pnl) || 0>y||y>=get_height(pnl)) abort();
    pnl->pixels[y*pnl->w + x] = pix;
}

Pixel overlay(Pixel top, Pixel bot){
    if(top.a==MAXCHN)return top;
    Pixel fin;
    CHN a = MIN(top.a + bot.a, MAXCHN);
#define CPY(c)\
    fin.c = top.c * top.a / (a?:1) + bot.c * (a - top.a) / (a?:1)
    CPY(r);
    CPY(g);
    CPY(b);
#undef CPY
    fin.a = top.a + bot.a * (MAXCHN - top.a) / MAXCHN;
    return fin;
}

void overlay_pixel(Panel * panel, int x, int y, Pixel pix){
    Pixel prev = get_pixel(panel, x, y);
    set_pixel(panel, x, y, overlay(pix, prev));
}

int combine(Panel * bot, Panel * top){
    Pixel * buf = bot->pixels;
    int x = MIN(bot->x, top->x);
    int y = MIN(bot->y, top->y);
    int w = MAX(bot->w+bot->x-x, top->w+top->x-x);
    int h = MAX(bot->h+bot->y-y, top->h+top->y-y);
    if(bot->w != w || bot->h != h){
        buf = malloc(w*h * sizeof(Pixel));
        if(!buf)ERR("OOM\n");
    }
    for(int j=0; j<h; j++) for(int i=0; i<w; i++){
        Pixel p1, p2;
        p1 = get_pixel(top, i-top->x+x, j-top->y+y);
        p2 = get_pixel(bot, i-bot->x+x, j-bot->y+y);
        buf[j*w+i] = overlay(p1, p2);
    }
    bot->x = x;
    bot->y = y;
    if(bot->w != w || bot->h != h){
        free(bot->pixels);
        bot->pixels = buf;
        bot->w = w;
        bot->h = h;
    }
    return 0;
}

int transform(Panel * pnl, int x, int y, double * mat){ // mat should have 4 entries
#define APPX(x, y) (mat[0]*(x) + mat[1]*(y))
#define APPY(x, y) (mat[2]*(x) + mat[3]*(y))
    printf("%f | %f\n%f | %f\n", mat[0], mat[1], mat[2], mat[3]);
    double det = mat[0]*mat[3] - mat[1]*mat[2];
    double inv[4] = {mat[3]/det, -mat[1]/det, -mat[2]/det, mat[0]/det};
#define FINDX(x, y) (inv[0]*(x) + inv[1]*(y))
#define FINDY(x, y) (inv[2]*(x) + inv[3]*(y))
    Pixel * old = pnl->pixels;
    int w = get_width(pnl), 
        h = get_height(pnl);
    int new_w = MAX(APPX(w, 0), APPX(w, h)),
        new_h = MAX(APPY(0, h), APPY(w, h));
    float blx = APPX(0-x, 0-y),
          bly = APPY(0-x, 0-y),
          tlx = APPX(0-x, h-y-1),
          tly = APPY(0-x, h-y-1),
          brx = APPX(w-x-1, 0-y),
          bry = APPY(w-x-1, 0-y),
          trx = APPX(w-x-1, h-y-1),
          try = APPY(w-x-1, h-y-1);
    float lx = MIN(MIN(MIN(blx, tlx), brx), trx),
          rx = MAX(MAX(MAX(brx, trx), blx), tlx),
          by = MIN(MIN(MIN(bly, bry), tly), try),
          ty = MAX(MAX(MAX(bly, bry), tly), try);
    new_w = rx - lx+1;
    new_h = ty - by+1;
    printf("%fx%f, %dx%d\n", lx, by, new_w, new_h);
    Pixel * new = calloc(new_w * new_h, sizeof(Pixel)); 
    if(!new) return 1;
    for(int j=0; j<new_h; j++) 
        for(int i=0; i<new_w; i++){
            float xp = FINDX(i+lx, j+by) + x;
            float yp = FINDY(i+lx, j+by) + y;
            //printf("%dx%d: %fx%f\n", i, j, xp, yp);
            new[j*new_w + i] = get_pixel_float(pnl, xp, yp);
            //new[LIM(0, (APPY(i-x, j-y) - by)*new_w + APPX(i-x, j-y) - rx, new_w*new_h)] = old[j*pnl->w + i];
        }
    free(old);
    pnl->pixels = new;
    pnl->w = new_w;
    pnl->h = new_h;
    return 0;
#undef FINDY
#undef FINDX
#undef APPY
#undef APPX
}

int apply_kernel(Panel * pnl, double * cells, int rw, int rh){ 
    int dw = rw*2+1;
    int dh = rh*2+1;
    Pixel * old = pnl->pixels;
    Pixel * new = malloc(pnl->w * pnl->h * sizeof(Pixel));//TODO: in place blur 
    if(!new)ERR("OOM\n");
    int w = pnl->w, h = pnl->h;
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
    pnl->pixels = new;
    return 0;
}
