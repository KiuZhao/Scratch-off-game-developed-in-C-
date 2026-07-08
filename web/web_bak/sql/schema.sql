-- ============================================================
-- 刮刮乐在线版 — 数据库初始化脚本 v2
-- ============================================================

CREATE DATABASE IF NOT EXISTS guaguale
    DEFAULT CHARACTER SET utf8mb4 DEFAULT COLLATE utf8mb4_unicode_ci;
USE guaguale;

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    username      VARCHAR(32)  NOT NULL UNIQUE,
    password_hash CHAR(64)     NOT NULL COMMENT 'SHA-256 hex',
    balance       BIGINT       NOT NULL DEFAULT 1000000,
    total_spent   BIGINT       NOT NULL DEFAULT 0,
    total_prizes  BIGINT       NOT NULL DEFAULT 0,
    profit        BIGINT       NOT NULL DEFAULT 0,
    souvenir_1    INT UNSIGNED NOT NULL DEFAULT 0,
    souvenir_2    INT UNSIGNED NOT NULL DEFAULT 0,
    souvenir_3    INT UNSIGNED NOT NULL DEFAULT 0,
    total_bought  INT UNSIGNED NOT NULL DEFAULT 0,
    total_won     INT UNSIGNED NOT NULL DEFAULT 0,
    cheat_money   BIGINT       NOT NULL DEFAULT 0,
    created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_profit (profit)
) ENGINE=InnoDB;

-- 购买记录表
CREATE TABLE IF NOT EXISTS cards (
    id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    user_id     BIGINT UNSIGNED NOT NULL,
    card_type   TINYINT UNSIGNED NOT NULL COMMENT '0=1万,1=2万,2=5万',
    cost        INT    UNSIGNED NOT NULL,
    positions   TINYINT UNSIGNED NOT NULL,
    total_prize INT    UNSIGNED NOT NULL DEFAULT 0,
    created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_user (user_id)
) ENGINE=InnoDB;

-- 每格明细
CREATE TABLE IF NOT EXISTS card_cells (
    id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    card_id       BIGINT UNSIGNED NOT NULL,
    position      TINYINT UNSIGNED NOT NULL,
    is_win        TINYINT(1) NOT NULL DEFAULT 0,
    prize         INT UNSIGNED NOT NULL DEFAULT 0,
    display_prize INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '前端展示金额(未中奖格也有)',
    FOREIGN KEY (card_id) REFERENCES cards(id) ON DELETE CASCADE,
    INDEX idx_card (card_id)
) ENGINE=InnoDB;
