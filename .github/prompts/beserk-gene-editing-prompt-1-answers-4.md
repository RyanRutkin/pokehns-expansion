Based on the new considerations, add these details to our plan:

- Add a compile-time kill switch (P_BERSERK_GENE_EDITING-style flag) so the feature can be disabled without reverting the save format if a serious bug surfaces post-release.
- Add a zero-regression guarantee: ordinary (non-gene) breeding must stay byte-for-byte identical, including RNG call count — extra Random() calls creeping into the unaffected path could silently perturb other systems.
- Make sure we use a phased, independently-buildable implementation order (storage → breeding pipeline → downstream wrappers → evolution rerun → Pokédex display → trade/link gating), each phase tested before the next starts — this bounds the blast radius of any single mistake instead of one giant diff.
- Add a debug-only command to directly synthesize a test profile on a party mon, skipping the full breed/hatch cycle — otherwise iterating on the riskiest phases (3-5) is far too slow.
- Yes, fusion pokemon will only occur from breeding. These Pokemon will not be used by other trainers in the game nor will they be found in the wild.