#pragma once

#include "LearnedHumModel.h"

#include <cstdint>

struct PendingLearnResult
{
    LearnedHumModel model {};

    std::uint64_t analysisStartSample = 0;
};