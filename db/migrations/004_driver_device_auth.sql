-- Multiple devices can generate identical local sequence numbers. Stable record
-- UUIDs and canonical hashes provide idempotency; sequence numbers are not global.
ALTER TABLE provider_records
    DROP INDEX uq_provider_record_type_sequence,
    ADD KEY ix_provider_record_type_sequence (record_type, sequence_no);

CREATE TABLE driver_accounts (
    driver_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    username VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    email VARCHAR(254) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    display_name VARCHAR(100) NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    status ENUM('active', 'suspended') NOT NULL DEFAULT 'active',
    created_at_utc DATETIME(3) NOT NULL,
    updated_at_utc DATETIME(3) NOT NULL,
    last_login_at_utc DATETIME(3) NULL,
    PRIMARY KEY (driver_id),
    UNIQUE KEY uq_driver_username (username),
    UNIQUE KEY uq_driver_email (email),
    KEY ix_driver_status_created (status, created_at_utc)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE driver_devices (
    device_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    driver_id BIGINT UNSIGNED NOT NULL,
    device_name VARCHAR(80) NOT NULL,
    key_hash CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    status ENUM('active', 'revoked') NOT NULL DEFAULT 'active',
    created_at_utc DATETIME(3) NOT NULL,
    last_used_at_utc DATETIME(3) NULL,
    revoked_at_utc DATETIME(3) NULL,
    replaced_by_device_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
    PRIMARY KEY (device_id),
    UNIQUE KEY uq_driver_device_key_hash (key_hash),
    KEY ix_driver_devices_owner_status (driver_id, status, created_at_utc),
    CONSTRAINT fk_driver_devices_driver
        FOREIGN KEY (driver_id) REFERENCES driver_accounts (driver_id)
        ON DELETE RESTRICT ON UPDATE RESTRICT,
    CONSTRAINT fk_driver_devices_replacement
        FOREIGN KEY (replaced_by_device_id) REFERENCES driver_devices (device_id)
        ON DELETE RESTRICT ON UPDATE RESTRICT,
    CONSTRAINT ck_driver_device_revocation
        CHECK ((status = 'active' AND revoked_at_utc IS NULL)
            OR (status = 'revoked' AND revoked_at_utc IS NOT NULL))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE driver_device_audit (
    audit_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    driver_id BIGINT UNSIGNED NOT NULL,
    device_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
    action ENUM('provisioned', 'registered', 'revoked', 'replaced', 'auth_denied') NOT NULL,
    actor ENUM('driver', 'system') NOT NULL,
    metadata_json JSON NOT NULL,
    created_at_utc DATETIME(3) NOT NULL,
    PRIMARY KEY (audit_id),
    KEY ix_device_audit_driver_time (driver_id, created_at_utc),
    KEY ix_device_audit_device_time (device_id, created_at_utc),
    CONSTRAINT fk_device_audit_driver
        FOREIGN KEY (driver_id) REFERENCES driver_accounts (driver_id)
        ON DELETE RESTRICT ON UPDATE RESTRICT,
    CONSTRAINT fk_device_audit_device
        FOREIGN KEY (device_id) REFERENCES driver_devices (device_id)
        ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE driver_login_attempts (
    attempt_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    identifier_hash CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    ip_hash CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    attempted_at_utc DATETIME(3) NOT NULL,
    PRIMARY KEY (attempt_id),
    KEY ix_login_attempts_identifier_time (identifier_hash, attempted_at_utc),
    KEY ix_login_attempts_ip_time (ip_hash, attempted_at_utc)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE api_auth_attempts (
    attempt_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    ip_hash CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    attempted_at_utc DATETIME(3) NOT NULL,
    PRIMARY KEY (attempt_id),
    KEY ix_api_auth_attempts_ip_time (ip_hash, attempted_at_utc)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

ALTER TABLE provider_records
    ADD COLUMN driver_id BIGINT UNSIGNED NULL AFTER record_id,
    ADD KEY ix_provider_records_driver_time (driver_id, captured_at_utc),
    ADD CONSTRAINT fk_provider_records_driver
        FOREIGN KEY (driver_id) REFERENCES driver_accounts (driver_id)
        ON DELETE RESTRICT ON UPDATE RESTRICT;
