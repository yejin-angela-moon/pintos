#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <stdint.h>

//14-bit fractional part
#define FP_SHIFT_AMOUNT 14

// Using a 32-bit int to store fixed-point numbers.
typedef int32_t fixed_t;

// Convert an integer to fixed-point
fixed_t int_to_fp(int n);

// Convert a fixed-point to integer (rounded towards zero)
int fp_to_int_round_zero(fixed_t x);

// Convert a fixed-point to integer (rounded to nearest integer).
int fp_to_int_round_nearest(fixed_t x);

// Add two fixed-point values
fixed_t fp_add(fixed_t x, fixed_t y);

// Subtract two fixed-point values
fixed_t fp_sub(fixed_t x, fixed_t y);

// Add an integer to a fixed-point value
fixed_t fp_add_int(fixed_t x, int n);

// Subtract an integer from a fixed-point value
fixed_t fp_sub_int(fixed_t x, int n);

// Multiply two fixed-point values.
fixed_t fp_mul(fixed_t x, fixed_t y);

// Multiply a fixed-point value by an integer
fixed_t fp_mul_int(fixed_t x, int n);

// Divide two fixed-point values
fixed_t fp_div(fixed_t x, fixed_t y);

// Divide a fixed-point value by an integer
fixed_t fp_div_int(fixed_t x, int n);

#endif /* threads/fixed-point.h */
