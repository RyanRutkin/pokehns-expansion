# Repo-wide notes for AI coding agents

## Known pre-existing build issue
A full `make` (without `-k`) currently aborts on an unrelated, pre-existing error:
`src/braille_puzzles.c: 'FLAG_RECEIVED_TOGEPI_EGG' undeclared`. This is not caused
by feature work done in agent sessions. To verify your own changes compile/assemble
without being blocked by this, use `make -k -j$(nproc)` and grep the log for the
specific files/symbols you touched, rather than relying on a clean full build.

## Level-up learnsets have two active data sources — check both
`GetSpeciesLevelUpLearnset()` in `src/pokemon.c` picks between two different
per-species level-up move tables depending on the player's "Modern Moves" challenge
setting (`gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves`):
- OFF → `gLevelUpLearnsets_Gen3`, built only from `src/data/pokemon/level_up_learnsets/gen_3.h`.
- ON  → `gSpeciesInfo[species].levelUpLearnset`, built from whichever single file
  `P_LVL_UP_LEARNSETS` (in `include/config/pokemon.h`) selects via the `#elif` chain
  in `src/pokemon.c` (currently `gen_7.h`).
The other files under `src/data/pokemon/level_up_learnsets/` (gen_1/2/4/5/6/8/9.h)
are inert unless that config changes. When adding/fixing a level-up move (e.g. a
move required for a level-based evolution that a species doesn't naturally learn),
update **both** `gen_3.h` and the currently-active gen file (check
`P_LVL_UP_LEARNSETS` first) so the fix holds regardless of the player's challenge
setting.

## TMs are a fixed, finite item list
This build only has as many TM item slots as entries in `FOREACH_TM`
(`include/constants/tms_hms.h`, HNS branch) — currently up to `ITEM_TM93`. There is
no separate "modern TM number" display system independent of the item ID; the TM
number *is* the item slot. Placeholder slots (e.g. unused `ITEM_TM93`..`ITEM_TM100`
in `src/data/items.h` with `sQuestionMarksDesc // Todo`) already exist for adding
new TMs — reuse the next unused slot rather than inventing a new item ID scheme.
`NUM_TECHNICAL_MACHINES`/`RANDOMIZER_MAX_TM` are macro-derived from `FOREACH_TM`, so
they update automatically when a TM is added.

## Map/event scripts (.inc)
See `.github/instructions/map-scripts.instructions.md` (auto-applied to
`data/maps/**/scripts.inc` and `data/scripts/**`) for macro-signature and shop-script
conventions.

## Never hand-edit generated learnset/teachable-move files
`src/data/pokemon/teachable_learnsets.h`, `src/data/tutor_moves.h`, and
`src/data/pokemon/all_learnables.json` are build-generated artifacts (see the
`TEACHABLE_DEPS`/`$(ALL_LEARNABLES_JSON)` rules in `Makefile`), produced by
`tools/learnset_helpers/make_teachables.py` / `make_learnables.py`. They are also
git-ignored (not tracked), so `git status`/`git reset` won't reflect or protect
manual edits to them. `teachable_learnsets.h` regenerates from scratch whenever any
of its listed dependencies change — notably `include/constants/tms_hms.h` — so a
direct hand-edit to it will be silently overwritten by the next `make` that touches
one of those dependencies (e.g. adding a new TM), discarding the manual change and
recomputing the whole list from canonical source data instead.
Do **not** hand-edit these generated files. Instead:
- If a species should learn a move as a TM/tutor move, the underlying learnable-move
  source data (whatever feeds `make_learnables.py`, e.g. files under
  `dev_scripts/`/`LEARNSET_HELPERS_DATA_DIR`) needs updating, not the generated `.h`.
- After changing anything in `TEACHABLE_DEPS` (e.g. `tms_hms.h`, `config/pokemon.h`,
  `special_movesets.json`), force a clean regeneration and inspect the *actual*
  output before assuming something is missing or broken:
  `make clean-teachables && make hns -j$(nproc)`, then check the regenerated
  `teachable_learnsets.h` directly.

## Custom per-shop item pricing
Standard `pokemart` shops always price items from that item's own global `.price`
in `src/data/items.h`, with no per-shop override built in. To sell one or more items
at a different price in a *specific* shop, without changing that item's price
everywhere: use the price-override mechanism in `src/shop.c`
(`SetShopPriceOverrides`/`GetShopItemPrice`), **not** a new `MART_TYPE` and **not** a
hand-rolled sequential yes/no purchase script — both were tried and rejected in
favor of this approach.
- The shop's item list/order still lives inline in that map's own `scripts.inc`, as
  a normal `.2byte` `pokemart`/`pokemartlistend` list — do not move it elsewhere and
  do not change its format.
- Define a `struct ShopPriceOverride { item, price }[]` table containing only the
  items that need a non-default price (see `src/data/shop_custom_prices.h` for the
  Goldenrod Underground evolution-item clerk's table as an example), plus a niladic
  wrapper function `void Set<Shop>Prices(void) { SetShopPriceOverrides(sTable); }`.
- Register that wrapper in `data/specials.inc` with `def_special`, then call
  `special Set<Shop>Prices` immediately before the existing `pokemart <list>` line
  in that shop's script. Nothing else needs to change.
- The override table is automatically cleared when the buy menu closes, so it never
  leaks into a later, unrelated shop. Shops that never call a setter are completely
  unaffected and behave exactly as before.

## `CreateMon` signature differs from vanilla pokeemerald
`void CreateMon(struct Pokemon *mon, u16 species, u8 level, u32 personality, struct
OriginalTrainerId);` — only **5 args**, not the 8-arg vanilla signature
(`species, level, fixedIV, hasFixedPersonality, fixedPersonality, otIdType, fixedOtId`).
The last two vanilla params (OT ID method/value) are collapsed into one
`struct OriginalTrainerId { enum OtIdMethod method; u32 value; }`. Use the existing
helper macros instead of building the struct by hand:
`OTID_STRUCT_PLAYER_ID`, `OTID_STRUCT_PRESET(value)`, `OTID_STRUCT_RANDOM_NO_SHINY`
(see `include/pokemon.h`). There is no separate fixed-IV/personality param pair —
pass `0` for personality unless a specific value is needed. Writing a `CreateMon`
call from memory (e.g. porting a snippet from upstream pokeemerald or an older test)
will silently compile-fail with a confusing "too many arguments" / "incompatible
type for argument 5" error pointing at `FALSE`/`0`, not at the real mismatch —
always check `include/pokemon.h`'s actual declaration first, and check
`src/script_pokemon_util.c` for a real call site to copy the pattern from.

## WSL/UNC file view can silently desync from what's actually on disk
When editing files through the `\\wsl.localhost\...` UNC path, the editor/tool file
view has been observed to return **stale or wrong-branch content** for a file,
even though `replace_string_in_file`/`read_file` reports success. This has caused
edits to silently apply to a phantom copy that never reaches disk (e.g. after a
branch switch, a file's tool-visible content matched an *older branch's* version
of that same file, not the currently checked-out one). Symptoms: a build/test
failure references code that "should" have been changed but wasn't, or repeated
edits to the same lines keep reporting success without fixing the error.
Mitigation, once this is suspected:
- Verify the real on-disk content natively via WSL before trusting further edits:
  `wsl -d Ubuntu -e bash -lc "cd /home/rutki/decomps/pokehns-expansion && cat -n <file>"`
  or `grep`/`wc -l` for a quick sanity check, and compare against what `read_file`
  returned.
- If they disagree, don't keep patching through the UNC path — rewrite the file
  directly via a WSL heredoc (`cat > <file> <<'EOF' ... EOF`), using an *outer
  single-quoted* heredoc delimiter so C code with `"`, `$`, and backticks passes
  through untouched, then re-verify with `grep -c`/`cat` from WSL before building.
- After any branch switch (`git checkout`/`git switch`), re-verify any file you're
  about to edit against WSL-native `git status`/`cat` first, rather than assuming
  the tool's cached view already reflects the new branch.

