Pixel get_pixel(Panel * panel, int x, int y);

void set_pixel(Panel * panel, int x, int y, Pixel pix);

int get_width(Panel * panel);
int get_height(Panel * panel);

Pixel overlay(Pixel top, Pixel bot);

void overlay_pixel(Panel * panel, int x, int y, Pixel pix);

/*
 * Draws the contents of panel onto old. 
 * If necessary, will expand width and height, and reallocate old->pixels to match. 
 * Returns non-zero on error.
 */
int combine(Panel * panel, Panel * old);


/*
 * Transforms the pixels of p about x,y according to mat.
 * mat should be an array of four doubles. 
 */
int transform(Panel * p, int x, int y, double * mat);

/* 
 * Mashes the pixels of p together with adjacent pixels according to cells.
 * cells should be (2rw+1)(2rh+1) large, and it's values should sum to 1.
 */
int apply_kernel(Panel * p, double * cells, int rw, int rh);
int blur_row(Panel * p, int rw);
