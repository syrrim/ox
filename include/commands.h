#include "mark.h"

int import(char * fn);

int export(char * fn);

int rotate(double degs);

int scale(double factor);

int blur(int r, int passes);

int gaussian_blur(int r);

int sharpen();

int join();

int collate();

int prev();

int next();

int dup();

int swap();

int crop(Mark tl, Mark br);

int line(Mark s, Mark e, Pixel p);

int fill(Mark cnt, Pixel c);

int noise(char *, Mark);
