#include <inttypes.h>
#include <stdio.h>
#include "ox.h"
#include "mark.h"

#include "uthash.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

typedef struct Font{
    char * name;
    uint8_t * buf;
    stbtt_fontinfo font;
    UT_hash_handle hh;
} Font;

Font * fonts;

int utf8_codepoint(char ** text){
    // ascii: 0b0xxxxxxx
    // utf8 1b:  0b110xxxxx, 10xxxxx
    int v;
    if(!(**text & (1<<7))) 
        return *((*text)++);
    if(!(**text & (1<<5))){
        v = *((*text)++) & 0x1f;
        return v << 6 | (*((*text)++) & 0x3f);
    }
    if(!(**text & (1<<4))){
        v = (*((*text)++) & 0x1f)<<12;
        v |= (*((*text)++) & 0x3f)<<6;
        return v | (*((*text)++) & 0x3f);
    }
    if(!(**text & (1<<3))){
        v = (*((*text)++) & 0x1f)<<18;
        v |= (*((*text)++) & 0x1f)<<12;
        v |= (*((*text)++) & 0x3f)<<6;
        return v | (*((*text)++) & 0x3f);
    }
    // Codepoint is invalid. Skip
    (*text)++; 
    return utf8_codepoint(text);
}

int load_font(char * name, char * fn){
    FILE * f = fopen(fn, "rb");
    int n;
    uint8_t * buf;
    char * name_buf;
    Font * font;
    Font * old_font;

    fseek(f, 0L, SEEK_END);
    n = ftell(f);
    rewind(f);
    buf = malloc(n * sizeof(char));
    if(!buf)ERR("OOM\n");
    fread(buf, n, sizeof(char), f);

    n = strlen(name) + 1;
    name_buf = malloc(n*sizeof(char));
    strncpy(name_buf, name, n);

    font  = malloc(sizeof(Font));
    *font = (Font){.buf=buf, .name=name_buf};

    stbtt_InitFont(&(font->font), font->buf, stbtt_GetFontOffsetForIndex(font->buf, 0));

    HASH_REPLACE_STR(fonts, name, font, old_font);
    if(old_font){
        free(old_font->buf);
        free(old_font->name);
        free(old_font);
    }
    return 0;
}

int draw_text(char * text, char * font_name, Mark pos, int size, Pixel c){
    Font * named_font;
    HASH_FIND_STR(fonts, font_name, named_font);
    if(!named_font) ERR("no font of name '%s'\n", font_name);
    stbtt_fontinfo * font = &named_font->font;
    float scale = stbtt_ScaleForMappingEmToPixels(font, size);
    int xpos = 0;
    int cp = utf8_codepoint(&text);
    while(1){
	int advance,lsb,x0,y0,x1,y1;
	int w, h, xoff, yoff;
	float x_shift = xpos - (float) floor(xpos);
	stbtt_GetCodepointHMetrics(font, cp, &advance, &lsb);
	stbtt_GetCodepointBitmapBoxSubpixel(font, cp, scale,scale,x_shift,0, &x0,&y0,&x1,&y1);

        uint8_t * data = stbtt_GetCodepointBitmap(font, scale, scale, cp, &w, &h, &xoff, &yoff);
        for(int y=0; y<h; y++) for(int x=0; x<w; x++)
            overlay_pixel(&panels[selection], xpos+x+xoff+pos.x, y+yoff+pos.y, 
                        (Pixel){.r=c.r,.g=c.g,.b=c.b, .a=c.a*data[y*w+x]/255});
	xpos += (advance * scale);
	if(*text){
	    int next_cp = utf8_codepoint(&text);
            xpos += scale*stbtt_GetCodepointKernAdvance(font, cp, next_cp);
            cp = next_cp;
        }else{
            break;
        }
    }
    return 0;
}
