<?php
declare(strict_types=1);

require_once __DIR__ . '/../lib/Config.php';
require_once __DIR__ . '/../lib/RecordValidator.php';
require_once __DIR__ . '/../lib/IngestionService.php';
require_once __DIR__ . '/../lib/PdoStore.php';
require_once __DIR__ . '/../lib/DeviceAuthenticator.php';

use Nlsi\Db\Config;
use Nlsi\Db\PdoStore;

function db_json_response(int $status, array $payload): never
{
    http_response_code($status);
    header('Content-Type: application/json; charset=utf-8');
    header('Cache-Control: no-store');
    echo json_encode($payload, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    exit;
}

function db_require_method(string $method): void
{
    if (($_SERVER['REQUEST_METHOD'] ?? '') !== $method) {
        header('Allow: ' . $method);
        db_json_response(405, ['error' => 'method_not_allowed']);
    }
}

function db_device_auth(): array
{
    $header = $_SERVER['HTTP_AUTHORIZATION'] ?? '';
    if (preg_match('/^Bearer\s+(.+)$/i', $header, $matches) !== 1) {
        db_json_response(401, ['error' => 'invalid_device_key']);
    }
    try {
        $authenticator = new \Nlsi\Db\DeviceAuthenticator(
            Config::pdo(Config::load())
        );
        $device = $authenticator->authenticate(
            trim($matches[1])
        );
        if ($device === null) {
            db_json_response(401, ['error' => 'invalid_device_key']);
        }
        return $device;
    } catch (OverflowException) {
        db_json_response(429, ['error' => 'authentication_rate_limited']);
    } catch (DomainException) {
        db_json_response(403, ['error' => 'device_revoked']);
    }
    return [];
}

function db_require_dashboard_access(): void
{
    if (db_dashboard_access_allowed($_SERVER)) {
        return;
    }
    db_json_response(403, ['error' => 'dashboard_access_required']);
}

function db_dashboard_access_allowed(array $server): bool
{
    return in_array($server['REMOTE_ADDR'] ?? '', ['127.0.0.1', '::1'], true)
        || (isset($server['REMOTE_USER']) && $server['REMOTE_USER'] !== '');
}

function db_body(array $config): array
{
    $length = (int)($_SERVER['CONTENT_LENGTH'] ?? 0);
    if ($length > $config['max_request_bytes']) {
        db_json_response(413, ['error' => 'request_too_large']);
    }
    $body = file_get_contents('php://input', false, null, 0, $config['max_request_bytes'] + 1);
    if ($body === false || strlen($body) > $config['max_request_bytes']) {
        db_json_response(413, ['error' => 'request_too_large']);
    }
    try {
        $decoded = json_decode($body, false, 64, JSON_THROW_ON_ERROR);
    } catch (JsonException) {
        db_json_response(400, ['error' => 'invalid_json']);
    }
    if (!$decoded instanceof stdClass) {
        db_json_response(400, ['error' => 'invalid_payload']);
    }
    return get_object_vars($decoded);
}

function db_store(?int $driverId = null): PdoStore
{
    static $stores = [];
    $key = $driverId === null ? 'none' : (string)$driverId;
    if (($stores[$key] ?? null) instanceof PdoStore) {
        return $stores[$key];
    }
    $config = Config::load();
    return $stores[$key] = new PdoStore(Config::pdo($config), $driverId);
}

function db_fail(Throwable $error): never
{
    if ($error instanceof InvalidArgumentException) {
        db_json_response(422, ['error' => 'invalid_record', 'message' => $error->getMessage()]);
    }
    if ($error instanceof DomainException) {
        db_json_response(409, ['error' => 'record_conflict']);
    }
    error_log('NLSI db endpoint failure: ' . get_class($error));
    db_json_response(500, ['error' => 'server_error']);
}
