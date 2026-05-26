#ifndef _MATH_H
#define _MATH_H

static inline int isinf(double x) { (void)x; return 0; }
static inline int isnan(double x) { (void)x; return 0; }

#define INFINITY 1e30
#define NAN      0.0

double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double log(double x);
double log10(double x);
double exp(double x);
double round(double x);
double fabs(double x);
double floor(double x);
double ceil(double x);
double pow(double b, double e);
double sqrt(double x);

double fmod(double x, double y);
#endif
