-- mod-temucraft-kinship-vault: the Kinship Vault Key (one more tab for the account), sold by the Temucraft Curator's
-- Gameplay list for 200 tokens (ItemExtendedCost 3210 from mod-temucraft-shop), and the player texts.
SET @KEY := 610104; SET @GAMEPLAY := 608003; SET @PRICE := 3210;
DELETE FROM `item_template` WHERE `entry` = @KEY;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `ItemLevel`, `maxcount`, `stackable`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `bonding`, `description`, `Material`, `ScriptName`) VALUES
(@KEY, 0, 8, -1, 'Kinship Vault Key', 3745, 3, 0, 1, 0, 0, 0, 1, 0, 20, 18282, 0, -1, 1, 'Adds a tab to the Kinship Vault of your whole account. Open the vault with .vault next to a banker.', 4, 'item_temucraft_kinship_key');
DELETE FROM `npc_vendor` WHERE `entry` = @GAMEPLAY AND `item` = @KEY;
INSERT INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `VerifiedBuild`) VALUES (@GAMEPLAY, 100, @KEY, 0, 0, @PRICE, 0);

DELETE FROM `module_string` WHERE `module` = 'mod-temucraft-kinship-vault';
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-temucraft-kinship-vault', 1, 'Put an item in'),
('mod-temucraft-kinship-vault', 2, 'Take an item out'),
('mod-temucraft-kinship-vault', 3, '{} of {} places used'),
('mod-temucraft-kinship-vault', 4, 'Close'),
('mod-temucraft-kinship-vault', 5, 'Nothing in your bags can go in the vault.'),
('mod-temucraft-kinship-vault', 6, 'More...'),
('mod-temucraft-kinship-vault', 7, 'Back'),
('mod-temucraft-kinship-vault', 8, 'The vault is empty.'),
('mod-temucraft-kinship-vault', 9, 'The vault is full. A Kinship Vault Key adds a tab.'),
('mod-temucraft-kinship-vault', 10, '|cff66ccffKinship Vault:|r put in {} x{}.'),
('mod-temucraft-kinship-vault', 11, 'No room in your bags (or you can carry only one of these).'),
('mod-temucraft-kinship-vault', 12, '|cff66ccffKinship Vault:|r took out {} x{}.'),
('mod-temucraft-kinship-vault', 13, 'Your Kinship Vault already has every tab.'),
('mod-temucraft-kinship-vault', 14, '|cff66ccffKinship Vault:|r now {} tabs ({} places).'),
('mod-temucraft-kinship-vault', 15, 'Stand next to a banker (or your Pocket Courier) to open the Kinship Vault.');
