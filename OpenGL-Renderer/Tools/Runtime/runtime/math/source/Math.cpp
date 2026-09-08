// Copyright (c) 2019 UX3D GmbH. All rights reserved.
#include "math/Math.h"

#include <cmath>

float ux3d::math::roundDecimals(float val, int decimals)
{
    int exp = 10 * decimals;
    return std::roundf(val * static_cast<float>(exp)) / static_cast<float>(exp);
}
