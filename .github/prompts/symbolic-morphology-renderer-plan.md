# Symbolic Morphology Renderer - Future Plan

Status: IDEA / FUTURE RESEARCH TRACK, NOT PART OF THE CURRENT BERSERK GENE EDITING IMPLEMENTATION.

This document explores a possible future visual-fusion system inspired by the same long-term goal
behind Berserk Gene Editing: children should not merely inherit gameplay traits, they should also
look like a believable blend of both parents. This is intentionally separated from the Berserk Gene
Editing plan so the core breeding/storage/gameplay feature can ship first without taking on the
much larger rendering problem.

## High-level idea

The closest GBA-compatible version of a runtime 3D creature-fusion system is probably not real
mesh fusion. It is a **symbolic morphology renderer**: each species is described as a compact set
of abstract body parts, and the game combines those descriptors to choose a fused arrangement that
can be rendered using prebuilt 2D sprite pieces, palette rules, and simple transforms.

Instead of storing a full 3D model per species, each species gets a small morphology descriptor:
- what parts it has,
- where those parts attach,
- what broad shape each part uses,
- what side/symmetry rules apply,
- what colors/palette roles the part uses,
- how strongly the part contributes to silhouette/identity,
- and a small vector/axis describing how that part scales or stretches.

The renderer then creates a child appearance by combining the two parents' part descriptors using
deterministic, seeded rules. The output should be visually stable for the same fusion pair/profile,
but not require manually approving every possible pair.

## Why this is different from full 3D fusion

The original dream version is essentially:
1. Store categorized 3D parts for every creature.
2. Match equivalent parts between two parents.
3. Average count, position, scale, and mesh geometry.
4. Assemble a new 3D creature.
5. Project/render that creature back into a 2D sprite.

That is a strong design for a modern engine, especially Unreal, but it is far beyond what should
run inside a GBA ROM. The GBA-friendly translation is:
1. Store compact symbolic descriptors, not meshes.
2. Match parts by category and attachment metadata.
3. Choose/average descriptor values using fixed-point math.
4. Compose from a limited 2D part atlas or overlay system.
5. Fall back gracefully whenever a combination cannot be made cleanly.

This preserves the spirit of infinite runtime fusion while reducing the problem to something closer
to sprite composition and deterministic procedural selection.

## Core data model

Each species would have a `MorphologyProfile` made of a small number of `MorphPart` entries,
probably 8-16 parts per species at most.

Conceptual fields for each part:
- `category`: torso, head, limb, wing, tail, horn, ear, fin, shell, plant, flame, armor, eye,
  marking, aura, etc.
- `shapeId`: index into a small shared part-shape atlas, not a unique custom mesh.
- `attachTo`: parent part category or explicit part index, e.g. wings attach to torso, horns to
  head.
- `attachPosition`: normalized 2D anchor such as top, back, lower-side, face, tail-base.
- `side`: center, left, right, mirrored-pair, radial, stack.
- `countPolicy`: fixed, average-rounded, inherit-from-base, inherit-from-secondary, capped.
- `axisVector`: short signed 2D vector used for stretching/scaling along the part's natural
  direction.
- `scale`: compact fixed-point scalar or small enum bucket.
- `paletteRole`: body-main, body-shadow, accent, eye, mouth, elemental-accent, etc.
- `typeAffinity`: optional hint for color/marking choices; can be related to Pokemon type but
  should not require battle type to match.
- `priority`: draw order and importance for silhouette.
- `identityWeight`: how strongly this part defines the species visually.

The goal is not to perfectly reconstruct a species' canonical sprite from these parts. The goal is
to produce a readable symbolic creature that carries recognizable hints from both parents.

## Fusion algorithm sketch

Inputs:
- parent species A morphology profile,
- parent species B morphology profile,
- Berserk Gene profile seed / personality seed,
- selected identity/base parent from the existing gameplay plan.

Process:
1. Pick the base body plan from the identity parent. This keeps the result readable and prevents
   every fusion from becoming a chaotic average.
2. Build a part-match table by `category`, then refine matches by `attachTo`, `side`, and
   `identityWeight`.
3. For matched parts, blend compact properties: scale, anchor offset, color role, and axis length.
4. For unmatched high-identity parts on the secondary parent, optionally import a capped number of
   them as accents. Examples: wings, horns, leaf stems, tails, shells, crests, armor plates.
5. Resolve symmetry/count. For paired parts, average counts using seeded rounding; for important
   odd-count parts, preserve one centered part when possible.
6. Resolve palette. Start with the base parent's main palette roles, then shift selected accent
   roles toward the secondary parent's palette. Avoid full-palette averaging when it destroys
   contrast.
7. Compose the final appearance from a small part atlas or overlay set.
8. Cache only lightweight output choices in the fusion profile if needed, not a full generated
   bitmap.

Important property: the same parents + same seed must produce the same visual result every time,
so no generated art needs to be persisted as pixels in save data.

## Rendering options on GBA

### Option A: base sprite plus overlay parts

Keep the identity parent's normal front/back sprite as the base. Add a small number of overlaid
parts from a shared atlas: horns, wings, tails, markings, leaves, spikes, aura bits, fins, etc.

Pros:
- Most feasible on GBA.
- Existing sprite remains clean and animated.
- Minimal runtime cost.
- Does not require generating a full new 64x64 bitmap.

Cons:
- Looks like a variant/accent system more than true fusion.
- Overlay placement needs per-species anchors to avoid obvious misalignment.
- Hard to support battle back sprites and icons with the same quality.

### Option B: generated composite sprite buffer

At load time, render selected parts into a small RAM buffer, then upload the result as sprite
tiles. This is closer to a real procedural sprite.

Pros:
- More visually integrated than separate overlays.
- Can bake part order into one sprite image.

Cons:
- Much harder: needs tile-safe drawing, transparency handling, palette reduction, and VRAM upload
  management.
- Runtime sprite generation on GBA is slow and brittle.
- Save data cannot reasonably cache many generated bitmaps, so the output must be regenerated.

### Option C: symbolic mini-creature renderer

Ignore canonical Pokemon sprites and render all fused creatures from shared primitive parts: body
ovals, heads, limbs, wings, tails, fins, shells, etc.

Pros:
- Truly infinite combinations become possible.
- Much more controllable than trying to fuse arbitrary existing pixel art.
- Strong fit for an original creature game.

Cons:
- It stops looking like a standard Pokemon battle sprite system.
- Requires an entirely new art direction and renderer.
- Better suited to a new game than a ROM-hack extension.

## Recommended path if this is ever pursued in this ROM

Do not start with full generated sprites. Start with **Option A: base sprite plus overlay parts**.

Milestone 1:
- Add a morphology descriptor for a small test set of species, maybe 10-20.
- Define only anchors and high-level categories, not full part geometry.
- Render no new visuals yet; just debug-print or test the fused part list.

Milestone 2:
- Add a tiny shared overlay atlas with a few generic parts: small horns, wings, tail accent,
  leaf, fin, spikes, glow/marking.
- Support only front battle sprite overlays.
- Use deterministic seeded selection from the fused morphology descriptor.

Milestone 3:
- Add palette-role remapping for overlays.
- Add back sprite overlays only for parts that can be placed reliably.
- Add party icon tinting or one simple icon overlay.

Milestone 4:
- Expand the descriptor authoring to more species.
- Add tests that verify deterministic output and bounds safety.
- Add fallback rules when no compatible overlay exists.

Only after these milestones work should Option B be considered. Option C should probably remain a
separate-engine idea, not something forced into the GBA project.

## Data/storage considerations

This feature should not store full generated sprites in save data. Save data should store only:
- the existing Berserk Gene profile / parent species,
- a deterministic appearance seed if the gameplay seed is not enough,
- maybe a few compact chosen overlay IDs if regeneration must remain stable across future ROM
  updates.

All actual art assets should live in ROM as shared atlas parts and palette data. A single fusion
should be reproducible from profile + seed + current ROM asset tables.

The main ROM-space cost would come from:
- morphology descriptors for many species,
- the shared overlay atlas,
- extra palette tables,
- front/back/icon overlay variants if supported.

The main save-space cost should be kept near zero.

## Major risks

- **Art coherence risk:** symbolic parts can look pasted-on unless anchors and draw order are very
  carefully curated.
- **Descriptor-authoring burden:** this avoids per-fusion art approval, but it still requires each
  species to have good morphology metadata.
- **Animation mismatch:** battle animations assume a sprite is one coherent image; overlays may
  not move correctly unless tied to the same affine/offset behavior as the base sprite.
- **Palette pressure:** GBA sprites have tight palette constraints. Overlay colors must be chosen
  from compatible palette slots or loaded as separate objects with their own palettes, which adds
  VRAM/OAM pressure.
- **Back sprite/icon parity:** a feature that only looks good from the front may feel unfinished.
- **Scope creep:** this can easily become a second full game engine inside the ROM hack.

## Relationship to Berserk Gene Editing

This should remain a future visual layer on top of Berserk Gene Editing, not part of the first
implementation. Berserk Gene should first establish the gameplay truth: parent species, inherited
traits, type provenance, evolution behavior, and persistence. A symbolic morphology renderer could
then read that already-existing profile and decide how to visualize it.

The first Berserk Gene release should keep using the selected parent sprite/identity as currently
planned. That gives the feature a stable gameplay foundation before any procedural visuals are
attempted.

## Bottom-line feasibility judgment

Full runtime 3D-part fusion projected back into polished Pokemon-style sprites is not realistic for
GBA. A symbolic 2D morphology system with shared overlay parts is possible in principle, but it is
a large feature with significant art/data/tooling burden. It should be treated as a separate future
research project after Berserk Gene Editing is complete and stable.

For an Unreal Engine project, the original skeleton/part/mesh averaging idea is much more natural
and should be explored there first. Lessons from that system could later inform a dramatically
simplified GBA symbolic renderer.