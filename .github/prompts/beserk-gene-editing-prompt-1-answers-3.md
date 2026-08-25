Does evolution retroactively touch a fusion's already-known moves? Vanilla Pokémon evolution never does this — it only adds newly-learnable moves as you level further; it doesn't swap out your current 4 moves. The plan currently says evolution "recalculates the learnset," which is ambiguous: does that mean only the future level-up/teachable tables get regenerated (safe, vanilla-consistent), or does it also touch the mon's currently equipped moves (more invasive, possibly jarring)? I'd recommend explicitly locking this to "only future-learnable tables change; currently known moves are untouched," matching vanilla behavior, unless you want it to behave differently.



Yes, evolution should only affect future learnable/teachable moves.




Interaction with the existing Randomizer and Fairy-type-challenge features. I noticed GetSpeciesType() already has its own override logic for RandomizerFeatureEnabled(RANDOMIZE_MON_TYPES) and the pre-Fairy-era tx_Mode_Fairy_Types setting, sitting in the exact same function we're wrapping. We need an explicit precedence decision: if a player has the randomizer on and breeds a fusion, which wins? I'd lean toward "Berserk Gene override wins outright" (since it's a deliberate, expensive player choice), but that's a call for you to make, not me.


That is a good point. The randomizer will give us a Pokemon instance with custom types. The Fairy flag will give us Pokemon that are now Fairy type and did not used to be. Both of these should be respected by the Berserk Gene Editing. We should perform the randomizer and Fairy checks before processing the Berserk Gene editing. It is not about any of them winning. They should all work properly together.