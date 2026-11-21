-- mod-temucraft-kinship-vault: what each account keeps in its vault, and how many tabs it has bought
CREATE TABLE IF NOT EXISTS `mod_temucraft_kinship_vault` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `account` INT UNSIGNED NOT NULL,
  `entry` INT UNSIGNED NOT NULL,
  `count` INT UNSIGNED NOT NULL DEFAULT 1,
  `random_property` INT NOT NULL DEFAULT 0,
  `durability` INT UNSIGNED NOT NULL DEFAULT 0,
  `deposited_by` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'character guid',
  `deposited_at` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `account` (`account`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `mod_temucraft_kinship_vault_account` (
  `account` INT UNSIGNED NOT NULL,
  `tabs` INT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`account`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
