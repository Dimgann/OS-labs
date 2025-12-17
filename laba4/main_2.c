#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include "contracts.h"

static float (*cos_derivative)(float, float);
static int (*gcd)(int, int);

static float stub_cos_derivative(float a, float dx) {
    (void)a; (void)dx;
    return 0.0f;
}

static int stub_gcd(int a, int b) {
    (void)a; (void)b;
    return 0;
}

void load_library(const char *path, void **lib) {
    if (*lib) dlclose(*lib);

    *lib = dlopen(path, RTLD_LOCAL | RTLD_NOW);
    if (*lib == NULL) {
        write(2, "warning: failed to load library\n", 33);
        cos_derivative = stub_cos_derivative;
        gcd = stub_gcd;
        return;
    }   

    cos_derivative = dlsym(*lib, "cos_derivative");
    if (!cos_derivative) cos_derivative = stub_cos_derivative;

    gcd = dlsym(*lib, "gcd");
    if (!gcd) gcd = stub_gcd;
}

int main() {
    void *lib = NULL;
    int toggle = 0;

    load_library("./lib_1.so", &lib);

    int cmd;
    while (scanf("%d", &cmd) == 1) {
        if (cmd == 0) {
            toggle = !toggle;
            load_library(toggle ? "./lib_2.so" : "./lib_1.so", &lib);
        } else if (cmd == 1) {
            float a, dx;
            scanf("%f %f", &a, &dx);
            printf("%f\n", cos_derivative(a, dx));
        } else if (cmd == 2) {
            int a, b;
            scanf("%d %d", &a, &b);
            printf("%d\n", gcd(a, b));
        }
    }

    if (lib) dlclose(lib);
}