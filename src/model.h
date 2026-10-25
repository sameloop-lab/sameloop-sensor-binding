// SPDX-License-Identifier: MIT
#ifndef SAMELOOP_THERMAL_MODEL_H
#define SAMELOOP_THERMAL_MODEL_H
#include <math.h>
static inline double thermal_step(double t, double duty, double heat, double dt) {
    double k = 0.015 + 0.12 * duty / 100.0;
    double equilibrium = 20.0 + heat / k;
    return equilibrium + (t - equilibrium) * exp(-k * dt);
}
#endif
