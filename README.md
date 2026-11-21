# mod-temucraft-kinship-vault

The **Kinship Vault**: an account-wide stash. Put an item in on one character and take it out on another of the same
account. Type `.vault` next to a banker (or anything with the banker flag, such as the Pocket Courier from
mod-temucraft-courier). A gossip menu lists what can go in from your bags and what the vault holds, 20 per page.

- **What can go in:** unbound or bound-to-account items with no enchantments, gems, wrapping or written text, not
  quest items, not conjured, not timed, not a bag with something in it, and with their charges untouched. The vault
  keeps the item, the count, its random property and its durability, so what comes out is the same item.
- **Space:** one tab of 28 places for every account. A **Kinship Vault Key** (item 610104, 200 tokens on the Temucraft
  Curator's Gameplay list) adds a tab, up to 5. All numbers are in the conf file.
- **Safe moves:** taking an item in or out saves the character's bags and the vault row in one transaction, so a crash
  can't copy or lose an item. A row is read again before it is taken out.

Install: put the folder in `modules/`, re-run CMake, build, copy the conf file. The characters SQL makes the tables, the
world SQL adds the key, its vendor row and the texts.

Not tested in game yet.
