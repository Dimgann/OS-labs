#include <stdio.h>
#include "contracts.h"

float cos_derivative(float, float);
int gcd(int, int);

int main() {
    int cmd;
    while (scanf("%d", &cmd) == 1) {
        if (cmd == 1) {
            float a, dx;
            scanf("%f %f", &a, &dx);
            printf("%f\n", cos_derivative(a, dx));
        } else if (cmd == 2) {
            int a, b;
            scanf("%d %d", &a, &b);
            printf("%d\n", gcd(a, b));
        }
    }
    return 0;
}