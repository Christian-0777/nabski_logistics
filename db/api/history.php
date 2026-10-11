<?php
declare(strict_types=1);

require __DIR__ . '/_init.php';

try {
    db_require_method('GET');
    db_require_dashboard_access();
    $limit = min(100, max(1, (int)($_GET['limit'] ?? 25)));
    $offset = min(1000000, max(0, (int)($_GET['offset'] ?? 0)));
    $jobId = $_GET['job_id'] ?? null;
    if ($jobId !== null && (!is_string($jobId)
        || preg_match('/^[A-Za-z0-9._:-]{1,128}$/D', $jobId) !== 1)) {
        db_json_response(400, ['error' => 'invalid_job_id']);
    }
    $rows = db_store()->history($limit, $offset, $jobId);
    foreach ($rows as &$row) {
        $row['payload'] = json_decode($row['payload_json'], true);
        unset($row['payload_json']);
    }
    unset($row);
    db_json_response(200, [
        'records' => $rows,
        'limit' => $limit,
        'offset' => $offset,
        'job_id' => $jobId,
    ]);
} catch (Throwable $error) {
    db_fail($error);
}
