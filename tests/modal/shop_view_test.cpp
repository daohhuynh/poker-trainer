#include "modal/shop_view.hpp"

#include "modal/modals.hpp"

#include "audio/audio_paths.hpp"

#include <gtest/gtest.h>

namespace pt = poker_trainer::modal;
namespace audio = poker_trainer::audio;

namespace {

[[nodiscard]] pt::ShopRowView row(bool owned, bool in_pool, bool affordable) {
    pt::ShopRowView r{};
    r.track = audio::MusicTrackId::LoungeJazz_Track2;
    r.owned = owned;
    r.in_pool = in_pool;
    r.affordable = affordable;
    r.price = 25;
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
