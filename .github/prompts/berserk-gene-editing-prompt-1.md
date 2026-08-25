Read the entire prompt. Do not make any changes during this prompt. Provide a proper step-by-step plan for implenting the described changes.

I would like to add a feature I call "Berserk Gene Editing." When Pokemon are bred in the daycare, if either Pokemon is holding a berserk gene, properties of that Pokemon will carry over to the resulting Pokemon that hatches from the egg. This may be the Pokemon's primary type, secondary type, partial learnset of moves, ability, secondary ability, hidden ability, color, shininess, cry, size, ability, gender, sprite, hatch rate, IVs, egg groups, base stats, and nature. Additionally, any Pokemon in the daycare holding a berserk gene will be able to breed with any other Pokemon, regardless if they are the same gender, genderless, or a legendary.

All current rules for Pokemon breeding will be maintained. All new logic will only affect if one or more Pokemon in the daycare are holding a berserk gene. In all following scenarios, it is assumed that one or more of the Pokemon in the daycare are holding a berserk gene unless stated otherwise.

In all scenarios, the parent Pokemon stored in the daycare are not affected. This will only affect the traits of the child Pokemon produced.

This is very important to note. Be sure to highlight your thoughts on this in your suggested plan. Much of the information being dynamically determined for the produce Pokemon is hardcoded in the game's source code. The results of Berserk Gene Editing will, however, need to be stored in memory and in the save file. Determine a safe way to store this new set of information.

When one of the parent's are holding a berserk gene, the odds of their traits carrying to the child Pokemon is 68%. If both parents are holding a berserk gene, the odds of their traits carrying to the child Pokemon is 50%. Use these values for all trait-carrying checks.

If there is no female Pokemon in the daycare and there is no Ditto in the daycare, then both Pokemon must be holding a berserk gene to produce an egg. If one of the Pokemon in the daycare is genderless and is not Ditto, both Pokemon must be holding a berserk gene to produce an egg. If a genderless Pokemon is in the daycare with a Ditto, both Pokemon must be holding a berserk gene to produce an egg.

If one of the Pokemon in the daycare is a female (and one or both Pokemon are holding a berserk gene), the resulting Pokemon from the resulting egg will have the same sprite and name as the female parent. If there is no female Pokemon in the daycare and there is no Ditto in the daycare, then the name and sprite of the resulting Pokemon will be selected from one of the parents. If one of the Pokemon in the daycare is genderless and is not Ditto, then the name and sprite of the resulting Pokemon will be selected from one of the parents. If a genderless Pokemon is in the daycare with a Ditto, then the name and sprite of the resulting Pokemon will be selected from one of the parents. The name and sprite of the resulting child Pokemon will always be selected from the same parent.

If the child Pokemon is later bred in the daycare without holding a berserk gene, it's set traits should be respected. For example, if the child Pokemon produced from "Berserk Gene Editing" is a female, then is later bred in the daycare without it or the partner holding a berserk gene, then the resulting child should have the matching name, primary type, secondary , ability, secondary ability, learnset, etc. just like normal breeding would appear to have. The female child Pokemon of the "Berserk Gene Editing" might be an "Eevee," but it's type might be Steel/Poison! If that child Pokemon is later bred in the daycare in a traditional sense, the resulting Eevee should also be Steel/Poison.

For all checks on traits, use the percentage chance rules mentioned above.

First, check the resulting child Pokemon's primary type.

Second, determine the resulting child Pokemon's secondary type.

Third, determine the child Pokemon's learnset. This has aditional logic, where the learnsets of the parents should be merged. Try to use an even spread from each parent's moveset so that the resulting child Pokemon does not learn all of it's early level moves from one parent and all of it's later level moves from the other. If one parent is holding a berserk gene, merge ~68% of it's move pool with the ~38% of the other parent's move pool. If both parents are holding a berserk gene, merge ~50% of each parent's move pool as the result. Prioritize the moves that match the resulting primary or secondary type of the resulting child pokemon.

Fourth, determine the child's ability. You will use the above percentages to determine which parent the ability comes from for the resulting child Pokemon. As the child Pokemon can later be bred in the daycare, you will need to determine and store the potential abilities for this Pokemon as well. This includes it's ability, secondary ability, and hidden ability. Each one will require a check to determine which parent the result will come from.

Fifth, determine the resulting color palette for the child pokemon.

Sixth, determine if the child Pokemon is shiny. If one of the parent's are shiny, and they are selected by the percentage rules mentioned above, the resulting child will be shiny. If not, fallback to the standard method for determining child shininess.

Seventh, determine the cry for the resulting child Pokemon.

Eighth, determine the size for the resulting child Pokemon.

Ninth, determine the gender for the resulting child Pokemon. This might be male, female, or genderless.

Tenth, determine the egg groups for the resulting child Pokemon. If neither parent belongs to an egg group, do not add an egg group to the child. If one of the parent's belong to the Undiscovered egg group, do not add this to the child. All following scenarios assume that the Undiscovered agg group has been filtered from the resulting egg group set. If any of the parents belong to one or more egg group, the resulting child may have a set of one or more of the parent egg groups respecting the selection-percentages mentioned above.

Eleventh, determine the nature of the resulting Pokemon. If the parent selected is holding a beserk gene, then there is an additional 75% chance that the child will have the same nature as the selected parent. If that check fails, nature selection for the resulting child Pokemon will fallback to the standard determination.

Twelfth, determine the resulting base stats for the child Pokemon. A check will be performed for each stat (i.e. HP, Attack, Defense, etc.) using the selection-percentage rule mentioned above.

Finally, determine the final IVs for the resulting Pokemon. This will not use the percentage rules mentioned above. If either parent is holding a berserk gene, the rules for the child Pokemon's resulting IVs are as follows. Iterate through each IV (i.e. HP, Attack, Defense, etc.). Generate a random number between 0 and 31. If that value is higher than both parent's IV for that stat, use that value as the resulting value for the child Pokemon. If not, use the higher of the two values from the parent Pokemon for that stat.

