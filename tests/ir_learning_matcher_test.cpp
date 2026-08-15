#include <assert.h>
#include <stdint.h>

#include <vector>

#include "IRLearningMatcher.h"

int main() {
    const std::vector<uint16_t> stableFirst = {9000, 4500, 560, 560, 560, 1690};
    const std::vector<uint16_t> stableSecond = {9020, 4480, 580, 540, 570, 1660};
    ir_learning::MatchResult stable = ir_learning::classify(
        stableFirst,
        stableSecond
    );
    assert(stable.kind == ir_learning::MatchKind::Exact);

    const std::vector<uint16_t> acFirst = {
        1038, 560, 596, 2164, 598, 1428, 598, 850, 596, 2164, 598,
        350, 596, 2144, 618, 350, 596, 1428, 596, 350, 596, 352, 594,
        352, 594, 352, 594, 350, 596, 852, 594, 352, 594, 352, 594,
        2144, 618, 352, 594, 850, 596, 848, 598, 350, 594, 350, 600,
        348, 594, 850, 594, 352, 594, 350, 594, 352, 594, 350, 594,
        350, 596, 1428, 598, 848, 596, 2164, 598, 350, 594, 352, 594,
        350, 596, 352, 594, 350, 596, 352, 594, 850, 596, 352, 594,
        2166, 598, 350, 594, 350, 596, 352, 594, 2164, 598, 1428, 596,
        352, 594, 1430, 628,
    };
    const std::vector<uint16_t> acSecond = {
        1042, 558, 596, 2144, 620, 1406, 618, 826, 620, 2142, 620,
        348, 596, 2140, 622, 348, 596, 1428, 596, 326, 620, 348, 598,
        348, 596, 350, 596, 356, 588, 850, 596, 350, 596, 350, 596,
        2146, 616, 348, 596, 850, 598, 848, 596, 348, 602, 346, 596,
        2142, 624, 846, 598, 348, 598, 348, 596, 350, 596, 348, 598,
        348, 596, 1428, 598, 850, 596, 2144, 620, 348, 598, 348, 598,
        348, 598, 348, 598, 348, 598, 348, 598, 848, 598, 348, 596,
        2140, 622, 348, 596, 350, 598, 348, 596, 2142, 620, 1428, 598,
        2144, 620, 1426, 626,
    };
    ir_learning::MatchResult dynamic = ir_learning::classify(acFirst, acSecond);
    assert(dynamic.kind == ir_learning::MatchKind::Dynamic);
    assert(dynamic.matchingTimingCount == 97);

    const std::vector<uint16_t> unrelated = {
        9000, 4500, 560, 1690, 560, 1690, 560, 1690, 560, 1690,
    };
    const std::vector<uint16_t> different = {
        2400, 600, 1200, 600, 1200, 600, 1200, 600, 1200, 600,
    };
    ir_learning::MatchResult mismatch = ir_learning::classify(
        unrelated,
        different
    );
    assert(mismatch.kind == ir_learning::MatchKind::None);

    const std::vector<uint16_t> shorter = {9000, 4500, 560, 560};
    ir_learning::MatchResult lengthMismatch = ir_learning::classify(
        stableFirst,
        shorter
    );
    assert(lengthMismatch.kind == ir_learning::MatchKind::None);
    return 0;
}
