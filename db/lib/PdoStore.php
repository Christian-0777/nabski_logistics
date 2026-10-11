<?php
declare(strict_types=1);

namespace Nlsi\Db;

final class PdoStore implements IngestionStore
{
    public function __construct(
        private \PDO $pdo,
        private ?int $driverId = null
    ) {
    }

    public function pdo(): \PDO
    {
        return $this->pdo;
    }

    public static function existingSessionId(\PDO $pdo, mixed $sessionId): ?string
    {
        if (!is_string($sessionId) || $sessionId === '') {
            return null;
        }
        $query = $pdo->prepare('SELECT session_id FROM driving_sessions WHERE session_id = :session_id');
        $query->execute(['session_id' => $sessionId]);
        $existing = $query->fetchColumn();
        return $existing === false ? null : (string)$existing;
    }

    public function begin(): void
    {
        $this->pdo->beginTransaction();
    }

    public function findRecord(string $type, string $id): ?string
    {
        $query = $this->pdo->prepare(
            'SELECT record_type, driver_id, payload_hash FROM provider_records
             WHERE record_id = :id LIMIT 1'
        );
        $query->execute(['id' => $id]);
        $existing = $query->fetch(\PDO::FETCH_ASSOC);
        if (!is_array($existing)) {
            return null;
        }
        if ((int)$existing['driver_id'] !== $this->driverId
            || $existing['record_type'] !== $type) {
            throw new \DomainException('Record identifier is already owned by another record.');
        }
        return (string)$existing['payload_hash'];
    }

    public function saveRecord(array $record, array $validated): void
    {
        if ($this->driverId === null) {
            throw new \LogicException('An authenticated driver is required to store records.');
        }
        $type = $validated['record_type'];
        $id = $validated['record_id'];
        $timestamp = $validated['timestamp'];
        $sessionId = self::existingSessionId($this->pdo, $record['session_id'] ?? null);
        $jobId = RecordValidator::jobId($record);
        $provider = $record['provider'];
        $sequence = $record['sequence'];
        $schemaVersion = $record['schema_version'];

        if ($jobId !== null) {
            $this->saveJobSummary($record, $jobId, $sessionId, $timestamp);
        }

        $recordInsert = $this->pdo->prepare(
            'INSERT INTO provider_records
             (record_id, driver_id, record_type, record_schema_version, sequence_no, session_id, job_id,
              captured_at_utc, provider_name, payload_json, payload_hash, received_at_utc)
             VALUES (:id, :driver_id, :type, :version, :sequence, :session_id, :job_id,
              :captured, :provider, :payload, :hash, UTC_TIMESTAMP(3))'
        );
        $recordInsert->execute([
            'id' => $id,
            'driver_id' => $this->driverId,
            'type' => $type,
            'version' => $schemaVersion,
            'sequence' => $sequence,
            'session_id' => $sessionId,
            'job_id' => $jobId,
            'captured' => $timestamp,
            'provider' => $provider,
            'payload' => $validated['json'],
            'hash' => $validated['fingerprint'],
        ]);

        if ($type === 'telemetry_sample' && $provider === 'TruckSim GPS') {
            $compressed = base64_decode($record['raw_mapping_base64'], true);
            $insert = $this->pdo->prepare(
                'INSERT INTO telemetry_samples
                 (record_id, record_schema_version, session_id, sequence_no, captured_at_utc,
                  provider_name, provider_revision, source_timestamps, raw_fields, raw_availability,
                  raw_mapping, normalized_fields, received_at_utc)
                 VALUES (:id, :version, :session_id, :sequence, :captured, :provider, :revision,
                  :source_timestamps, :raw_fields, :raw_availability, :raw_mapping,
                  :normalized_fields, UTC_TIMESTAMP(3))'
            );
            $insert->bindValue(':id', $id);
            $insert->bindValue(':version', $schemaVersion, \PDO::PARAM_INT);
            $insert->bindValue(':session_id', $sessionId);
            $insert->bindValue(':sequence', (string)$sequence);
            $insert->bindValue(':captured', $timestamp);
            $insert->bindValue(':provider', $provider);
            $insert->bindValue(':revision', $record['provider_revision'], \PDO::PARAM_INT);
            $insert->bindValue(':source_timestamps', json_encode($record['source_timestamps'], JSON_THROW_ON_ERROR));
            $insert->bindValue(':raw_fields', json_encode($record['raw_fields'], JSON_THROW_ON_ERROR));
            $insert->bindValue(':raw_availability', json_encode($record['raw_availability'], JSON_THROW_ON_ERROR));
            $insert->bindValue(':raw_mapping', gzuncompress(substr($compressed, 4)), \PDO::PARAM_LOB);
            $insert->bindValue(':normalized_fields', json_encode($record['normalized_fields'], JSON_THROW_ON_ERROR));
            $insert->execute();
        }

        if ($type === 'provider_event' && in_array($record['event'], [
            'job.started', 'job.delivered', 'job.cancelled',
        ], true) && $jobId !== null) {
            $details = $record['data'];
            $job = $this->pdo->prepare(
                'INSERT INTO job_records
                 (record_id, session_id, job_id, event_type, event_at_utc, details)
                 VALUES (:id, :session_id, :job_id, :event_type, :event_at, :details)'
            );
            $job->execute([
                'id' => $id,
                'session_id' => $sessionId,
                'job_id' => $jobId,
                'event_type' => $record['event'],
                'event_at' => $timestamp,
                'details' => json_encode($details, JSON_THROW_ON_ERROR),
            ]);
        }

        $sync = $this->pdo->prepare(
            'INSERT INTO sync_state (record_type, record_id, state, attempt_count,
             acknowledged_at_utc, updated_at_utc)
             VALUES (:type, :id, \'synced\', 0, UTC_TIMESTAMP(3), UTC_TIMESTAMP(3))'
        );
        $sync->execute(['type' => $type, 'id' => $id]);
    }

    public function commit(): void
    {
        $this->pdo->commit();
    }

    public function rollback(): void
    {
        if ($this->pdo->inTransaction()) {
            $this->pdo->rollBack();
        }
    }

    public function heartbeat(): array
    {
        $this->pdo->query('SELECT 1')->fetchColumn();
        return self::heartbeatResponse(gmdate('Y-m-d\TH:i:s\Z'));
    }

    public static function heartbeatResponse(string $checkedAt): array
    {
        return [
            'success' => true,
            'database' => ['status' => 'connected'],
            'checked_at' => $checkedAt,
        ];
    }

    public function syncStatus(array $ids): array
    {
        if ($ids === [] || $this->driverId === null) {
            return [];
        }
        $holders = implode(',', array_fill(0, count($ids), '?'));
        $query = $this->pdo->prepare(
            "SELECT s.record_type, s.record_id, s.state, s.attempt_count, s.acknowledged_at_utc,
                    s.last_error_code, s.updated_at_utc
             FROM sync_state s
             INNER JOIN provider_records p ON p.record_id = s.record_id
             WHERE p.driver_id = ? AND s.record_id IN ($holders)"
        );
        $query->execute(array_merge([$this->driverId], $ids));
        return $query->fetchAll();
    }

    public function history(int $limit, int $offset, ?string $jobId = null): array
    {
        $jobFilter = $jobId === null
            ? ''
            : ($this->pdo->getAttribute(\PDO::ATTR_DRIVER_NAME) === 'mysql'
                ? ' WHERE CAST(r.job_id AS BINARY) = CAST(:job_id AS BINARY)'
                : ' WHERE r.job_id = :job_id COLLATE BINARY');
        $ordering = $jobId === null
            ? 'r.captured_at_utc DESC, r.sequence_no DESC'
            : 'r.sequence_no ASC';
        $query = $this->pdo->prepare(
            'SELECT r.record_id, r.record_type, r.record_schema_version, r.sequence_no,
                    r.session_id, r.job_id, r.captured_at_utc, r.provider_name, r.payload_json,
                    s.state AS sync_state, s.last_error_code
             FROM provider_records r
             LEFT JOIN sync_state s ON s.record_id = r.record_id AND s.record_type = r.record_type
             ' . $jobFilter . '
             ORDER BY ' . $ordering . ' LIMIT :limit OFFSET :offset'
        );
        if ($jobId !== null) {
            $query->bindValue(':job_id', $jobId);
        }
        $query->bindValue(':limit', $limit, \PDO::PARAM_INT);
        $query->bindValue(':offset', $offset, \PDO::PARAM_INT);
        $query->execute();
        return $query->fetchAll();
    }

    public function diagnostics(): array
    {
        $result = ['database' => 'connected', 'counts' => [], 'recent_errors' => []];
        foreach (['driving_sessions', 'jobs', 'telemetry_samples', 'provider_records', 'job_records'] as $table) {
            $result['counts'][$table] = (int)$this->pdo->query("SELECT COUNT(*) FROM `$table`")->fetchColumn();
        }
        $query = $this->pdo->query(
            "SELECT record_type, record_id, last_error_code, updated_at_utc
             FROM sync_state WHERE state = 'error' ORDER BY updated_at_utc DESC LIMIT 20"
        );
        $result['recent_errors'] = $query->fetchAll();
        return $result;
    }

    private function saveJobSummary(
        array $record,
        string $jobId,
        ?string $sessionId,
        string $timestamp
    ): void {
        $eventType = $record['event'] ?? null;
        $details = $record['data'] ?? $record;
        $detailsJson = json_encode($details, JSON_THROW_ON_ERROR);
        $gameCode = $this->jobDetailText($details, ['game_code', 'game_id'], 16);
        $sourceCity = $this->jobDetailText($details, ['source_city', 'origin_city'], 128);
        $sourceCompany = $this->jobDetailText($details, ['source_company', 'origin_company'], 128);
        $destinationCity = $this->jobDetailText(
            $details,
            ['destination_city', 'target_city'],
            128
        );
        $destinationCompany = $this->jobDetailText(
            $details,
            ['destination_company', 'target_company'],
            128
        );
        $cargoName = $this->jobDetailText($details, ['cargo_name', 'cargo'], 255);
        $status = match ($eventType) {
            'job.started' => 'active',
            'job.delivered' => 'delivered',
            'job.cancelled' => 'cancelled',
            default => 'unknown',
        };
        $startedAt = $eventType === 'job.started' ? $timestamp : null;
        $completedAt = in_array($eventType, ['job.delivered', 'job.cancelled'], true)
            ? $timestamp
            : null;
        $query = $this->pdo->prepare(
            'INSERT INTO jobs
             (job_id, session_id, game_code, source_city, source_company,
              destination_city, destination_company, cargo_name, started_at_utc,
              completed_at_utc, status, job_details, created_at_utc, updated_at_utc)
             VALUES (:job_id, :session_id, :game_code, :source_city, :source_company,
                     :destination_city, :destination_company, :cargo_name, :started_at,
                     :completed_at, :status, :job_details, UTC_TIMESTAMP(3), UTC_TIMESTAMP(3))
             ON DUPLICATE KEY UPDATE
                session_id = COALESCE(jobs.session_id, VALUES(session_id)),
                game_code = COALESCE(jobs.game_code, VALUES(game_code)),
                source_city = COALESCE(jobs.source_city, VALUES(source_city)),
                source_company = COALESCE(jobs.source_company, VALUES(source_company)),
                destination_city = COALESCE(jobs.destination_city, VALUES(destination_city)),
                destination_company = COALESCE(jobs.destination_company, VALUES(destination_company)),
                cargo_name = COALESCE(jobs.cargo_name, VALUES(cargo_name)),
                started_at_utc = COALESCE(jobs.started_at_utc, VALUES(started_at_utc)),
                completed_at_utc = COALESCE(VALUES(completed_at_utc), jobs.completed_at_utc),
                status = CASE
                    WHEN VALUES(status) IN (\'delivered\', \'cancelled\')
                         AND (jobs.completed_at_utc IS NULL
                              OR VALUES(completed_at_utc) >= jobs.completed_at_utc)
                        THEN VALUES(status)
                    WHEN jobs.status IN (\'delivered\', \'cancelled\')
                        THEN jobs.status
                    WHEN VALUES(status) = \'active\' THEN \'active\'
                    ELSE jobs.status
                END,
                job_details = VALUES(job_details),
                updated_at_utc = UTC_TIMESTAMP(3)'
        );
        $query->execute([
            'job_id' => $jobId,
            'session_id' => $sessionId,
            'game_code' => $gameCode,
            'source_city' => $sourceCity,
            'source_company' => $sourceCompany,
            'destination_city' => $destinationCity,
            'destination_company' => $destinationCompany,
            'cargo_name' => $cargoName,
            'started_at' => $startedAt,
            'completed_at' => $completedAt,
            'status' => $status,
            'job_details' => $detailsJson,
        ]);
    }

    private function jobDetailText(mixed $details, array $keys, int $maximumLength): ?string
    {
        foreach ($keys as $key) {
            $value = is_array($details)
                ? ($details[$key] ?? null)
                : (is_object($details) ? ($details->{$key} ?? null) : null);
            if ($value === null || $value === '') {
                continue;
            }
            if (!is_string($value) || mb_strlen($value, 'UTF-8') > $maximumLength) {
                throw new \InvalidArgumentException('A job summary field is invalid.');
            }
            return $value;
        }
        return null;
    }
}
