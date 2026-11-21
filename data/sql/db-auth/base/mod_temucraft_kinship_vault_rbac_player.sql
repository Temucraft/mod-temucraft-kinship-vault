-- mod-temucraft-kinship-vault: its commands (all under .temucraft) as RBAC permissions 102550-102599. Every command is admin only by
-- default: linked to the administrator roles 192 and 1014. To give one to players, link it to 195 and 1010 here (or
-- grant it per account with .rbac account grant <account> <id>). "Temucraft: use ..." permissions are not
-- commands: they stay linked to the player roles so the items keep working.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 102550 AND 102599;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 102550 AND 102599;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(102550, 'Command: temucraft vault');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(192, 102550),
(1014, 102550);
