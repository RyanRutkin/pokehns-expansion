// Per-shop price override tables for MART_TYPE_NORMAL pokemarts (see SetShopPriceOverrides
// and SetShopSellPriceOverrides in shop.c; struct ShopPriceOverride is declared in shop.h).
// Each shop's item list/order still lives in that map's own scripts.inc as a normal
// `pokemart` list; a table here only overrides the price of specific items for that one
// shop. To use one, call `special Set<Shop>Prices` immediately before the `pokemart`
// command in that shop's script.

static const struct ShopPriceOverride sGoldenrodEvoItemShopPriceOverrides[] = {
    { ITEM_AUSPICIOUS_ARMOR, 6000 },
    { ITEM_MALICIOUS_ARMOR,  6000 },
    { ITEM_TM_DRAGON_CHEER,  4000 },
    { ITEM_MOON_STONE,       6000 },
    { ITEM_PRISM_SCALE,      6000 },
    { ITEM_BLUE_ORB,         10000 },
    { ITEM_RED_ORB,          10000 },
    { ITEM_RARE_CANDY,       0 },
    { ITEM_NONE,             0 },
};

void SetGoldenrodEvoItemShopPrices(void)
{
    SetShopPriceOverrides(sGoldenrodEvoItemShopPriceOverrides);
}

// Pays double the standard sell amount for evolution stones/items and Heart Scale.
static const struct ShopPriceOverride sGoldenrodEvoItemShopSellPriceOverrides[] = {
    { ITEM_FIRE_STONE,       10000 },
    { ITEM_WATER_STONE,      10000 },
    { ITEM_THUNDER_STONE,    10000 },
    { ITEM_LEAF_STONE,       10000 },
    { ITEM_ICE_STONE,        10000 },
    { ITEM_SUN_STONE,        10000 },
    { ITEM_MOON_STONE,       3000 },
    { ITEM_SHINY_STONE,      10000 },
    { ITEM_DUSK_STONE,       10000 },
    { ITEM_DAWN_STONE,       10000 },
    { ITEM_HEART_SCALE,      1000 },
    { ITEM_PRISM_SCALE,      3000 },
    { ITEM_MALICIOUS_ARMOR,  3000 },
    { ITEM_AUSPICIOUS_ARMOR, 3000 },
    { ITEM_TART_APPLE,       2200 },
    { ITEM_SWEET_APPLE,      2200 },
    { ITEM_SYRUPY_APPLE,     2200 },
    { ITEM_NONE,             0 },
};

void SetGoldenrodEvoItemShopSellPrices(void)
{
    SetShopSellPriceOverrides(sGoldenrodEvoItemShopSellPriceOverrides);
}

static const struct ShopPriceOverride sSafariZoneGateVitaminGuruPriceOverrides[] = {
    { ITEM_PROTEIN, 1500 },
    { ITEM_IRON,    1500 },
    { ITEM_CARBOS,  1500 },
    { ITEM_ZINC,    1500 },
    { ITEM_CALCIUM, 1500 },
    { ITEM_HP_UP,   1500 },
    { ITEM_NONE,    0 },
};

void SetSafariZoneGateVitaminGuruPrices(void)
{
    SetShopPriceOverrides(sSafariZoneGateVitaminGuruPriceOverrides);
}

static const struct ShopPriceOverride sSafariZoneGateVitaminGuruSellPriceOverrides[] = {
    { ITEM_PROTEIN, 750 },
    { ITEM_IRON,    750 },
    { ITEM_CARBOS,  750 },
    { ITEM_ZINC,    750 },
    { ITEM_CALCIUM, 750 },
    { ITEM_HP_UP,   750 },
    { ITEM_NONE,    0 },
};

void SetSafariZoneGateVitaminGuruSellPrices(void)
{
    SetShopSellPriceOverrides(sSafariZoneGateVitaminGuruSellPriceOverrides);
}

// Only three of each vitamin per day; DailyResetShopStock restocks him overnight.
static const struct ShopStockOverride sSafariZoneGateVitaminGuruStock[] = {
    { ITEM_PROTEIN, 3 },
    { ITEM_IRON,    3 },
    { ITEM_CARBOS,  3 },
    { ITEM_ZINC,    3 },
    { ITEM_CALCIUM, 3 },
    { ITEM_HP_UP,   3 },
    { ITEM_NONE,    0 },
};

STATIC_ASSERT(ARRAY_COUNT(sSafariZoneGateVitaminGuruStock) - 1 <= MAX_DAILY_SHOP_STOCK_ITEMS, SafariZoneGateVitaminGuruStockTooLarge);

void SetSafariZoneGateVitaminGuruStock(void)
{
    SetShopStockOverrides(DAILY_STOCK_SHOP_SAFARI_ZONE_VITAMIN_GURU, sSafariZoneGateVitaminGuruStock);
}
