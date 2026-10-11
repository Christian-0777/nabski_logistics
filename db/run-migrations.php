<?php
declare(strict_types=1);

require __DIR__ . '/lib/Config.php';

use Nlsi\Db\Config;

if (PHP_SAPI !== 'cli') {
    http_response_code(404);
    exit;
}

try {
    $config = Config::loadForMigration();
    $pdo = Config::migrationPdo($config);
    $pdo->exec(
        'CREATE TABLE IF NOT EXISTS nlsi_schema_migrations (
            version VARCHAR(32) NOT NULL PRIMARY KEY,
            applied_at_utc DATETIME(3) NOT NULL
        ) ENGINE=InnoDB'
    );
    $files = glob(__DIR__ . '/migrations/[0-9][0-9][0-9]_*.sql');
    sort($files, SORT_STRING);
    foreach ($files as $file) {
        $version = basename($file, '.sql');
        $check = $pdo->prepare('SELECT 1 FROM nlsi_schema_migrations WHERE version = ?');
        $check->execute([$version]);
        if ($check->fetchColumn()) {
            echo "Already applied: $version\n";
            continue;
        }

        if ($version === '003_stable_job_ids') {
            $hasProviderJobId = (int)$pdo->query(
                "SELECT COUNT(*) FROM information_schema.columns
                 WHERE table_schema = DATABASE() AND table_name = 'provider_records'
                   AND column_name = 'job_id'"
            )->fetchColumn() === 1;
            $hasJobRecordJobId = (int)$pdo->query(
                "SELECT COUNT(*) FROM information_schema.columns
                 WHERE table_schema = DATABASE() AND table_name = 'job_records'
                   AND column_name = 'job_id'"
            )->fetchColumn() === 1;
            if ($hasProviderJobId && $hasJobRecordJobId) {
                $hasReceivedAt = (int)$pdo->query(
                    "SELECT COUNT(*) FROM information_schema.columns
                     WHERE table_schema = DATABASE() AND table_name = 'job_records'
                       AND column_name = 'received_at_utc'"
                )->fetchColumn() === 1;
                if (!$hasReceivedAt) {
                    throw new RuntimeException(
                        'Existing job_records is missing received_at_utc; inspect the database before migration.'
                    );
                }
                foreach ([
                    ['provider_records', 'ix_provider_record_job_time'],
                    ['job_records', 'ix_job_records_job_time'],
                ] as [$table, $index]) {
                    $indexCheck = $pdo->prepare(
                        'SELECT COUNT(*) FROM information_schema.statistics
                         WHERE table_schema = DATABASE() AND table_name = ? AND index_name = ?'
                    );
                    $indexCheck->execute([$table, $index]);
                    if ((int)$indexCheck->fetchColumn() !== 1) {
                        throw new RuntimeException(
                            "Existing $table.job_id has no expected index; inspect the database before migration."
                        );
                    }
                }
                foreach ([
                    ['provider_records', 'fk_provider_record_job'],
                    ['job_records', 'fk_job_records_job'],
                ] as [$table, $constraint]) {
                    $foreignKeyCheck = $pdo->prepare(
                        'SELECT COUNT(*) FROM information_schema.key_column_usage
                         WHERE table_schema = DATABASE() AND table_name = ?
                           AND constraint_name = ? AND referenced_table_name = \'jobs\''
                    );
                    $foreignKeyCheck->execute([$table, $constraint]);
                    if ((int)$foreignKeyCheck->fetchColumn() !== 1) {
                        throw new RuntimeException(
                            "Existing $table.job_id has no expected jobs foreign key; inspect the database before migration."
                        );
                    }
                }
                $jobsPresent = (int)$pdo->query(
                    "SELECT COUNT(*) FROM information_schema.tables
                     WHERE table_schema = DATABASE() AND table_name = 'jobs'"
                )->fetchColumn() === 1;
                if (!$jobsPresent) {
                    throw new RuntimeException(
                        'Existing job ID foreign keys have no jobs table; inspect the database before migration.'
                    );
                }
                foreach ([
                    'job_id',
                    'session_id',
                    'game_code',
                    'source_city',
                    'source_company',
                    'destination_city',
                    'destination_company',
                    'cargo_name',
                    'started_at_utc',
                    'completed_at_utc',
                    'status',
                    'job_details',
                    'created_at_utc',
                    'updated_at_utc',
                ] as $column) {
                    $jobColumnCheck = $pdo->prepare(
                        'SELECT COUNT(*) FROM information_schema.columns
                         WHERE table_schema = DATABASE() AND table_name = \'jobs\' AND column_name = ?'
                    );
                    $jobColumnCheck->execute([$column]);
                    if ((int)$jobColumnCheck->fetchColumn() !== 1) {
                        throw new RuntimeException(
                            "Existing jobs table is missing $column; inspect the database before migration."
                        );
                    }
                }
                echo "Existing stable job ID columns detected; preserving them and recording $version.\n";
                $insert = $pdo->prepare(
                    'INSERT INTO nlsi_schema_migrations (version, applied_at_utc) VALUES (?, UTC_TIMESTAMP(3))'
                );
                $insert->execute([$version]);
                continue;
            }
            if ($hasProviderJobId || $hasJobRecordJobId) {
                throw new RuntimeException(
                    'Partial stable job ID schema found; inspect the database before migration.'
                );
            }
            $jobsExists = (int)$pdo->query(
                "SELECT COUNT(*) FROM information_schema.tables
                 WHERE table_schema = DATABASE() AND table_name = 'jobs'"
            )->fetchColumn() === 1;
            if ($jobsExists) {
                foreach ([
                    'job_id',
                    'session_id',
                    'game_code',
                    'source_city',
                    'source_company',
                    'destination_city',
                    'destination_company',
                    'cargo_name',
                    'started_at_utc',
                    'completed_at_utc',
                    'status',
                    'job_details',
                    'created_at_utc',
                    'updated_at_utc',
                ] as $column) {
                    $jobColumnCheck = $pdo->prepare(
                        'SELECT COUNT(*) FROM information_schema.columns
                         WHERE table_schema = DATABASE() AND table_name = \'jobs\' AND column_name = ?'
                    );
                    $jobColumnCheck->execute([$column]);
                    if ((int)$jobColumnCheck->fetchColumn() !== 1) {
                        throw new RuntimeException(
                            "Existing jobs table is missing $column; inspect the database before migration."
                        );
                    }
                }
            }
        }

        if ($version === '001_telemetry_sync') {
            $requiredColumns = [
                'driving_sessions' => ['session_id', 'started_at_utc', 'ended_at_utc'],
                'telemetry_samples' => ['record_id', 'record_schema_version', 'raw_mapping'],
                'job_records' => ['record_id', 'event_type', 'details'],
                'sync_state' => ['record_type', 'record_id', 'state'],
            ];
            $existing = 0;
            foreach ($requiredColumns as $table => $columns) {
                $query = $pdo->prepare(
                    'SELECT COUNT(*) FROM information_schema.tables
                     WHERE table_schema = DATABASE() AND table_name = ?'
                );
                $query->execute([$table]);
                $tableExists = (int)$query->fetchColumn() === 1;
                if ($tableExists) {
                    $existing++;
                    foreach ($columns as $column) {
                        $columnQuery = $pdo->prepare(
                            'SELECT COUNT(*) FROM information_schema.columns
                             WHERE table_schema = DATABASE() AND table_name = ? AND column_name = ?'
                        );
                        $columnQuery->execute([$table, $column]);
                        if ((int)$columnQuery->fetchColumn() !== 1) {
                            throw new RuntimeException("Existing $table table is missing a required column.");
                        }
                    }
                }
            }
            if ($existing === 4) {
                foreach ([
                    ['driving_sessions', 'ix_driving_sessions_started'],
                    ['telemetry_samples', 'uq_telemetry_session_sequence'],
                    ['telemetry_samples', 'ix_telemetry_captured'],
                    ['telemetry_samples', 'ix_telemetry_session_time'],
                    ['job_records', 'ix_job_session_time'],
                    ['job_records', 'ix_job_identity'],
                    ['sync_state', 'ix_sync_due'],
                    ['sync_state', 'ix_sync_updated'],
                ] as [$table, $index]) {
                    $indexCheck = $pdo->prepare(
                        'SELECT COUNT(*) FROM information_schema.statistics
                         WHERE table_schema = DATABASE() AND table_name = ? AND index_name = ?'
                    );
                    $indexCheck->execute([$table, $index]);
                    if ((int)$indexCheck->fetchColumn() !== 1) {
                        throw new RuntimeException(
                            "Existing $table schema is missing $index; inspect the database before migration."
                        );
                    }
                }
                foreach ([
                    ['telemetry_samples', 'fk_telemetry_session'],
                    ['job_records', 'fk_job_session'],
                ] as [$table, $constraint]) {
                    $foreignKeyCheck = $pdo->prepare(
                        'SELECT COUNT(*) FROM information_schema.key_column_usage
                         WHERE table_schema = DATABASE() AND table_name = ?
                           AND constraint_name = ? AND referenced_table_name = \'driving_sessions\''
                    );
                    $foreignKeyCheck->execute([$table, $constraint]);
                    if ((int)$foreignKeyCheck->fetchColumn() !== 1) {
                        throw new RuntimeException(
                            "Existing $table schema is missing $constraint; inspect the database before migration."
                        );
                    }
                }
                echo "Existing telemetry schema detected; preserving it and recording $version.\n";
                $insert = $pdo->prepare(
                    'INSERT INTO nlsi_schema_migrations (version, applied_at_utc) VALUES (?, UTC_TIMESTAMP(3))'
                );
                $insert->execute([$version]);
                continue;
            }
            if ($existing !== 0) {
                throw new RuntimeException('Partial base schema found; inspect the database before migration.');
            }
        }

        if ($version === '002_preserve_native_records') {
            $providerTableExists = (int)$pdo->query(
                "SELECT COUNT(*) FROM information_schema.tables
                 WHERE table_schema = DATABASE() AND table_name = 'provider_records'"
            )->fetchColumn() === 1;
            if ($providerTableExists) {
                foreach ([
                    'record_id',
                    'record_type',
                    'record_schema_version',
                    'sequence_no',
                    'session_id',
                    'captured_at_utc',
                    'provider_name',
                    'payload_json',
                    'payload_hash',
                    'received_at_utc',
                ] as $column) {
                    $columnCheck = $pdo->prepare(
                        'SELECT COUNT(*) FROM information_schema.columns
                         WHERE table_schema = DATABASE()
                           AND table_name = \'provider_records\' AND column_name = ?'
                    );
                    $columnCheck->execute([$column]);
                    if ((int)$columnCheck->fetchColumn() !== 1) {
                        throw new RuntimeException(
                            "Existing provider_records table is missing $column; inspect the database before migration."
                        );
                    }
                }
                foreach ([
                    'PRIMARY',
                    'uq_provider_record_type_sequence',
                    'ix_provider_record_time',
                    'ix_provider_record_session_time',
                ] as $index) {
                    $indexCheck = $pdo->prepare(
                        'SELECT COUNT(*) FROM information_schema.statistics
                         WHERE table_schema = DATABASE()
                           AND table_name = \'provider_records\' AND index_name = ?'
                    );
                    $indexCheck->execute([$index]);
                    if ((int)$indexCheck->fetchColumn() !== 1) {
                        throw new RuntimeException(
                            "Existing provider_records table is missing $index; inspect the database before migration."
                        );
                    }
                }
                $foreignKeyCheck = $pdo->prepare(
                    'SELECT COUNT(*) FROM information_schema.key_column_usage
                     WHERE table_schema = DATABASE() AND table_name = \'provider_records\'
                       AND constraint_name = \'fk_provider_record_session\'
                       AND referenced_table_name = \'driving_sessions\''
                );
                $foreignKeyCheck->execute();
                if ((int)$foreignKeyCheck->fetchColumn() !== 1) {
                    throw new RuntimeException(
                        'Existing provider_records table is missing its session foreign key; inspect the database before migration.'
                    );
                }
            }
        }

        $sql = preg_replace('/^\s*--.*$/m', '', (string)file_get_contents($file));
        foreach (explode(';', (string)$sql) as $statement) {
            if (trim($statement) !== '') {
                $pdo->exec($statement);
            }
        }
        $insert = $pdo->prepare(
            'INSERT INTO nlsi_schema_migrations (version, applied_at_utc) VALUES (?, UTC_TIMESTAMP(3))'
        );
        $insert->execute([$version]);
        echo "Applied: $version\n";
    }
} catch (Throwable $error) {
    fwrite(STDERR, 'Migration failed: ' . $error->getMessage() . PHP_EOL);
    exit(1);
}
