#ifndef GUARD_POKEDEX_PLUS_HGSS_H
#define GUARD_POKEDEX_PLUS_HGSS_H

void CB2_OpenPokedexPlusHGSS(void);
void Task_DisplayCaughtMonDexPageHGSS(u8);
void OpenPokedexInfoScreen(u16 species, void (*returnCallback)(void));
void OpenPokedexInfoScreenForMon(struct Pokemon *mon, void (*returnCallback)(void));

#endif // GUARD_POKEDEX_PLUS_HGSS_H
