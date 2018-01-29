
typedef struct Mark{
    int x, y;
} Mark;

enum args {
    ARG_FN,
    ARG_ID,
    ARG_FLT,
    ARG_INT,
    ARG_MRK,
};

#define COMMANDS(a, b)                                  \
    a(import,          b(ARG_FN,0))                     \
    a(export,          b(ARG_FN,0))                     \
    a(rotate,          b(ARG_FLT,0))                    \
    a(scale,           b(ARG_FLT,0))                    \
    a(blur,            b(ARG_INT,0), b(ARG_INT,1))      \
    a(crop,            b(ARG_MRK,0), b(ARG_MRK,1))      \
    a(gaussian_blur,   b(ARG_INT,0))                    \
    a(sharpen                    )                      \
    a(collate                    )                      \
    a(next                       )                      \
    a(prev                       )                      \
    a(dup                        )                      \

int import(char * fn);

int export(char * fn);

int rotate(double degs);

int scale(double factor);

int blur(int r, int passes);

int gaussian_blur(int r);

int sharpen();

int collate();

int prev();

int next();

int dup();

int crop(Mark tl, Mark br);

