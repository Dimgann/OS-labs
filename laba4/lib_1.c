#include <math.h>
#include "contracts.h"

float cos_derivative(float a, float dx) {
    return (cosf(a + dx) - cosf(a)) / dx;
}

int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a > 0 ? a : -a;
}