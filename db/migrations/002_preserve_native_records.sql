-- Preserve complete schema-v2 native records, including provider events,
-- provider configuration, and NLSI samples that do not fit the legacy sample
-- column requirements. This migration only adds storage and enum values.
ALTER TABLE sync_state
    MODIFY record_type ENUM(
        'telemetry_sample',
        'job_record',
        'provider_event',
        'provider_configuration'
    ) NOT NULL;

CREATE TABLE IF NOT EXISTS provider_records (
    record_id CHAR(36) NOT NULL,
    record_type ENUM('telemetry_sample', 'provider_event', 'provider_configuration') NOT NULL,
    record_schema_version TINYINT UNSIGNED NOT NULL,
    sequence_no BIGINT UNSIGNED NOT NULL,
    session_id VARCHAR(64) NULL,
    captured_at_utc DATETIME(3) NOT NULL,
    provider_name VARCHAR(64) NOT NULL,
    payload_json JSON NOT NULL,
    payload_hash CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    received_at_utc DATETIME(3) NOT NULL,
    PRIMARY KEY (record_id),
    UNIQUE KEY uq_provider_record_type_sequence (record_type, sequence_no),
    KEY ix_provider_record_time (captured_at_utc),
    KEY ix_provider_record_session_time (session_id, captured_at_utc),
    CONSTRAINT fk_provider_record_session
        FOREIGN KEY (session_id) REFERENCES driving_sessions (session_id)
        ON DELETE RESTRICT ON UPDATE RESTRICT,
    CONSTRAINT ck_provider_record_schema CHECK (record_schema_version = 2)
) ENGINE=InnoDB;
