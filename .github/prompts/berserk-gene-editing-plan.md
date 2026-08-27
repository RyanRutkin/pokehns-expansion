# Berserk Gene Editing — Implementation Plan (finalized)

Status: PHASE 1 STORAGE SKELETON STARTED, PAUSED BEFORE BREEDING BEHAVIOR.
This file consolidates `berserk-gene-editing-prompt-1.md` and `beserk-gene-editing-prompt-1-answers.md`
plus the resulting plan and decisions. Resume implementation from here when prompted.

## Resume checkpoint (paused 2026-08-27)

Implementation is intentionally paused after Phase 1 storage groundwork because the user is out of
quota/credits until next month. Do **not** restart planning from scratch. Resume from this state:
- Phase 1A audits are done: `u8 conditionSetId` is sufficient for current evolution conditions;
  `MAX_FUSION_POTENTIAL_EVOLUTIONS = 8`; `MAX_BERSERK_GENE_PROFILES = 16` after profile packing.
- Phase 1B storage skeleton is implemented: profile ID packed into existing encrypted
  `PokemonSubstruct0` spare bits; side table stored in `PokemonStorage`; basic profile
  allocate/free/get/clear helpers exist.
- Last successful validation: `make hns -j24` passed; size probe measured `BoxPokemon = 80`,
  `FusionPotentialEvolution = 6`, `BerserkGeneProfile = 92`, `PokemonStorage = 35708` against a
  35712-byte Pokémon-storage budget.
- Full `make check` remains blocked by the known unrelated `src/braille_puzzles.c` /
  `FLAG_RECEIVED_TOGEPI_EGG` test-build issue.
- Next step when resuming: finish Phase 1B orphan-prevention before implementing breeding logic.
  Audit and wire cleanup for `BoxPokemon` deletion/overwrite paths (`ZeroBoxMonAt`, `SetBoxMonAt`,
  `PurgeMonOrBoxMon`, `CompactPartySlots`, party `ZeroMonData`, PC multi-move operations, item
  fusion copy/clear paths, and later trade paths). Keep all profile access behind storage helper
  APIs so the confirmed future external-save migration remains feasible.

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
1. A new persistent per-mon override record ("BerserkGeneProfile"), stored in a **capped side
   table** referenced by a small index field on `BoxPokemon` (Mail-style precedent already in
   this codebase — see Data model / storage below for the finalized design).
2. Auditing/patching every call site that reads `gSpeciesInfo[species].X` for a *live* mon
   instance so it prefers the override when present (battle engine type/ability lookups, stat
   calc, cry playback, Pokédex/summary/party-menu display, and — critically — the daycare code
   itself so a Berserk-Gene child that's bred *again* without a gene present treats its stored
   overrides as its new baseline).

This downstream-consumption audit (item 2) is the largest hidden cost and should be scoped with
a dedicated `gSpeciesInfo[` call-site sweep at the start of implementation.

## Data model / storage — capped side table (decision, supersedes earlier embed plan)
**Superseded decision:** the profile is a **capped side table**, not embedded inline in
`struct BoxPokemon`. This follows the existing in-repo precedent for exactly this pattern: Mail.
`BoxPokemon` already stores only a `u8 mail;` index (`include/pokemon.h`) pointing into a fixed
global array `struct Mail mail[MAIL_COUNT]` (`include/global.h`, `MAIL_COUNT = 10 + PARTY_SIZE`
in `include/constants/global.h`) rather than the full mail message inline. `BerserkGeneProfile`
follows the same shape:
- `struct BoxPokemon` stores `berserkGeneProfileId` in existing encrypted unused bits inside
  `PokemonSubstruct0` (`unused_02`/`unused_04`/`unused_0A` repurposed as low/mid/high pieces),
  with 0 = "no profile." This preserves `sizeof(struct BoxPokemon) == 80` and avoids expanding
  every PC storage slot.
- A new fixed-size side-table array lives in `struct PokemonStorage`:
  `struct BerserkGeneProfile berserkGeneProfiles[MAX_BERSERK_GENE_PROFILES + 1];`. Initial Phase
  1 storage audit originally started at 8, then was raised to `MAX_BERSERK_GENE_PROFILES = 16`
  after compacting the profile. Slot 0 is reserved so profile IDs 1-16 are valid saved
  references. This cap is driven by current Pokémon-storage sector headroom and can only be
  raised further if a later save-layout audit frees/reallocates space or the profile is compressed
  more aggressively.
- Slot 0 of the array is unused/reserved (so index 0 can mean "none", consistent with the
  `mail`-field sentinel convention already used elsewhere) — valid profiles occupy indices 1..N.

**This directly caps the maximum number of concurrent fusion Pokémon in existence at once (across
party + all PC boxes + daycare, for the whole save) at `MAX_BERSERK_GENE_PROFILES`.** Once every
slot is occupied, breeding cannot create a new fusion egg until an existing one is freed (see the
new daycare-full-table behavior below). Non-fusion mons pay only the ~2-byte index field, not the
full profile cost — this was the deciding factor over the embed approach (see Pre-implementation
review for the full comparison).

### Orphaned-slot cleanup — top implementation risk, needs explicit handling + tests
Because the profile now lives independently of the mon that references it, **every code path that
deletes, releases, overwrites, or discards a `BoxPokemon` must explicitly free its
`berserkGeneProfileId` slot** (decrement/clear it back to unused), or that slot leaks forever,
permanently shrinking the effective cap. This is the single biggest new risk introduced by
switching away from the embed design and must be treated as a first-class implementation
concern, not an afterthought. Known deletion/overwrite paths that need an explicit free hook,
to be confirmed exhaustively at implementation time via a dedicated grep sweep (mirroring the
`gSpeciesInfo[` sweep methodology used elsewhere in this plan):
- Releasing a Pokémon from the party or a PC box.
- Depositing a Pokémon into an already-occupied PC box slot (overwrite-on-deposit paths, if any).
- Egg cancellation/discard before hatching (daycare egg is abandoned/overwritten).
- Mass PC storage operations (box compaction, "Move" operations that might drop/relocate data).
- Any debug/cheat mon-clone functionality this build may have (must **duplicate** the referenced
  profile into a new slot, not share/copy the index, or two mons silently mutate one entry).
- Trade completion (the local copy of a traded-away mon must free its slot after the trade
  finishes, mirroring however `mail` is already cleared on trade if applicable — check
  `trade.c`'s existing mail-clearing logic as a template).

**Testing requirement (explicit, non-optional):** add dedicated unit tests (new or extended
`test/` file) that exercise every deletion/overwrite path above and assert the freed slot is
actually reclaimed (e.g. create a profile, delete/release/overwrite the owning mon via each path,
then assert a subsequent profile allocation reuses that now-free slot rather than growing into a
new one, and assert no slot is left marked "in use" with no mon referencing it). This is called
out separately in the Testing/validation plan section below as well.

Fields (shape, to be bit-packed precisely at implementation time):
- `hasProfile:1` — **note:** with the side-table approach this may become redundant with the
  `berserkGeneProfileId != 0` check itself; keep as an explicit field only if a fast "has profile"
  check without touching the side table turns out to matter, otherwise drop it.
- `parentSpeciesA`, `parentSpeciesB` (u16 each) — **new**: the current two "input species" this
  fusion is derived from. Set at egg creation to the breeding parents' (effective) species, and
  updated in place (one side only) whenever the fusion evolves — this is what evolution reruns
  the whole trait pipeline against.
- `potentialEvolutions[]` + count — **new**: fixed-size list of this fusion's currently available
  **immediate next-stage** evolution entries only. Each entry must store enough to apply the
  evolution later: target species, evolution method, method parameter/argument, any additional
  `conditionSetId` needed by that method, and source parent side
  (A/B) so the correct `parentSpeciesA`/`parentSpeciesB` slot can be replaced on evolution.
  Do **not** store entire evolutionary lines or later-stage preview chains. Example: a Poliwag ×
  Dratini fusion may store `Poliwhirl` and/or `Dragonair`, but not `Politoed`, `Poliwrath`, or
  `Dragonite` until the relevant side actually reaches the stage where those become immediate
  next evolutions. This field exists specifically to prevent repeated Berserk Gene breeding from
  causing unbounded growth in available evolution options.
- `geneHolderWeight:1` — **new**: records whether the original breeding had one gene holder
  (62%) or two (50%), persisted so ability/color/cry/sprite can be freshly re-rolled with the
  *same* weighting at each future evolution (the original parents are gone/inaccessible by then).
- `type1SourceParent:1`, `type1SourceSlot:1`, `type2SourceParent:1`, `type2SourceSlot:1` —
  **new**: provenance for the "sticky" type-inheritance-across-evolution rule (see Evolutionary
  line handling below) — which parent (A/B) and which of that parent's own type slots
  (primary/secondary) the child's type1/type2 were drawn from at birth.
- `spriteSourceParent:1` — **new**: which parent (A/B) the current sprite/name/identity is
  drawn from; used both for display and to exclude that parent from the candidate pool on the
  *next* evolution re-roll (never repeat the same sprite immediately after evolving).
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
- growthRate override (u8) — **new**: binary pick, same weighting mechanism as ability/color/cry.
- evYield overrides, one per stat (2 bits each per the existing `evYield_HP`-style packed
  fields, 12 bits total) — **new**: binary pick per stat, same mechanism as ability.
- friendship override (u8) — **new**: the *starting* friendship value (distinct from the
  already-planned `eggCycles`/hatch-counter override); binary pick, same mechanism as ability.
- Tera type: **no new field needed** — always passively follows the mon's own (already sticky,
  possibly overridden) primary type, exactly like vanilla's unforced default. A fusion mon's Tera
  type resolution must **ignore** `forceTeraType` entirely (even if one of the underlying species
  has one set) and just read the profile's `type1`.
- moves handled separately via learnset-merge logic, not a scalar field

Shininess reuses the existing `shinyModifier` bit already on `BoxPokemon` — no new storage
needed.

**Save impact (Phase 1 measured):** after compacting the potential-evolution entry, each
`FusionPotentialEvolution` is 6 bytes and the current `BerserkGeneProfile` is 92 bytes. Because
the profile ID is packed into existing encrypted `BoxPokemon` spare bits,
`sizeof(struct BoxPokemon)` remains 80 and non-fusion mons pay no per-slot size increase. With
`MAX_BERSERK_GENE_PROFILES = 16`, `PokemonStorage` grows from 34144 to 35708 bytes and still fits
within the 35712-byte storage sector budget, leaving only 4 bytes of Pokémon-storage headroom.
`SaveBlock1` currently measures 15752 bytes in the Emerald test build and 15760 bytes in HNS
against its 15872-byte budget, leaving too little headroom for the side table; do not add the
side table there. Save version is bumped so old saves default cleanly to profile ID 0 ("no
profile") and the new side table initializes empty on first load.

**Cap-increase investigation result and roadmap decision:** with the current profile shape, the
in-place PokémonStorage sector can support at most 16 profiles plus the reserved slot. Going higher
requires a larger architectural move. **External/special save storage is a confirmed future goal**
for this feature — it is not a should-we/should-we-not question, only a timing question. Phase 1
will keep the internal PokémonStorage table for controlled implementation/testing, but all code
must treat profile IDs abstractly and go through helper APIs (`GetBerserkGeneProfile`,
`AllocBerserkGeneProfile`, `FreeBerserkGeneProfile`, future iterator/count helpers) so the backing
store can migrate later without rewriting breeding/evolution/display logic. Do not let downstream
feature code index `gPokemonStoragePtr->berserkGeneProfiles` directly.

Deferring external storage is acceptable only while this remains internal/dev or while the 16-slot
cap is explicitly tolerated for testing. Before a public release that is expected to support broad
fusion experimentation, add a dedicated external-storage migration phase: design the special save
area, preserve or migrate existing profile IDs 1-16, copy occupied internal profiles to the new
store on save-version upgrade, and leave the old internal table harmless/ignored afterward. Do not
raise `MAX_BERSERK_GENE_PROFILES` above 16 in the current in-place storage backend without a new
save-layout audit and a fresh build/size probe.

## Breeding eligibility bypass (`GetDaycareCompatibilityScore`, `src/daycare.c`)
- If at least one parent holds Berserk Gene, gender-match/genderless-without-Ditto restrictions
  can be bypassed, **except**:
  - No female present and no Ditto present → both parents must hold the gene.
  - A genderless (non-Ditto) parent present → both parents must hold the gene.
  - A genderless parent paired with a Ditto → both parents must hold the gene.
- `EGG_GROUP_NO_EGGS_DISCOVERED` (legendary/undiscovered) early-incompatibility check is
  bypassed **only if both parents hold the Berserk Gene**.
- **New: side-table-full check.** Before committing to producing a Berserk Gene egg, check
  whether `gBerserkGeneProfiles[]` has a free slot. If the table is completely full (no free slot
  to allocate for the prospective child), the daycare **refuses to produce that egg** and the
  Day Care Man displays a specific message distinct from the normal "no egg" message, along the
  lines of: *"Your Pokémon simply won't go near each other. It seems like something is wrong."*
  This must not be confused with the normal daycare-compatibility-failure message (which reflects
  actual breeding incompatibility) — this is purely a storage-capacity failure and should read as
  vague/mysterious in-universe flavor text rather than an explicit "table full" error, per the
  user's specified wording. Needs its own check inserted into the daycare egg-readiness logic
  (likely alongside/near the existing compatibility-score check in `src/daycare.c`), gated
  specifically on "would this pairing produce a Berserk Gene child" so it never fires for normal
  (non-gene) breeding pairs.

## Identity (name/sprite/species) parent selection
Extend `DetermineEggSpeciesAndParentSlots` (`src/daycare.c`): if a real female parent is present,
child's species/name/sprite come from her as normal. If breeding was only possible via the new
no-female/genderless bypass rules, pick one parent as a fixed "identity donor" (selected once,
consistently used for species/name/sprite — never split across two different parents).

## Percentage-roll helper
Shared helper `BerserkGeneShouldInheritFromParent(parentHasGene, numGeneHolders)`:
- 1 gene holder → 62% chance to inherit that parent's trait.
- 2 gene holders → 50% chance to inherit that parent's trait (each independently rolled).

For **numeric, blendable** fields (height, weight, sprite scale, sprite offset, base stats) —
NOT a binary roll, but a weighted average:
```
result = round(pGene * 0.62 + pOther * 0.38)   // one gene holder
result = round(pA * 0.5 + pB * 0.5)             // both gene holders
```
Implemented as a separate `BerserkGeneBlendNumeric(pGeneValue, pOtherValue, numGeneHolders)`
helper. Hatch rate (`eggCycles`) remains a **binary selection roll** (per original spec), not a
blend — this asymmetry (most numeric fields blend, this one doesn't) is intentional, not an
oversight.

**Update (superseding the original spec):** base stats were originally specified as six
independent binary rolls (see step 12 below); the user has revised this to use the same
weighted-average blend formula as height/weight/scale/offset instead. Step 12 below is updated
accordingly — read it as blended, not rolled.

## Per-trait inheritance order (as specified)
1. Primary type — **(updated)** selected from a combined pool of both parents' effective types
   (each parent's primary *and* secondary type are both eligible candidates — a child's primary
   type is not restricted to coming from a parent's own primary slot). Working algorithm
   (concrete mechanism to confirm at implementation time, since the user's illustrative examples
   don't fully pin down the exact weighting): roll a parent via the existing gene-weighted
   `BerserkGeneShouldInheritFromParent` helper, then pick uniformly between that parent's own
   1–2 distinct effective types (mono-type parents contribute only one candidate).
2. Secondary type — **(updated)** same pool/selection mechanism as primary type, but with
   whichever type was just chosen for primary **removed from the pool first**, to guarantee the
   child never ends up mono-type-duplicated (e.g. never "Poison/Poison" from a Poison/Fighting ×
   Ground/Poison pairing). If removing the primary type's candidate leaves the selected parent
   with no remaining distinct type to offer, fall back to re-rolling the parent choice (same
   retry-with-fallback pattern as the egg-group selection in step 10).
3. Learnset merge — **(updated)** applies to **level-up moves, egg moves, and teachable
   (TM/tutor) moves alike**, not just level-up. For each of the three move pools independently:
   even index-spaced sampling from both parents'/both current species' effective move pools at
   62/38 or 50/50 weighting (per the existing gene-holder split), then **heavily re-weighted to
   prioritize moves matching the resolved child's primary/secondary type** over the plain
   even-spaced sampling result, capped at each pool's existing size limits (initial moveset size
   for level-up, existing array caps for egg/teachable). Teachable-move merging is a **union of
   both current species' teachable move lists** (not a capped/sampled subset like level-up/egg),
   still type-priority-sorted for display/relevance purposes.
4. Ability / secondary ability / hidden ability — three independent binary rolls, one per slot
   (`ability1`, `ability2`, `abilityHidden`), each picking gene-parent's value vs. other-parent's
   value for that slot; store all three (not just the active one) since the child can breed
   again later.

   **Active-ability selection (new, closes a prior gap):** the three rolls above only determine
   what *values* fill the three stored slots — a separate step is needed to pick which one the
   child actually has as its live ability at birth. That selection is a single rarity-weighted
   pick among `{ability1, ability2, abilityHidden}` (mostly ability1, less often ability2, hidden
   uncommon — exact weighting TBD at implementation time, should read from/mirror whatever weights
   the existing vanilla ability-assignment logic already uses, for consistency).

   **Special case (updated): Flying-type-parent Levitate candidate.** Check: (a) the child's
   *already-resolved* type (from steps 1-2 above) is **not** Flying in either slot, and (b) at
   least one parent's effective type (primary or secondary) **is** Flying. If both hold,
   **Levitate is injected as a fourth candidate** into the active-ability selection pool above
   (i.e. the pool becomes `{ability1, ability2, abilityHidden, Levitate}` for this pick only) —
   it is **not** a separate pre-roll step before the normal ability determination, it's folded
   directly into the same weighted pick. If both parents are Flying-type, Levitate is still only
   a single candidate in the pool (not two), to avoid double-counting. If Levitate wins the pick,
   it also **overwrites the stored `ability1` slot** with Levitate (rather than being active-only
   and unstored), so the child's own future breeding has a coherent value to potentially pass
   down as an ability-inheritance candidate.
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
12. Base stats — **(updated)** blended per-stat via `BerserkGeneBlendNumeric`, same weighted-
    average formula as height/weight/scale/offset (not independent binary rolls as originally
    specified — see the "Percentage-roll helper" section above for the superseding note).
13. IVs (not percentage-based) — replaces `InheritIVs` entirely when either parent holds the
    gene: for each of 6 IVs, roll `Random() % 32`; if it beats both parents' actual IV for that
    stat, use it; else use `max(parentA_iv, parentB_iv)`.
14. Growth rate, EV yield (per stat), base friendship — **new**: each an independent binary
    roll, same mechanism/weighting as ability/color/cry (step 4/5/7).

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

## Evolutionary line handling (new)

A Berserk-Gene-bred child is conceptually a standing "fusion" between two specific species
(`parentSpeciesA`/`parentSpeciesB` on the profile), not a fixed final form. It carries the
potential to evolve along *either* parent's evolutionary line, and each evolution re-derives most
(not all) of its traits from the updated species pair.

### Available evolutions = stored immediate next-stage options only
The old full-line/full-union wording is intentionally superseded: a fusion must **not** accumulate
every possible future evolution branch from every fused ancestor across repeated daycare
generations. That would make re-entering fusion mons into the daycare scale poorly and could
produce children with absurdly large evolution menus. Instead, each fusion stores only a bounded
list of **immediate next-stage** potential evolutions (`potentialEvolutions[]`).

At child creation, build a weighted candidate pool of immediate next evolutions:
- For a non-fusion parent, candidates are that parent's normal immediate next evolutions from its
  current species/stage.
- For a fusion parent being re-entered into daycare, candidates are **that parent's stored
  `potentialEvolutions[]` entries**, not the full native evolution tables behind its
  `parentSpeciesA`/`parentSpeciesB` ancestry. This is the key rule that prevents re-breeding
  fusion parents from expanding the child's possible evolutions without bound.
- Candidates remain tagged with which parent side they came from so that, if the child later uses
  that evolution, the correct `parentSpeciesA`/`parentSpeciesB` side can be replaced with the
  evolved species.

Before selecting which entries are stored, determine how many potential-evolution slots this child
gets:
1. Let `highCount = max(parentAEvolutionCandidateCount, parentBEvolutionCandidateCount)`.
2. Let `lowCount = min(parentAEvolutionCandidateCount, parentBEvolutionCandidateCount)`.
3. Roll a random fixed-point value `r` in `[0, 1]`.
4. `slotCount = ceil(lowCount + highCount * r)`.
5. Clamp `slotCount` to the combined candidate-pool size and to the fixed
  `MAX_FUSION_POTENTIAL_EVOLUTIONS = 8` cap.

Example: Meowth has 1 immediate evolution and Eevee has 8. If `r = 0.5`, then
`ceil(1 + 8 * 0.5) = 5`, so the child stores 5 potential next-stage evolutions selected from the
combined Meowth+Eevee candidate pool.

**Cap decision:** `MAX_FUSION_POTENTIAL_EVOLUTIONS` is fixed at **8**. This is the intended
gameplay/storage cap for fusion mons, even if a future or edge-case species has more than 8 raw
immediate evolution entries. Implementation-time data check found Milcery as a current outlier
with many Alcremie form/method entries (72 literal entries / 63 unique targets), so oversized
candidate pools must be sampled down to 8 rather than assuming the source table naturally fits.
This keeps fusion profiles bounded and avoids special-case save growth for form-heavy species.

After `slotCount` is known, select that many unique entries from the combined candidate pool using
the existing 62%/50% gene-holder weighting: with one gene holder, candidates sourced from the
gene-holding parent are weighted 62 and candidates from the other parent are weighted 38; with two
gene holders, both parents' candidates are weighted 50/50. This gives each parent the **potential**
to contribute evolutions, but does not guarantee either parent contributes one on any given child.
The same two parents can therefore create children with different potential evolutions, just like
they can create children with different moves, types, and other inherited traits.

`GetMonEvolutions(mon)` for a fusion mon simply returns the stored `potentialEvolutions[]` list
(filtered for the current context/method as normal), rather than recomputing from full species
evolution lines.

### Compact potential-evolution entry representation
Each stored potential-evolution entry should be compact, but it needs enough information to behave
exactly like a normal evolution entry when checked later:
- `targetSpecies` — the species this source side will become if this evolution is taken.
- `method` — the evolution method (`EVO_LEVEL`, `EVO_ITEM`, trade method, etc.).
- `param` — the method parameter (level, item ID, move ID, mapsec, etc.).
- `conditionSetId` — stable ID for the source evolution's extra `CONDITIONS(...)` set, with 0
  meaning "no extra conditions." Do not store raw ROM pointers in save/profile data and do not
  copy full condition structs into every profile entry. Resolve the ID through a ROM-backed lookup
  table such as `gEvolutionConditionSets[conditionSetId]` when checking the evolution later. Do
  not drop conditions, or form-heavy/special evolutions like Alcremie-style entries will become
  incorrect.
- `sourceParent` — A or B, identifying whether the evolution updates `parentSpeciesA` or
  `parentSpeciesB`.

**Condition storage decision:** use the condition-set-ID approach with a **build-time generated
stable registry** (the preferred Option 2). Implementation must assign a stable ID to each unique
static evolution condition set used by source data and keep those IDs stable across future releases
once saves can contain them. Candidate-pool construction stores the ID, not the pointer; runtime
evolution checks resolve the ID back to the existing immutable condition data.

Careful implementation path for the registry:
1. Audit all current evolution `CONDITIONS(...)` usage and report max conditions per evolution,
  number of unique condition sets, and repeated/shared condition sets (Milcery/Alcremie-style
  entries are the important stress case). **Completed initial audit:** 679 total evolution
  entries; 220 condition-bearing entries; 104 unique non-empty condition sets; max 2 conditions
  per evolution; most common condition set is friendship-threshold evolution (16 uses).
2. Choose `u8` vs. `u16` for `conditionSetId` from the audit result. **Decision from initial
  audit:** start with `u8 conditionSetId` (0 = none, 1-255 = registered condition sets), because
  104 unique non-empty sets leaves comfortable headroom under 255.
3. Add an append-only manifest (committed source data, likely JSON or a simple generated-friendly
  table) mapping a normalized condition-set signature to a stable ID. Existing IDs must never be
  renumbered once released saves may contain them.
4. Add a generator that scans species evolution data, normalizes each `CONDITIONS(...)` set,
  appends new signatures to the manifest with new IDs, and emits the ROM lookup table
  `gEvolutionConditionSets[]` plus any helper metadata needed by candidate-pool construction.
5. Add validation that fails the build/tests if generated condition-set output is stale, if an
  evolution condition set is missing an ID, or if an existing manifest ID drifts.
6. Unit-test candidate-pool construction with condition-bearing evolutions to ensure stored
  `conditionSetId` values resolve back to the same behavior as the original evolution entries.

### On evolution: which side changes, what reruns, what's preserved
Evolving via one of the available triggers evolves *one side only* — replace whichever of
`parentSpeciesA`/`parentSpeciesB` owned that evolution trigger with its evolved-form species; the
other side's species is untouched. Example: Eevee×Applin fusion evolved via Thunder Stone (an
Eevee-side trigger) becomes a Jolteon×Applin fusion (`parentSpeciesA` updated to Jolteon,
`parentSpeciesB` stays Applin).

After updating the changed side, **rerun the full Berserk Gene trait pipeline between the new
species pair**, recalculating: base stats (still blended, per the updated formula above), height/
weight/pokemonScale/pokemonOffset (blended), learnset (full re-merge using the two new-current
species), color, cry, ability/secondary ability/hidden ability, sprite/identity, and the
potential-evolutions list.

Potential-evolution recalculation after evolution uses a rolling-pool update, not a full reset to
both species' native evolution tables:
1. Start with the fusion's existing `potentialEvolutions[]` entries.
2. Remove every entry whose `sourceParent` matches the source parent side that just evolved. This
  discards stale next-stage options for the side whose current species has changed.
3. Add the immediate next evolutions of the newly evolved/fused-into species for that same source
  parent side, preserving method/param/conditions and tagging those entries with that source
  parent.
4. Recalculate `slotCount` using the same `ceil(lowCount + highCount * r)` logic as at breeding,
  with counts based on the updated per-parent candidate counts.
5. Select the new stored `potentialEvolutions[]` list from the updated pool using the same
  62%/50% weighting that was established at breeding (`geneHolderWeight`), so evolution can
  reshuffle future options but still stays bounded at `MAX_FUSION_POTENTIAL_EVOLUTIONS = 8`.

**Learnset rerun scope (decided):** matches vanilla evolution behavior exactly — recalculating
the learnset on evolution only regenerates the **future** level-up and teachable/tutor move
tables (i.e. what the fusion can still learn going forward, and what TMs/tutors it's eligible
for from this point on). It does **not** retroactively touch the mon's **currently known** move
slots; those stay exactly as they were before the evolution, exactly like a normal Pokémon never
has its already-learned moves swapped out on evolving.

**Preserved unchanged across every evolution** (never recalculated): IVs, shininess, gender, egg
groups, nature, hatch rate (`eggCycles`). These are locked in at birth and are not part of the
rerun.

### Type inheritance across evolution is "sticky by provenance," not re-rolled
Unlike ability/color/cry/sprite (fresh re-roll each evolution, see below), each of the child's two
type slots remembers *which parent side and which of that parent's own type slots* it came from
at birth (`type1SourceParent`/`type1SourceSlot`, same for type2). On evolution, that type slot is
simply re-read from the (possibly new) species at that exact same provenance — not re-rolled
against the full type pool again. Example: if the fusion was born Normal/Dragon with primary type
sourced from parent A's (Eevee's) primary slot, and parent A's line is evolved into Jolteon
(whose primary type is Electric), the fusion's primary type becomes Electric/Dragon. If a
fusion's type slot's provenance parent isn't the one that evolved, that type slot is unaffected
by the evolution.

### Ability / secondary ability / hidden ability / color / cry: fresh reroll, same stored weight
These are **not** sticky-by-provenance. On every evolution, each is independently re-rolled from
scratch between the two new-current species, using `BerserkGeneShouldInheritFromParent` with the
*original* `geneHolderWeight` stored on the profile (62% for one gene holder, 50% for two) — not
a fresh determination of "who holds a gene," since the original breeding parents are no longer
necessarily available/relevant at evolution time.

### Sprite/identity: fresh reroll, same stored weight, but must always change
Also a fresh weighted reroll (same `geneHolderWeight` as above) between the two new-current
species, **except** the candidate that matches `spriteSourceParent` (i.e. whichever species the
fusion currently displays as) is excluded from the roll — guaranteeing the sprite always changes
as a visible sign an evolution happened. If the excluded side is the one that just evolved away
(no longer a candidate at all, since its species literally changed), there's nothing to exclude
and the roll proceeds normally between the two new options. Example: Applin-sprite Eevee×Applin
fusion evolves via Thunder Stone → candidates are {Jolteon, Applin} minus {Applin} → forced
Jolteon. Eevee-sprite version evolving the same way → candidates {Jolteon, Applin}, Eevee itself
isn't a candidate anymore (it turned into Jolteon), so both remain eligible, rolled normally with
the stored weight (62% chance of Jolteon if only Applin held the gene at breeding, per the user's
example). Update `spriteSourceParent` to whichever side is selected.

### Worked multi-step example (Eevee × Applin, for validation during implementation)
1. Birth: Eevee×Applin fusion. Candidate next-stage evolutions are Eevee's immediate branches
  (Vaporeon/Jolteon/Flareon/etc.) plus Applin's immediate branches (Flapple/Appletun/Dipplin).
  The child stores only the selected immediate next-stage entries. Example stored result:
  {Thunder Stone → Jolteon, Syrupy Apple → Dipplin}. It does **not** store Hydrapple yet,
  because Hydrapple is a later-stage evolution from Dipplin, not an immediate next stage from
  Applin.
2. Evolve via Thunder Stone (Eevee-side) → Jolteon×Applin fusion. Rerun stats/size/learnset/
   color/cry/ability/sprite between Jolteon and Applin; type slots re-read at their stored
  provenance (only the Eevee-sourced slot changes, if any). Potential evolutions are recalculated
  from the updated immediate-next-stage pool: Jolteon contributes none, while Applin may newly
  roll/store some subset of Flapple/Appletun/Dipplin.
3. Evolve via Syrupy Apple (Applin-side) → Jolteon×Dipplin fusion. Rerun again. Available
  evolutions are recalculated from the updated immediate-next-stage pool: Jolteon contributes
  none, while Dipplin may roll/store Hydrapple.

### Worked re-breeding example (Poliwag × Dratini lineage)
1. Birth: Poliwag×Dratini fusion. Candidate next-stage evolutions are {Poliwhirl, Dragonair}.
  The child may store both, one, or neither depending on the slot-count roll and weighted
  selection.
2. If the child evolves via the Poliwag side into Poliwhirl×Dratini, the fusion reruns the trait
  pipeline and recalculates potential evolutions from the updated immediate-next-stage pool:
  Poliwhirl may contribute Poliwrath/Politoed-style options, and Dratini may contribute
  Dragonair. It does **not** store Poliwhirl again because Poliwhirl is now the current side-A
  species, not a next-stage option.
3. If that fusion is later put back into daycare, it contributes only its currently stored
  `potentialEvolutions[]` entries as evolution candidates for the next child, not all later
  possibilities from the Poliwag or Dratini family trees.

## Fusion Pokédex display (new)

Berserk-Gene fusion mons never appear in the regular, species-browsable national Pokédex list —
only reachable via Summary → A (Pokédex) for that *specific owned individual* (party or box). That
per-individual Pokédex view needs new, profile-aware branches in `pokedex_plus_hgss.c` distinct
from the normal species-wide dex-entry code path:
- **INFO tab**: subtitle reads "Fusion Pokemon" instead of the species' normal category name.
  Description reads `"It appears to be a fusion of [Species A] and [Species B]."` using the
  current `parentSpeciesA`/`parentSpeciesB` species names (e.g. "...Eevee and Applin." or, after
  evolving, "...Jolteon and Dipplin.").
- **AREA tab**: both DAY and NIGHT read "AREA UNKNOWN" (fusion mons have no real encounter
  location).
- **STATS tab**: populated from the profile's overridden base stats and merged learnset (not the
  nominal displayed species' own data).
- **EVO tab**: populated from the profile's currently stored `potentialEvolutions[]` list (see
  Evolutionary line handling above), not the nominal species' own single-line evolution table.
- **CRY**: plays the profile's overridden cry, not the nominal species' cry.
- **Size display**: uses the profile's overridden height/weight (and by extension
  pokemonScale/pokemonOffset for the rendered model), not the nominal species' own values.

This is a distinct, additional downstream-consumption surface beyond the "Downstream consumption"
section below (which covers gameplay-affecting reads); this one is specifically about the
per-individual Pokédex/Summary display path and needs its own implementation-time investigation
of `pokedex_plus_hgss.c`'s per-mon view entry point (reached from `pokemon_summary_screen.c`,
already referenced in the Downstream consumption section's Option A checklist).

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
- **New, non-optional: side-table lifecycle/orphan tests.** Dedicated test coverage (new or
  extended `test/` file) for the `gBerserkGeneProfiles[]` side table specifically:
  - Allocating a profile consumes exactly one free slot; the table-full condition is detected
    correctly and triggers the daycare's specific "won't go near each other" refusal message
    (not the normal incompatibility message) instead of silently failing or crashing.
  - Every known deletion/overwrite path (release from party/box, PC box overwrite-on-deposit,
    egg cancellation, mass PC storage operations, trade-away, debug mon-clone if present) frees
    its slot correctly — assert a subsequent allocation reuses the freed slot rather than
    consuming a new one.
  - No path leaves a slot marked "in use" with zero mons referencing it (explicit orphan-scan
    assertion across all tested deletion paths).
  - Mon-clone paths (if any exist in this build) duplicate the profile into a distinct new slot
    rather than sharing an index between two independent mons.
- Manual in-game test: breed with one/both parents holding the item, hatch, inspect summary
  screen (type/ability/nature), verify battle type effectiveness matches the overridden type,
  then re-breed that child normally and confirm inheritance behaves like a "native" mon of its
  overridden traits.
- Build via existing project toolchain (`make`) to confirm no size/layout regressions.

## Files likely touched (implementation phase)
- `include/pokemon.h` — new structs, encrypted spare-bit `berserkGeneProfileId` storage in
  `PokemonSubstruct0`, new `MON_DATA_*` accessor.
- `include/pokemon_storage_system.h` — new
  `berserkGeneProfiles[MAX_BERSERK_GENE_PROFILES + 1]` side table in `struct PokemonStorage`.
- `include/save.h` — save version bump.
- `include/constants/global.h` — `MAX_BERSERK_GENE_PROFILES` constant (Mail's `MAIL_COUNT` as
  the precedent for how/where this kind of constant is declared).
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

## Downstream-consumption architecture decision (resolved)

Ran the `gSpeciesInfo[` sweep. Raw result: 373 matches across 48 files — but the large majority
are irrelevant to Berserk Gene (growth rate/exp tables, dex/mythical/legendary flags, sprite/anim
tables, `isFrontierBanned`, etc. — none of these are overridable fields). Narrowing to only the
actual overridable fields (`types`, `abilities`, `baseHP/Attack/Defense/SpAttack/SpDefense/Speed`,
`eggGroups`, `bodyColor`, `cryId`, `height`, `weight`, `pokemonScale`/`pokemonOffset`, `eggCycles`,
`genderRatio`) narrows this to **39 matches across 8 files**.

Critically, almost all of these route through a small set of **centralized, species-keyed
accessor functions in `src/pokemon.c`**: `GetSpeciesType()`, `GetSpeciesAbility()`,
`GetSpeciesBaseHP/Attack/Defense/SpAttack/SpDefense/Speed()`, `GetCryIdBySpecies()`,
`GetSpeciesHeight()`/`GetSpeciesWeight()`, plus `GetGenderFromSpeciesAndPersonality()`. Each takes
only a species ID (± slot/personality) — never a specific mon instance — so there's no direct way
to know "which individual Pokémon is asking."

**Decision: Option A (larger diff, more stable).** Rejected the "global current-mon context"
side-channel (Option B) as too fragile (silent leakage risk if a set/clear point is missed or
reentered). Going with explicit signature-level plumbing instead.

**Refined design (important scope reduction vs. a naive "edit all 373 call sites" reading):**
Do **not** change the existing `GetSpeciesX(species)` functions' signatures — they stay exactly
as-is and keep returning vanilla species-table data. They are still correct and required for every
call site that only has a bare species ID with no owning individual (Pokédex/species-reference
screens, wild-encounter template generation before a mon object exists, form-change tables, etc.).
Vanilla Pokémon games never reflect individual-level overrides in species-wide reference screens
(e.g. the Pokédex always shows the "canonical" model/stats for a species) — same intended
behavior here, so Pokédex-only call sites are explicitly **out of scope** and should keep calling
the existing species-only functions unchanged.

Instead, add new **parallel** `GetMonX(struct Pokemon *mon, ...)` / `GetBoxMonX(struct BoxPokemon
*boxMon, ...)` wrapper functions (one per overridable field) that: read the mon's species and
`BerserkGeneProfile`; if the profile has an override set for that field, return it; otherwise
delegate to the existing `GetSpeciesX(species, ...)` function. Only call sites that (a) already
have a concrete mon instance in hand, and (b) matter for actual gameplay/display correctness for
that individual, get switched to call the new wrapper instead of the old species-only function.

Concrete categorized checklist from the 39-match sweep:
- **Must convert to new mon-aware wrappers** (real per-individual gameplay behavior):
  - `src/pokemon.c` stat calculation call sites using `GetSpeciesBaseHP/Attack/Defense/SpAttack/
    SpDefense/Speed()` for a specific mon's stat calc.
  - `src/pokemon.c:5743` `GetSpeciesType()` call sites operating on an existing mon (battle mon
    population, STAB calc).
  - `src/pokemon.c:5803` `GetSpeciesAbility()` call sites resolving a mon's actual ability from
    its stored `abilityNum`.
  - `src/pokemon.c:9792` `GetTeraTypeFromPersonality(mon)` — already takes a `struct Pokemon *`;
    currently reads `gSpeciesInfo[species].types` inline rather than through a wrapper — needs to
    consult the mon's type override directly.
  - `src/pokemon.c:9652-9654` `GetCryIdBySpecies()` call sites tied to playing an owned mon's cry
    (menu send-out, Pokédex "play cry for caught mon," etc. — the *playback* sites, not the
    species-wide dex-entry sites).
  - `src/pokemon.c:4220-4244` `GetGenderFromSpeciesAndPersonality()` — needs a mon-aware wrapper
    for the explicit gender override field.
  - `src/pokemon.c:5694`/`5699` `GetSpeciesHeight()`/`GetSpeciesWeight()` call sites tied to a
    specific mon (size-based move calc, Pokédex "measured" caught-mon display if that screen is
    per-individual rather than species-wide — needs verification).
  - `src/daycare.c:34` `IS_DITTO` macro and the egg-group/eggCycles checks (`GetDaycareCompat
    ibilityScore`, `SetInitialEggData` friendship/eggCycles assignment) — these need to read a
    *parent's* effective (possibly overridden) egg groups/hatch cycles, not just the species
    table, per the plan's breeding-eligibility and identity sections above.
  - `src/scrcmd.c:1860` `eggCycles` usage — appears to be part of the hatch-countdown script path;
    needs verification that it operates on a real egg's stored (possibly overridden) hatch rate.
  - `src/ow_abilities.c:79,116` (`eggGroups`, `genderRatio`) — verify whether these operate on a
    concrete overworld mon instance (candidate) or a bare species template (skip).
- **Explicitly out of scope (species-wide reference, intentionally unaffected):**
  - `src/pokedex.c` / `src/pokedex_plus_hgss.c` — all `pokemonScale`/`pokemonOffset`/
    `genderRatio`/`eggGroups`/`eggCycles`/`bodyColor` usages here are Pokédex/species-reference
    screens (dex entry, search-by-color, stats tab for an arbitrary/not-necessarily-owned
    species) and should keep reading vanilla species data unchanged.
  - `src/pokemon.c:9515` `IsSpeciesEnabled()` (existence check via `baseHP > 0`) and
    `src/randomizer.c:335` (similar existence check) — infrastructure checks, not per-individual
    trait reads, unaffected.
  - `src/field_specials.c:5879` — checks whether a *species* has two abilities (likely for an
    overworld hint/dialogue), not a specific mon — leave unchanged unless further review shows
    otherwise.
  - **(new, resolved)** Dex/mythical/legendary/frontier-ban flags, and all sprite/animation/
    elevation/shadow-size/footprint/gender-difference-sprite tables — a fusion mon's
    `MON_DATA_SPECIES` is always literally one of its two real current species (whichever is the
    current `spriteSourceParent`/identity), so these are **already correct with zero plumbing** —
    they're read straight from whichever species is currently displayed. No override needed.
  - **(new, resolved)** Tera type — always passively derived from the mon's own (sticky,
    possibly-overridden) primary type; a fusion mon's Tera type resolution must explicitly
    **ignore** `forceTeraType` even if one of the underlying species has one set (see Data model
    section above).

This checklist should be re-verified (with actual file reads, not just grep context) at the start
of the implementation phase before writing the `GetMonX`/`GetBoxMonX` wrappers, since a couple of
entries above (`scrcmd.c:1860`, `ow_abilities.c:79,116`, height/weight Pokédex vs. per-mon) are
flagged as needing closer inspection to confirm which bucket they belong in.

## Implementation strategy / rollout considerations (decided)

Given the size of this change and how hard a storage/architecture mistake would be to unwind
after players start saving fusion mons, the following are **confirmed as requirements** for
implementation, not just suggestions:

- **Compile-time kill switch.** Gate the entire feature behind a config flag (following this
  repo's existing `P_*`/`tx_Mode_*` convention, e.g. `P_BERSERK_GENE_EDITING` in
  `include/config/`). If a serious defect is found post-release, this allows disabling the
  feature (falling back to vanilla breeding behavior) without needing to revert the save-format
  change itself, since existing fusion profiles in the side table would simply stop being
  consulted rather than being destroyed.
- **Zero-regression guarantee for ordinary breeding.** Breeding two non-Berserk-Gene-holding
  parents must remain **byte-for-byte identical** to current behavior — same results, and
  critically the **same number/order of `Random()` calls**, since introducing extra RNG
  consumption on a code path that didn't previously have it can silently perturb any other
  system relying on RNG-call-count parity. This should be an explicit, automated test
  (run the existing `test/daycare.c` suite unchanged after the change and diff results), not just
  an assumption.
- **Phased, independently-buildable implementation order**, rather than one large diff:
  1. Phase 1A: static/source-data audits before finalizing storage layout — condition-set audit
    (already completed initially: `u8 conditionSetId` is sufficient), evolution-entry count audit
    (`MAX_FUSION_POTENTIAL_EVOLUTIONS = 8` chosen despite Milcery outlier), and save-space/layout
    budget check target.
  2. Phase 1B: data model + side-table storage + save-version bump + `MON_DATA_*` accessors,
    with its own unit tests (allocation/orphan lifecycle) — buildable and testable in total
    isolation before any breeding logic exists.
  3. Breeding-time profile creation pipeline (eligibility bypass, identity selection, the 14-step
     trait inheritance, egg-group/type-pool algorithms) — testable via `test/daycare.c` extensions
     without touching any downstream display/battle code yet.
  4. Option A downstream-consumption wrappers (`GetMonX`/`GetBoxMonX`), converting the "must
     convert" checklist sites one at a time, each independently verifiable in-game.
  5. Evolutionary line handling (stored potential-evolution list, rerun pipeline, sticky-type
    provenance).
  6. Fusion Pokédex/Summary display branches.
  7. Trade/link-battle compatibility gating.
  Each phase should be built, tested, and confirmed working before starting the next — this
  bounds the blast radius of any single mistake and makes it much easier to identify which phase
  introduced a regression.
- **A debug/dev-only command to directly synthesize a test `BerserkGeneProfile`** on a chosen
  party mon, bypassing the full breed → deposit → wait → hatch cycle. This is the single biggest
  practical accelerator for iterating on phases 3-5 (which are the largest and riskiest), since
  manually breeding and hatching to get a test fusion for every single tweak is far too slow to
  be a realistic testing loop otherwise. Should be removed or hidden behind a debug-only build
  flag before any public release.
- **Explicit v1 scope boundary: player-bred fusions only (confirmed, not just a default).**
  Fusion Pokémon will **only** ever be produced via daycare breeding. Trainer/NPC-owned mons and
  wild encounters will **never** be fusions — this is a firm design decision, not a placeholder
  pending further scoping. Consequently, `GetMonX`/`GetBoxMonX` wrapper call sites tied purely to
  trainer-party construction (`CreateMon`-style template data) or wild-encounter generation can be
  skipped entirely during the Option A conversion sweep — they will never encounter a
  `berserkGeneProfileId != 0` mon and don't need defensive handling for one.
- **Test on a disposable/beta save, never a primary save**, while validating the save-version
  bump and side-table lifecycle — a bug in the migration path or table sizing is exactly the kind
  of defect that could corrupt or truncate an existing save's Pokémon data.
- **Document the finished feature** in `CHANGELOG.md`/`FEATURES.md` per this repo's existing
  convention, once implemented and stable — not before, to avoid documenting behavior that changes
  during implementation.

### On using a separate specialized agent for this
Not recommended as "more expertise" — I already have full context on this codebase and this exact
plan, and spinning up a separate agent persona doesn't add domain knowledge that isn't already
here. What actually reduces the risk of an architectural mistake is **process discipline**: the
phased rollout above, the zero-regression guarardrail, and the orphan/lifecycle unit tests. If
what you actually want is a second, independent pair of eyes to critique the plan or review the
resulting code with fresh skepticism (rather than "more expertise"), that's a reasonable thing to
ask for explicitly at specific checkpoints (e.g. "review the plan/diff for phase 1 as if you
hadn't written it") — but that's a review-technique choice you can invoke at any time, not a
capability gap that requires a dedicated agent to be configured up front.

## Additional open items from the evolutionary-line update
- **Potential-evolution storage packing**: target species, method, param, and source side A/B are
  required fields, and extra evolution `CONDITIONS(...)` are represented by stable
  `conditionSetId` values (0 = none) resolved through a ROM lookup table generated from an
  append-only manifest. **Phase 1 packing result:** `FusionPotentialEvolution` is now a packed
  6-byte entry (`targetSpecies`, `param`, `methodAndSourceParent`, `conditionSetId`), with
  source parent encoded into the method byte because current `enum EvolutionMethods` easily fits
  below 7 bits. The completed audit found only 104 unique non-empty condition sets, so
  `u8 conditionSetId` remains sufficient. Revisit only if later source data pushes unique
  condition sets near 255, if evolution methods approach 128 values, or if a future save-layout
  redesign allows a larger/more explicit entry.
  The key invariant is that the profile stores only immediate next-stage evolution entries, never
  full future lines; repeated daycare fusion can only inherit from a fusion parent's currently
  stored next-stage entries, not from that parent's full ancestry.
- **Evolution-trigger hook point**: need to find and patch the actual evolution-checking code
  (`GetSpeciesEvolutions`/`TryDoEvolution`-equivalent flow) to (a) use `GetMonEvolutions(mon)` for
  a fusion mon instead of its nominal species' own table, and (b) run the whole "rerun the
  pipeline between the updated species pair" sequence described above as part of completing an
  evolution for a fusion mon specifically.
- **Fusion Pokédex display hook points**: need to identify the exact functions in
  `pokedex_plus_hgss.c` (and possibly `pokemon_summary_screen.c`'s "open Pokédex for this specific
  mon" entry point) responsible for each of INFO/AREA/STATS/EVO/CRY/size, to know where to branch
  on `hasProfile`.
- Current measured byte budget: `FusionPotentialEvolution = 6` bytes and `BerserkGeneProfile = 92`
  bytes with `MAX_FUSION_POTENTIAL_EVOLUTIONS = 8`.

## Pre-implementation review (final pass before starting)

### Storage: capped side table (decision made — supersedes the earlier embed-in-`BoxPokemon` plan)
After comparing both designs, we're going with a **capped side table**, following the existing
Mail precedent in this codebase (`BoxPokemon.mail` index → `gSaveBlock1Ptr->mail[MAIL_COUNT]`).
See the Data model / storage section above for the finalized field/array shape. Key consequences
accepted with this choice:
- **Fusion count is now hard-capped** at `MAX_BERSERK_GENE_PROFILES = 16` from the Phase 1
  storage audit and compact-entry pass. This matches Mail's `MAIL_COUNT = 10 + PARTY_SIZE = 16`,
  but each Berserk Gene profile is much larger than a mail entry and current save headroom is now
  effectively exhausted. This is the **Phase 1 internal-backend cap**, not the desired final
  feature ceiling; moving profiles to external/special save storage is a confirmed future
  milestone.
- **Orphaned-slot cleanup is now the single biggest implementation risk** (promoted from a
  secondary concern under the old embed design, where it didn't apply at all). Every deletion/
  overwrite/discard path touching a `BoxPokemon` must explicitly free its referenced slot, or the
  effective cap silently shrinks over time. This has dedicated unit-test coverage requirements
  (see Testing/validation plan) and its own subsection under Data model / storage above.
- **New daycare UX case**: when the side table is completely full, breeding a would-be Berserk
  Gene child must be refused with the specific flavor-text message *"Your Pokémon simply won't
  go near each other. It seems like something is wrong."* rather than crashing, silently failing,
  or reusing the normal incompatibility message.
- Non-fusion mons do **not** increase `BoxPokemon` size because the profile ID is packed into
  existing encrypted spare bits. The total worst-case save cost is the deliberately bounded side
  table: `(MAX_BERSERK_GENE_PROFILES + 1) × 92 bytes = 1564 bytes`, rather than an unconditional
  tax on every boxed mon — this was the deciding factor in moving away from the embed design.
- **Migration-friendly access rule:** all gameplay/UI/link/daycare code must access profiles only
  through centralized helper APIs. Direct reads/writes of `gPokemonStoragePtr->berserkGeneProfiles`
  outside the storage module are forbidden so the backing store can later move to an external save
  area with minimal behavioral-code churn.

### Confirmed future storage migration
External/special save storage is planned for a later phase. Risks of deferring are accepted for now
because Phase 1 needs a small, testable storage backend before the breeding pipeline exists, but
the implementation must avoid baking in the internal table as the permanent architecture. The later
migration phase must include:
- a concrete special-sector/external-save design and corruption/dual-save-slot handling;
- migration from the internal 16-slot table to the external table on save-version upgrade;
- preservation of existing profile IDs where practical, or a full scan/rewrite of all mons'
  profile IDs if the ID scheme changes;
- compatibility gating for link/trade if one build uses internal storage and another uses the
  external backend;
- tests proving old internal-table saves load, migrate, save, reload, and free/reuse profile slots
  correctly after migration.

### Biggest concerns, ranked (revised for the side-table decision)
1. **Orphaned side-table slots.** Now the top risk (see above) — requires an exhaustive sweep of
   every `BoxPokemon` deletion/overwrite/discard path plus dedicated unit tests asserting freed
   slots are actually reclaimed and no slot is ever left "in use" with nothing referencing it.
2. **Silent staleness from a missed Option A call site.** Unchanged from the earlier review — a
   missed conversion doesn't crash, it just quietly shows vanilla species data instead of the
   override for that one screen/system.
3. **Trade/link transfer of side-table entries.** Unlike the embed design, a `berserkGeneProfileId`
   index is only meaningful within one save file. Trading a fusion mon to another player now
   requires transferring the actual profile payload (not the index), allocating a free slot in
   the recipient's table, and rewriting the transferred mon's index — mirroring however mail
   transfer already handles this same class of problem in `trade.c`, which should be read as a
   template before implementing this.
4. **Evolution-rerun edge cases.** Unchanged from the earlier review — the retry-with-fallback
   logic (type-pool exhaustion, egg-group duplicate retries) is the most likely place for an
   off-by-one or infinite-loop bug; needs deliberate unit tests, not just manual play-testing.

### Two clarifications needed before implementation starts (resolved)
- **Does evolution retroactively touch a fusion's already-known moves? Resolved: no.** Locked to
  "only future-learnable tables change; currently known moves are untouched," matching vanilla
  behavior exactly — see the Evolutionary line handling section above, which now states this
  explicitly.
- **Precedence vs. the existing Randomizer/Fairy-type-challenge features. Resolved: not a
  precedence question — all three compose together.** `GetSpeciesType()`'s existing
  `RandomizerFeatureEnabled(RANDOMIZE_MON_TYPES)` and `tx_Mode_Fairy_Types` branches must run
  **first**, exactly as they do today, producing each parent's *effective* (already
  randomized/Fairy-converted) type(s). The Berserk Gene type-inheritance pool (primary/secondary
  type selection, step 1/2 above) then operates on those already-resolved effective types, not
  the raw species-table types. In other words: randomizer output and Fairy-conversion output are
  simply what "a parent's type" *means* by the time Berserk Gene logic runs — there is no
  override-wins-over-override conflict, because Berserk Gene never reads `gSpeciesInfo[].types`
  directly; it reads whatever the existing `GetSpeciesType()`/effective-type resolution already
  produces (randomizer- and Fairy-aware) and blends *that*. This applies anywhere else the plan
  says "effective type(s)" (egg-group/type pool selection, evolutionary sticky-type re-read on
  evolution, etc.) — all of those already meant "post-randomizer/post-Fairy" and this just makes
  it explicit.

### Additional risk flagged, not yet in scope
- **Asynchronous mon transfer paths** (e.g. `RecordMixingGift`/record mixing referenced in
  `SaveBlock1`, or any offline/wireless-adapter save-data exchange, if such exist in this build)
  may not go through the live link-session version handshake at all, since that only guards
  *active* link sessions. If any such asynchronous path exists, it could bypass the trade/link
  compatibility gating entirely and needs its own investigation — flagged for the trade/link
  implementation phase, not resolved here.


