ALTER TABLE driver_accounts
    MODIFY password_hash VARCHAR(255) NULL,
    MODIFY status ENUM('pending', 'active', 'suspended') NOT NULL DEFAULT 'pending';

CREATE TABLE admin_accounts (
    admin_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    username VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    email VARCHAR(254) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    status ENUM('pending', 'active', 'disabled') NOT NULL DEFAULT 'pending',
    created_at_utc DATETIME(3) NOT NULL,
    activated_at_utc DATETIME(3) NULL,
    last_login_at_utc DATETIME(3) NULL,
    PRIMARY KEY (admin_id),
    UNIQUE KEY uq_admin_username (username),
    UNIQUE KEY uq_admin_email (email),
    KEY ix_admin_status_created (status, created_at_utc)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE admin_setup_guard (
    guard_id TINYINT UNSIGNED NOT NULL,
    setup_complete TINYINT(1) NOT NULL DEFAULT 0,
    pending_admin_id BIGINT UNSIGNED NULL,
    PRIMARY KEY (guard_id),
    CONSTRAINT ck_admin_setup_guard_singleton CHECK (guard_id = 1),
    CONSTRAINT fk_admin_setup_pending FOREIGN KEY (pending_admin_id)
        REFERENCES admin_accounts (admin_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO admin_setup_guard (guard_id, setup_complete, pending_admin_id)
VALUES (1, 0, NULL);

CREATE TABLE admin_verification_challenges (
    challenge_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    admin_id BIGINT UNSIGNED NOT NULL,
    code_hash VARCHAR(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    expires_at_utc DATETIME(3) NOT NULL,
    attempts SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    sent_at_utc DATETIME(3) NOT NULL,
    consumed_at_utc DATETIME(3) NULL,
    revoked_at_utc DATETIME(3) NULL,
    PRIMARY KEY (challenge_id),
    KEY ix_admin_challenge_owner_expiry (admin_id, expires_at_utc),
    CONSTRAINT fk_admin_challenge_admin FOREIGN KEY (admin_id)
        REFERENCES admin_accounts (admin_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE driver_invitations (
    invitation_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    driver_id BIGINT UNSIGNED NOT NULL,
    code_hash VARCHAR(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    status ENUM('pending', 'used', 'revoked', 'expired', 'delivery_failed') NOT NULL DEFAULT 'pending',
    expires_at_utc DATETIME(3) NOT NULL,
    attempts SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    created_by_admin_id BIGINT UNSIGNED NOT NULL,
    created_at_utc DATETIME(3) NOT NULL,
    sent_at_utc DATETIME(3) NULL,
    used_at_utc DATETIME(3) NULL,
    revoked_at_utc DATETIME(3) NULL,
    PRIMARY KEY (invitation_id),
    KEY ix_driver_invite_email_lookup (driver_id, status, expires_at_utc),
    KEY ix_driver_invite_expiry (status, expires_at_utc),
    CONSTRAINT fk_driver_invite_driver FOREIGN KEY (driver_id)
        REFERENCES driver_accounts (driver_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    CONSTRAINT fk_driver_invite_admin FOREIGN KEY (created_by_admin_id)
        REFERENCES admin_accounts (admin_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE driver_login_codes (
    code_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    driver_id BIGINT UNSIGNED NOT NULL,
    code_hash VARCHAR(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    expires_at_utc DATETIME(3) NOT NULL,
    attempts SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    sent_at_utc DATETIME(3) NOT NULL,
    consumed_at_utc DATETIME(3) NULL,
    revoked_at_utc DATETIME(3) NULL,
    PRIMARY KEY (code_id),
    KEY ix_driver_login_code_owner_expiry (driver_id, expires_at_utc),
    CONSTRAINT fk_driver_login_code_driver FOREIGN KEY (driver_id)
        REFERENCES driver_accounts (driver_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE admin_audit_events (
    event_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    admin_id BIGINT UNSIGNED NULL,
    driver_id BIGINT UNSIGNED NULL,
    invitation_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
    action VARCHAR(48) NOT NULL,
    metadata_json JSON NOT NULL,
    created_at_utc DATETIME(3) NOT NULL,
    PRIMARY KEY (event_id),
    KEY ix_admin_audit_time (created_at_utc),
    KEY ix_admin_audit_driver_time (driver_id, created_at_utc),
    CONSTRAINT fk_admin_audit_admin FOREIGN KEY (admin_id)
        REFERENCES admin_accounts (admin_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    CONSTRAINT fk_admin_audit_driver FOREIGN KEY (driver_id)
        REFERENCES driver_accounts (driver_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    CONSTRAINT fk_admin_audit_invitation FOREIGN KEY (invitation_id)
        REFERENCES driver_invitations (invitation_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE web_auth_attempts (
    attempt_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    purpose VARCHAR(24) NOT NULL,
    identifier_hash CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    ip_hash CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    attempted_at_utc DATETIME(3) NOT NULL,
    PRIMARY KEY (attempt_id),
    KEY ix_web_auth_identifier_time (purpose, identifier_hash, attempted_at_utc),
    KEY ix_web_auth_ip_time (purpose, ip_hash, attempted_at_utc)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
