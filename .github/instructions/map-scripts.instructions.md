---
applyTo: "data/maps/**/scripts.inc,data/scripts/**"
---

# Map/event script (.inc) conventions

- Before calling any macro from `asm/macros/event.inc` (or a custom `.macro` defined
  in a map's `scripts.inc`), check its actual `.macro name param1, param2, ...`
  signature — do not assume it matches a similarly-named sibling macro's calling
  convention. Example: `showmoneybox x:req, y:req, disable=0` takes 2-3 args, but
  `updatemoneybox disable=0` and `hidemoneybox` (0 params) take none of the x/y
  arguments despite living right next to `showmoneybox` and being used together in
  the same sequence. Passing extra positional args produces a GNU `as` error
  ("too many positional arguments") that gets attributed to the macro's body line,
  with each call site listed separately as "macro invoked from here" — always trace
  that body line back to the actual macro definition to find the mismatched call.
- When writing a custom shop/vendor script, prefer this pattern for a fixed custom
  price (when it differs from the item's default `.price` in items.h, so the
  built-in `pokemart` command can't be used as-is):
  `msgbox ..., MSGBOX_YESNO` → `goto_if_eq VAR_RESULT, NO, <decline>` → `checkmoney`
  → `checkitemspace` → `showmoneybox` → `removemoney` → `updatemoneybox` →
  `additem` → `hidemoneybox`.
- Reuse existing shared strings where possible instead of writing new ones, e.g.
  `gText_YouDontHaveMoney`, `gText_NoMoreRoomForThis`, `gText_HereYouGoThankYou`
  (defined in src/strings.c).
