#include "modal/offline_indicator.hpp"

#include "persistence/sync_state.hpp"

#include <gtest/gtest.h>

// Zone 11 — offline indicator visibility: shown only when the most recent sync has
// failed / is in backoff.

namespace {

using poker_trainer::modal::offline_indicator_visible;
using poker_trainer::persistence::SyncStatus;

TEST(OfflineIndicator, VisibleOnlyWhenSyncFailing) {
    EXPECT_FALSE(offline_indicator_visible(SyncStatus::Idle));
    EXPECT_FALSE(offline_indicator_visible(SyncStatus::SyncInProgress));
    EXPECT_FALSE(offline_indicator_visible(SyncStatus::SyncOk));
    EXPECT_TRUE(offline_indicator_visible(SyncStatus::SyncFailing));
}

}  // namespace
