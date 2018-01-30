#ifdef __cplusplus
extern "C" {
#endif

typedef enum PanelType {
    RGBA8,
    GREYA,
    MATX
} PanelType;

typedef struct {
    PanelType type;
    void * data;
} Panel;

typedef struct {
    uint8_t r, g, b, a;
} P_RGBA8;

typedef struct {
    int w, h;
    P_RGBA8 * pixels;
} S_RGBA8;

typedef struct {
    uint16_t g, a;
} P_GREYA;

typedef struct {
    int w, h;
    P_GREYA * pixels;
} S_GREYA;

typedef struct {
    double inv[4];
    Panel * panel;
} S_MATX;

void panel_free(Panel * panel);

//int create(Pixel * data, int width, int height, PanelType type);

//int get_pixel(Panel * panel, int x, int y);

P_RGBA8 overlay(P_RGBA8 top, P_RGBA8 bot);

/*
 * Draws the contents of panel onto data. 
 * If necessary, will expand width and height, and reallocate data to match. 
 * Returns non-zero on error.
 */
int combine(Panel * panel, P_RGBA8 ** data, int * width, int * height);

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

#ifdef __cplusplus
}
#endif
