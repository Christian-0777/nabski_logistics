<?php
declare(strict_types=1);

require __DIR__ . '/_init.php';

try {
    db_require_method('GET');
    $device = db_device_auth();
    $result = db_store($device['driver_id'])->heartbeat();
    $result['device'] = ['id' => $device['device_id'], 'status' => 'active'];
    db_json_response(200, $result);
} catch (Throwable $error) {
    db_fail($error);
}
