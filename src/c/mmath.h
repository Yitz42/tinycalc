// mmath.h - small double-precision math library for the watch.
// Self-contained (no libm), so it behaves the same on the watch and on a PC.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define M_PI_D   3.14159265358979323846
#define M_E_D    2.71828182845904523536
#define M_LN10_D 2.30258509299404568402

double m_nan(void);
double m_inf(void);
bool   m_isnan(double x);
bool   m_isfinite(double x);
double m_fabs(double x);
double m_floor(double x);
double m_round(double x);
double m_ldexp(double x, int e);
double m_frexp(double x, int *e);
double m_pow10i(int n);

double m_sqrt(double x);
double m_cbrt(double x);
double m_exp(double x);
double m_log(double x);
double m_log10(double x);
double m_pow(double x, double y);
double m_sin(double x);
double m_cos(double x);
double m_tan(double x);
double m_atan(double x);
double m_asin(double x);
double m_acos(double x);
double m_fact(double x);
double m_expm1(double x);
double m_log1p(double x);
double m_fmod(double x, double y);
