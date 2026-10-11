<?php
declare(strict_types=1);

require __DIR__ . '/_init.php';

try {
    db_require_method('POST');
    $device = db_device_auth();
    $config = \Nlsi\Db\Config::load();
    $payload = db_body($config);
    $ids = $payload['record_ids'] ?? null;
    if (!is_array($ids) || !array_is_list($ids) || count($ids) > 500) {
        db_json_response(400, ['error' => 'invalid_record_ids']);
    }
    foreach ($ids as $id) {
        if (!is_string($id) || !preg_match(
            '/^[0-9a-f]{8}-[0-9a-f]{4}-[1-8][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i',
            $id
        )) {
            db_json_response(400, ['error' => 'invalid_record_ids']);
        }
    }
    db_json_response(200, [
        'records' => db_store($device['driver_id'])->syncStatus(array_map('strtolower', $ids)),
    ]);
} catch (Throwable $error) {
    db_fail($error);
}
