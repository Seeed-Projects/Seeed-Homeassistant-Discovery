#include <assert.h>
#include <initializer_list>

#include "RoomDashboardCommandBatch.h"

int main() {
  RoomDashboardCommandBatch batch;

  batch.begin(100);
  for (uint32_t requestId = 1; requestId <= 6; ++requestId) {
    assert(batch.add(requestId));
  }
  assert(batch.resolve(2, false) ==
         DashboardCommandResolution::Pending);
  assert(batch.pendingCount() == 5);
  for (uint32_t requestId : {1U, 3U, 4U, 5U}) {
    assert(batch.resolve(requestId, true) ==
           DashboardCommandResolution::Pending);
  }
  assert(batch.resolve(6, true) ==
         DashboardCommandResolution::Completed);
  assert(batch.result() == DashboardCommandBatchResult::PartialSuccess);
  assert(batch.successCount() == 5);
  assert(batch.failureCount() == 1);

  batch.begin(1000);
  assert(batch.add(21));
  assert(batch.add(22));
  assert(batch.resolve(21, true) == DashboardCommandResolution::Pending);
  assert(!batch.expire(8999, 8000));
  assert(batch.expire(9000, 8000));
  assert(batch.result() == DashboardCommandBatchResult::PartialSuccess);
  assert(batch.resolve(22, true) == DashboardCommandResolution::Late);

  batch.begin(2000);
  assert(batch.add(31));
  assert(batch.add(32));
  assert(batch.resolve(31, false) == DashboardCommandResolution::Pending);
  assert(batch.resolve(32, false) == DashboardCommandResolution::Completed);
  assert(batch.result() == DashboardCommandBatchResult::AllFailed);
  assert(batch.resolve(99, true) ==
         DashboardCommandResolution::NotTracked);
  return 0;
}
