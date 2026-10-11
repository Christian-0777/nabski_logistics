<?php
declare(strict_types=1);

require_once __DIR__ . '/../lib/RecordValidator.php';
require_once __DIR__ . '/../lib/IngestionService.php';
require_once __DIR__ . '/../lib/PdoStore.php';
require_once __DIR__ . '/../lib/Config.php';
require_once __DIR__ . '/../lib/DeviceAuthenticator.php';
require_once __DIR__ . '/../api/_init.php';
require_once dirname(__DIR__, 3) . DIRECTORY_SEPARATOR . 'nlsi_vtc'
    . DIRECTORY_SEPARATOR . 'driver' . DIRECTORY_SEPARATOR . '_common.php';
require_once dirname(__DIR__, 3) . DIRECTORY_SEPARATOR . 'nlsi_vtc'
    . DIRECTORY_SEPARATOR . 'driver' . DIRECTORY_SEPARATOR . 'auth.php';

use Nlsi\Db\Config;
use Nlsi\Db\IngestionService;
use Nlsi\Db\IngestionStore;
use Nlsi\Db\PdoStore;
use Nlsi\Db\RecordValidator;

final class MemoryStore implements IngestionStore
{
    public array $rows = [];
    public bool $failCommit = false;
    public bool $committed = false;
    public bool $rolledBack = false;
    private array $staged = [];

    public function begin(): void
    {
        $this->staged = [];
        $this->committed = false;
        $this->rolledBack = false;
    }

    public function findRecord(string $type, string $id): ?string
    {
        return $this->rows[$type . ':' . $id] ?? null;
    }

    public function saveRecord(array $record, array $validated): void
    {
        $this->staged[$validated['record_type'] . ':' . $validated['record_id']] = $validated['fingerprint'];
    }

    public function commit(): void
    {
        if ($this->failCommit) {
            throw new RuntimeException('simulated commit failure');
        }
        $this->rows += $this->staged;
        $this->committed = true;
    }

    public function rollback(): void
    {
        $this->staged = [];
        $this->rolledBack = true;
    }
}

function check(bool $condition, string $message): void
{
    if (!$condition) {
        throw new RuntimeException($message);
    }
}

$environmentPath = tempnam(sys_get_temp_dir(), 'nlsi-env-');
if ($environmentPath === false) {
    throw new RuntimeException('could not create a temporary environment test file');
}
try {
    $environment = implode("\n", [
        'DB_HOST=localhost',
        'DB_PORT=3307',
        'DB_NAME=logbook_test',
        'DB_USER=app_test',
        'DB_PASSWORD="local=test-password"',
        'MAIL_HOST=smtp.example.test',
        'IGNORED_SETTING=must-not-be-loaded',
    ]);
    file_put_contents($environmentPath, $environment);
    $loadedConfig = Config::loadFromFile($environmentPath);
    check($loadedConfig['dsn'] === 'mysql:host=localhost;port=3307;dbname=logbook_test;charset=utf8mb4',
        'environment settings produce the expected PDO DSN');
    check($loadedConfig['migration_db_user'] === null,
        'runtime credentials are not reused as migration credentials');
    $previousEnvironmentPath = getenv('NLSI_SERVER_ENV_FILE');
    putenv('NLSI_SERVER_ENV_FILE=' . $environmentPath);
    try {
        check(Config::load()['dsn'] === $loadedConfig['dsn'],
            'runtime configuration reads the explicitly configured server environment file');
        check(Config::loadForMigration()['dsn'] === $loadedConfig['dsn'],
            'migration configuration reads the explicitly configured server environment file');
        $siteSettings = nlsi_site_env();
        check(($siteSettings['MAIL_HOST'] ?? null) === 'smtp.example.test'
            && !array_key_exists('IGNORED_SETTING', $siteSettings),
            'website email configuration ignores non-mail environment settings');
    } finally {
        if ($previousEnvironmentPath === false) {
            putenv('NLSI_SERVER_ENV_FILE');
        } else {
            putenv('NLSI_SERVER_ENV_FILE=' . $previousEnvironmentPath);
        }
    }
    $migrationConfig = Config::loadFromFile($environmentPath);
    try {
        Config::migrationPdo($migrationConfig);
        throw new RuntimeException('runtime account was accepted for migrations');
    } catch (RuntimeException $error) {
        check(str_contains($error->getMessage(), 'DB_MIGRATION_USER'),
            'schema migrations require a separate migration account');
    }
    file_put_contents(
        $environmentPath,
        preg_replace('/^DB_PASSWORD=.*$/m', '', $environment)
    );
    try {
        Config::loadFromFile($environmentPath);
        throw new RuntimeException('empty database password was accepted');
    } catch (RuntimeException $error) {
        check(str_contains($error->getMessage(), 'DB_PASSWORD'),
            'empty required environment values fail with a safe setting name');
    }
} finally {
    unlink($environmentPath);
}

$record = [
    'record_id' => 'd2719db0-4e2c-41f7-9371-b7f769cbb123',
    'record_type' => 'telemetry_sample',
    'schema_version' => 2,
    'sequence' => 1,
    'timestamp_utc' => '2026-10-11T12:04:16.123Z',
    'provider' => 'NLSI',
    'session_id' => 'unknown-session',
    'normalized_fields' => ['speed_kmh' => 81.5],
];

check(PdoStore::heartbeatResponse('2026-10-11T12:00:00Z') === [
    'success' => true,
    'database' => ['status' => 'connected'],
    'checked_at' => '2026-10-11T12:00:00Z',
], 'heartbeat response contract matches native client format');

$historyDb = new PDO('sqlite::memory:');
$historyDb->exec(
    'CREATE TABLE provider_records (
        record_id TEXT, record_type TEXT, record_schema_version INTEGER,
        sequence_no INTEGER, session_id TEXT, job_id TEXT, captured_at_utc TEXT,
        provider_name TEXT, payload_json TEXT
    )'
);
$historyDb->exec(
    'CREATE TABLE sync_state (
        record_id TEXT, record_type TEXT, state TEXT, last_error_code TEXT
    )'
);
$historyDb->exec(
    "INSERT INTO provider_records VALUES
     ('later-sequence', 'provider_event', 2, 12, NULL, 'job-42', '2026-01-01 00:00:00.000', 'SCS SDK', '{}'),
     ('earlier-sequence', 'provider_event', 2, 11, NULL, 'job-42', '2026-02-01 00:00:00.000', 'SCS SDK', '{}'),
     ('other-job', 'provider_event', 2, 13, NULL, 'job-43', '2026-03-01 00:00:00.000', 'SCS SDK', '{}')"
);
$jobHistory = (new PdoStore($historyDb))->history(10, 0, 'job-42');
check(count($jobHistory) === 2
    && $jobHistory[0]['record_id'] === 'earlier-sequence'
    && $jobHistory[1]['record_id'] === 'later-sequence',
    'job history filters and orders using stable job ID and sequence');

$sessionDb = new PDO('sqlite::memory:');
$authDb = new PDO('sqlite::memory:');
$authDb->exec(
    'CREATE TABLE driver_accounts (
        driver_id INTEGER PRIMARY KEY, status TEXT NOT NULL
    );
    CREATE TABLE driver_access_keys (
        access_key_id INTEGER PRIMARY KEY AUTOINCREMENT, driver_id INTEGER NOT NULL,
        key_hash TEXT NOT NULL UNIQUE, created_at_utc TEXT NOT NULL,
        revoked_at_utc TEXT
    )'
);
$authDb->exec("INSERT INTO driver_accounts (driver_id, status) VALUES (1, 'active'), (2, 'active')");
$firstKey = 'AbC123xYz789';
$secondKey = 'Def456uVw012';
$authDb->prepare(
    'INSERT INTO driver_access_keys (driver_id, key_hash, created_at_utc)
     VALUES (?, ?, CURRENT_TIMESTAMP), (?, ?, CURRENT_TIMESTAMP)'
)->execute([1, hash('sha256', $firstKey), 2, hash('sha256', $secondKey)]);
$authenticator = new \Nlsi\Db\DeviceAuthenticator($authDb);
$firstAuth = $authenticator->authenticate($firstKey);
$secondAuth = $authenticator->authenticate($secondKey);
check($firstAuth !== null && $firstAuth['driver_id'] === 1
    && $firstAuth['status'] === 'active'
    && $secondAuth !== null && $secondAuth['driver_id'] === 2,
    'active driver access keys resolve their database-owned driver identity');
check($authenticator->authenticate(str_repeat('f', 64)) === null,
    'unknown keys are rejected without exposing lookup details');
check($authenticator->authenticate('') === null,
    'malformed keys are rejected');
$authDb->prepare(
    'UPDATE driver_access_keys SET revoked_at_utc = CURRENT_TIMESTAMP WHERE key_hash = ?'
)->execute([hash('sha256', $firstKey)]);
check($authenticator->authenticate($firstKey) === null
    && $authenticator->authenticate($secondKey)['driver_id'] === 2,
    'revoked driver access keys are rejected without affecting other keys');
$authDb->exec("UPDATE driver_accounts SET status = 'suspended' WHERE driver_id = 2");
check($authenticator->authenticate($secondKey) === null,
    'suspended driver accounts cannot authenticate with active access keys');

$portalDb = new PDO('sqlite::memory:');
$portalDb->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$portalDb->exec(
    'CREATE TABLE driver_accounts (
        driver_id INTEGER PRIMARY KEY, username TEXT NOT NULL, email TEXT NOT NULL UNIQUE,
        display_name TEXT NOT NULL, password_hash TEXT, status TEXT NOT NULL,
        created_at_utc TEXT NOT NULL, updated_at_utc TEXT NOT NULL, last_login_at_utc TEXT
    );
    CREATE TABLE driver_devices (
        device_id TEXT PRIMARY KEY, driver_id INTEGER NOT NULL, device_name TEXT NOT NULL,
        key_hash TEXT NOT NULL UNIQUE, status TEXT NOT NULL, created_at_utc TEXT NOT NULL,
        last_used_at_utc TEXT, revoked_at_utc TEXT, replaced_by_device_id TEXT
    );
    CREATE TABLE driver_access_keys (
        access_key_id INTEGER PRIMARY KEY AUTOINCREMENT, driver_id INTEGER NOT NULL,
        key_hash TEXT NOT NULL UNIQUE, created_at_utc TEXT NOT NULL,
        revoked_at_utc TEXT
    );
    CREATE TABLE driver_device_audit (
        audit_id INTEGER PRIMARY KEY AUTOINCREMENT, driver_id INTEGER NOT NULL,
        device_id TEXT, action TEXT NOT NULL, actor TEXT NOT NULL,
        metadata_json TEXT NOT NULL, created_at_utc TEXT NOT NULL
    );
    CREATE TABLE driver_invitations (
        invitation_id TEXT PRIMARY KEY, driver_id INTEGER NOT NULL, code_hash TEXT NOT NULL,
        status TEXT NOT NULL, expires_at_utc TEXT NOT NULL, attempts INTEGER NOT NULL,
        created_at_utc TEXT NOT NULL, sent_at_utc TEXT, used_at_utc TEXT, revoked_at_utc TEXT
    );
    CREATE TABLE driver_login_codes (
        code_id TEXT PRIMARY KEY, driver_id INTEGER NOT NULL, code_hash TEXT NOT NULL,
        expires_at_utc TEXT NOT NULL, attempts INTEGER NOT NULL, sent_at_utc TEXT NOT NULL,
        consumed_at_utc TEXT, revoked_at_utc TEXT
    );
    CREATE TABLE web_auth_attempts (
        attempt_id INTEGER PRIMARY KEY AUTOINCREMENT, purpose TEXT NOT NULL,
        identifier_hash TEXT NOT NULL, ip_hash TEXT NOT NULL, attempted_at_utc TEXT NOT NULL
    );
    CREATE TABLE admin_audit_events (
        event_id INTEGER PRIMARY KEY AUTOINCREMENT, admin_id INTEGER, driver_id INTEGER,
        invitation_id TEXT, action TEXT NOT NULL, metadata_json TEXT NOT NULL,
        created_at_utc TEXT NOT NULL
    );
    INSERT INTO driver_accounts VALUES
      (10, \'new-driver\', \'new@example.test\', \'New Driver\', NULL, \'pending\', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP, NULL),
      (11, \'returning-driver\', \'returning@example.test\', \'Returning Driver\', NULL, \'active\', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP, NULL),
      (12, \'expired-driver\', \'expired@example.test\', \'Expired Driver\', NULL, \'pending\', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP, NULL);'
);
$portalDb->prepare(
    'INSERT INTO driver_invitations
     (invitation_id, driver_id, code_hash, status, expires_at_utc, attempts, created_at_utc, sent_at_utc)
     VALUES (?, ?, ?, \'pending\', datetime(\'now\', ?), 0, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)'
)->execute(['invite-new', 10, password_hash('7654321', PASSWORD_DEFAULT), '+10 minutes']);
$portalDb->prepare(
    'INSERT INTO driver_invitations
     (invitation_id, driver_id, code_hash, status, expires_at_utc, attempts, created_at_utc, sent_at_utc)
     VALUES (?, ?, ?, \'pending\', datetime(\'now\', ?), 0, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)'
)->execute(['invite-expired', 12, password_hash('2345678', PASSWORD_DEFAULT), '-10 minutes']);
$invalidInvite = driver_invitation_login($portalDb, 'new@example.test', '1111111');
check(!$invalidInvite['ok']
    && (int)$portalDb->query("SELECT attempts FROM driver_invitations WHERE invitation_id = 'invite-new'")->fetchColumn() === 1,
    'invalid invitation code is rejected and attempt-counted');
$firstPortalLogin = driver_invitation_login($portalDb, 'new@example.test', '7654321');
check($firstPortalLogin['ok'] && is_array($firstPortalLogin['device'])
    && strlen($firstPortalLogin['device']['key']) === 64,
    'valid single-use invitation activates the driver and provisions a primary key');
check($portalDb->query("SELECT status FROM driver_invitations WHERE invitation_id = 'invite-new'")->fetchColumn() === 'used'
    && $portalDb->query('SELECT status FROM driver_accounts WHERE driver_id = 10')->fetchColumn() === 'active',
    'invitation consumption and account activation are committed together');
check($portalDb->query('SELECT key_hash FROM driver_devices')->fetchColumn()
    === hash('sha256', $firstPortalLogin['device']['key']),
    'portal stores only the hash of the one-time displayed device key');
check(!driver_invitation_login($portalDb, 'new@example.test', '7654321')['ok'],
    'a consumed invitation cannot be reused');
check(!driver_invitation_login($portalDb, 'expired@example.test', '2345678')['ok'],
    'expired invitations cannot activate an account');
$portalDb->prepare(
    'INSERT INTO driver_login_codes
     (code_id, driver_id, code_hash, expires_at_utc, attempts, sent_at_utc)
     VALUES (?, ?, ?, datetime(\'now\', \'+10 minutes\'), 0, CURRENT_TIMESTAMP)'
)->execute(['login-returning', 11, password_hash('654321', PASSWORD_DEFAULT)]);
$returningLogin = driver_invitation_login($portalDb, 'returning@example.test', '654321');
check($returningLogin['ok'] && is_array($returningLogin['device'])
    && (int)$portalDb->query('SELECT COUNT(*) FROM driver_devices')->fetchColumn() === 2,
    'an active legacy driver without a device receives one primary device on first code login');
$returningKeyHash = $portalDb->query(
    "SELECT key_hash FROM driver_devices WHERE driver_id = 11"
)->fetchColumn();
check($returningKeyHash === hash('sha256', $returningLogin['device']['key']),
    'first-code-login provisioning stores only the key hash');
check(!driver_invitation_login($portalDb, 'returning@example.test', '654321')['ok'],
    'a consumed email sign-in code cannot be reused');
$portalDb->prepare(
    'INSERT INTO driver_login_codes
     (code_id, driver_id, code_hash, expires_at_utc, attempts, sent_at_utc)
     VALUES (?, ?, ?, datetime(\'now\', \'+10 minutes\'), 0, datetime(\'now\', \'+1 second\'))'
)->execute(['login-repeat', 11, password_hash('654322', PASSWORD_DEFAULT)]);
$repeatLogin = driver_invitation_login($portalDb, 'returning@example.test', '654322');
check($repeatLogin['ok'] && $repeatLogin['device'] === null
    && (int)$portalDb->query('SELECT COUNT(*) FROM driver_devices')->fetchColumn() === 2
    && $portalDb->query("SELECT key_hash FROM driver_devices WHERE driver_id = 11")->fetchColumn() === $returningKeyHash,
    'repeat email-code login consumes its code without rotating the active device key');
$portalDb->prepare(
    'INSERT INTO driver_login_codes
     (code_id, driver_id, code_hash, expires_at_utc, attempts, sent_at_utc)
     VALUES (?, ?, ?, datetime(\'now\', \'+10 minutes\'), 0, datetime(\'now\', \'+5 seconds\'))'
)->execute(['login-revoked', 11, password_hash('999999', PASSWORD_DEFAULT)]);
$portalDb->prepare(
    'UPDATE driver_login_codes SET revoked_at_utc = CURRENT_TIMESTAMP WHERE code_id = ?'
)->execute(['login-revoked']);
check(!driver_invitation_login($portalDb, 'returning@example.test', '999999')['ok'],
    'revoked email sign-in codes are rejected');
check(!driver_invitation_login($portalDb, 'someone-else@example.test', '7654321')['ok'],
    'an invitation cannot authenticate a different email identity');
$firstDriverKey = driver_issue_access_key($portalDb, 11, false);
check(preg_match('/^[A-Za-z0-9]{12}$/D', $firstDriverKey) === 1,
    'a generated driver key contains exactly twelve alphanumeric characters');
check($portalDb->query(
    "SELECT key_hash FROM driver_access_keys WHERE driver_id = 11 AND revoked_at_utc IS NULL"
)->fetchColumn() === hash('sha256', $firstDriverKey),
    'driver access keys are stored as SHA-256 hashes only');
$validatedDriver = driver_validate_access_key($portalDb, $firstDriverKey);
check($validatedDriver === [
    'driver_id' => 11,
    'display_name' => 'Returning Driver',
], 'an active driver key returns only its non-sensitive identity fields');
check(driver_validate_access_key($portalDb, 'not-a-key') === null
    && driver_validate_access_key($portalDb, 'AAAAAAAAAAAA') === null,
    'malformed and unknown driver keys are rejected');
$rotatedDriverKey = driver_issue_access_key($portalDb, 11, true);
check($rotatedDriverKey !== $firstDriverKey
    && driver_validate_access_key($portalDb, $firstDriverKey) === null
    && driver_validate_access_key($portalDb, $rotatedDriverKey)['driver_id'] === 11,
    'rotation immediately invalidates the previous key and validates its replacement');
$portalDb->exec("UPDATE driver_accounts SET status = 'suspended' WHERE driver_id = 11");
check(driver_validate_access_key($portalDb, $rotatedDriverKey) === null,
    'suspended drivers cannot validate access keys');
$portalDb->exec("UPDATE driver_accounts SET status = 'active' WHERE driver_id = 11");
$authEmail = 'rate-limited@example.test';
$authIp = '192.0.2.99';
for ($attempt = 0; $attempt < 10; $attempt++) {
    check(driver_auth_attempt_allowed($portalDb, $authEmail, $authIp),
        'driver portal permits the first ten authentication attempts');
}
check(!driver_auth_attempt_allowed($portalDb, $authEmail, $authIp),
    'driver portal blocks attempts above the configured per-window limit');
check(admin_setup_request_is_local(['REMOTE_ADDR' => '127.0.0.1'])
    && admin_setup_request_is_local(['REMOTE_ADDR' => '::1']),
    'administrator bootstrap permits direct IPv4 and IPv6 loopback requests');
check(!admin_setup_request_is_local(['REMOTE_ADDR' => '192.0.2.10'])
    && !admin_setup_request_is_local([
        'REMOTE_ADDR' => '127.0.0.1',
        'HTTP_X_FORWARDED_FOR' => '192.0.2.10',
    ]),
    'administrator bootstrap rejects remote and forwarded requests');
$ngrokSetupRequest = [
    'REMOTE_ADDR' => '127.0.0.1',
    'HTTP_X_FORWARDED_HOST' => 'emphasis-shady-creasing.ngrok-free.dev',
    'HTTP_X_FORWARDED_PROTO' => 'https',
    'HTTP_X_FORWARDED_FOR' => '198.51.100.24',
];
check(admin_setup_request_can_view($ngrokSetupRequest)
    && !admin_setup_request_is_authorized($ngrokSetupRequest),
    'ngrok users can view setup but cannot submit without tunnel authentication');
$ngrokSetupRequest['HTTP_X_NLSI_SETUP_AUTHORIZED'] = 'nlsi-ngrok-bootstrap-v1';
check(admin_setup_request_can_view($ngrokSetupRequest)
    && admin_setup_request_is_authorized($ngrokSetupRequest),
    'authenticated ngrok setup requests are accepted');
$spoofedNgrokRequest = $ngrokSetupRequest;
$spoofedNgrokRequest['REMOTE_ADDR'] = '192.0.2.10';
check(!admin_setup_request_is_authorized($spoofedNgrokRequest),
    'ngrok authorization header cannot authorize a direct remote request');
$websiteRoot = dirname(__DIR__, 3) . DIRECTORY_SEPARATOR . 'nlsi_vtc';
$loginSource = file_get_contents($websiteRoot . DIRECTORY_SEPARATOR . 'driver'
    . DIRECTORY_SEPARATOR . 'login.php');
$registerSource = file_get_contents($websiteRoot . DIRECTORY_SEPARATOR . 'driver'
    . DIRECTORY_SEPARATOR . 'register.php');
$adminSource = file_get_contents($websiteRoot . DIRECTORY_SEPARATOR . 'admin'
    . DIRECTORY_SEPARATOR . 'dashboard.php');
$setupSource = file_get_contents($websiteRoot . DIRECTORY_SEPARATOR . 'admin'
    . DIRECTORY_SEPARATOR . 'setup.php');
$adminLoginSource = file_get_contents($websiteRoot . DIRECTORY_SEPARATOR . 'admin'
    . DIRECTORY_SEPARATOR . 'login.php');
$adminLogoutSource = file_get_contents($websiteRoot . DIRECTORY_SEPARATOR . 'admin'
    . DIRECTORY_SEPARATOR . 'logout.php');
check(is_string($loginSource) && !str_contains($loginSource, 'register.php')
    && is_string($registerSource) && str_contains($registerSource, 'http_response_code(404)'),
    'public self-signup is disabled in the driver portal');
check(is_string($adminSource) && str_contains($adminSource, 'admin_require_login()')
    && str_contains($adminSource, 'admin_verify_csrf()')
    && str_contains($adminSource, 'driver_invitation_created'),
    'driver invitation management requires the protected administrator session and CSRF checks');
check(is_string($setupSource)
    && str_contains($setupSource, 'admin_setup_request_can_view($_SERVER)')
    && str_contains($setupSource, 'admin_setup_request_is_authorized($_SERVER)')
    && str_contains($setupSource, 'setup_complete = 1')
    && str_contains($setupSource, 'password_hash($password, PASSWORD_DEFAULT)')
    && !str_contains($setupSource, 'emphasis-shady-creasing.ngrok-free.dev'),
    'initial administrator creation is localhost-restricted, email-verified, and permanently guarded');
check(is_string($adminLoginSource) && str_contains($adminLoginSource, 'password_verify')
    && str_contains($adminLoginSource, 'admin_auth_attempt_allowed')
    && str_contains($adminLoginSource, 'admin_signed_in')
    && is_string($adminLogoutSource) && str_contains($adminLogoutSource, 'admin_verify_csrf()')
    && str_contains($adminLogoutSource, 'REQUEST_METHOD'),
    'administrator sign-in is rate-limited and logout is CSRF-protected');
$keyManagementSource = file_get_contents($websiteRoot . DIRECTORY_SEPARATOR . 'driver'
    . DIRECTORY_SEPARATOR . 'key.php');
$keyApiSource = file_get_contents($websiteRoot . DIRECTORY_SEPARATOR . 'api'
    . DIRECTORY_SEPARATOR . 'driver-key.php');
check(is_string($keyManagementSource)
    && str_contains($keyManagementSource, 'driver_require_login()')
    && str_contains($keyManagementSource, 'driver_verify_csrf()')
    && !str_contains($keyManagementSource, '$_POST[\'driver_id\']')
    && is_string($keyApiSource)
    && str_contains($keyApiSource, 'driver_validate_access_key')
    && str_contains($keyApiSource, 'https_required'),
    'key management is session-owned and API validation is bounded to HTTPS requests');
$authDb->exec("UPDATE driver_accounts SET status = 'suspended' WHERE driver_id = 1");
check($authenticator->authenticate($firstKey) === null,
    'suspended driver accounts cannot authenticate with an otherwise active key');
$ownershipDb = new PDO('sqlite::memory:');
$ownershipDb->exec(
    'CREATE TABLE provider_records (
        record_id TEXT PRIMARY KEY, record_type TEXT NOT NULL,
        driver_id INTEGER, payload_hash TEXT NOT NULL
    )'
);
$ownershipDb->exec(
    "INSERT INTO provider_records VALUES ('owned-record', 'telemetry_sample', 1, 'hash-a')"
);
check((new PdoStore($ownershipDb, 1))->findRecord('telemetry_sample', 'owned-record') === 'hash-a',
    'same-driver retries can inspect their existing record fingerprint');
try {
    (new PdoStore($ownershipDb, 2))->findRecord('telemetry_sample', 'owned-record');
    throw new RuntimeException('a different driver was allowed to claim an existing record ID');
} catch (DomainException) {
}
check(db_dashboard_access_allowed(['REMOTE_ADDR' => '127.0.0.1']),
    'dashboard access is allowed from loopback by default');
check(db_dashboard_access_allowed(['REMOTE_ADDR' => '203.0.113.8', 'REMOTE_USER' => 'operator']),
    'dashboard access is allowed through server-managed authentication');
check(!db_dashboard_access_allowed(['REMOTE_ADDR' => '203.0.113.8']),
    'dashboard access is denied remotely without server-managed authentication');
$sessionDb->exec('CREATE TABLE driving_sessions (session_id TEXT PRIMARY KEY)');
$sessionDb->exec("INSERT INTO driving_sessions (session_id) VALUES ('known-session')");
check(PdoStore::existingSessionId($sessionDb, 'known-session') === 'known-session',
    'existing FK session is preserved');
check(PdoStore::existingSessionId($sessionDb, 'unknown-session') === null,
    'unknown session does not create or populate an FK reference');

$validated = RecordValidator::validate($record);
check($validated['timestamp'] === '2026-10-11 12:04:16.123', 'timestamp milliseconds retained');
check(json_decode($validated['json'], true)['session_id'] === 'unknown-session',
    'full provider payload retains session ID even when its FK session is absent');

$jobEvent = [
    'record_id' => 'd2719db0-4e2c-41f7-9371-b7f769cbb123',
    'record_type' => 'provider_event',
    'schema_version' => 2,
    'sequence' => 1,
    'timestamp_utc' => '2026-10-11T12:04:16.123Z',
    'provider' => 'SCS SDK',
    'event' => 'job.delivered',
    'data' => (object)['job_id' => 'job-42'],
];
check(RecordValidator::jobId($jobEvent) === 'job-42',
    'job history uses the provider-supplied stable job ID');
check(RecordValidator::jobId(['data' => (object)['job_id' => '  ']]) === null,
    'records with no valid job ID remain unassigned');

$invalid = $record;
$invalid['record_id'] = 'not-a-uuid';
try {
    RecordValidator::validate($invalid);
    throw new RuntimeException('invalid UUID was accepted');
} catch (InvalidArgumentException) {
}

$store = new MemoryStore();
$service = new IngestionService($store);
$first = $service->ingest($record);
check($first['acknowledged'] && !$first['duplicate'] && $store->committed, 'first record committed');
$second = $service->ingest($record);
check($second['acknowledged'] && $second['duplicate'] && $store->committed, 'duplicate acknowledged idempotently');

$demoRecord = [
    'record_id' => '7c79bf34-b0e0-4a28-9eb8-e876aac589ab',
    'record_type' => 'provider_event',
    'schema_version' => 2,
    'sequence' => 2,
    'timestamp_utc' => '2026-10-11T12:04:16.123Z',
    'provider' => 'NLSI Logbook Demo',
    'event' => 'job.delivered',
    'data' => [
        'job_id' => 'demo-7c79bf34-b0e0-4a28-9eb8-e876aac589ab',
        'demo' => true,
        'test_data' => true,
        'data_source' => 'Send Demo button; not live telemetry',
        'driver_name' => 'DEMO DRIVER (TEST DATA)',
        'game_id' => 'ets2',
        'truck' => 'DEMO Volvo FH16 (TEST DATA)',
        'cargo_id' => 'demo-cargo-7c79bf34-b0e0-4a28-9eb8-e876aac589ab',
        'cargo' => 'Demo Cargo - TEST DATA',
        'source_company' => 'Demo Logistics Depot',
        'source_city' => 'Berlin (DEMO)',
        'destination_company' => 'Demo Distribution Center',
        'destination_city' => 'Prague (DEMO)',
        'driven_distance_km' => 248.6,
        'planned_distance_km' => 250.0,
        'income' => 8420,
        'delivery_time_game_minutes' => 215,
        'elapsed_real_minutes' => 18,
        'started_at_utc' => '2026-10-11T11:46:16.123Z',
        'completed_at_utc' => '2026-10-11T12:04:16.123Z',
        'xp' => 184,
        'damage' => 0,
        'weight_kg' => 18400,
        'fuel_used_liters' => 72.4,
    ],
];
$demoStore = new MemoryStore();
$demoService = new IngestionService($demoStore);
$demoResult = $demoService->ingest($demoRecord);
check($demoResult['acknowledged'] && !$demoResult['duplicate']
    && RecordValidator::jobId($demoRecord) === $demoRecord['data']['job_id'],
    'marked demo delivery validates and carries a stable database job identifier');
check($demoService->ingest($demoRecord)['duplicate'],
    'retrying the same demo delivery record is acknowledged idempotently');

$reordered = [
    'sequence' => 1,
    'schema_version' => 2,
    'record_type' => 'telemetry_sample',
    'record_id' => 'd2719db0-4e2c-41f7-9371-b7f769cbb123',
    'timestamp_utc' => '2026-10-11T12:04:16.123Z',
    'normalized_fields' => ['speed_kmh' => 81.5],
    'provider' => 'NLSI',
    'session_id' => 'unknown-session',
];
check($service->ingest($reordered)['duplicate'], 'reordered JSON keys remain idempotent');
$batch = $service->ingestBatch([$record, $record]);
check($batch === [
    'success' => true,
    'committed' => true,
    'acknowledged_record_ids' => [
        'd2719db0-4e2c-41f7-9371-b7f769cbb123',
        'd2719db0-4e2c-41f7-9371-b7f769cbb123',
    ],
    'acknowledgements' => [
        ['record_id' => 'd2719db0-4e2c-41f7-9371-b7f769cbb123', 'acknowledged' => true, 'duplicate' => true],
        ['record_id' => 'd2719db0-4e2c-41f7-9371-b7f769cbb123', 'acknowledged' => true, 'duplicate' => true],
    ],
], 'batch response includes committed duplicate IDs in requested order');

$event = [
    'record_id' => '50b8bb15-c705-4770-adf8-a16d75fb4869',
    'record_type' => 'provider_event',
    'schema_version' => 2,
    'sequence' => 2,
    'timestamp_utc' => '2026-10-11T12:04:16Z',
    'provider' => 'SCS SDK',
    'event' => 'job.delivered',
    'data' => (object)[],
];
check(RecordValidator::validate($event)['record_type'] === 'provider_event', 'provider event validates');
check(str_contains(RecordValidator::validate($event)['json'], '"data":{}'),
    'empty provider object remains a JSON object');

$configuration = [
    'record_id' => '50b8bb15-c705-4770-adf8-a16d75fb4870',
    'record_type' => 'provider_configuration',
    'schema_version' => 2,
    'sequence' => 3,
    'timestamp_utc' => '2026-10-11T12:04:16.1Z',
    'provider' => 'SCS SDK',
    'configuration_id' => 'configuration-1',
    'game_id' => 'ets2',
    'attributes' => (object)[],
];
check(RecordValidator::validate($configuration)['timestamp'] === '2026-10-11 12:04:16.100',
    'configuration validates and fractional UTC timestamp is normalized');

$gps = [
    'record_id' => '50b8bb15-c705-4770-adf8-a16d75fb4871',
    'record_type' => 'telemetry_sample',
    'schema_version' => 2,
    'sequence' => 4,
    'timestamp_utc' => '2026-10-11T12:04:16.123Z',
    'provider' => 'TruckSim GPS',
    'provider_revision' => 13,
    'source_timestamps' => (object)['sample' => '1'],
    'raw_fields' => (object)['speed' => 1],
    'raw_availability' => (object)[],
    'raw_mapping_encoding' => 'qcompress+base64',
    'raw_mapping_uncompressed_bytes' => 32768,
    'raw_mapping_base64' => base64_encode(pack('N', 32768) . gzcompress(str_repeat("\0", 32768))),
    'normalized_fields' => (object)['speed_kmh' => 1],
];
check(RecordValidator::validate($gps)['record_type'] === 'telemetry_sample',
    'compressed TruckSim GPS sample validates');

$conflict = $record;
$conflict['normalized_fields']['speed_kmh'] = 12;
try {
    $service->ingest($conflict);
    throw new RuntimeException('conflicting duplicate was accepted');
} catch (DomainException) {
}

$uncommittedStore = new MemoryStore();
$uncommittedStore->failCommit = true;
try {
    (new IngestionService($uncommittedStore))->ingest($record);
    throw new RuntimeException('failed transaction was acknowledged');
} catch (RuntimeException $error) {
    check($error->getMessage() === 'simulated commit failure', 'commit error propagated');
}
check(!$uncommittedStore->committed && $uncommittedStore->rolledBack && $uncommittedStore->rows === [],
    'failed commit rolls back and persists no record');

$partialStore = new MemoryStore();
$partialService = new IngestionService($partialStore);
$badAfterFirst = $record;
$badAfterFirst['record_id'] = '50b8bb15-c705-4770-adf8-a16d75fb4872';
$badAfterFirst['record_type'] = 'unsupported';
try {
    $partialService->ingestBatch([$record, $badAfterFirst]);
    throw new RuntimeException('batch with an invalid item returned a success contract');
} catch (InvalidArgumentException) {
}

echo "All PHP backend tests passed.\n";
