#include "RoomDashboardCommandBatch.h"

void RoomDashboardCommandBatch::begin(uint32_t startedAt) {
  for (CommandSlot& command : commands_) {
    command = {};
  }
  startedAt_ = startedAt;
  totalCount_ = 0;
  pendingCount_ = 0;
  successCount_ = 0;
  failureCount_ = 0;
}

bool RoomDashboardCommandBatch::add(uint32_t requestId) {
  if (requestId == 0 || totalCount_ >= kMaxCommands ||
      contains(requestId)) {
    return false;
  }
  commands_[totalCount_].requestId = requestId;
  ++totalCount_;
  ++pendingCount_;
  return true;
}

DashboardCommandResolution RoomDashboardCommandBatch::resolve(
    uint32_t requestId, bool success) {
  for (size_t index = 0; index < totalCount_; ++index) {
    CommandSlot& command = commands_[index];
    if (command.requestId != requestId) {
      continue;
    }
    if (command.resolved || pendingCount_ == 0) {
      return DashboardCommandResolution::Late;
    }

    command.resolved = true;
    --pendingCount_;
    if (success) {
      ++successCount_;
    } else {
      ++failureCount_;
    }
    return pendingCount_ == 0
               ? DashboardCommandResolution::Completed
               : DashboardCommandResolution::Pending;
  }
  return DashboardCommandResolution::NotTracked;
}

bool RoomDashboardCommandBatch::expire(uint32_t now, uint32_t timeoutMs) {
  if (!pending() || now - startedAt_ < timeoutMs) {
    return false;
  }
  failureCount_ += pendingCount_;
  pendingCount_ = 0;
  return true;
}

bool RoomDashboardCommandBatch::contains(uint32_t requestId) const {
  for (size_t index = 0; index < totalCount_; ++index) {
    if (commands_[index].requestId == requestId) {
      return true;
    }
  }
  return false;
}

bool RoomDashboardCommandBatch::pending() const {
  return pendingCount_ > 0;
}

size_t RoomDashboardCommandBatch::totalCount() const {
  return totalCount_;
}

size_t RoomDashboardCommandBatch::pendingCount() const {
  return pendingCount_;
}

size_t RoomDashboardCommandBatch::successCount() const {
  return successCount_;
}

size_t RoomDashboardCommandBatch::failureCount() const {
  return failureCount_;
}

DashboardCommandBatchResult RoomDashboardCommandBatch::result() const {
  if (pendingCount_ > 0) {
    return DashboardCommandBatchResult::Pending;
  }
  if (successCount_ == totalCount_ && totalCount_ > 0) {
    return DashboardCommandBatchResult::AllSucceeded;
  }
  if (successCount_ > 0) {
    return DashboardCommandBatchResult::PartialSuccess;
  }
  return DashboardCommandBatchResult::AllFailed;
}
