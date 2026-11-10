#ifndef GUARD_EVOLUTION_SCENE_H
#define GUARD_EVOLUTION_SCENE_H

void BeginEvolutionScene(struct Pokemon *mon, u16 postEvoSpecies, bool8 canStopEvo, u8 partyId, u8 fusionChoice);
void EvolutionScene(struct Pokemon *mon, u16 postEvoSpecies, bool8 canStopEvo, u8 partyId, u8 fusionChoice);
void TradeEvolutionScene(struct Pokemon *mon, u16 postEvoSpecies, u8 preEvoSpriteId, u8 partyId, u8 fusionChoice);

extern void (*gCB2_AfterEvolution)(void);

#endif // GUARD_EVOLUTION_SCENE_H
