<?php

declare(strict_types=1);

final class Security
{
    public static function bootSession(): void
    {
        if (session_status() === PHP_SESSION_ACTIVE) {
            return;
        }

        session_set_cookie_params([
            'httponly' => true,
            'secure' => isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off',
            'samesite' => 'Strict',
            'path' => '/',
        ]);

        session_name('openai_hub_session');
        session_start();

        if (!isset($_SESSION['last_regen'])) {
            session_regenerate_id(true);
            $_SESSION['last_regen'] = time();
        }

        if (time() - (int) $_SESSION['last_regen'] > 900) {
            session_regenerate_id(true);
            $_SESSION['last_regen'] = time();
        }
    }

    public static function setSecurityHeaders(): void
    {
        header("X-Frame-Options: DENY");
        header("X-Content-Type-Options: nosniff");
        header("Referrer-Policy: strict-origin-when-cross-origin");
        header("Permissions-Policy: geolocation=(), camera=(), microphone=()");
        header("Content-Security-Policy: default-src 'self'; style-src 'self' 'unsafe-inline'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'");
    }

    public static function csrfToken(): string
    {
        if (!isset($_SESSION['csrf'])) {
            $_SESSION['csrf'] = bin2hex(random_bytes(32));
        }

        return $_SESSION['csrf'];
    }

    public static function verifyCsrf(?string $token): bool
    {
        return isset($_SESSION['csrf']) && is_string($token) && hash_equals($_SESSION['csrf'], $token);
    }

    public static function e(string $value): string
    {
        return htmlspecialchars($value, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
    }

    public static function passwordIsStrong(string $password): bool
    {
        return strlen($password) >= 12
            && preg_match('/[A-Z]/', $password) === 1
            && preg_match('/[a-z]/', $password) === 1
            && preg_match('/[0-9]/', $password) === 1
            && preg_match('/[^A-Za-z0-9]/', $password) === 1;
    }
}

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

Security::bootSession();
Security::setSecurityHeaders();

date_default_timezone_set('UTC');

if (!is_dir(__DIR__ . '/storage')) {
    mkdir(__DIR__ . '/storage', 0775, true);
}

$pdo = new PDO('sqlite:' . __DIR__ . '/storage/hub.sqlite');
$repo = new HubRepository($pdo);
$repo->migrate();

$flash = null;
$flashType = 'ok';
$newToken = null;

function currentUserId(): ?int
{
    return isset($_SESSION['uid']) ? (int) $_SESSION['uid'] : null;
}

function requireUser(): int
{
    $uid = currentUserId();
    if ($uid === null) {
        header('Location: ?view=login');
        exit;
    }
    return $uid;
}

$action = $_POST['action'] ?? null;

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!Security::verifyCsrf($_POST['csrf'] ?? null)) {
        http_response_code(403);
        exit('Invalid CSRF token');
    }

    $ip = $_SERVER['REMOTE_ADDR'] ?? 'unknown';
    $repo->clearOldAttempts(time() - 900);

    if ($action === 'register') {
        $email = trim((string) ($_POST['email'] ?? ''));
        $password = (string) ($_POST['password'] ?? '');

        if (!filter_var($email, FILTER_VALIDATE_EMAIL)) {
            $flash = 'Invalid email address.';
            $flashType = 'error';
        } elseif (!Security::passwordIsStrong($password)) {
            $flash = 'Password must be at least 12 chars with upper/lowercase, number, and symbol.';
            $flashType = 'error';
        } else {
            $ok = $repo->createUser($email, password_hash($password, PASSWORD_ARGON2ID));
            if ($ok) {
                $repo->log(null, 'user_registered', 'A new account was created for ' . $email);
                $flash = 'Registration successful. You can now log in.';
            } else {
                $flash = 'Email already exists.';
                $flashType = 'error';
            }
        }
    }

    if ($action === 'login') {
        $attempts = $repo->countRecentAttempts($ip, time() - 900);
        if ($attempts >= 8) {
            $flash = 'Too many login attempts. Please wait 15 minutes.';
            $flashType = 'error';
        } else {
            $email = trim((string) ($_POST['email'] ?? ''));
            $password = (string) ($_POST['password'] ?? '');
            $user = $repo->getUserByEmail($email);

            if ($user && password_verify($password, (string) $user['password_hash'])) {
                $_SESSION['uid'] = (int) $user['id'];
                $_SESSION['email'] = (string) $user['email'];
                session_regenerate_id(true);
                $repo->log((int) $user['id'], 'login_success', 'Successful login from ' . $ip);
                header('Location: index.php');
                exit;
            }

            $repo->addLoginAttempt($ip);
            $repo->log($user ? (int) $user['id'] : null, 'login_failed', 'Failed login from ' . $ip);
            $flash = 'Invalid email or password.';
            $flashType = 'error';
        }
    }

    if ($action === 'logout') {
        $uid = currentUserId();
        if ($uid !== null) {
            $repo->log($uid, 'logout', 'User logged out.');
        }
        $_SESSION = [];
        session_destroy();
        header('Location: ?view=login');
        exit;
    }

    if ($action === 'create_agent') {
        $uid = requireUser();
        $name = trim((string) ($_POST['name'] ?? ''));
        $purpose = trim((string) ($_POST['purpose'] ?? ''));
        $prompt = trim((string) ($_POST['prompt'] ?? ''));
        $policy = trim((string) ($_POST['policy'] ?? ''));

        if ($name === '' || $purpose === '' || $prompt === '' || $policy === '') {
            $flash = 'All agent fields are required.';
            $flashType = 'error';
        } else {
            $repo->createAgent($uid, $name, $purpose, $prompt, $policy);
            $repo->log($uid, 'agent_created', 'Agent created: ' . $name);
            $flash = 'Agent created successfully.';
        }
    }

    if ($action === 'create_model_job') {
        $uid = requireUser();
        $model = trim((string) ($_POST['model_name'] ?? ''));
        $dataset = trim((string) ($_POST['dataset_summary'] ?? ''));
        $goal = trim((string) ($_POST['goal'] ?? ''));

        if ($model === '' || $dataset === '' || $goal === '') {
            $flash = 'Model name, dataset summary, and goal are required.';
            $flashType = 'error';
        } else {
            $repo->createModelJob($uid, $model, $dataset, $goal);
            $repo->log($uid, 'training_job_created', 'Model training queued for ' . $model);
            $flash = 'Training job queued.';
        }
    }

    if ($action === 'create_token') {
        $uid = requireUser();
        $newToken = $repo->createApiToken($uid);
        $repo->log($uid, 'api_token_created', 'A new API token was generated.');
        $flash = 'API token generated. Copy it now; it will not be shown again.';
    }
}

$view = (string) ($_GET['view'] ?? 'dashboard');
$uid = currentUserId();

if ($uid === null && $view === 'dashboard') {
    $view = 'login';
}
?>
<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>OpenAI Hub (Single File PHP)</title>
    <style>
        :root {
            color-scheme: light;
            --bg: #f4f7ff;
            --card: #ffffff;
            --surface: #edf2ff;
            --text: #0f172a;
            --muted: #5f6b81;
            --primary: #3867ff;
            --primary-dark: #274dd5;
            --danger: #c0392b;
            --ok: #0d8f58;
            --border: #dbe3f2;
            --shadow: 0 10px 28px rgba(24, 38, 74, 0.08);
        }

        * { box-sizing: border-box; }
        body {
            margin: 0;
            font-family: Inter, ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, Arial, sans-serif;
            background: linear-gradient(180deg, #eef4ff 0%, var(--bg) 60%);
            color: var(--text);
        }

        .container {
            max-width: 1280px;
            margin: 1rem auto;
            padding: 0 0.85rem 2rem;
        }

        .topbar {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 0.75rem;
            margin-bottom: 1rem;
            background: var(--card);
            border: 1px solid var(--border);
            border-radius: 16px;
            padding: 0.85rem 1rem;
            box-shadow: var(--shadow);
        }

        h1 { margin: 0; }
        .subtitle { color: var(--muted); margin: 0.35rem 0 0; font-size: 0.92rem; }

        .grid {
            display: grid;
            gap: 1rem;
            margin-bottom: 1rem;
        }
        .cols-2 { grid-template-columns: repeat(auto-fit, minmax(290px, 1fr)); }
        .cols-3 { grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); }
        .stats { grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); }

        .card {
            background: var(--card);
            border: 1px solid var(--border);
            border-radius: 14px;
            box-shadow: var(--shadow);
            padding: 1rem;
        }
        .card h2, .card h3 { margin-top: 0.2rem; }

        .layout {
            display: grid;
            gap: 1rem;
        }
        .sidebar {
            background: var(--card);
            border: 1px solid var(--border);
            border-radius: 14px;
            padding: 0.9rem;
            box-shadow: var(--shadow);
            height: fit-content;
        }
        .sidebar h3 { margin: 0 0 0.8rem; font-size: 1rem; }
        .sidebar .item { margin-top: 0.5rem; }
        .main { min-width: 0; }
        .metric {
            background: linear-gradient(180deg, #f7f9ff 0%, var(--surface) 100%);
            border: 1px solid var(--border);
            border-radius: 12px;
            padding: 0.75rem;
        }
        .metric .value {
            display: block;
            font-size: 1.45rem;
            font-weight: 700;
            margin-top: 0.2rem;
        }

        label { display: block; margin-bottom: 0.65rem; font-size: 0.95rem; }
        input, textarea, button {
            width: 100%;
            border-radius: 10px;
            border: 1px solid var(--border);
            padding: 0.62rem 0.75rem;
            margin-top: 0.35rem;
            font-size: 0.95rem;
        }
        textarea { resize: vertical; }
        input:focus, textarea:focus {
            outline: 2px solid rgba(56, 103, 255, 0.2);
            border-color: var(--primary);
        }

        button {
            background: var(--primary);
            color: white;
            border: none;
            font-weight: 600;
            cursor: pointer;
            transition: transform .12s ease, background .2s ease;
        }
        button:hover { background: var(--primary-dark); transform: translateY(-1px); }
        button.secondary { background: #213248; }
        button.secondary:hover { background: #172536; }
        .inline { display: inline-flex; }
        .inline button { width: auto; }

        .hint, small { color: var(--muted); }
        .item {
            border: 1px solid var(--border);
            border-radius: 10px;
            padding: 0.55rem 0.7rem;
            margin-top: 0.55rem;
            background: #f9fbff;
        }
        .item strong { display: block; }
        .item-list {
            max-height: 280px;
            overflow: auto;
            padding-right: 0.15rem;
        }
        .flash {
            border-radius: 10px;
            padding: 0.75rem 0.85rem;
            margin-bottom: 1rem;
            color: white;
        }
        .flash.ok { background: var(--ok); }
        .flash.error { background: var(--danger); }
        .token {
            font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
            word-break: break-all;
            background: #0d1b2f;
            color: #96f5c4;
            padding: 0.7rem;
            border-radius: 8px;
        }
        .auth-wrap {
            max-width: 980px;
            margin: 1rem auto 0;
        }
        @media (min-width: 992px) {
            .layout { grid-template-columns: 280px minmax(0, 1fr); }
            .container { margin-top: 1.25rem; }
        }
        @media (max-width: 700px) {
            .topbar { padding: 0.8rem; }
            .topbar h1 { font-size: 1.2rem; }
            .subtitle { font-size: 0.85rem; }
            .card { padding: 0.85rem; }
            .item-list { max-height: 240px; }
        }
    </style>
</head>
<body>
<div class="container">
    <header class="topbar">
        <div>
            <h1>OpenAI Hub</h1>
            <p class="subtitle">Smooth responsive dashboard for mobile + desktop agent operations</p>
        </div>
        <?php if ($uid !== null): ?>
            <form method="post" class="inline">
                <input type="hidden" name="csrf" value="<?= Security::e(Security::csrfToken()) ?>">
                <input type="hidden" name="action" value="logout">
                <button class="secondary" type="submit">Logout <?= Security::e((string) $_SESSION['email']) ?></button>
            </form>
        <?php endif; ?>
    </header>

    <?php if ($flash): ?>
        <div class="flash <?= Security::e($flashType) ?>"><?= Security::e($flash) ?></div>
    <?php endif; ?>

    <?php if ($view === 'login'): ?>
        <div class="auth-wrap">
            <section class="grid cols-2">
                <article class="card">
                    <h2>Login</h2>
                    <form method="post">
                        <input type="hidden" name="csrf" value="<?= Security::e(Security::csrfToken()) ?>">
                        <input type="hidden" name="action" value="login">
                        <label>Email
                            <input required type="email" name="email" autocomplete="email">
                        </label>
                        <label>Password
                            <input required type="password" name="password" autocomplete="current-password">
                        </label>
                        <button type="submit">Sign in</button>
                    </form>
                </article>
                <article class="card">
                    <h2>Create account</h2>
                    <form method="post">
                        <input type="hidden" name="csrf" value="<?= Security::e(Security::csrfToken()) ?>">
                        <input type="hidden" name="action" value="register">
                        <label>Email
                            <input required type="email" name="email" autocomplete="email">
                        </label>
                        <label>Password
                            <input required type="password" name="password" autocomplete="new-password">
                        </label>
                        <p class="hint">Use 12+ chars with uppercase, lowercase, number, and symbol.</p>
                        <button type="submit">Register</button>
                    </form>
                </article>
            </section>
        </div>
    <?php endif; ?>

    <?php if ($uid !== null): ?>
        <?php
            $agents = $repo->listAgents($uid);
            $jobs = $repo->listModelJobs($uid);
            $tokens = $repo->listApiTokens($uid);
            $logs = $repo->listAuditLogs($uid);
            $runningJobs = count(array_filter($jobs, static fn(array $job): bool => $job['status'] === 'queued'));
        ?>
        <section class="grid stats">
            <article class="metric">
                <small>Agents</small>
                <span class="value"><?= Security::e((string) count($agents)) ?></span>
            </article>
            <article class="metric">
                <small>Training Jobs</small>
                <span class="value"><?= Security::e((string) count($jobs)) ?></span>
            </article>
            <article class="metric">
                <small>Queued Jobs</small>
                <span class="value"><?= Security::e((string) $runningJobs) ?></span>
            </article>
            <article class="metric">
                <small>API Tokens</small>
                <span class="value"><?= Security::e((string) count($tokens)) ?></span>
            </article>
        </section>

        <div class="layout">
            <aside class="sidebar">
                <h3>Quick Actions</h3>
                <div class="item"><strong>Create Agent</strong><small>Define role, prompt, and safety policy.</small></div>
                <div class="item"><strong>Queue Training</strong><small>Submit dataset goals and model target.</small></div>
                <div class="item"><strong>Generate Token</strong><small>Issue a new secure API token.</small></div>
                <div class="item"><strong>Audit</strong><small>Review recent security-sensitive events.</small></div>
            </aside>

            <main class="main">
                <section class="grid cols-2">
                    <article class="card">
                        <h2>Create Agent</h2>
                        <form method="post">
                            <input type="hidden" name="csrf" value="<?= Security::e(Security::csrfToken()) ?>">
                            <input type="hidden" name="action" value="create_agent">
                            <label>Name <input required type="text" name="name" maxlength="80"></label>
                            <label>Purpose <input required type="text" name="purpose" maxlength="120"></label>
                            <label>System Prompt <textarea required name="prompt" rows="4" maxlength="1200"></textarea></label>
                            <label>Safety Policy <textarea required name="policy" rows="3" maxlength="1000"></textarea></label>
                            <button type="submit">Create Agent</button>
                        </form>
                    </article>

                    <article class="card">
                        <h2>Queue Model Training</h2>
                        <form method="post">
                            <input type="hidden" name="csrf" value="<?= Security::e(Security::csrfToken()) ?>">
                            <input type="hidden" name="action" value="create_model_job">
                            <label>Base Model <input required type="text" name="model_name" placeholder="gpt-4.1-mini"></label>
                            <label>Dataset Summary <textarea required name="dataset_summary" rows="3"></textarea></label>
                            <label>Training Goal <textarea required name="goal" rows="3"></textarea></label>
                            <button type="submit">Queue Training Job</button>
                        </form>
                    </article>
                </section>

                <section class="grid cols-3">
                    <article class="card">
                        <h3>Your Agents</h3>
                        <div class="item-list">
                            <?php foreach ($agents as $agent): ?>
                                <div class="item">
                                    <strong><?= Security::e((string) $agent['name']) ?></strong>
                                    <small><?= Security::e((string) $agent['purpose']) ?></small>
                                </div>
                            <?php endforeach; ?>
                        </div>
                        <?php if (!$agents): ?><p class="hint">No agents yet.</p><?php endif; ?>
                    </article>

                    <article class="card">
                        <h3>Training Jobs</h3>
                        <div class="item-list">
                            <?php foreach ($jobs as $job): ?>
                                <div class="item">
                                    <strong><?= Security::e((string) $job['model_name']) ?></strong>
                                    <small>Status: <?= Security::e((string) $job['status']) ?></small>
                                </div>
                            <?php endforeach; ?>
                        </div>
                        <?php if (!$jobs): ?><p class="hint">No training jobs queued.</p><?php endif; ?>
                    </article>

                    <article class="card">
                        <h3>API Tokens</h3>
                        <form method="post">
                            <input type="hidden" name="csrf" value="<?= Security::e(Security::csrfToken()) ?>">
                            <input type="hidden" name="action" value="create_token">
                            <button type="submit">Generate Token</button>
                        </form>
                        <?php if ($newToken): ?>
                            <p class="token">New token: <?= Security::e($newToken) ?></p>
                        <?php endif; ?>
                        <div class="item-list">
                            <?php foreach ($tokens as $token): ?>
                                <div class="item">
                                    <strong><?= Security::e((string) $token['token_hint']) ?></strong>
                                    <small><?= Security::e((string) $token['created_at']) ?></small>
                                </div>
                            <?php endforeach; ?>
                        </div>
                    </article>
                </section>

                <section class="card">
                    <h3>Audit Trail</h3>
                    <div class="item-list">
                        <?php foreach ($logs as $log): ?>
                            <div class="item">
                                <strong><?= Security::e((string) $log['event_type']) ?></strong>
                                <small><?= Security::e((string) $log['details']) ?> · <?= Security::e((string) $log['created_at']) ?></small>
                            </div>
                        <?php endforeach; ?>
                    </div>
                    <?php if (!$logs): ?><p class="hint">No security events recorded yet.</p><?php endif; ?>
                </section>
            </main>
        </div>
    <?php endif; ?>
</div>
</body>
</html>
