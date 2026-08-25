I would like to make some changes to the plan.

For determining the base stats of a Pokemon resulting from the Beserk Gene editing, we should use a similar weight-average formula as we do with `height`, `weight`, `pokemonScale`, and `pokemonOffset`.




For determining the primary and secondary type of the resulting Pokemon, be sure that the type selected for the primary type is removed from the selection pool when selecting the secondary type. We don't want to end up with a Poison/Poison Pokemon when breeding a Poison/Fighting Pokemon with a Ground/Poison Pokemon. Also, when selecting the potential primary and secondary types for the resulting Pokemon, both the primary and secondary types are available to be selected. For example, if we bred a Normal/Psychic type Pokemon with a Rock/Fairy Pokemon, and the child could end up being Normal/Psychic, Rock/Fairy, Normal/Fairy, Rock/Psychic, Psychic/Normal, Fairy/Rock, Psychic/Rock, Fairy/Normal, Psychic/Fairy, or Fairy/Psychic. If we were to breed a Normal type Pokemon with a Normal/Psychic type Pokemon, the resulting child could be Normal, Normal/Psychic, Psychic/Normal, or Psychic type.






I also had not previously stated anything about the Pokemon's evolutionary line.
When chosing traits for a given Pokemon, similar affects should be made to each potential evolution, like which parent we derive the primary or secondary typing from, as well as which parent we derive the ability from.

When a Pokemon is created through the Beserk Gene Editing feature, it has the potential for every possible evolution between both parents.

For example, if you were to breed Eevee and Applin together (resulting in the highest number of potential evolutions), the resulting child Pokemon could evolve either by Sweet Apple, Tart Apple, Syruppy Apple, Thunder Stone, Water Stone, Fire Stone, Leaf Stone, Nevermelt Ice, leveled up while holding a Heart Scale, leveled up with high friendship during the day, or leveled up with high friendship during the night.

The result of evolution will rerun the Berserk Gene overrides and recalculate all base stats, except for IVs, shininess, gender, egg groups, nature, and hatch rate (which remain the same throughout evolution). For example, if Eevee and Applin were bred, and then their child was evolved by a Thunder Stone, the result of the evolution would be running the same Berserk Gene Editing rules between Jolteon and Applin.

This would recalculate base stats, height, weight, pokemonScale, and pokemonOffset based on the weight-average formula.

This would recalculate the learnset based on the two new input Pokemon (in this example that would be Jolteon and Applin).

This would recalculate the primary type of the resulting Pokemon, however, there are some changes. We will maintain the type (primary or secondary) of the parent throughout their evolutionary line. If the primary type was derived from parent A (in our example this would be Eevee), and an evolutionary line from this parent was chosen (in our example it was), and the primary type of that parent was changed in this evolutionary line (in our example it changed from Normal to Electric), then the evolved Pokemon's type would change to match it. If our Eevee/Applin hybrid was born as a Normal/Dragon Pokemon (Normal primary type from Eevee and Dragon secondary type from Applin), then evolved with a Thunder Stone, the evolution would result in our Pokemon having an Electric/Dragon typing. This same logic follows for secondary typing.

This would recalculate the Pokemon's cry, color, ability, secondary ability, and hidden ability using the binary pick weighted by which parents were holding berserk genes when the egg was created (therefore we need to make sure those weights are stored and remembered so they can be referenced during evolution).

This would update the Pokemon's sprite as well, using the binary pick weighted by which parents were holding berserk genes when the egg was created. However, never use the same sprite as before the evolution. For our example, if our Eevee/Applin hybrid was born with the Applin sprite and evolved into Jolteon, the only sprite option would be Jolteon. If the Eevee/Applin hybrid was born with the Eevee sprite, the available options of sprites would be Applin and Jolteon. If only our Applin was holding a berserk gene while breeding, then there would be a 68% chance that the evolution would result in the Applin sprite.

This would also update the potential evolutions for our Pokemon. The Pokemon should always have the full combined result of evolutionary options from both parties involved. In our example, we evolved our Eevee/Applin hybrid to get a Jolteon/Applin hybrid. After this evolution, the remaining evolutionary options would be Flapple, Appletun, and Dipplin. If evolved into Dipplin to form a Jolteon/Dipplin hybrid, there would be one remaining evolution option left of evolving into Hydrapple to become a Jolteon/Hydrapple hybrid.

While none of the Pokemon fused in Berserk Gene Editing will be visible in the Pokedex, we should still have the "Pokedex" option when viewing their summary. We should be able to populate the data with our Berserk Gene overides. When the INFO tab is displayed, it should say "Fusion Pokemon" under the name. The description should say "It appears to be a fusion of [Pokemon A] and [Pokemon B]." (i.e. "...Eevee and Applin." or "...Jolteon and Dipplin."). Under the AREA tab it should say "AREA UNKNOWN" for both DAY and NIGHT. The STATS tab should be properly populated, included the moves from our overrides. The EVO tab should be properly populated, including the evolutions from our overrides. The CRY should be properly populated, using the cry from our overrides. The size should be properly populated, using the values from our overrides.