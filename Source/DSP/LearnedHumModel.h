#pragma once

#include "HumEstimator.h"

struct LearnedHumModel
{
    HumEstimator::Result harmonics {};

    double frequencyHz = 0.0;

    bool valid = false;
    bool humDetected = false;
};