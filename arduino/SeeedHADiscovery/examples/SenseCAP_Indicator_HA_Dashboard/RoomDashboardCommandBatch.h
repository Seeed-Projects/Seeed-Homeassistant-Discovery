#pragma once

#include <stddef.h>
#include <stdint.h>

enum class DashboardCommandBatchResult : uint8_t {
  Pending,
  AllSucceeded,
  PartialSuccess,
  AllFailed,
};

enum class DashboardCommandResolution : uint8_t {
  NotTracked,
  Pending,
  Completed,
  Late,
};

// Tracks independent commands that together form one dashboard action.
// 跟踪共同组成一次看板操作的多条独立命令。
class RoomDashboardCommandBatch {
 public:
  static constexpr size_t kMaxCommands = 20;

  void begin(uint32_t startedAt);
  bool add(uint32_t requestId);
  DashboardCommandResolution resolve(uint32_t requestId, bool success);
  bool expire(uint32_t now, uint32_t timeoutMs);

  bool contains(uint32_t requestId) const;
  bool pending() const;
  size_t totalCount() const;
  size_t pendingCount() const;
  size_t successCount() const;
  size_t failureCount() const;
  DashboardCommandBatchResult result() const;

 private:
  struct CommandSlot {
    uint32_t requestId = 0;
    bool resolved = false;
  };

  CommandSlot commands_[kMaxCommands] = {};
  uint32_t startedAt_ = 0;
  size_t totalCount_ = 0;
  size_t pendingCount_ = 0;
  size_t successCount_ = 0;
  size_t failureCount_ = 0;
};
