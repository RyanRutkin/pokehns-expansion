# Berserk Gene Editing — Implementation Plan (finalized)

Status: IMPLEMENTATION IN PROGRESS; milestone 1 is complete at the 2026-11-10 same-target checkpoint below.
This file consolidates `berserk-gene-editing-prompt-1.md` and `beserk-gene-editing-prompt-1-answers.md`
plus the resulting plan and decisions. Resume implementation from here when prompted.

## Resume checkpoint (2026-11-09)

Validated the preserved November 4 parent-side evolution fix. Also normalized the paired-sibling
source-parent flag before boolean comparisons, and extended the paired Phase 1 regression to
resolve from either parent's target while preserving name priority and both selected roots.

The rebreeding timeout was a real empty-evolution-list bug, not just a script setup problem:
`GetSpeciesEvolutions()` falls back to `SPECIES_NONE` for a terminal species, but that fallback's
evolution pointer was also null. The native descendant collector then scanned address zero without
finding an end marker. `SPECIES_NONE` now provides an explicitly terminated empty evolution array.
Dedicated regressions check both the fallback species and terminal Raticate.

The rebreeding regression now calls `GiveEggFromDaycare()` directly, after checking compatibility.
Its six parameter cases cover an unevolved fusion, a fusion after Phase 1, and a completed
Phase 2 fusion, in either daycare slot. Assertions cover saved descendant grouping, remaining
lineage only, source-side retagging, age increments/saturation, a fresh child phase/profile, and
an unchanged parent profile. All six cases pass; this supersedes the November 4 timeout warning.

Validation:
- `make -k hns -j24`: passed; production ROM linked successfully.
- `make -k check BUILD=hns -j24 TESTS="(Daycare) Rebreeding"`: passed (six parameter cases).
- `make -k check BUILD=hns -j24 TESTS="test/berserk_gene.c"`: 24 tests passed, including the
  evolution regressions; the existing egg-group compatibility fixture still fails, as it did
  before these changes.
- `make -k check BUILD=hns -j24 TESTS="test/daycare.c"`: seven tests passed, including rebreeding,
  empty evolution lists, and eight-node pruning; 26 other tests failed, predominantly script-driven
  parent/egg setup. The paired-line script test was also confirmed failing before the empty-list
  fix. These broader failures are not resolved by this slice; do not claim a green daycare suite.

### Multi-root follow-up (2026-11-09)

Added a four-root/eight-node regression with two roots on each parent side. Its 12 parameter cases
cover every Phase 1 choice and every remaining Phase 2 choice, including crossing to the opposite
side. Assertions verify same-side sibling removal, root-contiguous descendants, aligned age
nibbles after compaction, rejection of consumed item-evolution targets, counterpart resolution,
branch-average base stats, and completion after Phase 2.

Added eight rebreeding cases (all four Phase 1 choices, either daycare slot). The child samples only
the parent's three retained active descendants, never consumed siblings or native ancestry; those
descendants become fresh roots with preserved methods/parameters, incremented ages, and correctly
retagged source sides. The parent's profile remains unchanged. New parameterized tests explicitly
release their profiles between cases; resetting PC storage does not clear party/daycare profiles.

A separate regression exposed and fixed a Phase 2 counterpart bug: an opposite-side node was
excluded merely because its target species matched the chosen target. Counterpart membership now
depends on the node's source side, not a different-species requirement. No save layout or profile
size changed.

Validation for this follow-up:
- `make -k check BUILD=hns -j24 TESTS="Fusion lineage"`: passed, three tests / 21 parameter cases.
- `make -k check BUILD=hns -j24 TESTS="Fusion"`: ten tests passed; three unrelated Fusion
  Flare/Bolt tests remain marked TO_DO.
- `make -k check BUILD=hns -j24 TESTS="(Daycare) Rebreeding"`: passed, six existing parameter cases.
- `make -k check BUILD=hns -j24 TESTS="test/berserk_gene.c"`: 25 passed, two failed: the existing
  egg-group compatibility fixture and the allocation fixture's assumption that slot 1 is free.
  The latter passes alone with `make -k check BUILD=hns -j24 TESTS="Berserk Gene profile slots"`;
  it is sensitive to other tests leaving profiles allocated. Do not claim a green combined file.
- `make -k hns -j24`: passed; production ROM linked successfully.

### Pseudo-fusion move aggregation (2026-11-09)

Profile-aware level-up, egg, and teachable projections now reconstruct both resolved sides from
the saved lineage. The selected side contributes its concrete target; a pseudo side contributes
all retained same-phase branch species, not merely its unchanged display species. Consumed
same-side siblings never contribute. Phase 2 resolution now marks its chosen descendant as selected
while retaining Phase 1 root markers, so the final move pools remain reconstructible after the
active evolution menu is empty.

The existing 62/38 or 50/50 per-side move budgets remain unchanged. Within a multi-species side,
level-up and egg slots are allocated evenly across branch lists, redistributing unused slots from
short lists within that side, and sampled evenly within each list. This extends the existing
deterministic sampling approach rather than adding RNG consumption on menu access. Level ordering,
type-relevance priority, deduplication, and the 20-level-up/18-egg-move caps are preserved.
TM/tutor eligibility remains the complete union, now over all contributing species; its IWRAM
buffer is bounded by `MOVES_COUNT_ALL` instead of the former two-species estimate of 744.
Existing party/box relearner, level-up, and owned-mon Pokédex call sites use these shared helpers.

`evolutionFlags` bit 4 (`BERSERK_GENE_RESOLVED_MOVE_SOURCES`) marks profiles whose resolution
metadata supports completed-phase reconstruction. Older already-completed profiles lack the
Phase 2 selection information; they deliberately retain their previous two-display-species move
projection rather than being incorrectly regressed to Phase 1. No structure or save layout grew.
Terminal Phase 1 pseudo pools also remain available when there are no Phase 2 descendants.

Validation:
- `make -k check BUILD=hns -j24 TESTS="Fusion pseudo-side"`: passed, two tests / 25 cases.
  The main 24-case regression covers both phases, selected sides, Modern Moves settings, and
  50/50 plus both 62/38 weightings. Checks include exact teachable union, bounded/deduplicated
  level-up and egg pools, level/type ordering, party/box parity, unchanged known moves/profile/RNG,
  and normal-mon species accessors. The second test covers terminal Phase 1 and legacy completed
  profiles. Synthetic descendant species give both move modes nonempty egg lists; tests do not
  invent or modify native evolution/learnset data.
- `make -k check BUILD=hns -j24 TESTS="Fusion"`: 12 passed; three unrelated Fusion Flare/Bolt
  tests remain TO_DO.
- `make -k check BUILD=hns -j24 TESTS="(Daycare) Rebreeding"`: passed, six existing cases.
- `make -k check BUILD=hns -j24 TESTS="test/berserk_gene.c"`: 28 passed; the existing egg-group
  compatibility fixture still fails. The order-sensitive allocation fixture passed in this run.
- `make -k check BUILD=hns -j24 TESTS="test/save.c"`: PokemonStorage passed at 35,680 bytes.
  Three other save-size pins do not match this build: actual SaveBlock1/2/3 are 15,760/3,892/52
  versus pins 15,752/3,884/84. Those structures were not modified; this suite is not green.
- `make -k hns -j24`: passed; EWRAM 260,116/262,144 bytes, IWRAM 27,644/32,768 bytes.

### Pseudo-fusion color/cry and birth-selected growth (2026-11-09)

Implemented Option 1 for evolution-time color and cry: independently roll each trait's parent
side with the saved 62/38 or 50/50 weighting, then choose a sibling uniformly within that side.
Both traits reuse the saved-lineage reconstruction used by movepools; neither advances the
pseudo-side's displayed identity. Types and abilities still use the displayed species. Existing
single-source/paired behavior and legacy completed-profile source fallback remain intact.

Growth rate is selected only when breeding writes the fusion profile and is never rerolled
on evolution (user clarification, 2026-11-09). A fusion parent contributes its stored growth
curve when rebreeding, not its nominal species' curve. Newly created fusion eggs now receive
the chosen curve's EXP at `EGG_HATCH_LEVEL`.

Added shared party/box growth accessors and wired level calculation, level increments, candy
thresholds/caps, daycare withdrawal/caps, battle EXP and hard caps, battle/summary EXP bars,
summary next-level EXP, and existing-mon facility level setters. Normal Pokemon retain species
growth. Randomizer evolution EXP conversion now excludes valid fusion profiles: evolving a fusion
must preserve both its birth-selected curve and its raw EXP. Species-only creation/facility wild
generation retains native growth. No saved structures or persistent buffers grew.

Validation:
- `make -k check BUILD=hns -j24 TESTS="Fusion"`: 17 passed; three unrelated Fusion Flare/Bolt
  tests remain TO_DO. New coverage includes:
  - Color/cry: 12 parameter cases (both phases, selected sides, and all three weightings), each
    with 1,024 seeded rolls. Counts must stay within 50 rolls of the expected weighted/uniform
    distribution, and all nine color/cry source combinations must occur. Hidden siblings
    contribute; abilities remain restricted to displayed species; growth and EXP remain unchanged.
  - Growth/levels/candies: all six curves, both evolution phases, party/box parity, unchanged
    evolution EXP, Rare Candy, EXP Candy, maximum-EXP clamping, and normal-species fallback.
  - Breeding: both daycare slots, stored fusion-parent growth inheritance, and egg EXP.
  - Daycare withdrawal: Fast/Slow curves in either slot, clamped to the curve's level-cap EXP.
  - Battle: Fast/Slow fusion curves plus normal-mon controls, each as active battler or inactive
    Exp. Share recipient, starting one EXP below the hard cap. EXP bars and final level/EXP pass.
- `make -k check BUILD=hns -j24 TESTS="Fusion growth battle"`: passed with four fusion cases;
  the final `Fusion` run additionally verifies both normal-mon controls (six cases total).
- `make -k check BUILD=hns -j24 TESTS="(Daycare) Rebreeding"`: passed, six existing cases.
- `make -k check BUILD=hns -j24 TESTS="test/berserk_gene.c"`: 30 passed; the previously documented
  egg-group compatibility fixture still fails. This run preceded the final expanded roll count;
  the final `Fusion` run verifies the expanded categorical regression.
- `make -k check BUILD=hns -j24 TESTS="test/battle/exp.c"`: new fusion battle regression passed;
  four other tests failed (three unmatched-message fixtures and an invalid Ultra Ball fixture
  lacking an explicit party index). This suite is not green; no baseline comparison was run.
- `make -k hns -j24`: passed; EWRAM 260,116/262,144 bytes, IWRAM 27,644/32,768 bytes,
  ROM 31,696,756/33,554,432 bytes.

### Pseudo-side constructed names (2026-11-09)

Implemented pseudo-side names in `GetMonDisplaySpeciesName()` using the same deduplicated,
saved-order resolved sibling sources as movepools/color/cry. The user clarified that each sibling
contributes its natural chunk length, even when the result differs from the rounded average.
This supersedes the earlier requirement to force the pseudo-name to the rounded-average length.
For sibling `i` of `N`, the chunk starts at `floor(i * nameLength / N)` and contributes
`ceil(nameLength / N)` characters. Rounded chunks may overlap within a source name, just as
the original two-source rule retains an odd name's middle character in either half.
Example: Eevee + Jolteon yields `EEVTEON`, not the average-budget `EEVEON`.

The two side names are then combined using the existing rounded-up prefix/suffix halves and
stored leading-parent priority. If those halves exceed `POKEMON_NAME_LENGTH` (12), the leading
count is scaled proportionally and rounded to the nearest character (ties toward the leader);
the trailing count receives the remaining budget. Prefix/suffix anchoring is preserved.
Only bounded stack workspaces were added; no save structures or static buffers grew.

Existing summary/Pokedex species-name and hatch-default-name callers inherit the new names.
Evolution's existing default-nickname update now follows reconstructed sibling names, while
custom nicknames remain unchanged. Reading names changes neither the saved profile nor RNG.
Birth names and legacy completed profiles without resolved metadata keep their two-source
behavior; paired phases keep stored priority.

Validation:
- `make -k check BUILD=hns -j24 TESTS="Fusion names"`: three tests / 20 parameter cases passed:
  - 16 cases cover either selected side, two/three uneven-length siblings, default/custom
    nicknames, both evolution phases or terminal Phase 1, party/box-restored parity, stable
    profile/RNG, and legacy/normal fallback. Exact expected strings are asserted.
  - Two cases cover paired-phase priority and deduplication of equal sibling target species.
  - Two cases cover five/seven long sibling names in both priority orders, proportional
    shortening (including both contributions shrinking), and the exact 12-character limit.
- `make -k check BUILD=hns -j24 TESTS="Fusion"`: 20 passed; three unrelated Fusion Flare/Bolt
  tests remain TO_DO.
- `make -k hns -j24`: passed; EWRAM 260,116/262,144 bytes, IWRAM 27,644/32,768 bytes,
  ROM 31,696,948/33,554,432 bytes.
- `git diff --check`: passed. No manual emulator UI inspection was performed.

### Read-only evolution name previews (2026-11-09)

Added `GetMonEvolutionDisplaySpeciesName(mon, targetSpecies)`. It projects the same lineage
transition used by live evolution onto a stack-local profile copy, then uses the shared fused-name
builder. Projection skips numeric/appearance rerolls and nickname writes, consumes no RNG, and
does not allocate a profile or modify the active-evolution projection cache. The existing active
choice collector now also supports counting choices without writing that cache.

Normal and trade evolution scenes now prepare their target-name text from the projected fusion
name. The owned-mon Pokedex evolution list also uses projected names and measures those names for
method-text positioning/font selection. Native-species Pokedex entries and non-fusion evolution
names remain unchanged. Existing unseen-name hiding, target icons, caught markers, and native
target-species bookkeeping are preserved.

Validation:
- `make -k check BUILD=hns -j24 TESTS="Fusion"`: 21 passed; three unrelated Fusion Flare/Bolt
  tests remain TO_DO.
  - Existing name tests now compare previews with exact expected names and committed results
    across both parent sides, two/three siblings, both phases or terminal Phase 1, paired phases,
    default/custom nicknames, and five/seven long-sibling name-limit cases.
  - The 12-case multi-root regression also compares previews with committed names for every
    retained Phase 1/Phase 2 path, including switching to an opposite-side descendant.
  - Before committing each tested transition, all current options are previewed in reverse order,
    then the chosen option twice. Byte comparisons verify unchanged mon/profile/RNG and the
    caller's cached active-choice list.
  - A dedicated regression verifies ordinary species-name fallback and completed legacy-profile
    behavior without mutation.
- `make -k check BUILD=hns -j24 TESTS="(Daycare) Rebreeding"`: passed, six existing cases.
- `make -k hns -j24`: passed; EWRAM 260,116/262,144 bytes, IWRAM 27,644/32,768 bytes,
  ROM 31,697,172/33,554,432 bytes. No persistent data/buffer size increased.
- No manual emulator inspection of evolution/Pokedex text was performed.

### Hidden-sibling sprite donors and PC rendering checkpoint

The user chose to include hidden pseudo-side siblings as sprite donors: first select a side
with the saved parent weighting, then select a distinct eligible species uniformly within that
side. Remove the actual previous sprite species from both sides before rolling; force the other
side if only it has alternatives. If no different species exists anywhere, retain the sole
possible species rather than attempting an impossible visible change.

- Persist hidden donors in bits 5-7 of the existing `evolutionFlags` byte. Zero keeps the legacy
  displayed-parent donor; values 1-7 are one-based indices into the selected side's reconstructed,
  deduplicated sources. The side still uses `BERSERK_GENE_FLAG_SPRITE_SOURCE_PARENT`.
- At least one of the eight lineage nodes belongs to the chosen side, leaving at most seven
  sibling sources on the opposite side. No profile, BoxPokemon, or saved-storage expansion.
- Capture the actual old donor before changing lineage. Do not overwrite parent species,
  type/ability identity, or nominal `MON_DATA_SPECIES` to record a hidden donor.
- Existing party-menu icons, summary portraits, and box icons consume the shared donor getter.
  PC selected-mon graphics and palettes now use a separate transient donor field while keeping
  nominal species for text/identity; party icons, held/moving icons, and incoming scrolling-box
  icons also use the getter.
- Tests cover all seven sibling ordinals on either side, party/box restoration, legacy zero
  encoding, prior hidden donors, shared donors excluded from both sides, the no-alternative
  case, and saved 50/50 or 62/38 side weighting with uniform sibling selection.

Validation:
- `make -k check BUILD=hns -j24 TESTS="Fusion"`: 24 passed; three unrelated Fusion Flare/Bolt TODOs.
- `make -k check BUILD=hns -j24 TESTS="test/berserk_gene.c"`: 36 passed, two failed:
  daycare egg-group compatibility and profile-slot allocation. The latter expects slot 1 after
  a storage reset that does not reset all profile allocations.
- `make -k check BUILD=hns -j24 TESTS="Berserk Gene profile slots"`: passed in isolation.
  The full-file failures were not baseline-compared for this slice; do not claim a clean full suite.
- `make -k hns -j24`: passed; EWRAM 260,116/262,144 bytes, IWRAM 27,644/32,768 bytes,
  ROM 31,697,396/33,554,432 bytes, including the final PC icon wiring.
- No manual emulator inspection or actual save/reload was performed; restoration coverage uses
  the saved profile and `BoxMonToMon`.

### Battle visual-donor checkpoint

- `GetBattlerVisualSpecies` is now rendering-only: explicit Transform/form graphics take
  precedence, then an active Illusion model's saved donor, then the actual mon's saved donor.
  `GetIllusionMonSpecies` remains nominal for gameplay/AI consumers.
- Battle loading uses donor front/back graphics and the matching normal/shiny palette.
  Controller sprite templates, animations, shadows, offsets, elevation, and picture dimensions
  follow the same visual species. Role Play/Snatch duplicate sprites also resolve the donor,
  Illusion model personality, and transformed personality/shininess.
- Transform copies the visible target donor without copying it into gameplay species; its
  existing Gigantamax base-form exception remains. Illusion reveal restores the user's own donor.
  Temporary form graphics still override the donor. Switching clears the temporary override,
  restoring the saved donor; Substitute restoration uses the same loader.
- Dynamax species restrictions explicitly retain their previous nominal/Illusion-species checks,
  rather than following sprite donors. KO animation cry playback also retains its previous
  species choice; this slice does not make the sprite donor override the independent cry trait
  or complete the remaining cry-playback consumer audit.
- No saved layout or persistent buffer increased.

Validation:
- `make -k check BUILD=hns -j24 TESTS="Fusion"`: 32 passed; three unrelated Fusion Flare/Bolt TODOs.
  Eight new battle tests cover 21 parameter cases, including front/back loading, ordinary/legacy/
  hidden donors, shiny palettes, dimensions, Illusion reveal, Transform, form changes,
  switch/Substitute restoration, Role Play/Snatch animations, and nominal Dynamax restrictions.
  Assertions compare actual loaded first-frame pixels with raw source assets and all 16 palette
  entries (including Transform tint). Raw assets avoid stack-heavy decompressors on the battle
  DSL's 1KB stack. Form/reveal/Substitute/duplicate-sprite fixtures force real animations in
  headless mode; ordinary Transform already runs there.
- `make -k check BUILD=hns -j24 TESTS="test/battle/move_effect/role_play.c"`: four passed.
- Broader affected suites were run both with these changes and in an isolated worktree at
  committed baseline `92f4e6574d`, with matching outcomes:
  - `make -k check BUILD=hns -j24 TESTS="test/battle/move_effect/transform.c"`:
    two passed, five unmatched-message failures, one TODO.
  - `make -k check BUILD=hns -j24 TESTS="test/battle/ability/illusion.c"`:
    five passed, three unmatched-message failures.
  - `make -k check BUILD=hns -j24 TESTS="Dynamax"`:
    16 passed, 67 failed, five TODOs. All 88 individual outcomes were normalized and compared
    with the baseline, not just the totals; they match exactly.
  - `make -k check BUILD=hns -j24 TESTS="test/battle/move_effect/snatch.c"`:
    one passed, five unmatched-message failures, two TODOs.
  These existing suite failures were not changed or hidden; do not claim a clean broad suite.
- `make -k hns -j24`: passed; EWRAM 260,116/262,144 bytes, IWRAM 27,644/32,768 bytes,
  ROM 31,697,332/33,554,432 bytes.
- No manual emulator inspection or link-battle verification was performed.

### Evolution-animation visual-donor checkpoint

- Evolution now prepares a complete transient profile, including the final appearance rolls,
  before loading its sprites. Preparation never writes the live Pokemon, profile, nickname, or
  shared active-choice projection. Phase 2 stat blending projects choices from the supplied
  profile instead of accessing the live mon's shared projection.
- Normal and trade scenes use separate pre/post visual species for graphics, matching shiny/
  normal palettes, templates, and front animations. Their task species remain nominal for
  species assignment, evolution text, moves, Pokedex registration, and Shedinja handling.
  The incoming trade sprite is refreshed in its existing right-side buffer and palette slot;
  move-summary return callbacks reload the retained post-evolution donor without preparing again.
  The trade return callback's sprite template now matches the right-side buffer it loads.
- At the existing successful-evolution state, commit copies that exact prepared profile and
  updates a matching default fused nickname, without consuming RNG. Custom nicknames, the
  birth-fixed growth curve, raw EXP, and the 96-byte saved profile layout remain unchanged.
- Cancellation discards the heap-owned preparation with the scene; it never commits the
  profile. The existing cancellation window/control flow is unchanged. Preparation does consume
  RNG even if the player subsequently cancels; global RNG is deliberately not rewound, since
  animation/unrelated RNG may already have advanced. A later evolution attempt prepares afresh.
- Visual animation species and cry species are explicitly separate. Cry playback retains its
  previous nominal-species choices; this slice does not complete the independent cry-trait
  playback audit. Read-only name previews remain RNG-free and do not prepare appearance.

Validation:
- `make -k check BUILD=hns -j24 TESTS="Fusion"`: 34 passed; three unrelated Fusion Flare/Bolt
  TODOs. Two new tests cover seven parameter cases: both evolution phases, default/custom
  nicknames, preparation/discard isolation, unchanged cached choices, RNG advancement only at
  preparation, exact prepared/committed profile and donor parity, fixed growth/EXP, ordinary
  mons, unmatched targets, and legacy completed profiles.
- `make -k check BUILD=hns -j24 TESTS="*pseudo-fusion"`: one passed, covering Phase 2
  counterpart averaging with the pure-profile stat projection.
- `make -k check BUILD=hns -j24 TESTS="(Daycare) Rebreeding"`: one passed / six parameter cases.
- `make -k hns -j24`: passed; EWRAM 260,116/262,144 bytes, IWRAM 27,644/32,768 bytes,
  ROM 31,697,812/33,554,432 bytes.
- `git diff --check`: passed.
- The automated assertions exercise the shared transaction API, not the full scene state
  machines. Manual emulator checks of normal/trade animation, B-button cancellation, and
  move-summary return remain outstanding, as does live link/profile-transfer verification.

### Hatch and owned-Pokedex portrait checkpoint (2026-11-10)

- Fixed a prerequisite hatch lifecycle bug: `CreateHatchedMon` rebuilt the Pokemon without
  restoring its fusion profile ID. Valid profiles now survive both script-driven and animated
  hatching, and hatch-level EXP is initialized on the existing, birth-selected growth curve.
  The profile itself is neither rewritten nor rerolled. Moves, IVs, personality, shininess, and
  the existing nominal-species Pokedex registration remain intact.
- Hatch reveal graphics, normal/shiny palettes, template animations, and front animation species
  now use the saved donor, including hidden siblings. Egg shell graphics/palettes still use the
  nominal species, and hatch cry playback retains the nominal species rather than accidentally
  following the sprite donor. Default hatch nicknames continue using the fused display name.
- Added `DoMonFrontSpriteAnimationWithCry` to select visual and cry species separately while
  retaining the original frame/delay/panning/skip behavior. The original entry point delegates
  with identical visual/cry species, preserving existing consumers. Evolution scenes also use
  this helper: the previous `noCry=TRUE` workaround inadvertently suppressed second-frame
  animation; this shared-path correction restores it without changing their cry choices.
- Owned-mon HGSS Pokedex info/cry portraits and size silhouettes share the donor-aware portrait
  factory, retaining the actual mon's personality and shininess. The owned entry also remembers
  its nominal form, so an alternate-form fusion does not lose donor rendering during dex-number
  conversion. Only a matching owned-mon entry uses its donor; species-only catalogs and browsing
  other forms retain representative species graphics/personality and non-shiny palettes.
  Measurements, silhouette scaling, types, names, and dex identity remain their existing
  gameplay/profile values, not the sprite donor's values.
- No saved layout or persistent buffer increased.

Validation:
- `make -k check BUILD=hns -j24 TESTS="test/fusion_portraits.c"`: three passed / 20 parameter cases.
  Tests cover ordinary, displayed-parent, and hidden-sibling donors with normal/shiny palettes;
  retained profile/growth/EXP/moves/IVs/personality/nickname after hatching; a complete hatch scene
  through the no-nickname return; actual loaded front pixels, template animation, and all 16
  palette colors; actual owned-Pokedex info loading/exit, alternate-form nominal species, and
  unchanged species-only entries. Live mon/profile snapshots are checked for unintended writes.
  The isolated hatch fixture releases detached background allocations instead of entering the
  field return callback; this is not a full overworld/resource-lifecycle audit.
- With that selector still patched into the non-headless test ELF:
  `tools/mgba-rom-test-hydra/mgba-rom-test-hydra tools/mgba/mgba-rom-test arm-none-eabi-objcopy pokehns-test.elf`:
  three passed / the same 20 parameter cases, with non-headless front animations enabled.
  This additionally checks second-frame playback when `HasTwoFramesAnimation` permits it;
  ordinary `make check` always patches headless mode and disables that behavior.
- `make -k check BUILD=hns -j24 TESTS="Fusion"`: 37 passed; three unrelated Fusion Flare/Bolt TODOs.
- `make -k check BUILD=hns -j24 TESTS="(Daycare) Rebreeding"`: one passed / six parameter cases.
- `make -k hns -j24`: passed; EWRAM 260,116/262,144 bytes, IWRAM 27,644/32,768 bytes,
  ROM 31,698,084/33,554,432 bytes.
- `git diff --check`: passed.
- No manual visual inspection or live link verification was performed. Cry/size subpage
  navigation was audited through the shared factory but not exercised in the new UI fixture.

Rendering status after the hatch checkpoint: Battle/PC/evolution/hatch/owned-Pokedex portrait
rendering is wired, but feature-wide rendering/cry auditing is not complete. In particular,
naming-screen icons/titles and the surrounding trade presentation still need a separate consumer
audit; this checkpoint does not change those species-only interfaces.
The same-target attribution gap from this checkpoint is resolved by the following checkpoint.
Previously saved default nicknames are not automatically migrated when their newly reconstructed
species name differs: preserve existing nicknames until a deliberate migration policy is chosen.
Older saved fusion profiles
already contain a growth curve, but their existing EXP may have been earned on the nominal species'
curve before these consumers honored the stored value. No legacy EXP migration is performed here;
audit/decide migration before treating old-save behavior as release-ready. Summary/battle UI code
was compiled, but the summary screen has not been manually inspected in an emulator.
Rebreeding of the remaining non-lineage override traits also remains to be audited. The entire Berserk Gene
feature is not complete.

### Same-target evolution attribution checkpoint (2026-11-10; milestone 1 complete)

- Added an explicit transient choice ordinal returned alongside the target species by
  `GetEvolutionTargetSpeciesWithChoice`. It identifies the exact condition-matched entry in
  `GetMonPotentialEvolutions`, not a target-species lookup or a shared pending-choice cache.
  Selection order and condition side effects remain unchanged. The ordinal is valid until the
  profile changes; ordinary/unsuccessful checks return `FUSION_EVOLUTION_CHOICE_NONE`.
- The existing active-choice projector optionally maps ordinals back to saved node indices.
  Preparation and read-only previews use that mapping, validating target/active membership
  before mutation. This preserves same-side duplicate roots with different descendants,
  opposite-side choices with the same target, and exact Phase 2 descendant selection.
  Legacy retained but inactive Phase 2 groups cannot intercept an earlier target match.
- All scene-launch paths now pass the selected ordinal from their `DO_EVO` check: post-battle,
  Rare Candy/party evolution, evolution stones, script/special entry points, in-game party/PC
  trades, and link-trade evolution. The fade task retains it until preparation; normal/trade
  scenes prepare once and use the same choice for evolution text. No condition recheck is
  needed after the trigger consumes a held item. Existing prepared-profile commit/discard
  semantics, appearance roll count, growth, EXP, and save layout remain unchanged.
- The owned-Pokedex evolution list previews each actual row's ordinal, so different rows with
  identical target species can show different fused names. Previews remain RNG-free and do not
  overwrite live profiles, mons, cached projections, or another caller's explicit choice.
- Existing target-only helpers remain deterministic compatibility wrappers selecting the first
  active matching target; they cannot identify a condition-matched duplicate by themselves.
  Production trigger/scene paths and row-specific Pokedex previews use the explicit APIs.
- Paired-root matching also requires equal condition-set IDs, in addition to the existing
  matching method/parameter/source-side checks.

Validation:
- `make -k check BUILD=hns -j24 TESTS="test/fusion_evolution_choice.c"`: six passed /
  30 parameter cases. Covers both phases and source sides; normal/battle-only/item/trade/
  battle-special triggers; matching additional conditions and consumed held items; differing
  primary item parameters; retained descendants, selected flags, parent/name priority and
  preview/commit parity; unchanged preview RNG/cache/profile/mon; invalid/inactive choices;
  ordinary/no-match results; and legacy inactive duplicate-target descendants.
  Real normal evolution scenes run both immediate and fade-delayed entry points through success
  and B-button cancellation, asserting exact prepared-profile commit or unchanged Pokemon/profile.
  The fixture drives the usual per-frame sound update as well as scene callbacks.
- With the same file selector patched into the non-headless ELF:
  `tools/mgba-rom-test-hydra/mgba-rom-test-hydra tools/mgba/mgba-rom-test arm-none-eabi-objcopy pokehns-test.elf`:
  six passed / the same 30 parameter cases, including real front animations.
- `make -k check BUILD=hns -j24 TESTS="Fusion"`: 43 passed; three unrelated Fusion Flare/Bolt TODOs.
- `make -k check BUILD=hns -j24 TESTS="(Daycare) Rebreeding"`: one passed / six parameter cases.
- `make -k hns -j24`: passed; EWRAM 260,116/262,144 bytes, IWRAM 27,644/32,768 bytes,
  ROM 31,698,516/33,554,432 bytes.
- `git diff --check`: passed.
- Trigger-level trade attribution is tested and trade entry points compile; live trade/link UI
  and profile-transfer verification, manual visual inspection, and full lifecycle validation
  remain milestone 5 work. No new support for previously unsupported fusion evolution methods
  is introduced by this attribution change.

Next: milestone 2, remaining display/cry consumers. Four current-implementation milestones
remain; the external-storage migration remains deferred and separate.

## Milestone 2 checkpoint: naming, trade presentation, and cry playback groundwork

Implemented and automatically verified:
- Added a boxed-mon display-name accessor and a mon-aware naming entry. Hatch (including
  mandatory Nuzlocke naming), caught-mon naming, and party/PC nickname changes now copy the
  fused title and use the saved sprite donor for the icon. Species-only/player/box/rival
  naming remains on its original entry; nominal species, gender, personality, shininess,
  and the caller's nickname buffer retain their existing roles.
- Trade menu icons and both trade-animation loaders use the saved visual donor, including
  hidden siblings. Graphics, palettes, templates, offsets, flipping, and arrival release
  follow that identity without writing donor species into the Pokemon.
- The real trade scene regression exposed shared palette tags when the two mons have the
  same donor but different shininess. Each trade portrait now has its own palette tag,
  preserving both palettes simultaneously.
- Outgoing trade cries and incoming ball-release cries use the saved independent cry.
  Added cry-ID mode playback through the existing sound engine, preserving normal/weak,
  reverse/echo, pitch, length, priority, panning, and BGM-ducking behavior. Summary playback
  now uses the saved cry for valid fusion profiles; ordinary mons retain species-mode
  handling, including the configured Mega cry modifier.
- The front-animation helper now supports a cry ID independently of visual species while
  retaining second-frame playback. Existing species-based entry points remain available.
- Foreign link profile IDs must not resolve against the local profile table. The current
  trade-party packets copy Pokemon records without their associated profile entries; remote
  portraits/icons/cries therefore retain species-only presentation until milestone 5
  supplies safe profile transfer/import. This guard does NOT make cross-save fusion trades
  safe or establish link compatibility.

Verification:
- `make -k check BUILD=hns -j24 TESTS="test/fusion_presentation.c"`:
  **3 passed / 28 parameter cases** (six real naming-screen cases, fourteen sound modes,
  eight complete in-game trade scenes).
- `tools/mgba-rom-test-hydra/mgba-rom-test-hydra tools/mgba/mgba-rom-test arm-none-eabi-objcopy pokehns-test.elf`
  with that selector retained: **3 passed / the same 28 cases**, without headless
  suppression. Checks actual cry tones (not merely the selected profile ID), second-frame
  animation, loaded portrait pixels and both palettes, rendered naming-title pixels,
  donor icons, ordinary/legacy/hidden donors, eggs, and normal/shiny collisions.
- The foreign-ID case simulates remote presentation during the real loader, deliberately
  using a foreign ID that aliases a valid local profile; it is not a two-instance link test.
- `make -k check BUILD=hns -j24 TESTS="Fusion"`: **46 passed / 3 existing move TODOs**.
- `make -k hns -j24`: passed; EWRAM **260,116 / 262,144**, IWRAM **27,644 / 32,768**,
  ROM **31,699,860 / 33,554,432**. `git diff --check`: passed. No save-layout growth.

Milestone 2 is **in progress, not complete**. Next: finish independent cry routing for
prepared evolution appearances, hatch reveal, battle/ball release and remaining per-mon
consumers; classify species-reference/world/intro consumers rather than changing them
blindly. Summary playback is wired but has no new full-summary scene regression in this
checkpoint. Trade menu return, PC-origin trades, and real cable/wireless transfer still need
their lifecycle/link verification. No manual visual/listening or live link validation was
performed. Four current-implementation milestones still remain (2-5); their order and count
have not changed, and external-storage migration remains deferred.

## Milestone 2 checkpoint: hatch/evolution and battle entrance/release cries

The hatch/evolution checkpoint was committed as `312e40b3d8`; this entry restores the
bookkeeping blocked by the earlier workspace-access outage.
- Hatch reveal uses the saved independent cry, including silence, without suppressing the
  donor's second-frame animation. Ordinary hatch cries retain species-based playback.
- Normal and trade evolution snapshot the original and prepared future cries. Intro and
  cancellation use the original; successful reveal uses the prepared cry before commit.
  Tests verify unchanged live data at first reveal, exact committed profiles, silence,
  panning, and cancellation replay.
- The prior targeted hatch/evolution suites passed headless and non-headless. The full
  Fusion rerun blocked at that checkpoint has now completed.

Battle entrance/release checkpoint implemented:
- Wild entrance, player slide-in, single/double ball release, fainting, and KO/recovery
  animations use saved cries independently of visual donors, including hidden donors.
- Presentation follows the Illusion model where the original consumer did; fainting uses
  the actual party mon. KO/recovery playback preserves explicit temporary battle-species
  overrides rather than substituting a donor cry.
- Ball-release tasks snapshot the resolved cry before delayed playback. Weak modes still
  depend on the actual battler's condition. Ordinary species/Mega behavior, doubles timing,
  multi-battle music handling, and BGM restoration ownership remain unchanged.
- Added mon-aware mode playback and a cry-ID release-double entry preserving existing
  no-restore semantics. No profile-layout or runtime-memory allocation growth.
- Real release fixtures cover both sides, silence, Illusion, weak single/double modes,
  first/second doubles, multi battles, and profile changes after the cry is snapshotted.
  Wild entrance tests cover portrait animation and skipped animations. Faint, slide-in,
  and KO/recovery fixtures check exact tones and panning.
- Full non-headless runs exposed clock-zero-only observers in existing evolution/trade
  fixtures. They now detect tone changes/clock resets, retaining exact cry/pan/count and
  pre-commit assertions without depending on sampling the first audio tick.

Validation:
- `make -k check BUILD=hns -j24 TESTS=test/battle/fusion_visuals.c`: **12 passed**;
  the same selector through the unpatched/non-headless runner also passed.
- `make -k check BUILD=hns -j24 TESTS=Fusion`: **51 passed / 3 existing move TODOs**.
- `tools/mgba-rom-test-hydra/mgba-rom-test-hydra tools/mgba/mgba-rom-test arm-none-eabi-objcopy pokehns-test.elf`
  with the full Fusion selector: **51 passed / 3 existing move TODOs**, including actual
  audio-tone and second-frame assertions.
- `make -k hns -j24`: passed; EWRAM **260,116 / 262,144**, IWRAM **27,644 / 32,768**,
  ROM **31,700,628 / 33,554,432**.
- `git diff --check`: passed. A second full non-headless Fusion run also passed.

Milestone 2 remains **in progress**. Next: move-animation cries (Growl/Roar, high pitch,
normal, echo, Dynamax), retaining contest/species fallbacks and delayed playback identity.
Then finish the owned-mon cry/UI audit and summary-scene regression. No manual listening,
live link validation, or external-storage migration has been performed. Milestone order
and count are unchanged: four current-implementation milestones remain (2-5).

## Milestone 2 checkpoint: move-animation cries

Implemented:
- High-pitch, normal, Dynamax, two-part Growl/Roar, and echo animation tasks use the saved
  independent cry, including silence, without substituting the visual donor's cry.
- Illusion follows the model's profile until reveal. Transform retains the original
  animation cry source rather than copying the target's or donor's cry.
- Explicit animation-species snapshots that differ from the actual mon remain species
  references. Ordinary/Mega playback retains its configured species-mode behavior.
- Delayed tasks capture the cry identity once. Profile changes or animation-species changes
  after launch do not change the second Growl/Roar segment or either echo segment.
- Added cry-ID duck-without-restore playback for echoes. Intermediate echoes do not acquire
  a BGM restoration task; final echoes restore normally. Existing sound modes, panning,
  target visibility guards, and species-based contest playback remain intact.
- No save-layout, profile-layout, EWRAM, or IWRAM growth.

Verification:
- Three new fixtures in `test/battle/fusion_cries.c` cover 29 parameter cases and invoke
  multiple real animation-task paths per case. Coverage includes both sides, hidden donors,
  independent/silent cries, Illusion/reveal, actual Transform, ordinary/Mega fallbacks,
  species snapshots, all four battler selectors, invisible targets, contests, delayed cry
  snapshots, exact audio song parameters/tones, and BGM restoration task ownership.
- Extended the existing 14-mode sound regression to compare species and cry-ID
  duck-without-restore playback, including the doubles exception.
- Focused new fixtures passed headless and through the unpatched/non-headless runner.
- Full Fusion suite passed headless and non-headless: **54 passed / 3 existing move TODOs**.
- Production build passed: EWRAM **260,116 / 262,144**, IWRAM **27,644 / 32,768**,
  ROM **31,700,996 / 33,554,432**.

Milestone 2 remains **in progress**. Next: finish the remaining owned-mon cry/UI consumers
(including party/field and applicable contest/Pokeblock/Hall of Fame paths) and full-summary
scene regression, distinguishing owned-mon presentation from species-only references.
Manual audio/visual and live-link validation remain outstanding. Four implementation
milestones remain (2-5); order/count are unchanged and external-storage migration is deferred.

## Milestone 2 checkpoint: field-move banner portraits and cries

Implemented and verified:
- The party-mon field-move entry resolves its saved visual donor before loading the
  indoor/outdoor banner portrait. Actual personality and shininess are retained.
- Cry playback is independent of that portrait: valid profiles snapshot their saved cry,
  including silence, when the banner is created. Delayed playback does not read a changed
  profile. Ordinary mons retain their nominal species cry and configured species modes.
- The species-only banner entry remains species-based and cannot inherit an owned profile
  accidentally. Ambient overworld cries remain wild-species references.
- Fixed the tightly coupled no-ducking flag extraction: the bit-31 flag was shifted into
  bit 15 and then truncated when assigned to sprite data. It is now normalized to boolean,
  restoring the intended no-ducking behavior for both ordinary and fusion banners.
- A real field-effect regression runs ten cases through indoor/outdoor banner creation,
  delayed cry, slide-off, and cleanup. It checks ordinary, direct/hidden donors, shiny
  palettes, silent profiles, species-only entry, profile mutation after snapshot, exact
  loaded portrait pixels, audio tone/pan, BGM-restoration ownership, and unchanged mon data.
  The fixture supplies the overworld windows and chained VBlank callback the effect needs.
- No save-layout, profile-layout, EWRAM, or IWRAM growth.

Validation:
- `make -k check BUILD=hns -j24 TESTS=test/fusion_portraits.c`: **4 passed / 31 parameter
  cases**, also passed through the unpatched/non-headless runner.
- `make -k check BUILD=hns -j24 TESTS=Fusion`: **55 passed / 3 existing move TODOs**.
- `tools/mgba-rom-test-hydra/mgba-rom-test-hydra tools/mgba/mgba-rom-test arm-none-eabi-objcopy pokehns-test.elf`
  with the full Fusion selector: **55 passed / 3 existing move TODOs**.
- `make -k hns -j24`: passed; EWRAM **260,116 / 262,144**, IWRAM **27,644 / 32,768**,
  ROM **31,701,140 / 33,554,432**. `git diff --check`: passed.

Milestone 2 remains **in progress**. Next: party form-change cries/icons, applicable
Pokeblock/contest/Hall of Fame owned-mon consumers, and the full-summary scene regression.
The field banner checkpoint does not complete that broader audit. Four implementation
milestones remain (2-5), in the same order; external profile-storage migration stays
separate and deferred. No manual visual/listening or live link validation was performed.

## Milestone 2 checkpoint: party form-change cries and icons

Implemented and verified:
- Item-use and built-in fusion-form changes play the selected mon's saved independent cry,
  including silence. Ordinary mons retain species-based playback and configured modes.
- Held-item form changes use the saved cry without ducking BGM, preserving existing volume
  and priority arguments. Ordinary held-item changes retain their original species entry.
- Item-use and held-item icon replacements reuse the existing mon-aware party icon helper,
  retaining saved donors (including hidden siblings), personality, egg handling and HP-bar
  setup instead of replacing the icon with the nominal target species.
- The mosaic callback is attached to the newly created icon rather than assuming the
  replacement always reuses the destroyed icon's sprite slot.
- Built-in fusion-form refresh already used mon-aware icons. Its cry now resolves the mon
  after party compaction, including when the primary occupied a later slot than the partner.
- No gameplay form targets, saved fusion traits, moves, profile layouts, or save layouts
  were changed by this presentation checkpoint.

Verification:
- Real party-menu fixture exercises 14 cases through item-use Tornadus, held-item Giratina,
  and built-in Kyurem/Reshiram fusion changes. It covers ordinary mons, direct/hidden donors,
  independent cries, silence, both fusion party orders, icon tile identity, exact cry song
  parameters/tones, no-ducking behavior, unchanged profiles, and normal menu cleanup.
- `make -k check BUILD=hns -j24 TESTS="Fusion party item"`: passed; the same selector through
  the unpatched/non-headless runner also passed.
- `make -k check BUILD=hns -j24 TESTS=Fusion`: **56 passed / 3 existing move TODOs**.
- `tools/mgba-rom-test-hydra/mgba-rom-test-hydra tools/mgba/mgba-rom-test arm-none-eabi-objcopy pokehns-test.elf`
  with the full Fusion selector: **56 passed / 3 existing move TODOs**.
- `make -k hns -j24`: passed; EWRAM **260,116 / 262,144**, IWRAM **27,644 / 32,768**,
  ROM **31,700,980 / 33,554,432**. `git diff --check`: passed.

Milestone 2 remains **in progress**. Next: applicable Pokeblock/contest/Hall of Fame
owned-mon consumers and the full-summary scene regression. Four implementation milestones
remain (2-5), in the same order. External profile-storage migration remains separate and
deferred. No manual visual/listening or live link validation was performed.

## Milestone 2 checkpoint: Pokeblock feeding and Hall of Fame induction (2026-11-10)

Implemented and verified:
- The Pokeblock feeding scene loads the saved visual donor's portrait and palette (eggs
  retain egg graphics) and snapshots the saved independent cry, including silence, when the
  mon sprite is created. The jump-peak cry uses that snapshot; later profile changes do not
  change it. Ordinary mons retain species-based graphics and cry playback. Nature-based jump
  animation, Pokeblock gain, and the fed mon's data/profile are unchanged.
- Hall of Fame induction (both the save path and the post-save replay entry) resolves each
  displayed party mon's saved visual donor for the pic sprite, palette, and front animation,
  and snapshots the saved independent cry at sprite creation, including silence and hidden
  siblings. The printed info panel uses the fused display species name and the profile-aware
  gender for valid fusion profiles; dex number, nickname, level, and OT ID remain the
  existing nominal/record values.
- The saved Hall of Fame record intentionally keeps the nominal species: `HallofFameMon`
  has no room for donor/cry fields, old saves cannot carry them, and profiles may be freed
  or reused after induction. The Hall of Fame PC viewer therefore remains a species-only
  reference and is unchanged, consistent with the species-reference classification used by
  prior checkpoints.
- No gameplay values, saved layouts, profile layout, EWRAM, or IWRAM grew.

Verification:
- `make -k check BUILD=hns -j24 TESTS="Fusion Pokeblock"`: **1 passed / 6 parameter cases**
  (ordinary, direct/hidden donors, silent cry, shiny palettes, egg), also passed through the
  unpatched/non-headless runner. Cases drive the real feeding scene to completion and check
  loaded portrait pixels, all 16 palette entries, exact cry tones, snapshot isolation from
  later profile changes, Pokeblock gain, and unchanged mon/profile data.
- `make -k check BUILD=hns -j24 TESTS="Fusion Hall of Fame"`: **1 passed / 4 parameter
  cases**, also passed through the unpatched/non-headless runner. Cases drive the real
  DontSaveData induction scene from entry through the credits handoff and check loaded
  portrait pixels, shiny palettes, snapshot-isolated cry tones and silence, and the actual
  rendered info-window pixels (fused name and profile-aware gender versus nominal), plus
  unchanged mon/profile data. The flash-save path and PC viewer rendering are not exercised
  by this fixture; the PC viewer is deliberately unchanged.
- `make -k check BUILD=hns -j24 TESTS=Fusion`: **58 passed / 3 existing move TODOs**,
  headless and through the unpatched/non-headless runner.
- `make -k hns -j24`: passed; EWRAM **260,116 / 262,144**, IWRAM **27,644 / 32,768**,
  ROM **31,701,460 / 33,554,432**. `git diff --check`: passed.

Milestone 2 remains **in progress**. Next: classify and finish applicable contest owned-mon
consumers (player-mon presentation versus species/link references) and the full-summary
scene regression. Four implementation milestones remain (2-5), in the same order. External
profile-storage migration remains separate and deferred. No manual visual/listening or live
link validation was performed.

### Contest classification (user decision, 2026-11-10)

All contest surfaces remain species-only references; no code changes. Rationale and scope:
- `gContestMons` are standalone `ContestPokemon` copies, also transmitted raw over the link,
  so remote entries can never resolve local fusion profiles.
- Museum paintings and saved `ContestWinner` records have no room for donor/cry data, the
  same constraint that kept the Hall of Fame PC viewer species-only.
- The user explicitly chose to keep the player's own contestant species-only as well:
  appeal slide-in/back sprite, winner slide-in portrait and cry, the contest entry mon pic
  special, appeal move-animation cries (already species-based from the move-anim checkpoint),
  paintings, and saved winner records all retain nominal species presentation.
This supersedes any plan to resolve the player's contestant through
`gContestMonPartyIndex`. Contest consumers are complete for milestone 2 by classification.

## Milestone 2 checkpoint: full summary scene (2026-11-12; milestone 2 complete)

The full-summary regression now drives the actual scene through entry and exit for five cases:
an ordinary mon, direct and hidden fusion sprite donors, a silent fusion cry, and a shiny boxed
fusion. It checks portrait pixels, all 16 palette entries, the rendered species name and gender,
independent cry playback, unchanged mon/profile data, and the boxed source where applicable.
The fixture initializes the player name before creating the mon: a cleared save has no
string terminator there, so the summary's trainer memo would otherwise corrupt test state.
Fusion fixtures also initialize EXP on their stored growth curve before checking party/box
parity. Temporary summary/window debug instrumentation was removed.

Verification:
- `make -k check BUILD=hns -j8 TESTS='Fusion summary screens'`: **1 passed / 5 cases**,
  headless.
- With the same selector patched into the unpatched test ELF,
  `tools/mgba-rom-test-hydra/mgba-rom-test-hydra tools/mgba/mgba-rom-test arm-none-eabi-objcopy pokehns-test.elf`:
  **1 passed / 5 cases**, including actual cry tone and silence assertions.
- No manual visual or listening validation was performed. Full Fusion regression, production
  build and broader non-headless regression remain for the agreed commit-time validation.

## MVP profile capacity (2026-11-12)

The user set the internal MVP limit to **8 active fusion profiles**, plus the reserved slot.
External storage remains deferred until the user can experiment with the MVP, and is planned
before release. No compatibility with previous fusion saves is required. Older capacity and
save-size figures below are historical and do not describe the current limit.

The user approved a bounded rebreeding move-source design: up to four species sources per parent
side, with deterministic pruning and equal weighting within each side. That design remains
unimplemented. The reduction from 15 active profiles creates additional internal save space;
the current `PokemonStorage` size is **35,008 / 35,712 bytes**, leaving **704 bytes**. Remeasure
after the move-source fields are implemented.

Verification:
- `make -k check BUILD=hns -j8 TESTS='Berserk Gene profile'`: **7 passed**, including the exact
  eight-slot limit, full-table detection, freeing a slot, and reuse.
- `make -k check BUILD=hns -j8 TESTS='PokemonStorage is backwards compatible'`: **1 passed**,
  verifying the updated storage size pin. This size test does not establish old-save compatibility.

### Milestone 3 checkpoint: bounded rebreeding inheritance (2026-11-12)

Gene breeding reads each fusion parent's effective scalar traits and snapshots up to four species
move sources per parent. Sources are deduplicated and sampled deterministically, with equal
weight within each side; this remains a bounded, lossy approximation, not a reconstruction of
the parent's exact move pool or internal weights. An evolving side uses its resolved lineage;
an unchanged side retains its birth snapshot, or its previous resolved phase when applicable.
Egg moves, initial moves, TM/tutor eligibility, daycare egg-move transfer, hatching friendship,
and inherited growth/EXP use the profile-aware data.

User decisions: with no gene and no Ditto, an offspring of a female fusion parent receives an
independent copy of her fusion profile, used for standard breeding mechanics; with no gene and
Ditto, it receives an independent copy of the fusion parent's profile. If either parent holds a
gene in a fusion-parent/Ditto pairing, generate a new fusion profile incorporating Ditto's traits.
For copied profiles, determine the offspring's gender from its nominal species and personality,
and choose the active ability using standard inheritance against the copied profile's ability
slots. For a new fusion with Ditto, exclude Ditto's special egg group and use the other parent's
effective groups. Full-table gating covers both generated and copied profiles. Non-fusion/Ditto
breeding retains its earlier behavior.

Verification:
- `make -k check BUILD=hns -j8 TESTS='Fusion rebreeding'`: **11 passed**. Coverage includes
  copy eligibility and parent position, Ditto and gene combinations, full storage, two distinct
  fusion parents, zero-valued traits, weighted numeric blends, move-source deduplication and
  reuse, both evolution phases, teachable union, initial and inherited egg moves, and daycare
  move transfer.
- The headless mGBA runner was invoked on the same test ELF with patched selectors: `Fusion
  growth` **4 passed**; `Fusion lineage rebreeding` **1 passed**; `(Daycare) Rebreeding
  preserves only` **1 passed**; `Fusion hatching preserves` **1 passed**; `PokemonStorage is
  backwards compatible` **1 passed**; `(Daycare) Berserk Gene bypasses` **4 passed**.
  The storage-size pin is **35,152 bytes**; the nine storage sectors allow 35,712 bytes.
  This size check does not establish old-save compatibility.
- Two existing test fixtures were corrected: the scripted compatibility tests disable the
  one-type challenge, and the parameterized lineage test releases its profiles after each case.
  Temporary diagnostics were removed. Random trait outcomes were sampled, not exhaustively
  proven at every probability threshold.

Milestone 3 is complete. Full Fusion suite, non-headless regression, production build, and manual
testing remain for the agreed final/commit-time checkpoint. In milestone 5, investigate battle
EV awards: `MonGainEVs` currently receives only defeated species and therefore does not consume a
defeated fusion's stored EV yields. Resolve ownership/link safety before changing that path.

## Remaining milestone tracker (agreed 2026-11-10)

The current goal is a working, manually testable implementation using the internal capped
profile store. Keep external storage migration separate; do not begin it before that version
is available for manual testing. Automated builds/tests alone do not establish that manual
validation has been completed.

| Milestone | Status | Completion criteria |
| --- | --- | --- |
| 1. Same-target evolution attribution | Complete (2026-11-10) | Exact choice passed from trigger through preview/scene preparation/commit; same-target condition/descendant regressions and real normal-scene success/cancellation tests pass. See checkpoint above. |
| 2. Remaining display/cry consumers | Complete (2026-11-12); manual visual/listening validation remains outstanding | Full-summary scene regression passed headless and non-headless; contests classified species-only by user decision. |
| 3. Rebreeding trait audit | Complete (2026-11-12); focused automated regressions passed | Verify remaining non-lineage inherited overrides, including fusion parents; add targeted regression coverage and fix gaps. |
| 4. Old-save compatibility | Closed: not required (user decision, 2026-11-12) | No prior fusion profiles need support; do not add historical-save compatibility or migration code unless the user explicitly identifies a need. Normal save/reload validation remains in milestone 5. |
| 5. Final lifecycle validation and manual-testable checkpoint | In progress | Verify profile cleanup/reuse, normal save/reload, the temporary fusion-recording safeguard, and the offline feature lifecycle; provide a working ROM and manual test checklist, record actual emulator/manual results and remaining limitations, and update documentation. Link compatibility is deferred as noted below. |
| Link compatibility | Deferred until after initial MVP | Complete and verify link restrictions or profile transfer, including coordinated cancellation and multiplayer testing. Until then, do not use fusion link actions. |
| External profile storage | Deferred until after initial MVP, planned before release | Replace the temporary eight-profile internal capacity with external storage after the user can experiment with the MVP. |
| Fusion battle recording support | Deferred until after initial MVP | Enable reliable recording and playback with the fusion profiles captured at battle time. Until then, disable recording fusion battles while retaining ordinary battle recordings. |
| Pyramid fusion rollback | In progress for MVP (decision revised 2026-11-12) | Restore fusion entry state, including profile, species and moves, on win, loss and retirement, including after pause/reload. Preserve ordinary Pyramid healing and held-item behavior. |

### Post-MVP decisions and MVP limitations (2026-11-12)

- **Link compatibility:** defer safe fusion trades and battles, profile transfer or complete
  restrictions, and multiplayer validation until after the initial MVP. Existing guards are
  incomplete; do not use fusion link actions.
- **External profile storage:** defer implementation until after the user can experiment with
  the initial MVP, but plan it before release. The MVP uses eight active internal profiles.
- **Fusion battle recordings:** temporarily disable recording battles involving fusion Pokémon
  for the MVP; ordinary battle recordings should remain available. This restriction is
  implemented and has focused automated coverage; full regression and manual validation remain.
  Defer full fusion recording and playback support, including safe profile snapshots, until
  after the initial MVP.
- **Battle Pyramid rollback (revised 2026-11-12):** fusion Pokémon should behave like ordinary
  Pokémon on exit and return to their entry state, including their profiles, evolutions, learned
  moves and Sketch moves. The earlier decision to retain those changes is superseded. The
  existing implementation still retains them and must be changed before this criterion is met.
- **Mirror Thief in other Frontier facilities:** protect selected fusion Pokémon from permanent
  replacement; restore them after the challenge. This is not yet implemented.
- **Previous-build/save compatibility:** not required unless the user explicitly requests it;
  it is not an automatic post-MVP migration requirement. Normal save/reload for the current
  build remains part of MVP validation.

The dated implementation checkpoints below are historical; their old "still deferred" notes
may describe work completed or decisions changed in later checkpoints. The tracker and decisions
above reflect current status.

### Milestone 5 investigation: link profile ownership blocker (2026-11-12)

The current handshake advertises Berserk Gene support to all peers, and `CanTradeMon` permits
fusion trades to a peer advertising that bit. However, the link trade party exchange sends
`Pokemon` records, not their profiles; the trade animation explicitly avoids resolving remote
profiles for this reason. `TradeMons` swaps the records without transferring, allocating, or
freeing profiles. A received profile ID can therefore resolve to an unrelated local profile
or to no profile, while the departing profile remains allocated. The remote summary also uses
the ordinary profile-aware summary path, so remote IDs can be interpreted as local IDs before
the trade completes. Link battle has not yet been validated.

The user chose temporary restrictions on fusion link trades and battles for the MVP; profile
transfer remains deferred. Work in progress rejects locally selected fusions in the ordinary
trade menu, strips local profile IDs from exchanged preview copies, and remembers and rejects
fusion IDs received from an older profile-aware partner. Trading Board registration and offers
also reject locally selected fusions. An in-game trade now frees the outgoing fusion's profile.
Focused helper and trade-scene regressions cover the current changes; they do not simulate two
linked systems. The Trading Board's incoming/stale-registration paths and link battles are
not yet protected or validated. Do not use fusion link actions or claim link compatibility.

The user deferred link compatibility until after the initial MVP and clarified that support
for previous builds is not needed unless explicitly requested.
Fusion selection is now rejected for linked choose-half menus and Battle Tower link multis, with
a focused context regression. Two-player Colosseum and remaining wireless paths still need
coordinated guards; do not assume all link battles are restricted. Multiplayer validation is
also deferred. This changes the remaining milestone scope: milestone 5 is the offline MVP
checkpoint, and link compatibility is now a separate post-MVP milestone.

New-game initialization now clears the profile table without clearing it when boxes alone are
reset; a focused test verifies all eight slots are reusable. Scripted permanent gifts now release
their outgoing profile; `remove5mons` is intentionally left alone because the Bug-Catching
Contest temporarily saves and restores those party members. Focused scripted-gift tests pass for
named gifts and both generic level branches, including preservation of another mon's profile.
`make -k check BUILD=hns -j8 TESTS='Fusion normal save'` passed a flash-backed normal
save/reload regression for party and boxed fusion records and their profiles. This does not
validate every special partial-save pathway.

An offline recording issue remains: Frontier battle recordings save Pokémon records but not
their fusion profiles. Later playback resolves those saved IDs against the current profile
table, which may have changed, evolved, or reused those slots. Recording full profile snapshots
requires addressing the single-sector recorded-battle size limit. The user chose to temporarily
disable recording fusion battles for the MVP. The save path now rejects snapshots with a
nonzero profile ID in either recorded party, the Frontier prompt respects that restriction,
and existing recordings with such IDs cannot be replayed. Ordinary recordings remain available
and a failed fusion recording does not overwrite one. The restriction checks all saved party
slots, even if a Pokémon did not participate, because all slots are in the recording.
Full fusion recording support is deferred until after the initial MVP.
`make -k check BUILD=hns -j8 TESTS='Fusion battle recordings'` passed: the focused
flash-backed test covers both recorded parties, a freed profile ID, the Frontier restriction,
preservation of an existing ordinary recording, and rejection of unsafe existing recordings.

Offline lifecycle audit finding: Nuzlocke's box-instead-of-delete paths call `CopyMonToPC`
without checking for a full PC, then purge the party Pokémon anyway. A failed transfer loses
the Pokémon and leaves its fusion profile allocated. This affects both normal and easy
Nuzlocke. The user chose to preserve the fainted Pokémon in the party and notify the player
when the PC is full. If a whiteout occurs, the user approved normal party healing as a
temporary full-PC exception. Both transfer paths now leave the Pokémon, its profile and held
item intact on failure; a pending field message explains the full-PC condition. During that
whiteout, the normal replacement-party selection is bypassed so it cannot overwrite the
preserved Pokémon before healing. Hardcore Nuzlocke's existing whiteout reset remains unchanged.
`make -k check BUILD=hns -j8 TESTS='Fusion Nuzlocke'` passed two tests, including both Nuzlocke
modes, ordinary and fusion Pokémon, full and available boxes, notice state, whiteout healing,
and the existing replacement-party choices. The on-screen message and complete whiteout
sequence still need manual validation.
Pike and Pyramid challenge entry scripts perform a full save before their partial challenge
saves, and `BATTLE_TYPE_FRONTIER` includes both facilities, preventing their battles from
awarding EXP or triggering battle evolutions. This observation does not yet complete the
audit of other ways profile data could change during a challenge. In particular, Pyramid
held items can be moved into the persistent Pyramid bag after a completed challenge; a later
challenge can use that bag's field items, including an evolution item. An item-triggered
fusion evolution mutates its profile, while a Pyramid partial save writes the party but not
profile storage, and the original party can also be restored after the challenge. The user
chose to support fusion evolution in Pyramid challenges rather than block it.

A temporary flash-backed test confirmed the underlying partial-save mismatch: after a full
save, changing a fusion's species and profile, then writing `SAVE_LINK` and reloading, the
new species was paired with the old profile. The test exercised the actual save/load path
but did not simulate the Pyramid scene; it was removed after characterization. A separate
issue is that `RestorePyramidPlayerParty` matches by species, so it cannot match an evolved
Pokémon to the original species. Pyramid can also restore the original party on exit.
The earlier decision to keep fusion evolutions after the challenge was subsequently reversed.

Pyramid challenge saves use the full-save path, and in-challenge party restoration matches valid
fusion profiles by ID. Entry snapshots now store selected fusion profiles and box records in
checksummed PokemonStorage and SaveBlock1 space; win/loss/retire restore those snapshots before
loading the full party. Existing healing and held-item clearing still apply. The snapshot is
necessary because pause/resume saves update the working party. On ordinary floor transitions
`frontier_getstatus` converts challenge status
zero to 0xFF, so the floor's `ReadyChallenge` save is not invoked; when resuming a paused
challenge, `PausePyramidChallenge` previously restored the full party before the pause save.
Focused command `make -k check BUILD=hns -j8 TESTS='Fusion Pyramid'` passed ten tests:
profile-ID restoration, ordinary same-species party ordering, entry-state rollback, held
item transfer and clearing, a flash-backed challenge save, a flash-backed pause/resume sequence,
three fusion snapshots persisted across reload and an additional post-rollback full-save reload,
an actual evolution scene followed by rollback,
selected exit-script segments, and interrupted full-save recovery. The script test runs the real win, loss and
retirement bytecode in both map variants until the warp, then the lobby bytecode through party
restoration; HNS win/loss cases also cover a replacement active Pokémon. It does not simulate
the warp, dialogue, item-selection UI, or final save through the scripts.
The interruption test writes 0–13 complete sectors using the incremental writer shared with
normal saves, plus a final sector without its signature, and checks that loading recovers the
previous matching Pokémon and profile. It does not simulate interruption within each sector.
The item-use scene and complete manual behavior remain unverified. Do not treat Pyramid fusion
evolution as safe for manual testing yet.
The updated SaveBlock1 and PokemonStorage size fixtures pass when run separately. Running
`make -k check BUILD=hns -j8 TESTS='SaveBlock'` also exposed pre-existing mismatches in the
unmodified SaveBlock2 and SaveBlock3 size expectations (actual 3892 vs 3884, and 52 vs 84);
these are not evidence of compatibility with older saves. In the current HNS build the new
sizes are 15840/15872 bytes for SaveBlock1 and 35660/35712 bytes for PokemonStorage; further
save-data growth must respect the remaining space.

The EV-yield audit is not complete. Normal wild and scripted enemies are created from species,
secret-base opponents are reconstructed from species-only records, and EV/EXP awards are blocked
in link and Frontier battles. Recorded wild battles are an explicit EXP exception, but fusion
recordings are currently blocked. Do not resolve an opponent's local profile ID in this path
without first establishing its ownership and reachability. Enemy-party copy sites inspected so
far include double wild encounters (copies of generated wild Pokémon), link reception, and
recorded-battle playback; none establishes a locally owned fusion as an EXP-awarding opponent.
The Union Room temporarily copies the player's Pokémon to the enemy array during setup, but
starts a link battle. Further nonstandard and scripted paths still need review.
The partial-save audit remains open. Pike's start menu has no bag, its party menu prevents held
item management, and fusion potential evolutions currently reject overworld-special and
script-trigger modes; this limits the profile-mutation routes considered for its partial saves.
Its pause/resume scripts restore the full saved party before saving. Link-contest partial saves
follow an entry full save but remain part of the deferred multiplayer investigation. Other
`SaveGameFrontier` callers include Tower, Arena, Dome, Palace, Factory and all three Battle
Tents; their entry scripts were inspected for a full save. Factory and Slateport Tent replace the
active party with rental Pokémon after preserving the original party. A flash-backed regression,
`make -k check BUILD=hns -j8 TESTS='Fusion facility partial saves'`, passed for the shared
Frontier save and Pike save: it verifies reload of the full party and an unchanged fusion
profile after an entry full save. It does not simulate facility scripts or prove that no
in-challenge profile mutation is possible. Other special-save paths still need review.

Mirror Thief replaces the active player party with the opponent's party. In ordinary battles,
profiles belonging to the discarded party are now released before replacement, so those slots
can be reused; ordinary Mirror mode still retains the original party and profiles. Focused
helper-level coverage checks two party fusions, an unrelated boxed fusion, slot reuse, ordinary
Mirror, trainer/double/Trainer Hill battles, and preservation of profiles for special cases. It
does not execute a complete battle scene. In follower-NPC partner battles, cleanup happens
instead immediately before selected party slots are written back: selected profiles missing
from the active party are released, while unselected saved party members remain intact.
`make -k check BUILD=hns -j8 TESTS='Fusion Mirror'` passed two tests, including a direct
follower-party restore and ordinary Mirror control; it does not run the full partner scene.

For non-Pyramid Frontier battles, `frontier_resetsketch` now leaves a saved fusion unchanged
when Mirror Thief has replaced its active slot; Pike's pause-party save applies the same
protection. Other selected Pokémon retain existing behavior.
Pyramid entry snapshots restore fusions regardless of their current battle-party replacement.
`make -k check BUILD=hns -j8 TESTS='Fusion Mirror'` passed three focused tests including the
Tower/Pike sketch-reset helper; a fourth test verifies the shared Frontier partial save and
Pike pause save after Mirror Thief replacement, including reload. Full facility scenes and exit scripts remain untested.
The audit remains open for non-follower partner paths and optionally
configured Sky Battles; avoid Mirror Thief with fusions in those paths.
The battle EV-award investigation, remaining lifecycle audit, special saves, full regression,
and production build remain open.

Stop when a milestone completes and report the status of all milestones; resume only when the
user instructs. If investigation changes scope, ordering, or the number of milestones, update this tracker and explicitly notify the
user rather than silently changing the completion estimate. Keep implementation completion,
automated verification, and outstanding manual verification distinct in progress reports.

## Historical resume checkpoint (paused 2026-08-27)

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
- growthRate override (u8) — **new**: binary pick at breeding, same weighting mechanism as
  ability/color/cry. Selected once when the fusion profile is written; never rerolled on evolution.
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

## Same-species Berserk Gene breeding (new, implemented 2026-09-04)
If two daycare parents are the **same species**, hold Berserk Gene (one or both), and **neither
already has an existing `BerserkGeneProfile`** (i.e. neither is itself already a fusion), no
fusion profile is created at all — there's nothing meaningful to fuse a species with itself.
Instead: all the breeding-eligibility bypass rules above still apply (so e.g. two same-species
male gene-holders can still breed at all), but the child otherwise uses **standard breeding
rules** for everything, with two exceptions: IVs still use `InheritIVsBerserkGene` (not vanilla
`InheritIVs`), and ability inheritance uses equal weighting across all valid ability slots (see
below) instead of vanilla `InheritAbility`. Implemented as an early return in
`BuildBerserkGeneProfile` (checked right after the Ditto guard, before allocating a profile) so
the egg simply ends up with `MON_DATA_BERSERK_GENE_PROFILE_ID == 0`, same as an ordinary hatch.

## Hidden ability gets equal weight in all Berserk Gene breeding (new, resolved + implemented
2026-09-04)
Clarified: vanilla `InheritAbility`'s hidden-ability branch only ever fires when a parent
*already has* the hidden ability as their own active ability — hidden ability is rare to obtain
via standard breeding because you need a hidden-ability parent in the first place, not because
the 60%/80% pass-down roll itself is stingy. For **any** Berserk Gene breeding — whether or not a
fusion profile ends up being generated — the hidden ability is no longer gated behind a parent
already having it: it's just one more equally-weighted candidate.
- **Fusion-profile case** (`BuildBerserkGeneProfile`'s active-ability pick): changed from the
  earlier rarity-weighted 70/20/10 split (a placeholder invented at implementation time, now
  superseded) to an **equal-weight pick** among whichever of `{ability1, ability2, abilityHidden,
  Levitate-if-eligible}` are actually valid (non-`ABILITY_NONE`) candidates.
- **Same-species-skip case** (no profile): new `InheritAbilityBerserkGeneEqualWeight` replaces
  vanilla `InheritAbility` entirely (regardless of `P_ABILITY_INHERITANCE`) — picks equally among
  whichever of the shared species' own ability slots (`ability1`/`ability2`/hidden) are non-empty,
  ignoring which slot either parent currently has active.

## Undiscovered egg group is swapped for Monster in Berserk Gene fusions (new, implemented
2026-09-04)
When building a Berserk Gene profile's egg groups (step 10), `EGG_GROUP_NO_EGGS_DISCOVERED` is no
longer excluded from the pool — it's **swapped for `EGG_GROUP_MONSTER`** before the pool is built,
for either/both parents. This means a fusion involving an Undiscovered-group parent (e.g. a
legendary) now always has real, breedable egg groups rather than defaulting to Undiscovered in
both slots. (The old "both slots become `EGG_GROUP_NO_EGGS_DISCOVERED`" fallback path in the
union algorithm is effectively dead code now, since the swap means the pool is never actually
empty — left in place defensively rather than removed.)

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

**Update (2026-09-03, supersedes the line-selection/storage details below):** the user has
refined the evolution model further after the 14-step trait pipeline was implemented. This
section is the current source of truth; the "Available evolutions = stored immediate next-stage
options only" subsection further down still holds for the *slot-count/weighting/condition-ID/cap*
mechanics, but its "does not guarantee either parent contributes" line and its "single next stage
only" storage scope are both superseded by what follows.

### Line selection at breeding (updated)
- Both parents are now guaranteed at least one selected evolutionary line — not just "the
  potential to contribute" as originally written. Additional line slots (up to the existing
  `slotCount = ceil(lowCount + highCount * r)` cap, capped at `MAX_FUSION_POTENTIAL_EVOLUTIONS`)
  are still filled using the existing 62%/50% gene-holder weighting, but treated as a priority
  ordering over the combined candidate pool rather than a single independent sample per slot,
  so the guaranteed one-per-parent minimum is satisfied before any extra weighted picks are added.
- Selecting a line no longer means storing just that line's single immediate-next-stage target.
  It means the **entire remaining line from the child's current stage** becomes available: both
  the immediate next stage ("Phase 1") and, if that Phase 1 target itself has a further evolution
  ("Phase 2"), that next stage too. In practice this still stays bounded, since almost no family
  has more than two remaining stages past a freshly-bred basic-stage child, and the existing
  rolling-pool recalculation on evolution (see below, unchanged) means a re-bred fusion parent
  only ever contributes forward from its *current* stage, never re-expanding earlier stages.

### Only one Phase 1 and one Phase 2 evolution may ever be taken
A fusion can only ever evolve into one Phase 1 target and, later, one Phase 2 target overall —
never both parents' Phase 1 targets (that's simply "picking a second, different Phase 1 evolution
for the same mon," which is meaningless). Critically, though, the Phase 2 choice is **not**
locked to continuing down whichever side supplied the Phase 1 evolution: after taking one parent's
Phase 1, the *other* parent's Phase 2 options are still legal candidates for the next evolution
(see the pseudo-fusion-profile mechanic below for why that's necessary rather than simply unfair).

### Deterministic full-line model (supersedes rolling re-selection)
All evolution-line randomness happens exactly once: when the egg is produced. The 62/38 or 50/50
weighted selection chooses **whole lines**, not only their immediate target. For every selected
Phase 1 target, the profile must also persist every Phase 2 target reachable from it, including
conditional branches. Evolution never makes a new weighted roll, never re-samples candidates, and
never discovers a previously unselected line.

At Phase 1, the player chooses exactly one selected Phase 1 target. The chosen side becomes that
target. The selected Phase 1 targets on the *opposite* parent side are combined into that side's
transient pseudo-fusion when there is more than one; a single opposite-side target is used directly.
All other selected Phase 1 targets on the chosen side are consumed and can never be chosen later.
The Phase 2 menu is then the deterministic union of:
- all Phase 2 descendants of the selected Phase 1 target, and
- all Phase 2 descendants of every selected opposite-side Phase 1 target represented by the
  pseudo-fusion.

At Phase 2, the player again chooses exactly one active target. The chosen target becomes one
concrete side. Every active Phase 2 target on the opposite side is folded into the transient
pseudo-fusion side, even if there is only one such target. No Phase 1 target can reappear, and no
further potential evolutions remain unless a future design explicitly introduces a Phase 3.

**Slowpoke x Poliwag example:** repository evolution data makes Slowbro and Slowking alternative
Phase 1 targets from Slowpoke; Slowbro does not evolve into Slowking. Poliwag's Phase 1 target is
Poliwhirl, whose Phase 2 entries include Poliwrath and conditional Politoed options. Choosing
Poliwhirl at Phase 1 therefore exposes its stored Phase 2 branches (plus any retained descendants
from selected opposite-side Slowpoke roots). This test must follow the actual species evolution
tables rather than treating Slowbro -> Slowking as a native line.

This model requires a persisted, birth-selected lineage graph (phase, source parent, and parent
Phase 1 relationship for every selected target) plus a persisted phase-selection state. The current
flat `potentialEvolutions[]` array can remain a derived active-menu projection, but it is not by
itself sufficient to reconstruct deterministic Phase 2 choices or pseudo-fusion membership.

### Deterministic lineage storage and capacity (audited 2026-09-06)
The current storage budget is fixed: `FusionPotentialEvolution` is 6 bytes,
`BerserkGeneProfile` is 96 bytes, and 15 profile slots make `PokemonStorage` 35,680 bytes against
a 35,712-byte save capacity. The deterministic model must therefore not grow either structure.

Retain the existing eight-entry `potentialEvolutions[]` allocation, but redefine it as the
canonical birth-selected lineage-node store rather than an immediate active menu:
- bits 0-3 of `methodAndSourceParent` remain the evolution method;
- bit 4 remains source parent;
- bit 5 remains paired-with-sibling;
- bit 6 records a Phase 2 node (`0` means Phase 1);
- bit 7 records the player-selected node for the current resolved phase.
`conditionSetId` remains a full byte; it must not be repacked because the generated registry has
more than 64 possible IDs. `evolutionFlags` bit 0 remains name priority; its spare bits store the
current evolution phase/state. Keep the four-byte `potentialEvolutionAge` field as one 4-bit age
stamp per node (0-15); pruning and re-breeding still need age information, so these nibbles cannot
also hold root indices. Instead, store nodes contiguously by root: each Phase 1 root is followed
immediately by all of its retained Phase 2 descendants, and the next Phase 1 root starts the next
group. A Phase 2 node's owning root is found by scanning backward to the nearest Phase 1 node
(identified by `EVO_POTENTIAL_PHASE_2` being clear). This remains unambiguous when multiple roots
come from the same source parent, requires no additional bytes, and keeps each node's age directly
associated with its array index. Any operation that inserts, removes, or compacts nodes must move
the corresponding age nibbles in lockstep and preserve this root-grouped order.

At birth, select and store Phase 1 roots with their eligible Phase 2 descendants, preserving each
root's group contiguously. Capacity pruning may remove individual Phase 2 nodes under the finalized
continuation-preserving rule below; it must never remove a root while retaining any of that root's
descendants. The selected root and all retained descendants remain deterministic after birth.

### Finalized iterative capacity pruning rule (resolved 2026-09-19)
The eight-node cap is applied to total stored lineage nodes, but pruning is iterative and stops
immediately when the count reaches eight. It must not reduce every branch to a smaller symmetric
shape. The birth-time process is:
1. Preserve at least one Phase 1 root from each parent whenever either parent has a selectable root.
2. While over eight nodes, remove exactly one eligible Phase 2 node at a time, preferring the oldest
  eligible node and randomizing equal-age ties.
3. Do not remove the last Phase 2 descendant of a retained root while any other removable Phase 2
  node exists. A root with descendants therefore retains at least one continuation until that
  root is itself purged.
4. Recount after every removal and stop immediately at eight nodes.
5. Only if no Phase 2 node can be removed without violating the continuation rule, remove one whole
  root and all of its retained descendants at a time, again preferring the oldest root and
  randomizing equal-age ties. Continue until the count is at most eight.

For example, an 11-node graph with roots of sizes 4, 4, and 3 is reduced by exactly three Phase 2
node removals to eight nodes, not flattened to six nodes. The final distribution depends on age and
tie-breaking, but every retained root keeps a Phase 2 continuation whenever possible. This entire
pruning process happens at birth; evolution never re-rolls, re-samples, or reprunes the lineage.

### Resume checkpoint
Implementation progress (2026-10-31): the age-preserving, root-contiguous collector and active
Phase 1/Phase 2 projection have been started in `daycare.c` and `pokemon.c`. It now collects
conditional Phase 2 entries through the stable condition-set registry, carries stored descendants
when a fusion parent is rebred, and prunes the temporary node graph to the eight-node cap. Tests
cover the real Poliwag branches, a simple two-stage line, and the 11-to-8 pruning case.

Implementation progress (2026-11-02): Phase 1 resolution now records the selected root, pairs and
advances the sibling root when the entry is marked as a merged paired evolution, retains the
opposite-side root groups, and removes consumed same-side sibling groups while preserving aligned
age metadata. When the opposite side has exactly one Phase 1 target, that target is applied to the
fusion immediately. Phase 2 similarly applies a unique opposite-side Phase 2 target and then marks
the lineage complete. Regression tests cover same-side sibling filtering, paired Phase 1, and
single-target counterpart resolution. Full multi-root pseudo-fusion aggregation remains
incomplete; only its base-stat averaging/blending slice is now implemented.

Implementation progress (2026-11-02, next slice): persisted the sole gene-holder side in unused
`evolutionFlags` bit 3 (no profile-size growth). Phase 1/Phase 2 evolution now recomputes base stats
using the selected target and the opposite-side candidate set; when that set has multiple targets,
their per-stat values are averaged first, then blended with the selected target using the original
62/38 or 50/50 weighting. The same branch-average-then-blend rule now applies to height, weight,
sprite scale, and sprite offset. Tests cover multi-branch base stats/height and B-side sole-gene
holder persistence. Friendship and per-stat EV yields now use the same branch-average-then-blend
rule, with tests for both. Evolution now also rerolls the three ability slots, active slot, color,
and cry between the evolved target and the currently displayed species for the counterpart side,
using the saved gene-holder weighting. Existing Wonder Guard and Flying/Levitate safeguards remain
in place. Sprite-source parent is also rerolled with the saved weighting, excluding the previous
displayed species when it remains a candidate; a regression covers the forced alternate-donor case.
For a non-paired evolution, the chosen path now leads the fused display-name prefix; paired phases
retain the birth-time weighted-name priority. Growth-rate behavior, learnset merging, and the
remaining name/display aggregation still need to be handled according to the pseudo-fusion rules
below.

Sprite display progress (2026-11-02): added `GetMonDisplaySpriteSpecies()` and wired the chosen
sprite donor into party-menu icons and the summary portrait graphics/palette. The underlying
species ID remains unchanged for species mechanics and Pokédex indexing. Regression coverage checks
donor selection, including the forced alternate donor after evolution. Added the BoxPokemon variant
and wired PC storage-box icons and the selected-mon front-sprite preview to the donor, while keeping
the underlying stored species for sorting/mechanics. Other rendering surfaces still need review.

Before treating this phase as complete, review/fix the new code and extend coverage for ambiguous
same-side roots and re-breeding after each evolution phase. Then complete the full deterministic
evolution resolution in `UpdateBerserkGeneProfileAfterEvolution()`: pseudo-fusion aggregation for
the remaining traits and learnsets, rebreeding across both phases, consumption of sibling roots,
paired Phase 1 behavior across their descendants, and Phase 2 counterpart resolution. Add tests for
those transitions and the 11-node case, then run the test
binary when the HNS test-runner build blocker is resolved. Do not claim full Berserk Gene evolution
completion until those transitions are implemented and validated.

Evolution review follow-up (2026-11-04): fixed a parent-side comparison bug in
`UpdateBerserkGeneProfileAfterEvolution()` by normalizing the source-parent bit to a boolean before
comparing it with other side flags. A focused regression verifies a parent-B evolution keeps parent
A unchanged; the display-name priority test now compares distinct results without assuming a
localized spelling. A script-driven Phase 2 rebreeding test was
attempted but timed out in daycare egg generation before validating lineage behavior; do not count
that integration case as covered, and revisit it with a lower-level test or a corrected daycare
setup.

Learnset progress (2026-11-02): added `GetMonLevelUpLearnset()` as a bounded, profile-aware
projection over the current parent species' level-up tables. It evenly samples each parent's list
at the stored 62/38 or 50/50 contribution, deduplicates, keeps level ordering, and prioritizes
fusion-type moves within the same level. Concrete-mon level-up and evolution move-learning paths
use this projection; species-only helpers and initial BoxPokemon moves remain unchanged. Current
moves are not modified by the evolution hook. Added `GetMonTeachableLearnset()` as a deduplicated
union of both current parent species' teachable lists, sorted with fusion-type moves first, and
`GetMonEggMoveLearnset()` as a weighted, evenly sampled, capped merge with type-relevant moves
prioritized. The Pokédex STATS tab uses these profile-aware projections for owned fusion entries,
and the party menu plus PC move relearner accept the fusion teachable union. The PC move relearner
also uses the fusion level-up and egg-move projections without walking the nominal species'
pre-evolution chain. Regression coverage checks bounded level-up merging, unchanged known moves,
the teachable union, and both-parent egg-move sampling. Species-wide reference views and ordinary
Pokémon remain on their existing species-keyed code paths.

Gameplay follow-up: `MonTryLearningNewMoveAtLevel()` and evolution move teaching now use the
profile-aware level-up projection, while BoxPokemon move-relearner level-up availability builds
the projection once and does not walk a fusion's nominal pre-evolution chain. Party-menu and PC
teachable eligibility use the profile union exclusively for fusion mons (not a union with the
nominal species' teachables), while ordinary mon behavior stays unchanged. A targeted level-up
integration test verifies a fusion learns a move from its projected list and retains its prior
known move.

Move-relearner follow-up (2026-11-02): added BoxPokemon-safe level-up, egg, and teachable
accessors and routed PC move-relearner listing/availability through them. Fusion level-up and egg
lists do not traverse the nominal species' ancestry; TM/tutor eligibility includes the fusion's
teachable union. Species-only routines for ordinary Pokémon are unchanged. Added a profile-backed
relearner predicate regression for all four move categories; execution still depends on resolving
the known HNS test-runner build issue. This was fixed in test infrastructure by including global
save-block definitions before `fake_rtc.h`; targeted HNS tests for relearner predicates, level-up
learning/known-move preservation, and egg/teachable merge now pass.

The selected-node bit plus the profile phase state identifies the Phase 1 choice. A Phase 2 node's
root is identified by its contiguous group, not by a stored root index; retained opposite-side
roots identify pseudo-fusion membership without re-querying or re-sampling native species
evolution data. The existing per-entry age nibble remains available for oldest-first pruning and
age propagation when a fusion is bred again.

### Pre-merging simple, unconditional, same-phase level-up lines
After the initial per-parent line selection (above), lines are checked for merging: if, at a
given phase, **both** parents' lines resolve to a plain level-up evolution with no extra
`CONDITIONS(...)` (no item, no trade, no location, no move, etc. — just a level requirement, or
no further evolution at all on one side), the two are pre-merged into a **single fused line**
rather than kept as two independent lines. This merge happens once, after line selection, before
any player-facing evolution choice exists.
- Per merged phase, the target is a fusion of whichever species each side contributes at that
  phase. If only one side still has a species change at that phase (the other side's line already
  capped out at an earlier stage), the capped-out side simply keeps contributing its last species
  unchanged at that phase — this is not a special case, it's the same "evolving one side of a
  fusion" mechanic the rest of this feature already uses.
- The level requirement for a merged phase, when both sides have one, is `ceil(average(levelA,
  levelB))`. When only one side has a level requirement at that phase (the other side already
  capped out), that level is used directly.
- Worked example: Larvitar (Pupitar @ 30, Tyranitar @ 55) bred with Rattata (Raticate @ 20, no
  further evolution). Both lines are simple unconditional level-ups, so they merge into one fused
  line: Phase 1 = Pupitar×Raticate fusion at `ceil((30+20)/2) = 25`; Phase 2 = Tyranitar×Raticate
  fusion at level 55 (Raticate has nothing further, so it just carries forward unchanged at that
  phase while Larvitar's side continues to Tyranitar).
- Type inheritance follow-through for a merged phase: if neither side's type actually changes at
  that evolution step (e.g. Rattata→Raticate and Larvitar→Pupitar are both type-preserving), the
  profile's `type1`/`type2` are **not** recalculated — they're already correct by construction,
  since this is exactly what the existing "sticky by provenance" type rule already produces (a
  type slot only changes if the species at its stored provenance parent/slot actually changed
  type). This is a clarifying restatement of that existing rule for the merged-line case, not a
  new rule.
- Lines that have any `CONDITIONS(...)` at a phase (item, trade, move, location, friendship,
  etc.) are **never** merged into a fused line at that phase or any phase after it — conditional
  branches always stay as distinct, player-choosable options (see the pseudo-fusion mechanic
  immediately below for how the *other* parent's line still participates when the player picks
  a conditional-branch option instead of that side's own path).

### Conditional-branch evolutions and the pseudo-fusion-profile mechanic
When a parent's line has a genuinely branching/conditional Phase 2 (multiple possible targets
from the same Phase 1 species, distinguished by item/trade/etc.), those branches are exposed to
the player as-is — never rolled into a fused line. But the *other* parent's Phase 2 options
remain legal alternate choices at that same decision point (per "only one Phase 1 and one Phase 2
evolution" above), which creates an asymmetry that needs correcting: choosing a branch from the
side that has multiple options, while ignoring the *other* side's multiple options entirely,
would otherwise leave that other side "stuck" one stage behind relative to what choosing one of
its own branches would have produced.

The fix is a **pseudo-fusion-profile**: when the player picks a Phase 2 target that belongs to
only one side (i.e. they did not, and structurally could not, also pick one of the *other* side's
own Phase 2 options), that other side's full set of un-taken Phase 2 branches is collapsed into a
single synthetic ("pseudo") stand-in profile, and it is that pseudo-profile — not the untouched
Phase 1 species — that gets fused with the player's chosen Phase 2 target:
- The pseudo-profile keeps the *type1/type2 and abilities* of the currently-displayed phase of
  the side it's built from (i.e. that side's Phase 1 species/identity is unchanged — evolving the
  other side doesn't retroactively change this side's own type/ability identity).
- Base stats are the average across **all** of that side's un-taken Phase 2 branches (not just
  one of them) — e.g. averaging Poliwrath's and Politoed's base stats together, not picking one.
- Moves are a pseudo-random sample drawn from the combined movepools of all of that side's
  un-taken branches (same "sample from a pool" spirit as the normal learnset-merge step, just
  sourced from multiple candidate species instead of one).
- Any other blended/rolled field for this synthetic side is just a plain 50/50 average across
  its own un-taken branches (not a gene-weighted roll — there's no "parent" left to weight
  against once we're synthesizing a stand-in for multiple sibling branches of the same side).
- This pseudo-profile is transient — it is not stored anywhere as its own `BerserkGeneProfile`;
  it exists only long enough to be blended with the player's actual chosen Phase 2 target when
  recalculating the fusion's post-evolution profile.
- If the *other* side's Phase 2 pool has only one branch (no ambiguity — e.g. Dratini only ever
  has one next evolution, Dragonite), no pseudo-profile is needed at all: that single branch is
  used directly as a normal fusion target, exactly like the existing non-conditional case.

**Worked example (Poliwag × Dratini):** Phase 1 merges into a single Poliwhirl×Dragonair line
(both are plain unconditional level-ups at that stage). That merged Phase 1 then has three
Phase 2 branch options available in total: Poliwrath (Water Stone, Poliwag-side), Politoed
(trade holding King's Rock, Poliwag-side), and Dragonite (level 55, Dratini-side).
- Choosing **Politoed**: Dratini-side's Phase 2 pool has only one option (Dragonite), so no
  pseudo-profile is needed — the result is a direct Politoed×Dragonite fusion.
- Choosing **Dragonite**: Poliwag-side's Phase 2 pool has two un-taken options (Poliwrath,
  Politoed), so a pseudo-profile is built from both of them (type/ability from Poliwhirl, stats
  averaged across Poliwrath+Politoed, moves sampled from both) and *that* is fused with Dragonite
  — not a direct Dragonite×Poliwhirl fusion, which would otherwise leave the Poliwag side a full
  stage behind.

### Open questions — implementation is blocked on these being resolved
Raised 2026-09-03; resolved 2026-09-03 (answers below), still holding off on code per explicit
user instruction until any remaining follow-ups are also answered.
1. **Data model for merged phases — resolved.** A single `FusionPotentialEvolution` entry has
   one `targetSpecies` field and can only describe one side changing; a genuine "Both" third
   `sourceParent` state can't fix this, since the bit-width isn't the constraint — one entry
   cannot carry two different target species no matter how source-parent is encoded. Resolution:
   a merged phase is represented as **two ordinary entries** (one sourceParent=A, one
   sourceParent=B), stored adjacent in the array, both carrying the same pre-averaged `param`
   (level), tagged with one new spare bit `pairedWithSibling:1` on `methodAndSourceParent` (room
   exists: `EvolutionMethods` has only 9 values, needing 4 bits, leaving 3 bits spare in that
   byte — no struct growth) meaning "always select/apply this together with its sibling entry,
   never as independent alternatives." **Scoping note:** only phases where *both* sides have a
   simultaneous same-phase unconditional level-up need this pairing (e.g. Larvitar×Rattata Phase
   1). A phase where only one side still has a species change (e.g. that example's Phase 2,
   Tyranitar with Raticate capped out) is just the ordinary already-supported single-sided
   "evolve one side, other side unchanged" entry — no pairing needed there.
2. **Pseudo-fusion scope — resolved.** Only the un-taken sibling branches feed the pseudo-profile
   average/move-sample; the end result is that pseudo-profile fused with the player's selected
   branch evolution.
3. **Deeper branching — resolved.** All siblings at that phase (whichever phase, 1 or 2, is being
   resolved) get equal weight in the pseudo-profile average — no gene-holder weighting applied to
   the pseudo-profile construction itself.

### Follow-up round 1 — resolved 2026-09-04
1. **Paired-entry solution confirmed.** Also confirmed: the asymmetric-phase-count scoping is
   exactly as stated (e.g. Rattata capped after Phase 1 while Dratini continues to Dragonite at
   Phase 2 — that Phase 2 is a plain single-sided entry, not a forced/invented pairing).
2. **Fused display/species name — new mechanic, replaces the earlier "Fusion Pokédex uses a
   description sentence" naming approach with an actual constructed name used everywhere**
   (evolution menu text, the default nickname/species-name shown for the egg and hatched mon,
   Pokédex display, etc.) — not just Pokédex flavor text:
   - **Two-source name fusion** (the normal case: two species being merged at a phase): take the
     first half of source A's name (`ceil(len/2)` characters) and append the last half of source
     B's name (`ceil(len/2)` characters, i.e. the last `ceil(len/2)` characters of B's name).
   - **Multi-sibling name fusion** (pseudo-fusion side with N un-taken siblings, N > 1):
     use each sibling's natural chunk length, even when the result differs from the rounded
     average (clarified 2026-11-09). For zero-based sibling `i`, take `ceil(nameLength / N)`
     characters starting at `floor(i * nameLength / N)` from that sibling's name, concatenating
     the chunks in saved sibling order. Do not pad/resample to an average-length output budget.
   - **Ordering/priority:** the chosen evolution path's name segment always leads (is the
     prefix) when there *is* a chosen path (i.e. the branching-evolution case, not the always-
     paired-together merge case). When there is no "chosen path" to prioritize (the
     always-applied-together paired-merge case, and the base unevolved fusion at birth), ordering
     falls back to the existing 62%/50% gene-holder priority — the higher-weighted parent's name
     leads. On an exact 50/50 tie, which parent leads is decided by one coin flip **at breeding
     time** and persisted on the profile (needs a new small stored field, since `geneHolderWeight`
     only records 1-vs-2 holders, not *which* parent is prioritized in the 50/50 case).
3. **Multiple potential-evolution lines per single parent are normal, not capped at one per
   side** — the existing `slotCount = ceil(lowCount + highCount * r)` weighted-sampling mechanism
   already supports this and is unchanged; the *only* new rule on top of it is the "at least one
   line guaranteed from each parent" floor. Confirmed via worked example: Eevee alone can
   contribute 3 separate eeveelution lines (Sylveon/Umbreon/Jolteon) to one child simultaneously.
4. **Multi-generation re-breeding, confirmed mechanism:** when a parent going into a new breeding
   already has its own `BerserkGeneProfile` (i.e. it's itself a fusion), that parent's
   "candidate pool" for the new child is its own currently-stored `potentialEvolutions[]` list
   (not full ancestry, consistent with the existing rule) — and the new child does a **fresh
   weighted re-selection** from that pool sized by the same `slotCount` formula (candidate count
   for that side = however many entries that fusion parent currently has stored), not an
   unconditional carry-forward of all of them. Worked 3-generation example confirmed: gen 1
   Rattata×Applin(50/50) stores {Raticate, Appletun, Dipplin}; gen 2 re-breeds that gen-1 fusion
   ×Eevee(62/38), selecting 3 of Eevee's 8 lines (Sylveon/Umbreon/Jolteon) and re-selecting 2 of
   the gen-1 fusion's own 3 stored lines (Raticate, Dipplin — Appletun dropped), for 5 total; gen
   3 re-breeds that ×Dratini(50/50), guaranteeing ≥1 from Dratini (its only line, Dragonair) and
   ≥1 from the gen-2 fusion's pool, landing on 3 total (Dragonair, Sylveon, Dipplin) in the
   example, well under the 8 cap.
5. **Purge priority when combined candidates would exceed `MAX_FUSION_POTENTIAL_EVOLUTIONS = 8`:**
   purge **oldest lines first**; tie-break by **fewest remaining evolutionary phases** (a
   1-phase-only line before a 2-phase line); tie-break ties by **random** selection among the
   remaining candidates.
6. **Storage/save-impact implication (new, needs resolving):** the naming and purge-priority
   mechanics above both require information not currently in the packed 6-byte
   `FusionPotentialEvolution` entry or the profile: (a) a per-entry "how old is this line"
   signal for purge ordering, and (b) a single persisted "which parent is name-priority on a
   50/50 tie" bit on the profile. Given the profile is already at a razor-thin size budget (92
   bytes, only 4 bytes of `PokemonStorage` headroom left per the Phase 1 measurements), these
   need a byte-conscious design rather than naive new fields — see follow-up questions below.

### Follow-up round 2 — new, raised 2026-09-04, still blocking implementation
1. **Age tracking without growing the struct:** can "oldest line" simply mean **array position**
   (lines are always kept in insertion order — oldest at the lowest index, newly-selected lines
   always appended after existing carried-forward ones during a re-breeding's re-selection step),
   avoiding any new per-entry field? Or is an explicit small per-entry age/generation stamp
   needed (e.g. packed into the `pairedWithSibling` byte's remaining spare bits) because
   array position alone can't be trusted (e.g. if the re-selection step ever needs to reorder,
   or if paired entries must stay adjacent and that conflicts with strict age ordering)?
2. **Name-priority tie-break storage:** confirm adding one new profile-level bit (e.g.
   `namePriorityParent:1`, packed into the existing `inheritanceFlags` byte — bits 0-7 of that
   byte are currently: bit 0 gene-holder-weight, bits 1-4 type1/2 source parent/slot, bit 5
   sprite-source-parent, bits 6-7 active-ability-slot, i.e. **fully used already** — so this new
   bit needs a different home, such as stealing a bit from an existing byte-aligned-but-not-
   fully-packed field, or accepting the profile grows by a byte if nothing else fits) is the
   right approach, versus deriving name-priority some other way that needs no new storage at all
   (e.g. always deterministically re-deriving it from `geneHolderWeight` plus some other already-
   stored value at the moment a name needs to be generated, rather than persisting a dedicated
   bit)?
### Follow-up round 2 — resolved 2026-09-04
1. **Age tracking — resolved: explicit per-entry age stamp needed** (array position isn't
   trustworthy; user confirmed).
2. **Name-priority tie-break storage — resolved: dedicated new field, profile grows.**
3. **Output name length limit — resolved: scale both halves down proportionally** so the
   combined name never needs truncating, rather than hard-truncating to `POKEMON_NAME_LENGTH`.

### Final field layout and measured save-size impact (decided + measured 2026-09-04)
Chose the recommended **Option C** for age storage: a nibble-packed per-entry age stamp (0-15
range, two entries per shared byte) rather than a full byte per entry (Option B) or reusing
leftover bits with only a 0-3 range (Option A) — same byte cost as Option A, more wraparound
headroom.

Implemented in `include/pokemon.h`:
- `FusionPotentialEvolution.methodAndSourceParent` bit layout finalized: method in bits 0-3
  (`EVO_POTENTIAL_METHOD_MASK`), source parent in bit 4 (`EVO_POTENTIAL_SOURCE_PARENT_BIT`, 0=A/
  1=B), and the new pairing flag in bit 5 (`EVO_POTENTIAL_PAIRED_WITH_SIBLING`) — struct stays 6
  bytes, no growth.
- `BerserkGeneProfile` gains `u8 potentialEvolutionAge[MAX_FUSION_POTENTIAL_EVOLUTIONS / 2]` (4
  bytes, nibble per entry) and `u8 evolutionFlags` (1 byte, bit 0 =
  `BERSERK_GENE_NAME_PRIORITY_PARENT`, 7 bits reserved for future use).

**Measured impact** (via `sizeof` probes, `arm-none-eabi-nm -S` — the earlier awk-based
multi-symbol probe script was found to be unreliable for this measurement and was replaced by an
`nm`-based check): `BerserkGeneProfile` grew from 92 → **96 bytes**. At the previous
`MAX_BERSERK_GENE_PROFILES = 16` (17 stored profiles including the reserved slot), `PokemonStorage`
would have grown to 35776 bytes, exceeding the 35712-byte sector budget by 64 bytes.
**`MAX_BERSERK_GENE_PROFILES` reduced to 15** (16 stored profiles) brings `PokemonStorage` to a
measured **35680 bytes** — comfortably under budget with **32 bytes of headroom** (more headroom
than the original 4-byte margin at the old 92-byte/16-cap configuration). `test/save.c`'s pinned
`T_POKEMONSTORAGE_SIZE` updated to 35680 accordingly. `FusionPotentialEvolution` remains 6 bytes;
`SaveBlock1` unaffected (still 15752). Full `make hns` and `make check` builds both validated
clean (the latter blocked only by the pre-existing unrelated `braille_puzzles.c` issue).

### Line selection, first increment (implemented 2026-09-04)
Implemented the birth-time candidate-collection and selection piece of evolutionary line
handling, deliberately scoped down from the full spec — see "Deferred" below.

- `CollectEvolutionCandidates(daycare, mon, sourceParentB, outCandidates, maxCandidates)`: for a
  non-fusion parent, reads that species' own `GetSpeciesEvolutions()` table and takes only
  **unconditional** entries (`Evolution.params == NULL`); for a fusion parent (has an existing
  `BerserkGeneProfile`), takes that parent's own currently-stored `potentialEvolutions[]` instead
  of its native ancestry (the existing "prevents unbounded growth across re-breeding" rule),
  re-tagging each entry's source-parent bit to reflect the *current* breeding's side.
- `BuildBerserkGeneProfile` now: computes `countA`/`countB` via the above, computes
  `slotCount = lowCount + ceil(highCount * r)` (unchanged formula) clamped to the combined pool
  size and `MAX_FUSION_POTENTIAL_EVOLUTIONS`, guarantees one candidate from each side that has
  any (new floor rule), then fills remaining slots via the existing `BerserkGeneShouldInheritFromParent`
  gene-weighted roll without replacement. Every entry gets the profile's zero-initialized default
  age stamp, correct for a freshly-allocated profile.

**Deferred to later increments (not yet implemented):**
- **Conditional evolutions are entirely excluded from the candidate pool** — an evolution with
  any `CONDITIONS(...)` (item, trade, friendship, location, move, etc.) is skipped outright rather
  than incorrectly stored as if it were free. This means branching lines like Poliwag/Dratini are
  not yet representable at all; they simply won't appear as potential evolutions until the
  condition-set-ID registry (build-time generated stable table, spec'd earlier in this doc) is
  built and wired through `CollectEvolutionCandidates`.
- **Merging simultaneous unconditional same-phase lines** (the paired-entry mechanic, e.g.
  Larvitar×Rattata Phase 1) is not implemented — right now Rattata's and Larvitar's Phase 1
  entries would simply both be stored as independent single-sided entries, not merged into one
  fused choice.
- **The pseudo-fusion-profile mechanic** for un-taken sibling branches is not implemented (depends
  on conditional-evolution support existing first).
- **Fused-name generation** (the portmanteau naming algorithm) is not implemented.
- **Age-based purge-priority** is not implemented — there's no code path yet where combined
  candidates could exceed `MAX_FUSION_POTENTIAL_EVOLUTIONS`, since only single-phase unconditional
  entries are collected right now (real multi-line, multi-generation stacking that could approach
  the cap requires conditional-evolution support to produce enough candidates in the first place).
- **The actual evolution-trigger hook point** — nothing yet reads `potentialEvolutions[]` to let a
  fusion mon actually evolve into one of its stored options; `GetMonEvolutions`/evolution-checking
  code has not been touched.

### Simple paired level-evolution merge, second increment (implemented 2026-09-04)
The unambiguous base case of the paired-entry design is now implemented: when **each parent has
exactly one selected candidate**, both candidates are unconditional `EVO_LEVEL` entries, and the
selection produced exactly those two entries, they become one logical fused phase. The adjacent
entries retain their own target species/source-parent bits, both gain
`EVO_POTENTIAL_PAIRED_WITH_SIBLING`, and both `param` values become
`ceil((levelA + levelB) / 2)`. This implements the initial Larvitar×Rattata-style phase
(Pupitar×Raticate at level 25) without inventing behavior for multi-branch cases. Regression
test added for that exact pair/level/flag invariant.

**Still deferred:** one-vs-many and many-vs-many selected-line combinations (including
Rattata×Applin) need the later pseudo-fusion/branch model before pairing can be defined correctly;
they remain independent stored entries for now. The flags are storage metadata only until the
future evolution-trigger hook teaches the runtime to consume both adjacent entries atomically.

### Potential-line age propagation, third increment (implemented 2026-09-04)
The nibble-packed per-entry age field is now active in candidate collection. A native evolution
line first introduced by a non-fusion parent is stamped age 0. A line inherited from a parent
that already has a profile carries its stored age forward and increments it, saturating at 15.
`GetPotentialEvolutionAge`/`SetPotentialEvolutionAge` centralize the two-entries-per-byte packing;
the selected candidate's age is copied into the newly allocated child's profile alongside its
evolution entry. This makes the required "oldest first" information survive multi-generation
re-breeding rather than relying on array order.

**Still deferred:** cap pruning itself. Age is sufficient for the first priority key, but the
user's required second key is "fewest remaining evolutionary phases." The current 6-byte entry
stores only its immediate target, so a correct implementation must first define how to compute
the remaining reachable phase depth for conditional and paired/multi-sibling lines without
accidentally expanding a fusion parent's full ancestry. Do not replace the current weighted
selection with an age-only pruning heuristic; that would silently violate the specified second
tie-break rule.

### Oldest-line cap pruning, fourth increment (implemented 2026-09-04)
The user removed the "fewest remaining evolutionary phases" secondary tie-break. When candidate
collection exceeds `MAX_FUSION_POTENTIAL_EVOLUTIONS`, candidate construction now repeatedly:
finds the greatest age among candidates whose source side has more than one remaining line,
randomly picks one entry from that oldest-age set, and removes it. This preserves the required
at-least-one-line floor for each parent side while making age the only priority criterion; ties
between equally-old paths are deliberately random as specified. Pruning happens before the
normal 62%/50% weighted selection fills the child's requested slots, so an old route cannot
survive merely due to source-array ordering.


(`parentSpeciesA`/`parentSpeciesB` on the profile), not a fixed final form. It carries the
potential to evolve along *either* parent's evolutionary line, and each evolution re-derives most
(not all) of its traits from the updated species pair.

A Berserk-Gene-bred child is conceptually a standing "fusion" between two specific species
(as above). The subsections immediately above this point (line selection, merging, and the
pseudo-fusion-profile mechanic) are the current spec; what follows next covers storage
representation, the condition-set-ID registry, and per-evolution rerun/preservation rules, which
still apply on top of the updated line-selection/merging rules above.

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

### On evolution: deterministic phase resolution, reruns, and preserved traits
The deterministic full-line model above supersedes the earlier one-side rolling-pool update. At
Phase 1, both resulting sides advance: the chosen target is concrete and the opposite side is the
selected opposite-parent Phase 1 target(s), directly or as a pseudo-fusion. At Phase 2, the chosen
target is concrete and all active opposite-side Phase 2 targets become the direct/pseudo-fusion
counterpart. `parentSpeciesA`/`parentSpeciesB` alone are not enough to represent a multi-species
pseudo side; the implementation must retain the selected lineage membership needed to rebuild that
transient side whenever traits, names, display, or future choices are calculated.

After updating the changed side, **rerun the full Berserk Gene trait pipeline between the new
species pair**, recalculating: base stats (still blended, per the updated formula above), height/
weight/pokemonScale/pokemonOffset (blended), learnset (full re-merge using the two new-current
species), color, cry, ability/secondary ability/hidden ability, sprite/identity, and the
potential-evolutions list.

Potential-evolution updates after evolution are deterministic projections of the lineage graph,
not a rolling-pool update:
1. Consume the chosen phase's target and every other Phase 1 target after a Phase 1 choice.
2. Derive the next phase's active choices only from descendants recorded at birth for the chosen
  line and the selected opposite-side lines that form the direct/pseudo counterpart.
3. At Phase 2, consume every remaining active choice and clear the active evolution projection.
4. Do not call `Random()`, reapply 62/38 or 50/50 weighting, query fresh species evolution data,
  or add newly discovered targets during evolution.

**Learnset rerun scope (decided):** matches vanilla evolution behavior exactly — recalculating
the learnset on evolution only regenerates the **future** level-up and teachable/tutor move
tables (i.e. what the fusion can still learn going forward, and what TMs/tutors it's eligible
for from this point on). It does **not** retroactively touch the mon's **currently known** move
slots; those stay exactly as they were before the evolution, exactly like a normal Pokémon never
has its already-learned moves swapped out on evolving.

**Preserved unchanged across every evolution** (never recalculated): IVs, shininess, gender, egg
groups, nature, hatch rate (`eggCycles`), growth rate. These are locked in at birth and are not part of the
rerun.

**Growth rate (clarified 2026-11-09):** select the EXP curve only when breeding writes the fusion
profile. Neither evolution phase nor pseudo-side aggregation changes it. EXP/level consumers must
use that stored curve consistently; evolution must not remap EXP onto a different curve.

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

**Pseudo-side color/cry (decided 2026-11-09):** independently for each trait, select a parent side
with the stored 62/38 or 50/50 weighting, then select uniformly among that side's resolved sibling
species. Do not average categorical IDs. Abilities still use the displayed pseudo-side species;
growth rate is excluded from all evolution rerolls.

**Flying/Levitate exclusion carries forward to every reroll (new, 2026-09-04):** a Berserk Gene
mon must never have Levitate as its *active* ability while typed Flying. At breeding time
(`BuildBerserkGeneProfile`), this is enforced two ways: the special Flying-parent Levitate
candidate is never injected when the child's already-resolved type is Flying, and — the gap
found later — any *normally-rolled* ability slot that merely happens to already be Levitate (a
non-Flying parent that naturally has it, e.g. Baltoy) is also excluded from the active-ability
pick when the child is Flying-typed (falls back to `ability1` in the astronomically unlikely case
every candidate gets excluded). The stored `ability1`/`ability2`/`abilityHidden` values themselves
are **not** scrubbed — only the *active* pick is constrained — so a later non-Flying descendant
can still potentially get Levitate active. The user has confirmed no current evolution line
introduces the Flying type partway through (so the type side of this is a birth-time-only
concern), but a species **can** gain Levitate partway through an evolution line — so **the
evolution-time ability fresh-reroll above must apply this exact same exclusion check** (using the
mon's type *at that point in the evolution*, not just at birth) once that rerun logic is
implemented; do not port the fresh-reroll logic without also porting this guard.

**Wonder Guard is excluded from fusion profiles entirely (new, 2026-09-04):** unlike Levitate
(which is only excluded from the *active* pick, and only while Flying-typed), Wonder Guard is
never allowed into `ability1`/`ability2`/`abilityHidden` **at all** for a fusion profile — this
only applies when the same-species-skip case (above) does *not* apply, i.e. only for genuine
two-different-species fusions. Each of the three per-slot rolls falls back to the *other*
parent's value for that slot if the initially-rolled value is Wonder Guard, and to Sap Sipper in
the (currently unreachable in practice, since Wonder Guard is Shedinja-exclusive and same-species
pairs already skip profile generation) case where both sides would be Wonder Guard.

### Sprite/identity: fresh reroll, same stored weight, but must always change
Also a fresh weighted reroll (same `geneHolderWeight` as above) between the two resolved parent
sides, including hidden pseudo-side siblings. Remove the actual previous sprite species from
**both** sides, then select a side with the saved weighting and a sibling uniformly within that
side. Force the sole side with alternatives if necessary. The sprite changes whenever a distinct
candidate exists; if both sides resolve only to the old donor, keep that sole possible species.
If the old donor evolved away and is absent from both new pools, nothing is excluded and both
sides remain eligible. Example: Applin-sprite Eevee×Applin
fusion evolves via Thunder Stone → candidates are {Jolteon, Applin} minus {Applin} → forced
Jolteon. Eevee-sprite version evolving the same way → candidates {Jolteon, Applin}, Eevee itself
isn't a candidate anymore (it turned into Jolteon), so both remain eligible, rolled normally with
the stored weight (62% chance of Jolteon if only Applin held the gene at breeding, per the user's
example). Update `spriteSourceParent` and the packed sibling ordinal; leave the displayed
parent identity used by types/abilities unchanged.

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

### Phase 4, first wrapper increment (implemented 2026-09-04)
Implemented the first two centralized concrete-mon override surfaces in `src/pokemon.c`:
- `GetMonBaseStat(struct Pokemon *mon, u32 statIndex)` returns the profile's `baseStats[statIndex]`
  for a mon with a live Berserk Gene profile, otherwise delegates to unchanged
  `GetSpeciesBaseStat(species, statIndex)`. `CalculateMonStats` now uses this wrapper for all six
  stat calculations unless its existing equalized-BST override applies.
- `GetMonAbility(struct Pokemon *mon)` now returns the profile-selected active ability slot for a
  mon with a live profile; otherwise it preserves `GetAbilityBySpecies(species, abilityNum)`.
  Existing gameplay consumers already route through `GetMonAbility`, avoiding broad migration.

`test/berserk_gene.c` now verifies both overrides against manually-populated profile values.
`make hns` builds clean; `make tidycheck && make check` reaches only the known pre-existing
`braille_puzzles.c` `FLAG_RECEIVED_TOGEPI_EGG` failure.

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
  - **Correction:** nominal `MON_DATA_SPECIES` is not necessarily the saved sprite donor.
    Dex/mythical/legendary/frontier-ban flags must not change merely because a different sprite
    was selected. Per-mon sprite/animation/elevation/shadow-size/footprint/gender-difference
    consumers need an explicit visual-donor audit; species-reference tables remain unchanged.
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
migration phase must include the applicable requirements below. Historical requirements to
migrate or support existing saves and builds apply only if the user later explicitly requests
compatibility; the current decision is that previous-build/save compatibility is not required.
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
