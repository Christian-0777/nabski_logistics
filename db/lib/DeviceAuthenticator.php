<?php
declare(strict_types=1);

namespace Nlsi\Db;

final class DeviceAuthenticator
{
    public function __construct(private \PDO $pdo)
    {
    }

    /** @return array{device_id:string, driver_id:int, status:string}|null */
    public function authenticate(string $presentedKey): ?array
    {
        if (preg_match('/^[A-Za-z0-9]{12}$/D', $presentedKey) === 1) {
            $query = $this->pdo->prepare(
                'SELECT access_keys.access_key_id, access_keys.driver_id
                 FROM driver_access_keys AS access_keys
                 INNER JOIN driver_accounts AS drivers ON drivers.driver_id = access_keys.driver_id
                 WHERE access_keys.key_hash = :key_hash
                   AND access_keys.revoked_at_utc IS NULL
                   AND drivers.status = \'active\'
                 LIMIT 1'
            );
            $query->execute(['key_hash' => hash('sha256', $presentedKey)]);
            $accessKey = $query->fetch();
            if (!is_array($accessKey)) {
                return null;
            }
            return [
                'device_id' => (string)$accessKey['access_key_id'],
                'driver_id' => (int)$accessKey['driver_id'],
                'status' => 'active',
            ];
        }

        if (preg_match('/^[a-fA-F0-9]{64}$/D', $presentedKey) !== 1) {
            return null;
        }

        $legacyTableQuery = $this->pdo->getAttribute(\PDO::ATTR_DRIVER_NAME) === 'sqlite'
            ? 'SELECT 1 FROM sqlite_master WHERE type = \'table\' AND name = \'driver_devices\''
            : 'SELECT 1 FROM information_schema.tables
               WHERE table_schema = DATABASE() AND table_name = \'driver_devices\'';
        $legacyTable = $this->pdo->prepare($legacyTableQuery);
        $legacyTable->execute();
        if (!$legacyTable->fetchColumn()) {
            return null;
        }
        $query = $this->pdo->prepare(
            'SELECT devices.device_id, devices.driver_id
             FROM driver_devices AS devices
             INNER JOIN driver_accounts AS drivers ON drivers.driver_id = devices.driver_id
             WHERE devices.key_hash = :key_hash
               AND devices.status = \'active\'
               AND drivers.status = \'active\'
             LIMIT 1'
        );
        $query->execute(['key_hash' => hash('sha256', strtolower($presentedKey))]);
        $device = $query->fetch();
        if (!is_array($device)) {
            return null;
        }
        return [
            'device_id' => (string)$device['device_id'],
            'driver_id' => (int)$device['driver_id'],
            'status' => 'active',
        ];
    }
}
