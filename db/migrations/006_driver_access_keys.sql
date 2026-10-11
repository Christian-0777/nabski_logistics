CREATE TABLE driver_access_keys (
    access_key_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    driver_id BIGINT UNSIGNED NOT NULL,
    key_hash CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    created_at_utc DATETIME(3) NOT NULL,
    revoked_at_utc DATETIME(3) NULL,
    PRIMARY KEY (access_key_id),
    UNIQUE KEY uq_driver_access_key_hash (key_hash),
    KEY ix_driver_access_key_owner_active (driver_id, revoked_at_utc),
    CONSTRAINT fk_driver_access_key_driver
        FOREIGN KEY (driver_id) REFERENCES driver_accounts (driver_id)
        ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
