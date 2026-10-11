<?php
declare(strict_types=1);

namespace Nlsi\Db;

final class Config
{
    public static function load(): array
    {
        return self::loadFromFile(self::environmentFile());
    }

    public static function loadForMigration(): array
    {
        return self::loadFromFile(self::environmentFile());
    }

    public static function loadFromFile(string $path): array
    {
        $values = self::readEnvironment($path);
        $required = [
            'DB_HOST',
            'DB_PORT',
            'DB_NAME',
            'DB_USER',
            'DB_PASSWORD',
        ];
        foreach ($required as $key) {
            if (!isset($values[$key]) || $values[$key] === '') {
                throw new \RuntimeException("Required server setting $key is missing.");
            }
        }
        $hasMigrationUser = isset($values['DB_MIGRATION_USER']);
        $hasMigrationPassword = isset($values['DB_MIGRATION_PASSWORD']);
        if ($hasMigrationUser !== $hasMigrationPassword
            || ($hasMigrationUser && ($values['DB_MIGRATION_USER'] === ''
                || $values['DB_MIGRATION_PASSWORD'] === ''))) {
            throw new \RuntimeException(
                'DB_MIGRATION_USER and DB_MIGRATION_PASSWORD must both be configured or both omitted.'
            );
        }

        $port = filter_var($values['DB_PORT'], FILTER_VALIDATE_INT, [
            'options' => ['min_range' => 1, 'max_range' => 65535],
        ]);
        if ($port === false) {
            throw new \RuntimeException('DB_PORT must be a valid TCP port.');
        }
        $charset = $values['DB_CHARSET'] ?? 'utf8mb4';
        if (!preg_match('/^[a-zA-Z0-9_]+$/', $charset)) {
            throw new \RuntimeException('DB_CHARSET contains invalid characters.');
        }
        $config = [
            'dsn' => sprintf(
                'mysql:host=%s;port=%d;dbname=%s;charset=%s',
                $values['DB_HOST'],
                $port,
                $values['DB_NAME'],
                $charset
            ),
            'db_user' => $values['DB_USER'],
            'db_password' => $values['DB_PASSWORD'],
            'migration_db_user' => $hasMigrationUser ? $values['DB_MIGRATION_USER'] : null,
            'migration_db_password' => $hasMigrationPassword ? $values['DB_MIGRATION_PASSWORD'] : null,
            'max_request_bytes' => 1048576,
        ];
        return $config;
    }

    private static function environmentFile(): string
    {
        $configuredPath = getenv('NLSI_SERVER_ENV_FILE');
        if ($configuredPath !== false && trim($configuredPath) !== '') {
            return $configuredPath;
        }

        $websiteEnvironment = dirname(__DIR__, 3)
            . DIRECTORY_SEPARATOR . 'nlsi_vtc'
            . DIRECTORY_SEPARATOR . '.env';
        return is_file($websiteEnvironment)
            ? $websiteEnvironment
            : __DIR__ . '/../.env';
    }

    private static function readEnvironment(string $path): array
    {
        if (!is_file($path) || !is_readable($path)) {
            throw new \RuntimeException('Server environment file is missing or unreadable.');
        }
        $values = [];
        foreach (file($path, FILE_IGNORE_NEW_LINES) ?: [] as $lineNumber => $line) {
            $line = trim($line);
            if ($line === '' || str_starts_with($line, '#')) {
                continue;
            }
            if (!preg_match('/^([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)$/', $line, $matches)) {
                throw new \RuntimeException('Invalid server environment file syntax on line '
                    . ($lineNumber + 1) . '.');
            }
            $key = $matches[1];
            $value = trim($matches[2]);
            if (strlen($value) >= 2
                && (($value[0] === '"' && str_ends_with($value, '"'))
                    || ($value[0] === "'" && str_ends_with($value, "'")))) {
                $value = substr($value, 1, -1);
            }
            if (array_key_exists($key, $values)) {
                throw new \RuntimeException('Duplicate server environment setting ' . $key . '.');
            }
            $values[$key] = $value;
        }
        return $values;
    }

    public static function pdo(array $config): \PDO
    {
        return self::connect($config['dsn'], $config['db_user'], $config['db_password']);
    }

    public static function migrationPdo(array $config): \PDO
    {
        if (!is_string($config['migration_db_user'] ?? null)
            || $config['migration_db_user'] === ''
            || !is_string($config['migration_db_password'] ?? null)
            || $config['migration_db_password'] === '') {
            throw new \RuntimeException(
                'DB_MIGRATION_USER and DB_MIGRATION_PASSWORD are required for schema migrations.'
            );
        }
        return self::connect(
            $config['dsn'],
            $config['migration_db_user'],
            $config['migration_db_password']
        );
    }

    private static function connect(string $dsn, string $user, string $password): \PDO
    {
        return new \PDO($dsn, $user, $password, [
            \PDO::ATTR_ERRMODE => \PDO::ERRMODE_EXCEPTION,
            \PDO::ATTR_EMULATE_PREPARES => false,
            \PDO::ATTR_DEFAULT_FETCH_MODE => \PDO::FETCH_ASSOC,
        ]);
    }
}
