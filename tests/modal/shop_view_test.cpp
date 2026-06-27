#include "modal/shop_view.hpp"

#include "modal/modals.hpp"

#include "audio/audio_paths.hpp"

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

namespace pt = poker_trainer::modal;
namespace audio = poker_trainer::audio;
namespace bb = poker_trainer::backbone;

namespace {

[[nodiscard]] pt::ShopRowView row(bool owned, bool in_pool, bool affordable) {
    pt::ShopRowView r{};
    r.track = audio::MusicTrackId::LoungeJazz_Track2;
    r.owned = owned;
    r.in_pool = in_pool;
    r.affordable = affordable;
    r.price = 5;  // LoungeJazz_Track2 is its genre's second track (price 5)
    return r;
}

}  // namespace

TEST(ShopButtonKind, LockedAffordableUnarmedIsBuy) {
    EXPECT_EQ(pt::shop_button_kind(row(false, false, true), /*armed=*/false),
              pt::ShopButtonKind::Buy);
}

TEST(ShopButtonKind, LockedAffordableArmedIsConfirm) {
    EXPECT_EQ(pt::shop_button_kind(row(false, false, true), /*armed=*/true),
              pt::ShopButtonKind::Confirm);
}

TEST(ShopButtonKind, LockedUnaffordableIsBuyDisabledEvenWhenArmed) {
    EXPECT_EQ(pt::shop_button_kind(row(false, false, false), /*armed=*/false),
              pt::ShopButtonKind::BuyDisabled);
    // An unaffordable row never arms, so an armed flag cannot promote it to Confirm.
    EXPECT_EQ(pt::shop_button_kind(row(false, false, false), /*armed=*/true),
              pt::ShopButtonKind::BuyDisabled);
}

TEST(ShopButtonKind, OwnedNotInPoolIsAdd) {
    EXPECT_EQ(pt::shop_button_kind(row(true, false, true), /*armed=*/false),
              pt::ShopButtonKind::Add);
}

TEST(ShopButtonKind, OwnedInPoolIsRemove) {
    EXPECT_EQ(pt::shop_button_kind(row(true, true, true), /*armed=*/false),
              pt::ShopButtonKind::Remove);
}

TEST(ShopButtonKind, OwnershipWinsOverArmed) {
    // Owned rows ignore the armed flag (arming only applies to a locked Buy).
    EXPECT_EQ(pt::shop_button_kind(row(true, false, true), /*armed=*/true),
              pt::ShopButtonKind::Add);
}

// ----- shop_focus_list: icon first, interactive rows top-to-bottom, X close last -----

TEST(ShopFocusList, IconFirstCloseLastInteractiveRowsBetween) {
    pt::ShopSnapshot snap{};
    for (std::size_t i = 0; i < audio::kMusicTrackCount; ++i) {
        snap.rows[i].track = static_cast<audio::MusicTrackId>(i);
        // defaults (owned=false, affordable=false) make every row BuyDisabled -> skipped.
    }
    snap.rows[0].affordable = true;  // locked + affordable -> Buy (a stop)
    snap.rows[2].affordable = true;  // Buy (a stop)
    snap.rows[5].owned = true;       // owned -> Add (a stop)

    const std::vector<bb::FocusableId> list = pt::shop_focus_list(snap);

    ASSERT_EQ(list.size(), 5u);  // icon + 3 interactive rows + close
    EXPECT_EQ(list.front(), pt::kShopLeaderboardIcon);
    EXPECT_EQ(list.back(), pt::kShopShellClose);
    EXPECT_EQ(list[1], pt::shop_row_focus_id(static_cast<audio::MusicTrackId>(0)));
    EXPECT_EQ(list[2], pt::shop_row_focus_id(static_cast<audio::MusicTrackId>(2)));
    EXPECT_EQ(list[3], pt::shop_row_focus_id(static_cast<audio::MusicTrackId>(5)));
}

TEST(ShopFocusList, AllDisabledLeavesOnlyIconAndClose) {
    pt::ShopSnapshot snap{};
    for (std::size_t i = 0; i < audio::kMusicTrackCount; ++i) {
        snap.rows[i].track = static_cast<audio::MusicTrackId>(i);  // all BuyDisabled
    }
    const std::vector<bb::FocusableId> list = pt::shop_focus_list(snap);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list.front(), pt::kShopLeaderboardIcon);
    EXPECT_EQ(list.back(), pt::kShopShellClose);
}

TEST(ShopRowFocusId, RoundTripsThroughTrackLookup) {
    for (std::size_t i = 0; i < audio::kMusicTrackCount; ++i) {
        const auto track = static_cast<audio::MusicTrackId>(i);
        const auto back = pt::shop_track_for_focus_id(pt::shop_row_focus_id(track));
        ASSERT_TRUE(back.has_value());
        EXPECT_EQ(*back, track);
    }
    // Non-row ids (icon / X close) are not tracks.
    EXPECT_FALSE(pt::shop_track_for_focus_id(pt::kShopShellClose).has_value());
    EXPECT_FALSE(pt::shop_track_for_focus_id(pt::kShopLeaderboardIcon).has_value());
}
