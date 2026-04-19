<?php

declare(strict_types=1);

require_once __DIR__ . '/../src/Security.php';
require_once __DIR__ . '/../src/HubRepository.php';

Security::bootSession();
Security::setSecurityHeaders();

date_default_timezone_set('UTC');

$pdo = new PDO('sqlite:' . __DIR__ . '/../storage/hub.sqlite');
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
        header('Location: /?view=login');
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
                header('Location: /');
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
        header('Location: /?view=login');
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
    <title>OpenAI Hub (PHP)</title>
    <link rel="stylesheet" href="/style.css" />
</head>
<body>
<div class="container">
    <header>
        <h1>OpenAI Hub</h1>
        <p class="subtitle">Secure PHP workspace for user-owned agents and training workflows</p>
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
    <?php endif; ?>

    <?php if ($uid !== null): ?>
        <?php
            $agents = $repo->listAgents($uid);
            $jobs = $repo->listModelJobs($uid);
            $tokens = $repo->listApiTokens($uid);
            $logs = $repo->listAuditLogs($uid);
        ?>
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
                <?php foreach ($agents as $agent): ?>
                    <div class="item">
                        <strong><?= Security::e((string) $agent['name']) ?></strong>
                        <small><?= Security::e((string) $agent['purpose']) ?></small>
                    </div>
                <?php endforeach; ?>
                <?php if (!$agents): ?><p class="hint">No agents yet.</p><?php endif; ?>
            </article>

            <article class="card">
                <h3>Training Jobs</h3>
                <?php foreach ($jobs as $job): ?>
                    <div class="item">
                        <strong><?= Security::e((string) $job['model_name']) ?></strong>
                        <small>Status: <?= Security::e((string) $job['status']) ?></small>
                    </div>
                <?php endforeach; ?>
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
                <?php foreach ($tokens as $token): ?>
                    <div class="item">
                        <strong><?= Security::e((string) $token['token_hint']) ?></strong>
                        <small><?= Security::e((string) $token['created_at']) ?></small>
                    </div>
                <?php endforeach; ?>
            </article>
        </section>

        <section class="card">
            <h3>Audit Trail</h3>
            <?php foreach ($logs as $log): ?>
                <div class="item">
                    <strong><?= Security::e((string) $log['event_type']) ?></strong>
                    <small><?= Security::e((string) $log['details']) ?> · <?= Security::e((string) $log['created_at']) ?></small>
                </div>
            <?php endforeach; ?>
            <?php if (!$logs): ?><p class="hint">No security events recorded yet.</p><?php endif; ?>
        </section>
    <?php endif; ?>
</div>
</body>
</html>
