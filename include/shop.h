#ifndef GUARD_SHOP_H
#define GUARD_SHOP_H

extern struct ItemSlot gMartPurchaseHistory[3];

struct ShopPriceOverride
{
    u16 item;
    u32 price;
};

void CreatePokemartMenu(const u16 *itemsForSale);
void CreateDecorationShop1Menu(const u16 *itemsForSale);
void CreateDecorationShop2Menu(const u16 *itemsForSale);
void SetShopPriceOverrides(const struct ShopPriceOverride *overrides);
void SetShopSellPriceOverrides(const struct ShopPriceOverride *overrides);
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
