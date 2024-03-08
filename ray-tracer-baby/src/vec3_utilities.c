#include "vec3_utilities.h"

#include <stdbool.h>
#include <cmm/random.h>
#include <cmm/types.h>

void RandomVec3(vec3 result) {
    glm_vec3_copy((vec3){ RANDOM_UNIFORM(f32), RANDOM_UNIFORM(f32), RANDOM_UNIFORM(f32) }, result);
}

internal void RandomVec3InUnitSphere(vec3 result) {
    while (true) {
        RandomVec3(result);
        if (glm_vec3_dot(result, result) < 1) return;
    }
}

void RandomUnitVec3(vec3 result) {
    RandomVec3InUnitSphere(result);
    glm_vec3_norm(result);
}

bool IsNonZeroVec3(vec3 result) {
    return glm_vec3_dot(result, result) < 0.001;
}