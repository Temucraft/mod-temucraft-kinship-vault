-- mod-temucraft-kinship-vault: its player commands (all under .temucraft) as RBAC permissions 102550-102599. Linked to the stock player
-- role 195 and the Temucraft player role 1010, so every account has them by default. To take one away, remove its
-- link here (or deny it per account with .rbac account deny <account> <id>); to give it to staff only, link it to
-- 1011-1014 instead.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 102550 AND 102599;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 102550 AND 102599;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(102550, 'Command: temucraft vault');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(195, 102550),
(1010, 102550);
