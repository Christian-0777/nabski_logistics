<?php
declare(strict_types=1);

namespace Nlsi\Db;

final class RecordValidator
{
    public const MAX_RECORD_BYTES = 1048576;

    /** @return array{record_type:string, record_id:string, json:string, timestamp:string} */
    public static function validate(mixed $record): array
    {
        if (!is_array($record) || array_is_list($record)) {
            throw new \InvalidArgumentException('Each record must be a JSON object.');
        }
        $json = json_encode($record, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE | JSON_THROW_ON_ERROR);
        if (strlen($json) > self::MAX_RECORD_BYTES) {
            throw new \InvalidArgumentException('Record exceeds the size limit.');
        }

        $recordId = $record['record_id'] ?? null;
        if (!is_string($recordId) || !preg_match(
            '/^[0-9a-f]{8}-[0-9a-f]{4}-[1-8][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i',
            $recordId
        )) {
            throw new \InvalidArgumentException('Record identifier is invalid.');
        }
        $type = $record['record_type'] ?? null;
        if (!in_array($type, ['telemetry_sample', 'provider_event', 'provider_configuration'], true)) {
            throw new \InvalidArgumentException('Record type is unsupported.');
        }
        if (($record['schema_version'] ?? null) !== 2) {
            throw new \InvalidArgumentException('Record schema version is unsupported.');
        }
        if (!is_int($record['sequence'] ?? null) || $record['sequence'] < 1) {
            throw new \InvalidArgumentException('Record sequence is invalid.');
        }
        $timestamp = $record['timestamp_utc'] ?? null;
        if (!is_string($timestamp) || !preg_match(
            '/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,3})?Z$/',
            $timestamp
        )) {
            throw new \InvalidArgumentException('UTC timestamp is invalid.');
        }
        $parseValue = preg_replace_callback(
            '/\.(\d{1,2})Z$/',
            static fn(array $match): string => '.' . str_pad($match[1], 3, '0') . 'Z',
            $timestamp
        );
        if (!str_contains($parseValue, '.')) {
            $parseValue = substr($parseValue, 0, -1) . '.000Z';
        }
        $date = \DateTimeImmutable::createFromFormat(
            '!Y-m-d\TH:i:s.v\Z',
            $parseValue,
            new \DateTimeZone('UTC')
        );
        $dateErrors = \DateTimeImmutable::getLastErrors();
        if ($date === false || ($dateErrors !== false
            && ($dateErrors['warning_count'] !== 0 || $dateErrors['error_count'] !== 0))) {
            throw new \InvalidArgumentException('UTC timestamp is invalid.');
        }
        $provider = $record['provider'] ?? null;
        if (!is_string($provider) || trim($provider) === '' || mb_strlen($provider, 'UTF-8') > 64) {
            throw new \InvalidArgumentException('Provider name is invalid.');
        }

        if ($type === 'telemetry_sample') {
            self::validateTelemetry($record);
        } elseif ($type === 'provider_event') {
            if (!is_string($record['event'] ?? null) || trim($record['event']) === ''
                || !self::isObjectMap($record['data'] ?? null)) {
                throw new \InvalidArgumentException('Provider event data is invalid.');
            }
        } elseif (!is_string($record['configuration_id'] ?? null)
            || trim($record['configuration_id']) === ''
            || !is_string($record['game_id'] ?? null)
            || trim($record['game_id']) === ''
            || !self::isObjectMap($record['attributes'] ?? null)) {
            throw new \InvalidArgumentException('Provider configuration data is invalid.');
        }

        $canonical = self::canonicalize($record);
        $canonicalJson = json_encode(
            $canonical,
            JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE | JSON_THROW_ON_ERROR
        );
        return [
            'record_type' => $type,
            'record_id' => strtolower($recordId),
            'json' => $json,
            'fingerprint' => hash('sha256', $canonicalJson),
            'timestamp' => $date->setTimezone(new \DateTimeZone('UTC'))->format('Y-m-d H:i:s.v'),
        ];
    }

    public static function jobId(array $record): ?string
    {
        $data = $record['data'] ?? null;
        $details = $record['details'] ?? null;
        $dataJobId = is_array($data)
            ? ($data['job_id'] ?? null)
            : (is_object($data) ? ($data->job_id ?? null) : null);
        $detailsJobId = is_array($details)
            ? ($details['job_id'] ?? null)
            : (is_object($details) ? ($details->job_id ?? null) : null);
        foreach ([
            $record['job_id'] ?? null,
            $dataJobId,
            $detailsJobId,
        ] as $candidate) {
            if (is_string($candidate)) {
                $candidate = trim($candidate);
                if ($candidate !== '' && strlen($candidate) <= 128
                    && preg_match('/^[A-Za-z0-9._:-]+$/D', $candidate) === 1) {
                    return $candidate;
                }
            }
        }
        return null;
    }

    private static function validateTelemetry(array $record): void
    {
        if (!self::isObjectMap($record['normalized_fields'] ?? null)) {
            throw new \InvalidArgumentException('Normalized telemetry fields are invalid.');
        }
        if (isset($record['session_id'])
            && (!is_string($record['session_id']) || trim($record['session_id']) === ''
                || strlen($record['session_id']) > 64)) {
            throw new \InvalidArgumentException('Session identifier is invalid.');
        }
        if (($record['provider'] ?? '') === 'TruckSim GPS') {
            if (($record['provider_revision'] ?? null) !== 13
                || !self::isObjectMap($record['source_timestamps'] ?? null)
                || !self::isObjectMap($record['raw_fields'] ?? null)
                || (is_array($record['raw_fields']) && $record['raw_fields'] === [])
                || ($record['raw_fields'] instanceof \stdClass
                    && get_object_vars($record['raw_fields']) === [])
                || !self::isObjectMap($record['raw_availability'] ?? null)
                || !is_string($record['raw_mapping_encoding'] ?? null)
                || $record['raw_mapping_encoding'] !== 'qcompress+base64'
                || ($record['raw_mapping_uncompressed_bytes'] ?? null) !== 32768
                || !is_string($record['raw_mapping_base64'] ?? null)) {
                throw new \InvalidArgumentException('Raw telemetry payload is invalid.');
            }
            $compressed = base64_decode($record['raw_mapping_base64'], true);
            if ($compressed === false || strlen($compressed) < 5 || strlen($compressed) > 65540
                || unpack('Nsize', substr($compressed, 0, 4))['size'] !== 32768) {
                throw new \InvalidArgumentException('Raw telemetry mapping is invalid.');
            }
            $rawMapping = gzuncompress(substr($compressed, 4));
            if ($rawMapping === false || strlen($rawMapping) !== 32768) {
                throw new \InvalidArgumentException('Raw telemetry mapping is invalid.');
            }
        } elseif (($record['provider'] ?? '') !== 'NLSI') {
            throw new \InvalidArgumentException('Telemetry provider is unsupported.');
        }
    }

    private static function isObjectMap(mixed $value): bool
    {
        return $value instanceof \stdClass || (is_array($value) && !array_is_list($value));
    }

    private static function canonicalize(mixed $value): mixed
    {
        if ($value instanceof \stdClass) {
            $properties = get_object_vars($value);
            ksort($properties, SORT_STRING);
            $normalized = new \stdClass();
            foreach ($properties as $key => $item) {
                $normalized->{$key} = self::canonicalize($item);
            }
            return $normalized;
        }
        if (is_array($value)) {
            if (!array_is_list($value)) {
                ksort($value, SORT_STRING);
            }
            foreach ($value as $key => $item) {
                $value[$key] = self::canonicalize($item);
            }
        }
        return $value;
    }
}
