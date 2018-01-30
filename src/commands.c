#include <stdlib.h>
#include <stdio.h>

#include "ox.h"
#include "commands.h"

#include "lodepng.h"

#include "tiny_jpeg.h"

#include "stb_image.h"


int import(char * fn){
    int w, h, chan;

    uint8_t * data = stbi_load(fn, &w, &h, &chan, 4);
    if(!data || !w || !h){
        //fprintf(stderr, "failed to import (%s)\n", stbi__g_failure_reason);
        return 1;
    }

    S_RGBA8 * s = malloc(sizeof(S_RGBA8));
    *s = (S_RGBA8){.w=w, .h=h, .pixels=(P_RGBA8 *)data};
    panels[panel_head++] = (Panel){RGBA8, s};
    selection++;
    return 0;
}

#define WBYT(f, val) fputc((char)val, f)
#define WSHT(f, val) WBYT(f,(val)>>8);WBYT(f,val)
#define WINT(f, val) WSHT(f,(val)>>16);WSHT(f,val)
int export(char * fn){
    char ext[10];
    int f=1, e=-1;
    P_RGBA8 * data = NULL;
    int w=0, h=0;

    while(fn[f]){
        if(fn[f-1] == '.') e = 0;
        if(e>-1 && e<10)
            ext[e++] = fn[f];
        f++;
    }
    ext[e] = '\0';
    
    for(int i = 0; i < panel_head; i++){
        combine(&panels[i], &data, &w, &h);
    }
    if(strcmp(ext, "png") == 0){
        int err;
        if((err = lodepng_encode32_file(fn, (uint8_t * )data, w, h))){
            printf("error encoding\n");
            return 1;
        };
    }else if(strcmp(ext, "jpg") == 0){
        tje_encode_to_file(fn, w, h, 4, (uint8_t *)data);
    }else if(strcmp(ext, "ff") == 0){
        FILE * f = fopen(fn, "wb");
        fwrite("farbfeld", 1, 8, f);
        printf("farbfeld %x, %x\n", w, h);
        putc(w>>24, f);
        putc(w>>16, f);
        putc(w>>8, f);
        putc(w, f);
        putc(h>>24, f);
        putc(h>>16, f);
        putc(h>>8, f);
        putc(h, f);
        for(int y=0; y<h; y++)for(int x=0; x<w; x++){
            P_RGBA8 p = data[y*w + x];
            putc(p.r,f);
            putc(p.r,f);
            putc(p.g,f);
            putc(p.g,f);
            putc(p.b,f);
            putc(p.b,f);
            putc(p.a,f);
            putc(p.a,f);
        }
    }

    printf("done\n");
    free(data);
    return 0;
}

int rotate(double degs){
    double rads = degs*M_PI / 180;
    double mat[4] = {cos(rads), sin(rads), -sin(rads), cos(rads)};
    return transform(&panels[selection], 0, 0, mat);
}

int scale(double factor){
    double mat[4] = {factor, 0, 0, factor};
    return transform(&panels[selection], 0, 0, mat);
}

int blur(int r, int passes){
    printf("r:%d, p:%d\n", r, passes);
    int d = (2*r+1);
    double * kernel = malloc(d * sizeof(double));
    for(int i = 0; i < d; i++){
        kernel[i] = 1.0/d;
    }
    for(int i=0; i<passes; i++)
        if(apply_kernel(&panels[selection], kernel, r, 0) ||
           apply_kernel(&panels[selection], kernel, 0, r) ) {
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
    return apply_kernel(&panels[selection], kernel, 1, 1);
}

int gaussian_blur(int r){
    int d = (2*r+1);
    double * kernel = calloc(d, sizeof(double));
    if(!kernel) return 1;
    double sum = 0;
    for(int i=0; i<d; i++){
        double val = exp(-pow(i-r, 2)/(2*r*r) * 16);
        printf("val: %f\n", val);
        sum += val;
        kernel[i] = val;
    }
    for(int i=0; i<d; i++){
        kernel[i] /= sum;
    }
    
    apply_kernel(&panels[selection], kernel, 0, r);
    apply_kernel(&panels[selection], kernel, r, 0);
    free(kernel);
    return 0;
}

int collate(){
    S_RGBA8 * s = calloc(sizeof(S_RGBA8), 1);
    for(int i = 0; i < panel_head; i++){
        if(combine(&panels[i], &s->pixels, &s->w, &s->h)){
            fprintf(stderr, "could not collate\n"); 
            return 1;
        }
        panel_free(&panels[i]);
    }
    panel_head = 1;
    selection = 0;
    panels[0] = (Panel){RGBA8, s};

    return 0;
}

int prev(){
    if(selection > 0){
        selection--;
        return 0;
    }
    fprintf(stderr, "already on first item\n");
    return 1;
}

int next(){
    if(selection < panel_head-1){
        selection++;
        return 0;
    }
    fprintf(stderr, "already on last item\n");
    return 1;
}

int dup(){
    S_RGBA8 * s = calloc(1, sizeof(S_RGBA8));
    if(combine(&panels[selection], &s->pixels, &s->w, &s->h))return 1;
    for(int i = panel_head - 1; i > selection; i--){
        panels[i+1] = panels[i];
    }
    panel_head++;
    panels[++selection] = (Panel){RGBA8, s};
    return 0;
}

int swap(){
    if(selection <= 0){
        fprintf(stderr, "nothing to swap with\n");
        return 1;
    }
    Panel p = panels[selection];
    panels[selection] = panels[selection-1];
    panels[selection-1] = p;
    return 0;
}

int crop(Mark tl, Mark br){
    int w = br.x - tl.x,
        h = br.y - tl.y;
    if(w < 0 || h < 0){
        fprintf(stderr, "br above or to the left of tl \n");
        return 1;
    }
    S_RGBA8 * s = panels[selection].data;
    P_RGBA8 * old = s->pixels;
    P_RGBA8 * new = malloc(w * h * sizeof(P_RGBA8));
    for(int y=0; y<h; y++) for(int x=0; x<w; x++)
        new[y*w + x] = old[(y+tl.y)*s->w + x+tl.x];
    free(old);
    s->pixels = new;
    s->w = w;
    s->h = h;
    printf("%dx%d\n", w, h);
    return 0;
}

#define ABS(a) (a>0?a:-a)

int line(Mark srt, Mark end, P_RGBA8 p){
    S_RGBA8 * s = panels[selection].data;
    int w = end.x - srt.x;
    int h = end.y - srt.y;
    int pix;// the total number of pixels we should place
    //pix = ABS(w) + ABS(h); 
    //pix = sqrt(w*w + h*h);
    pix = MAX(ABS(w), ABS(h));
    for(int i=0; i<pix; i++){
        int x = srt.x + w * i / pix;
        int y = srt.y + h * i / pix;
        s->pixels[y*s->w+x] = overlay(p, s->pixels[y*s->w+x]);
    }
    return 0;
}
