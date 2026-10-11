<?php
declare(strict_types=1);

require __DIR__ . '/_init.php';

try {
    db_require_method('GET');
    db_require_dashboard_access();
    db_json_response(200, db_store()->heartbeat());
} catch (Throwable $error) {
    db_fail($error);
}
