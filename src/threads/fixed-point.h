#ifndef THREAD_FIXED_POINT_H
#define THREAD_FIXED_POINT_H

/* 17.14 fixed-point number representation */

/* define the type of fixed point */
typedef int FP_t;

/* FP_F=2**(14) */ 
#define FP_F (1<<14)      


/* Convert n to fixed point: n * f */
#define INT_TO_FP(n) (n) * (FP_F)

/* Convert x to integer (rounding toward zero):  x / f */ 
#define FP_TO_INT(x) (x) / (FP_F)

/* Convert x to integer (rounding to nearest): (x + f / 2) / f if x >= 0, (x - f / 2) / f if x <= 0. */
#define FP_TO_INT_nearest(x) ((x) >= 0 ? ((x) + (FP_F) / 2) / (FP_F) : ((x) - (FP_F) / 2) / (FP_F))

/* Add x and y:  x + y   */
#define FP_ADD_x_y(x, y) (x) + (y)

/* Subtract y from x:  x - y */
#define FP_SUB_x_y(x, y) (x) - (y)

/* Add x and n:  x + n * f */
#define FP_ADD_x_n(x, n) (x) + (n) * (FP_F)

/* Subtract n from x:  x - n * f */
#define FP_SUB_x_n(x, n) (x) - (n) * (FP_F)

/* Multiply x by y:  ((int64_t) x) * y / f */
#define FP_MUL_x_y(x, y) ((int64_t)(x)) * (y) / (FP_F)

/* Multiply x by n:  x * n */
#define FP_MUL_x_n(x, n) (x) * (n)

/* Divide x by y:  ((int64_t) x) * f / y */
#define FP_DIV_x_y(x, y) ((int64_t) (x)) * (FP_F) / (y)

/* Divide x by n:  x / n   */
#define FP_DIV_x_n(x, n) (x) / (n)

#endif /* thread/fixed-point.h */