#ifndef MATH_UTILS_H
#define MATH_UTILS_H

/**
 * @brief Fast vector magnitude approximation (Octagonal Envelope).
 * @note Avoids sqrtf() to save CPU cycles and bypass <math.h> dependencies.
 */
inline float fast_mag(float a, float b) {
    float abs_a = (a < 0) ? -a : a;
    float abs_b = (b < 0) ? -b : b;
    float mx = (abs_a > abs_b) ? abs_a : abs_b;
    float mn = (abs_a < abs_b) ? abs_a : abs_b;
    return mx + 0.375f * mn;
}

/**
 * @brief Fast atan2 approximation returning degrees directly.
 * @note Uses rational function approximation to avoid heavy trigonometric libraries.
 */
inline float fast_atan2_deg(float y, float x) {
    float abs_y = (y < 0) ? -y : y;
    float angle;
    if (x == 0.0f && y == 0.0f) return 0.0f;

    if (x >= 0) {
        angle = 45.0f - 45.0f * ((x - abs_y) / (x + abs_y));
    } else {
        angle = 135.0f - 45.0f * ((x + abs_y) / (abs_y - x));
    }
    return (y < 0) ? -angle : angle;
}

#endif
