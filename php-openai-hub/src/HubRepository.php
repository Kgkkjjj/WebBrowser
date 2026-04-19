<?php

declare(strict_types=1);

final class HubRepository
{
    public function __construct(private readonly PDO $pdo)
    {
        $this->pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
        $this->pdo->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
    }

    public function migrate(): void
    {
        $this->pdo->exec(<<<SQL
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            email TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS agents (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            purpose TEXT NOT NULL,
            system_prompt TEXT NOT NULL,
            safety_policy TEXT NOT NULL,
            created_at TEXT NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS model_jobs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            model_name TEXT NOT NULL,
            dataset_summary TEXT NOT NULL,
            training_goal TEXT NOT NULL,
            status TEXT NOT NULL,
            created_at TEXT NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS api_tokens (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            token_hash TEXT NOT NULL,
            token_hint TEXT NOT NULL,
            created_at TEXT NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS login_attempts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ip_address TEXT NOT NULL,
            attempted_at INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS audit_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER,
            event_type TEXT NOT NULL,
            details TEXT NOT NULL,
            created_at TEXT NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL
        );
        SQL);
    }

    public function addLoginAttempt(string $ip): void
    {
        $stmt = $this->pdo->prepare('INSERT INTO login_attempts (ip_address, attempted_at) VALUES (:ip, :ts)');
        $stmt->execute(['ip' => $ip, 'ts' => time()]);
    }

    public function clearOldAttempts(int $beforeTs): void
    {
        $stmt = $this->pdo->prepare('DELETE FROM login_attempts WHERE attempted_at < :threshold');
        $stmt->execute(['threshold' => $beforeTs]);
    }

    public function countRecentAttempts(string $ip, int $afterTs): int
    {
        $stmt = $this->pdo->prepare('SELECT COUNT(*) FROM login_attempts WHERE ip_address = :ip AND attempted_at >= :after');
        $stmt->execute(['ip' => $ip, 'after' => $afterTs]);
        return (int) $stmt->fetchColumn();
    }

    public function createUser(string $email, string $passwordHash): bool
    {
        $stmt = $this->pdo->prepare('INSERT INTO users (email, password_hash, created_at) VALUES (:email, :hash, :created)');
        try {
            return $stmt->execute([
                'email' => strtolower($email),
                'hash' => $passwordHash,
                'created' => gmdate(DATE_ATOM),
            ]);
        } catch (PDOException) {
            return false;
        }
    }

    public function getUserByEmail(string $email): ?array
    {
        $stmt = $this->pdo->prepare('SELECT * FROM users WHERE email = :email');
        $stmt->execute(['email' => strtolower($email)]);
        $row = $stmt->fetch();
        return $row ?: null;
    }

    public function createAgent(int $userId, string $name, string $purpose, string $systemPrompt, string $safetyPolicy): void
    {
        $stmt = $this->pdo->prepare(
            'INSERT INTO agents (user_id, name, purpose, system_prompt, safety_policy, created_at)
             VALUES (:uid, :name, :purpose, :prompt, :policy, :created)'
        );
        $stmt->execute([
            'uid' => $userId,
            'name' => $name,
            'purpose' => $purpose,
            'prompt' => $systemPrompt,
            'policy' => $safetyPolicy,
            'created' => gmdate(DATE_ATOM),
        ]);
    }

    public function createModelJob(int $userId, string $model, string $datasetSummary, string $goal): void
    {
        $stmt = $this->pdo->prepare(
            'INSERT INTO model_jobs (user_id, model_name, dataset_summary, training_goal, status, created_at)
             VALUES (:uid, :model, :dataset, :goal, :status, :created)'
        );
        $stmt->execute([
            'uid' => $userId,
            'model' => $model,
            'dataset' => $datasetSummary,
            'goal' => $goal,
            'status' => 'queued',
            'created' => gmdate(DATE_ATOM),
        ]);
    }

    public function createApiToken(int $userId): string
    {
        $plain = 'oh_' . bin2hex(random_bytes(20));
        $stmt = $this->pdo->prepare('INSERT INTO api_tokens (user_id, token_hash, token_hint, created_at) VALUES (:uid, :hash, :hint, :created)');
        $stmt->execute([
            'uid' => $userId,
            'hash' => hash('sha256', $plain),
            'hint' => substr($plain, 0, 8) . '...' . substr($plain, -4),
            'created' => gmdate(DATE_ATOM),
        ]);

        return $plain;
    }

    public function listAgents(int $userId): array
    {
        $stmt = $this->pdo->prepare('SELECT * FROM agents WHERE user_id = :uid ORDER BY id DESC');
        $stmt->execute(['uid' => $userId]);
        return $stmt->fetchAll();
    }

    public function listModelJobs(int $userId): array
    {
        $stmt = $this->pdo->prepare('SELECT * FROM model_jobs WHERE user_id = :uid ORDER BY id DESC');
        $stmt->execute(['uid' => $userId]);
        return $stmt->fetchAll();
    }

    public function listApiTokens(int $userId): array
    {
        $stmt = $this->pdo->prepare('SELECT token_hint, created_at FROM api_tokens WHERE user_id = :uid ORDER BY id DESC');
        $stmt->execute(['uid' => $userId]);
        return $stmt->fetchAll();
    }

    public function log(?int $userId, string $event, string $details): void
    {
        $stmt = $this->pdo->prepare('INSERT INTO audit_logs (user_id, event_type, details, created_at) VALUES (:uid, :event, :details, :created)');
        $stmt->execute([
            'uid' => $userId,
            'event' => $event,
            'details' => $details,
            'created' => gmdate(DATE_ATOM),
        ]);
    }

    public function listAuditLogs(int $userId): array
    {
        $stmt = $this->pdo->prepare('SELECT event_type, details, created_at FROM audit_logs WHERE user_id = :uid ORDER BY id DESC LIMIT 30');
        $stmt->execute(['uid' => $userId]);
        return $stmt->fetchAll();
    }
}
