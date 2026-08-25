// Per-shop price override tables for MART_TYPE_NORMAL pokemarts (see SetShopPriceOverrides
// in shop.c). Each shop's item list/order still lives in that map's own scripts.inc as a
// normal `pokemart` list; a table here only overrides the price of specific items for that
// one shop. To use one, call `special Set<Shop>Prices` immediately before the `pokemart`
// command in that shop's script.
struct ShopPriceOverride
{
    u16 item;
    u32 price;
};

static const struct ShopPriceOverride sGoldenrodEvoItemShopPriceOverrides[] = {
    { ITEM_AUSPICIOUS_ARMOR, 8000 },
    { ITEM_MALICIOUS_ARMOR,  8000 },
    { ITEM_TM_DRAGON_CHEER,  5000 },
    { ITEM_BLUE_ORB,         10000 },
    { ITEM_RED_ORB,          10000 },
    { ITEM_NONE,             0 },
};

void SetGoldenrodEvoItemShopPrices(void)
{
    SetShopPriceOverrides(sGoldenrodEvoItemShopPriceOverrides);
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
