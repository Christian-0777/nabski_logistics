-- Add stable, provider-supplied job IDs while preserving legacy history.
CREATE TABLE IF NOT EXISTS jobs (
    job_id VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    session_id VARCHAR(64) NULL,
    game_code VARCHAR(16) NULL,
    source_city VARCHAR(128) NULL,
    source_company VARCHAR(128) NULL,
    destination_city VARCHAR(128) NULL,
    destination_company VARCHAR(128) NULL,
    cargo_name VARCHAR(255) NULL,
    started_at_utc DATETIME(3) NULL,
    completed_at_utc DATETIME(3) NULL,
    status ENUM('active', 'delivered', 'cancelled', 'unknown') NOT NULL DEFAULT 'unknown',
    job_details JSON NOT NULL,
    created_at_utc DATETIME(3) NOT NULL,
    updated_at_utc DATETIME(3) NOT NULL,
    PRIMARY KEY (job_id),
    KEY ix_jobs_session (session_id),
    KEY ix_jobs_completed (completed_at_utc),
    KEY ix_jobs_status_completed (status, completed_at_utc),
    CONSTRAINT fk_jobs_session
        FOREIGN KEY (session_id) REFERENCES driving_sessions (session_id)
        ON DELETE RESTRICT ON UPDATE RESTRICT,
    CONSTRAINT ck_jobs_time
        CHECK (completed_at_utc IS NULL OR started_at_utc IS NULL OR completed_at_utc >= started_at_utc)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

ALTER TABLE provider_records
    ADD COLUMN job_id VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL AFTER session_id,
    ADD KEY ix_provider_record_job_time (job_id, captured_at_utc);

ALTER TABLE job_records
    ADD COLUMN job_id VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL AFTER session_id,
    ADD COLUMN received_at_utc DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    ADD KEY ix_job_records_job_time (job_id, event_at_utc);

UPDATE job_records
SET job_id = job_identity
WHERE job_identity IS NOT NULL
  AND CHAR_LENGTH(TRIM(job_identity)) BETWEEN 1 AND 128
  AND TRIM(job_identity) REGEXP '^[A-Za-z0-9._:-]+$';

UPDATE provider_records
SET job_id = CASE
    WHEN JSON_TYPE(JSON_EXTRACT(payload_json, '$.job_id')) = 'STRING'
        AND CHAR_LENGTH(TRIM(JSON_UNQUOTE(JSON_EXTRACT(payload_json, '$.job_id')))) BETWEEN 1 AND 128
        AND TRIM(JSON_UNQUOTE(JSON_EXTRACT(payload_json, '$.job_id'))) REGEXP '^[A-Za-z0-9._:-]+$'
        THEN TRIM(JSON_UNQUOTE(JSON_EXTRACT(payload_json, '$.job_id')))
    WHEN JSON_TYPE(JSON_EXTRACT(payload_json, '$.data.job_id')) = 'STRING'
        AND CHAR_LENGTH(TRIM(JSON_UNQUOTE(JSON_EXTRACT(payload_json, '$.data.job_id')))) BETWEEN 1 AND 128
        AND TRIM(JSON_UNQUOTE(JSON_EXTRACT(payload_json, '$.data.job_id'))) REGEXP '^[A-Za-z0-9._:-]+$'
        THEN TRIM(JSON_UNQUOTE(JSON_EXTRACT(payload_json, '$.data.job_id')))
    ELSE NULL
END
WHERE JSON_TYPE(JSON_EXTRACT(payload_json, '$.job_id')) = 'STRING'
   OR JSON_TYPE(JSON_EXTRACT(payload_json, '$.data.job_id')) = 'STRING';

-- Existing records seed only a neutral summary with the original record payload;
-- no start/completion time or provider-supplied field is invented.
INSERT INTO jobs (
    job_id, status, job_details, created_at_utc, updated_at_utc
)
SELECT DISTINCT
    records.job_id,
    'unknown',
    (
        SELECT source.payload_json
        FROM provider_records AS source
        WHERE source.job_id = records.job_id
        ORDER BY source.sequence_no, source.record_id
        LIMIT 1
    ),
    UTC_TIMESTAMP(3),
    UTC_TIMESTAMP(3)
FROM provider_records AS records
WHERE records.job_id IS NOT NULL
ON DUPLICATE KEY UPDATE job_id = VALUES(job_id);

ALTER TABLE provider_records
    ADD CONSTRAINT fk_provider_record_job
        FOREIGN KEY (job_id) REFERENCES jobs (job_id)
        ON DELETE RESTRICT ON UPDATE RESTRICT;

ALTER TABLE job_records
    ADD CONSTRAINT fk_job_records_job
        FOREIGN KEY (job_id) REFERENCES jobs (job_id)
        ON DELETE RESTRICT ON UPDATE RESTRICT;
