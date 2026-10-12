<?php
declare(strict_types=1);

namespace Nlsi\Db;

require_once dirname(__DIR__, 3) . DIRECTORY_SEPARATOR . 'xtian_tools'
    . DIRECTORY_SEPARATOR . 'config' . DIRECTORY_SEPARATOR . 'Database.php';

use XtianTools\Config\Database;

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
        return Database::loadConfig($path);
    }

    public static function environmentFile(): string
    {
        return Database::environmentFile();
    }

    public static function pdo(array $config): \PDO
    {
        return Database::pdo($config);
    }

    public static function migrationPdo(array $config): \PDO
    {
        return Database::migrationPdo($config);
    }
}
