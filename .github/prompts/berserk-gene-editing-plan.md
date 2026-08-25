# Berserk Gene Editing — Implementation Plan (finalized)

Status: PLANNING COMPLETE, NOT YET IMPLEMENTED.
This file consolidates `berserk-gene-editing-prompt-1.md` and `beserk-gene-editing-prompt-1-answers.md`
plus the resulting plan and decisions. Resume implementation from here when prompted.

## Feature summary
When Pokémon are bred in the daycare and one or both parents hold a Berserk Gene
(`ITEM_BERSERK_GENE` / `HOLD_EFFECT_BERSERK_GENE`, both already defined in the codebase),
properties of the gene-holding parent(s) can carry over to the hatched child: primary type,
secondary type, partial learnset, ability/secondary ability/hidden ability, color, shininess,
cry, size (height/weight and sprite scale/offset), gender, sprite/name, hatch rate, IVs, egg
groups, base stats, and nature. Berserk Gene holders also bypass normal gender/genderless/
legendary breeding restrictions under specific conditions. Parent mons in the daycare are never
modified — only the child's traits are affected.

## Key architectural finding
Types, abilities, egg groups, base stats, color, cry, and size are **not stored per-mon** today —
they are read live from the static `gSpeciesInfo[species]` table (`include/pokemon.h`). A
`BoxPokemon`/`Pokemon` only stores `species` plus selector fields (`abilityNum`, IVs/EVs,
personality, etc.). Implementing this feature requires:
1. A new persistent per-mon override record ("BerserkGeneProfile").
2. Auditing/patching every call site that reads `gSpeciesInfo[species].X` for a *live* mon
   instance so it prefers the override when present (battle engine type/ability lookups, stat
   calc, cry playback, Pokédex/summary/party-menu display, and — critically — the daycare code
   itself so a Berserk-Gene child that's bred *again* without a gene present treats its stored
   overrides as its new baseline).

This downstream-consumption audit (item 2) is the largest hidden cost and should be scoped with
a dedicated `gSpeciesInfo[` call-site sweep at the start of implementation.

## Data model / storage
New struct `BerserkGeneProfile`, embedded directly inside `struct BoxPokemon`
(`include/pokemon.h`), not a separate keyed side-table — so it travels automatically through
party/PC box/daycare/trade exactly like other per-mon data, avoiding orphaned-entry risk from
box compaction.

Fields (shape, to be bit-packed precisely at implementation time):
- `hasProfile:1`
- type1 (5 bits), type2 (5 bits) + set flags
- color (7 bits) + set flag
- ability1, ability2, abilityHidden (enum Ability, ~9 bits each — biggest bit cost)
- eggGroup1, eggGroup2 (u8 each)
- baseStats[NUM_STATS] (u8 each — second biggest cost, 48 bits total)
- eggCycles override (u8)
- cryId override
- height, weight overrides (u16 each)
- sprite scale, sprite offset overrides
- gender override (explicit, decoupled from species genderRatio)
- moves handled separately via learnset-merge logic, not a scalar field

Shininess reuses the existing `shinyModifier` bit already on `BoxPokemon` — no new storage
needed.

**Save impact:** ~10–14+ bytes added per `BoxPokemon` (mostly abilities + base stats). Multiplied
across party (6) + all PC boxes (~420) + daycare (2) ≈ a few KB total — acceptable for GBA
saves, but requires a save-version bump (`saveVersionMagic` in `include/global.h`) so existing
saves default cleanly to "no profile" (all-zero → falls back to normal species-table behavior).

## Breeding eligibility bypass (`GetDaycareCompatibilityScore`, `src/daycare.c`)
- If at least one parent holds Berserk Gene, gender-match/genderless-without-Ditto restrictions
  can be bypassed, **except**:
  - No female present and no Ditto present → both parents must hold the gene.
  - A genderless (non-Ditto) parent present → both parents must hold the gene.
  - A genderless parent paired with a Ditto → both parents must hold the gene.
- `EGG_GROUP_NO_EGGS_DISCOVERED` (legendary/undiscovered) early-incompatibility check is
  bypassed **only if both parents hold the Berserk Gene**.

## Identity (name/sprite/species) parent selection
Extend `DetermineEggSpeciesAndParentSlots` (`src/daycare.c`): if a real female parent is present,
child's species/name/sprite come from her as normal. If breeding was only possible via the new
no-female/genderless bypass rules, pick one parent as a fixed "identity donor" (selected once,
consistently used for species/name/sprite — never split across two different parents).

## Percentage-roll helper
Shared helper `BerserkGeneShouldInheritFromParent(parentHasGene, numGeneHolders)`:
- 1 gene holder → 68% chance to inherit that parent's trait.
- 2 gene holders → 50% chance to inherit that parent's trait (each independently rolled).

For **numeric, blendable** fields (height, weight, sprite scale, sprite offset) — NOT a binary
roll, but a weighted average:
```
result = round(pGene * 0.68 + pOther * 0.32)   // one gene holder
result = round(pA * 0.5 + pB * 0.5)             // both gene holders
```
Implemented as a separate `BerserkGeneBlendNumeric(pGeneValue, pOtherValue, numGeneHolders)`
helper. Base stats and hatch rate (`eggCycles`) remain **binary selection rolls** (per original
spec), not blends — this asymmetry between numeric fields is intentional, not an oversight.

## Per-trait inheritance order (as specified)
1. Primary type — binary roll per parent.
2. Secondary type — independent binary roll.
3. Learnset merge — new function; even index-spaced sampling from both parents' effective
   level-up movesets at 68/38 or 50/50 weighting, then re-sorted to prioritize moves matching
   the resolved child type(s), capped at existing initial-moveset size.
4. Ability / secondary ability / hidden ability — three independent binary rolls, one per slot;
   store all three (not just the active one) since the child can breed again later.
5. Color — one binary roll.
6. Shininess — reuse existing `shinyModifier` bit; if a shiny parent is selected by roll, force
   it; else fall back to standard shiny-roll code.
7. Cry — one binary roll.
8. Size (height, weight, sprite scale, sprite offset) — numeric blend (see above), not roll.
9. Gender — one binary roll; stored as explicit override.
10. Egg groups — union/selection algorithm (below).
11. Nature — extend `GetParentToInheritNature`/`_TriggerPendingDaycareEgg`: if roll selects a
    gene-holding parent, additional 75% chance to force that parent's exact nature (reusing the
    existing personality-search loop used for Everstone inheritance); else fall back to standard.
12. Base stats — six independent binary rolls (HP/Atk/Def/Spd/SpAtk/SpDef).
13. IVs (not percentage-based) — replaces `InheritIVs` entirely when either parent holds the
    gene: for each of 6 IVs, roll `Random() % 32`; if it beats both parents' actual IV for that
    stat, use it; else use `max(parentA_iv, parentB_iv)`.

### Egg group union/selection algorithm (step 10 detail)
1. `pool = unique(effectiveEggGroups(parentA) ∪ effectiveEggGroups(parentB))`, excluding
   `EGG_GROUP_NO_EGGS_DISCOVERED`.
2. `len(pool) == 0` → child gets `EGG_GROUP_NO_EGGS_DISCOVERED` in both slots.
3. `len(pool) == 1` → both child slots = that group.
4. `len(pool) == 2` → child slots = those two groups directly.
5. `len(pool) > 2`:
   - Slot 1: roll `BerserkGeneShouldInheritFromParent` to pick a parent, then `Random()` over
     that parent's own effective egg-group list (≤2 entries) to pick one group.
   - Slot 2: repeat; if result duplicates slot 1, retry (re-roll parent, prefer their other
     group if they have one). Cap retries (e.g. 10) with deterministic fallback (first pool
     entry ≠ slot 1) to guarantee termination — always reachable since `pool > 2` guarantees ≥3
     distinct groups across both parents' ≤4 total slots.

## Downstream consumption ("make it real")
Every subsystem reading a trait from `gSpeciesInfo[species]` for a specific mon instance must
check the profile first, at minimum:
- Battle type effectiveness/STAB (type lookups when populating battle mon data).
- Ability resolution from `abilityNum` → `enum Ability`.
- Stat calculation (`CalculateMonStats`/equivalent) reading base stats.
- Egg-group checks the *next* time this mon is bred (`GetDaycareCompatibilityScore`,
  `DetermineEggSpeciesAndParentSlots`).
- Cry playback, Pokédex size/color display, party menu/summary screen palette/icon selection,
  footprint/height-weight display.
- Form-change/evolution logic, if keyed off type or ability (verify; species ID itself doesn't
  change so likely unaffected).

A definitive `gSpeciesInfo[` call-site checklist should be produced via grep sweep at the start
of implementation.

## Trade / link battle compatibility
Existing version-handshake mechanism (`gLocalLinkPlayer.version`,
`AreAnyLinkPlayersUsingVersions` in `src/link.c`) is the hook point. Plan: extend this (or add a
parallel feature-flag) exchanged during link init. If a mon with `hasProfile` set is involved in
a link trade or link battle, and the partner isn't running this same ROM/fork build, refuse that
specific action (not the whole link session) with an appropriate message — analogous to existing
version-mismatch trade restrictions. Needs a closer read of `trade.c` and relevant `link.c`
sections before specifying exact hook points/messages — first sub-task when implementing this
part.

## Save compatibility
- Bump `saveVersionMagic` (or equivalent) in `include/global.h` so old saves don't get
  corrupted/misread by the new `BoxPokemon` layout.
- All new fields default to zero/"no profile" for every pre-existing save's Pokémon, naturally
  falling back to standard species-table behavior.

## Testing/validation plan
- Extend `test/daycare.c`: compatibility bypass matrix (female present / no female no Ditto /
  genderless / genderless+Ditto), single-vs-double gene-holder percentage behavior (statistical
  sampling over many trials), IV roll edge cases, re-breeding a Berserk-Gene child without a gene
  present preserves its stored profile as baseline.
- Manual in-game test: breed with one/both parents holding the item, hatch, inspect summary
  screen (type/ability/nature), verify battle type effectiveness matches the overridden type,
  then re-breed that child normally and confirm inheritance behaves like a "native" mon of its
  overridden traits.
- Build via existing project toolchain (`make`) to confirm no size/layout regressions.

## Files likely touched (implementation phase)
- `include/pokemon.h` — new struct, `BoxPokemon` extension, new `MON_DATA_*` accessors.
- `include/global.h` — save version bump if needed.
- `src/daycare.c` — `GetDaycareCompatibilityScore`, `DetermineEggSpeciesAndParentSlots`,
  `_GiveEggFromDaycare`, `SetInitialEggData`, `InheritIVs`, `InheritAbility`, `BuildEggMoveset`,
  `GetParentToInheritNature`, `_TriggerPendingDaycareEgg`.
- `src/pokemon.c` (stat calc, `GetMonData`/`SetMonData` for new fields) — not yet reviewed in
  detail, needs review at implementation time.
- Battle engine files resolving type/ability from species table for live mons — enumerate via
  grep sweep.
- Cry/Pokédex/party-menu/summary-screen files for cosmetic overrides — enumerate similarly.
- `src/link.c`, `src/trade.c` — link/trade compatibility gating.
- `test/daycare.c` — new test cases.

## Open items to revisit at implementation start
- Exact bit-packed layout of `BerserkGeneProfile` (report byte cost before finalizing).
- `gSpeciesInfo[` call-site sweep to produce the definitive downstream-consumption checklist.
- Read `trade.c` and relevant `link.c` sections to pin down exact trade/link-battle gating hook
  points and messaging.
