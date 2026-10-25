// SPDX-License-Identifier: MIT
#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"check failed: %s at line %d\n",#x,__LINE__); return EXIT_FAILURE; } } while (0)
int main(void) {

    CHECK(fabs(thermal_step(32,0,0.18,100)-32) < 1e-8);
    CHECK(thermal_step(32.4,100,0.18,10)<25);
    CHECK(thermal_step(32.4,0,1.2,10)>40);
    double a=thermal_step(55,40,0.18,20), b=55;
    for(int i=0;i<200;i++) b=thermal_step(b,40,0.18,0.1);
    CHECK(fabs(a-b)<1e-8);

    puts("model tests passed"); return 0;
}
