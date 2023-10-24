#include "threads/fixed-point.h"
#include <stdint.h>

// Convert n to fixed point
fixed_t int_to_fp(int n) {
    return n << FP_SHIFT_AMOUNT;
}

int fp_to_int_round_zero(fixed_t x) {
    return x >> FP_SHIFT_AMOUNT;
}

int fp_to_int_round_nearest(fixed_t x) {
    return (x >= 0) ? ((x + (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT)
                    : ((x - (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT);
}

fixed_t fp_add(fixed_t x, fixed_t y) {
    return x + y;
}

fixed_t fp_sub(fixed_t x, fixed_t y) {
    return x - y;
}

fixed_t fp_add_int(fixed_t x, int n) {
    return x + (n << FP_SHIFT_AMOUNT);
}

fixed_t fp_sub_int(fixed_t x, int n) {
    return x - (n << FP_SHIFT_AMOUNT);
}

fixed_t fp_mul(fixed_t x, fixed_t y) {
    return ((int64_t) x) * y / (1 << FP_SHIFT_AMOUNT);
}

fixed_t fp_mul_int(fixed_t x, int n) {
    return x * n;
}

fixed_t fp_div(fixed_t x, fixed_t y) {
    return ((int64_t) x) * (1 << FP_SHIFT_AMOUNT) / y;
}

fixed_t fp_div_int(fixed_t x, int n) {
    return x / n;
}
