#include <math.h>
#include "contracts.h"

float cos_derivative(float a, float dx) {
    return (cosf(a + dx) - cosf(a - dx)) / (2 * dx);
}

int gcd(int a, int b) {
    int min = (a < b) ? a : b;
    int result = 1;
    
    for (int i = 2; i <= min; i++) {
        if (a % i == 0 && b % i == 0) {
            result = i;
        }
    }
    return result;
}