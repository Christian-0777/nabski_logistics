<?php
declare(strict_types=1);

require __DIR__ . '/_init.php';

use Nlsi\Db\Config;
use Nlsi\Db\IngestionService;

try {
    db_require_method('POST');
    $device = db_device_auth();
    $config = Config::load();
    $payload = db_body($config);
    $records = isset($payload['records']) ? $payload['records'] : [$payload];
    if (!is_array($records) || !array_is_list($records) || count($records) < 1 || count($records) > 100) {
        db_json_response(400, ['error' => 'invalid_batch']);
    }
    $result = (new IngestionService(db_store($device['driver_id'])))->ingestBatch($records);
    db_json_response(200, $result);
} catch (Throwable $error) {
    db_fail($error);
}
