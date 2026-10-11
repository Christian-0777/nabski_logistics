<?php
declare(strict_types=1);

namespace Nlsi\Db;

interface IngestionStore
{
    public function begin(): void;
    public function findRecord(string $type, string $id): ?string;
    public function saveRecord(array $record, array $validated): void;
    public function commit(): void;
    public function rollback(): void;
}

final class IngestionService
{
    public function __construct(private IngestionStore $store)
    {
    }

    /** @return array{record_id:string, acknowledged:bool, duplicate:bool} */
    public function ingest(array $record): array
    {
        $validated = RecordValidator::validate($record);
        $this->store->begin();
        try {
            $existing = $this->store->findRecord($validated['record_type'], $validated['record_id']);
            $duplicate = $existing !== null;
            if ($duplicate) {
                if (!hash_equals($existing, $validated['fingerprint'])) {
                    throw new \DomainException('Record identifier conflicts with a previously stored record.');
                }
            } else {
                $this->store->saveRecord($record, $validated);
            }
            $this->store->commit();
        } catch (\Throwable $error) {
            $this->store->rollback();
            throw $error;
        }

        // This acknowledgement is constructed only after a successful commit.
        return ['record_id' => $validated['record_id'], 'acknowledged' => true, 'duplicate' => $duplicate];
    }

    /** @return array{success:bool, committed:bool, acknowledged_record_ids:list<string>, acknowledgements:list<array>} */
    public function ingestBatch(array $records): array
    {
        $acknowledgements = [];
        $recordIds = [];
        foreach ($records as $record) {
            if ($record instanceof \stdClass) {
                $record = get_object_vars($record);
            }
            if (!is_array($record)) {
                throw new \InvalidArgumentException('Each record must be a JSON object.');
            }
            $acknowledgement = $this->ingest($record);
            $acknowledgements[] = $acknowledgement;
            $recordIds[] = $acknowledgement['record_id'];
        }
        return [
            'success' => true,
            'committed' => true,
            'acknowledged_record_ids' => $recordIds,
            'acknowledgements' => $acknowledgements,
        ];
    }
}
