#ifndef GUARD_SHOP_H
#define GUARD_SHOP_H

extern struct ItemSlot gMartPurchaseHistory[3];

struct ShopPriceOverride
{
    u16 item;
    u32 price;
};

// Per-day purchase limit for one item in one shop. Terminate tables with ITEM_NONE.
struct ShopStockOverride
{
    u16 item;
    u8 stock;
};

// Identifies which dailyShopStockPurchased slot a shop's stock table counts against.
// These are saved indices, so only append new shops to the end.
enum DailyStockShopId
{
    DAILY_STOCK_SHOP_SAFARI_ZONE_VITAMIN_GURU,
};

void CreatePokemartMenu(const u16 *itemsForSale);
void CreateDecorationShop1Menu(const u16 *itemsForSale);
void CreateDecorationShop2Menu(const u16 *itemsForSale);
void SetShopPriceOverrides(const struct ShopPriceOverride *overrides);
void SetShopSellPriceOverrides(const struct ShopPriceOverride *overrides);
void SetShopStockOverrides(u8 shopId, const struct ShopStockOverride *overrides);
void DailyResetShopStock(void);
u32 GetShopItemSellPrice(u16 itemId);
void CB2_ExitSellMenu(void);

void CreateBPVitaminShop(void);
void CreateBPHoldItemShop(void);
void CreateBPDecorShop1(void);
void CreateBPDecorShop2(void);
void CreateBPPokeBallShop(void);
void CreateBPHoldItemShop2(void);
void CreateBPPowerShop(void);
void CreateKurtBallShop(void);

#endif // GUARD_SHOP_H
