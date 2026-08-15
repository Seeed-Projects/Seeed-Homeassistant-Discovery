/**
 * Infrared learning waveform classification
 * 红外学习波形分类
 */

#ifndef IR_LEARNING_MATCHER_H
#define IR_LEARNING_MATCHER_H

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace ir_learning {

static constexpr uint16_t MATCH_TOLERANCE_US = 250;
static constexpr uint8_t MATCH_TOLERANCE_PERCENT = 25;
static constexpr uint8_t STRUCTURAL_MATCH_PERCENT = 90;

enum class MatchKind : uint8_t {
    None = 0,
    Exact = 1,
    Dynamic = 2,
};

struct MatchResult {
    MatchKind kind;
    size_t matchingTimingCount;
};

/**
 * Classify two captures as exact, dynamic-state, or unrelated waveforms
 * 将两次采集分类为一致、动态状态或无关波形
 */
inline MatchResult classify(
    const std::vector<uint16_t>& first,
    const std::vector<uint16_t>& second
) {
    if (first.size() != second.size() || first.empty()) {
        return {MatchKind::None, 0};
    }

    size_t matchingTimingCount = 0;
    for (size_t index = 0; index < first.size(); index++) {
        uint16_t high = first[index] > second[index] ? first[index] : second[index];
        uint16_t low = first[index] > second[index] ? second[index] : first[index];
        uint32_t tolerance = static_cast<uint32_t>(high) *
                             MATCH_TOLERANCE_PERCENT / 100;
        if (tolerance < MATCH_TOLERANCE_US) {
            tolerance = MATCH_TOLERANCE_US;
        }
        if (static_cast<uint32_t>(high - low) <= tolerance) {
            matchingTimingCount++;
        }
    }

    if (matchingTimingCount == first.size()) {
        return {MatchKind::Exact, matchingTimingCount};
    }
    if (matchingTimingCount * 100 >=
        first.size() * STRUCTURAL_MATCH_PERCENT) {
        return {MatchKind::Dynamic, matchingTimingCount};
    }
    return {MatchKind::None, matchingTimingCount};
}

}

#endif
