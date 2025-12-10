<?php
// Single-file web hosting platform with SQLite and per-user isolation.
// Auto-creates database and user folders.

session_start();

const DB_FILE = __DIR__ . '/portal.sqlite';
const USERS_ROOT = __DIR__ . '/users';

if (!file_exists(USERS_ROOT)) {
    mkdir(USERS_ROOT, 0775, true);
}

function init_db(): PDO {
    $isNew = !file_exists(DB_FILE);
    $pdo = new PDO('sqlite:' . DB_FILE);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    if ($isNew) {
        $pdo->exec('CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE NOT NULL, password_hash TEXT NOT NULL, created_at TEXT NOT NULL)');
        $pdo->exec('CREATE TABLE login_events (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL, event_at TEXT NOT NULL, ip TEXT, FOREIGN KEY(user_id) REFERENCES users(id))');
    }
    return $pdo;
}

$pdo = init_db();

function flash(string $key, ?string $message = null): ?string {
    if ($message !== null) {
        $_SESSION['flash'][$key] = $message;
        return null;
    }
    if (!empty($_SESSION['flash'][$key])) {
        $msg = $_SESSION['flash'][$key];
        unset($_SESSION['flash'][$key]);
        return $msg;
    }
    return null;
}

function current_user(PDO $pdo): ?array {
    if (!isset($_SESSION['user_id'])) {
        return null;
    }
    $stmt = $pdo->prepare('SELECT * FROM users WHERE id = :id');
    $stmt->execute([':id' => $_SESSION['user_id']]);
    $user = $stmt->fetch(PDO::FETCH_ASSOC);
    return $user ?: null;
}

function sanitize_relative_path(string $path): string {
    $clean = str_replace(['..', "\\", chr(0)], '', $path);
    return ltrim($clean, '/');
}

function user_root(array $user): string {
    $safeName = preg_replace('/[^a-zA-Z0-9_-]/', '_', $user['username']);
    $root = USERS_ROOT . '/' . $user['id'] . '_' . $safeName;
    if (!file_exists($root)) {
        mkdir($root, 0775, true);
    }
    return $root;
}

function ensure_path(array $user, string $relative): string {
    $relative = sanitize_relative_path($relative);
    $base = realpath(user_root($user));
    $target = $base . '/' . $relative;
    $real = realpath($target) ?: $target;
    if (strpos($real, $base) !== 0) {
        throw new RuntimeException('Access denied');
    }
    return $real;
}

function list_directory(string $dir): array {
    $items = array_diff(scandir($dir), ['.', '..']);
    $files = [];
    foreach ($items as $item) {
        $full = $dir . '/' . $item;
        $files[] = [
            'name' => $item,
            'is_dir' => is_dir($full),
            'size' => is_file($full) ? filesize($full) : 0,
            'modified' => date('Y-m-d H:i:s', filemtime($full)),
        ];
    }
    usort($files, fn($a, $b) => strcmp($a['is_dir'] ? '0'.$a['name'] : '1'.$a['name'], $b['is_dir'] ? '0'.$b['name'] : '1'.$b['name']));
    return $files;
}

function compute_usage(string $dir): array {
    $size = 0;
    $files = 0;
    $folders = 0;
    $iterator = new RecursiveIteratorIterator(
        new RecursiveDirectoryIterator($dir, FilesystemIterator::SKIP_DOTS),
        RecursiveIteratorIterator::SELF_FIRST
    );
    foreach ($iterator as $item) {
        if ($item->isDir()) {
            $folders++;
        } else {
            $files++;
            $size += $item->getSize();
        }
    }
    return ['bytes' => $size, 'files' => $files, 'folders' => $folders];
}

function copy_recursive(string $source, string $destination): void {
    if (is_dir($source)) {
        if (!file_exists($destination)) {
            mkdir($destination, 0775, true);
        }
        $items = array_diff(scandir($source), ['.', '..']);
        foreach ($items as $item) {
            copy_recursive($source . '/' . $item, $destination . '/' . $item);
        }
    } else {
        $dir = dirname($destination);
        if (!file_exists($dir)) {
            mkdir($dir, 0775, true);
        }
        copy($source, $destination);
    }
}

function recursive_zip(string $source, string $zipPath): void {
    if (!class_exists('ZipArchive')) {
        throw new RuntimeException('ZipArchive extension is required for backups.');
    }
    $zip = new ZipArchive();
    if ($zip->open($zipPath, ZipArchive::CREATE | ZipArchive::OVERWRITE) !== true) {
        throw new RuntimeException('Unable to create backup');
    }
    $source = realpath($source);
    $sourceLen = strlen($source) + 1;
    $iterator = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($source, FilesystemIterator::SKIP_DOTS));
    foreach ($iterator as $file) {
        $path = $file->getPathname();
        $localName = substr($path, $sourceLen);
        if ($file->isDir()) {
            $zip->addEmptyDir($localName);
        } else {
            $zip->addFile($path, $localName);
        }
    }
    $zip->close();
}

function require_login(PDO $pdo): array {
    $user = current_user($pdo);
    if (!$user) {
        header('Location: ?');
        exit;
    }
    return $user;
}

$action = $_POST['action'] ?? null;

if ($action === 'register') {
    $username = trim($_POST['username'] ?? '');
    $password = $_POST['password'] ?? '';
    if ($username === '' || $password === '') {
        flash('error', 'Username and password are required.');
    } else {
        $hash = password_hash($password, PASSWORD_DEFAULT);
        try {
            $stmt = $pdo->prepare('INSERT INTO users (username, password_hash, created_at) VALUES (:u, :p, :c)');
            $stmt->execute([':u' => $username, ':p' => $hash, ':c' => date('c')]);
            $userId = (int)$pdo->lastInsertId();
            mkdir(user_root(['id' => $userId, 'username' => $username]), 0775, true);
            flash('success', 'Account created. You can log in now.');
        } catch (PDOException $e) {
            flash('error', 'Username already exists.');
        }
    }
    header('Location: ?');
    exit;
}

if ($action === 'login') {
    $username = trim($_POST['username'] ?? '');
    $password = $_POST['password'] ?? '';
    $stmt = $pdo->prepare('SELECT * FROM users WHERE username = :u');
    $stmt->execute([':u' => $username]);
    $user = $stmt->fetch(PDO::FETCH_ASSOC);
    if ($user && password_verify($password, $user['password_hash'])) {
        $_SESSION['user_id'] = $user['id'];
        $pdo->prepare('INSERT INTO login_events (user_id, event_at, ip) VALUES (:u, :t, :ip)')
            ->execute([':u' => $user['id'], ':t' => date('c'), ':ip' => $_SERVER['REMOTE_ADDR'] ?? 'cli']);
        flash('success', 'Welcome back, ' . htmlspecialchars($user['username']) . '!');
    } else {
        flash('error', 'Invalid credentials.');
    }
    header('Location: ?');
    exit;
}

if ($action === 'logout') {
    session_destroy();
    header('Location: ?');
    exit;
}

if ($action && in_array($action, ['create_folder','upload_file','save_file','delete_path','create_backup','rename_path','clone_path','generate_template','restore_backup'], true)) {
    $user = require_login($pdo);
    $root = user_root($user);
    try {
        switch ($action) {
            case 'create_folder':
                $folder = sanitize_relative_path($_POST['folder'] ?? '');
                if ($folder === '') {
                    throw new RuntimeException('Folder name required.');
                }
                $path = ensure_path($user, $folder);
                if (!file_exists($path)) {
                    mkdir($path, 0775, true);
                }
                flash('success', 'Folder created.');
                break;
            case 'upload_file':
                $targetFolder = sanitize_relative_path($_POST['target_folder'] ?? '');
                $destDir = ensure_path($user, $targetFolder === '' ? '.' : $targetFolder);
                if (!is_dir($destDir)) {
                    throw new RuntimeException('Invalid target directory.');
                }
                if (!empty($_FILES['upload']['name'])) {
                    $name = basename(sanitize_relative_path($_FILES['upload']['name']));
                    $dest = $destDir . '/' . $name;
                    move_uploaded_file($_FILES['upload']['tmp_name'], $dest);
                    flash('success', 'File uploaded.');
                } else {
                    throw new RuntimeException('No file selected.');
                }
                break;
            case 'save_file':
                $path = sanitize_relative_path($_POST['path'] ?? '');
                $content = $_POST['content'] ?? '';
                $real = ensure_path($user, $path);
                $dir = dirname($real);
                if (!file_exists($dir)) {
                    mkdir($dir, 0775, true);
                }
                file_put_contents($real, $content);
                flash('success', 'File saved.');
                break;
            case 'delete_path':
                $path = sanitize_relative_path($_POST['path'] ?? '');
                $real = ensure_path($user, $path);
                if (is_dir($real)) {
                    $iterator = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($real, FilesystemIterator::SKIP_DOTS), RecursiveIteratorIterator::CHILD_FIRST);
                    foreach ($iterator as $file) {
                        $file->isDir() ? rmdir($file) : unlink($file);
                    }
                    rmdir($real);
                } elseif (is_file($real)) {
                    unlink($real);
                }
                flash('success', 'Deleted.');
                break;
            case 'create_backup':
                $backupDir = $root . '/backups';
                if (!file_exists($backupDir)) {
                    mkdir($backupDir, 0775, true);
                }
                $backupFile = $backupDir . '/backup_' . date('Ymd_His') . '.zip';
                recursive_zip($root, $backupFile);
                flash('success', 'Backup created: ' . basename($backupFile));
                break;
            case 'rename_path':
                $path = sanitize_relative_path($_POST['path'] ?? '');
                $newName = sanitize_relative_path($_POST['new_name'] ?? '');
                if ($path === '' || $newName === '') {
                    throw new RuntimeException('Path and new name are required.');
                }
                $real = ensure_path($user, $path);
                $dest = dirname($real) . '/' . $newName;
                if (file_exists($dest)) {
                    throw new RuntimeException('Destination already exists.');
                }
                rename($real, $dest);
                flash('success', 'Renamed successfully.');
                break;
            case 'clone_path':
                $path = sanitize_relative_path($_POST['path'] ?? '');
                $copyName = sanitize_relative_path($_POST['copy_name'] ?? '');
                if ($path === '' || $copyName === '') {
                    throw new RuntimeException('Path and clone name required.');
                }
                $real = ensure_path($user, $path);
                $dest = dirname($real) . '/' . $copyName;
                if (file_exists($dest)) {
                    throw new RuntimeException('Clone target exists.');
                }
                copy_recursive($real, $dest);
                flash('success', 'Cloned into ' . htmlspecialchars($copyName));
                break;
            case 'generate_template':
                $targetFolder = sanitize_relative_path($_POST['target_folder'] ?? 'public_html');
                $dest = ensure_path($user, $targetFolder);
                if (!file_exists($dest)) {
                    mkdir($dest, 0775, true);
                }
                $html = "<!doctype html><html><head><meta charset='utf-8'><title>Galaxy Site</title><link rel='stylesheet' href='style.css'></head><body><main><h1>Welcome to your galaxy site</h1><p>Launchpad ready.</p><div id='stats'></div><script src='app.js'></script></main></body></html>";
                $css = "body{font-family:system-ui;background:#050915;color:#e9ecf6;margin:0;display:grid;place-items:center;min-height:100vh;}main{padding:30px;border-radius:16px;background:linear-gradient(135deg,#0f172a,#0b1222);box-shadow:0 20px 80px rgba(0,0,0,0.5);}h1{letter-spacing:0.08em;}";
                $js = "fetch('stats.json').then(r=>r.json()).then(d=>{document.querySelector('#stats').innerHTML='<strong>Stats:</strong> '+JSON.stringify(d)}).catch(()=>{});";
                file_put_contents($dest . '/index.html', $html);
                file_put_contents($dest . '/style.css', $css);
                file_put_contents($dest . '/app.js', $js);
                file_put_contents($dest . '/stats.json', json_encode(['generated' => date('c'), 'user' => $user['username']]));
                flash('success', 'Starter template generated in ' . $targetFolder);
                break;
            case 'restore_backup':
                $backupName = sanitize_relative_path($_POST['backup_file'] ?? '');
                if ($backupName === '') {
                    throw new RuntimeException('Select a backup to restore.');
                }
                $backupDir = $root . '/backups';
                $backupPath = ensure_path($user, 'backups/' . $backupName);
                if (!file_exists($backupPath)) {
                    throw new RuntimeException('Backup not found.');
                }
                if (!class_exists('ZipArchive')) {
                    throw new RuntimeException('ZipArchive extension is required to restore.');
                }
                $restoreTarget = $root . '/restore_' . date('Ymd_His');
                mkdir($restoreTarget, 0775, true);
                $zip = new ZipArchive();
                if ($zip->open($backupPath) !== true) {
                    throw new RuntimeException('Unable to open backup.');
                }
                $zip->extractTo($restoreTarget);
                $zip->close();
                flash('success', 'Backup restored to ' . basename($restoreTarget));
                break;
        }
    } catch (Throwable $e) {
        flash('error', $e->getMessage());
    }
    header('Location: ?');
    exit;
}

$user = current_user($pdo);
$files = $user ? list_directory(user_root($user)) : [];
$backups = [];
$usage = null;
$loginEvents = [];
if ($user) {
    $backupDir = user_root($user) . '/backups';
    if (file_exists($backupDir)) {
        $backups = list_directory($backupDir);
    }
    $usage = compute_usage(user_root($user));
    $stmt = $pdo->prepare('SELECT * FROM login_events WHERE user_id = :u ORDER BY event_at DESC LIMIT 10');
    $stmt->execute([':u' => $user['id']]);
    $loginEvents = $stmt->fetchAll(PDO::FETCH_ASSOC);
}

function card(string $title, string $body, string $accent): string {
    return "<div class='card' style='--accent: {$accent};'><header><span>{$title}</span></header><div class='card-body'>{$body}</div></div>";
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>GalaxyHost Portal</title>
    <style>
        :root {
            --bg: #0b0f1a;
            --panel: #11182a;
            --muted: #778; 
            --accent: #5ef0ff;
            --accent-2: #8f7cff;
            --accent-3: #ffd166;
            --danger: #ff6b6b;
            --success: #5ce29a;
            --text: #e9ecf6;
        }
        * { box-sizing: border-box; }
        body {
            margin: 0;
            font-family: 'Segoe UI', Tahoma, sans-serif;
            background: radial-gradient(circle at 20% 20%, #112, #0b0f1a 40%),
                        radial-gradient(circle at 80% 0%, rgba(94,240,255,0.2), transparent 35%),
                        linear-gradient(135deg, rgba(143,124,255,0.12), transparent 30%),
                        #0b0f1a;
            color: var(--text);
            min-height: 100vh;
        }
        header.top {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 18px 32px;
            position: sticky;
            top: 0;
            z-index: 10;
            background: rgba(11,15,26,0.9);
            backdrop-filter: blur(12px);
            border-bottom: 1px solid rgba(255,255,255,0.04);
        }
        header.top h1 {
            margin: 0;
            font-size: 1.4rem;
            letter-spacing: 0.08em;
            text-transform: uppercase;
        }
        .pill {
            padding: 8px 14px;
            border-radius: 999px;
            border: 1px solid rgba(255,255,255,0.1);
            background: linear-gradient(135deg, rgba(94,240,255,0.18), rgba(143,124,255,0.16));
            color: var(--text);
            font-size: 0.9rem;
        }
        .layout {
            display: grid;
            grid-template-columns: 280px 1fr;
            gap: 18px;
            padding: 18px 22px 40px;
        }
        aside {
            background: var(--panel);
            border: 1px solid rgba(255,255,255,0.05);
            border-radius: 18px;
            padding: 18px;
            display: flex;
            flex-direction: column;
            gap: 12px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.35);
        }
        .nav-item {
            padding: 12px 14px;
            border-radius: 12px;
            cursor: pointer;
            display: flex;
            align-items: center;
            gap: 12px;
            color: var(--text);
            background: rgba(255,255,255,0.02);
            border: 1px solid rgba(255,255,255,0.04);
            transition: transform 0.1s ease, border 0.2s ease, background 0.2s ease;
        }
        .nav-item:hover { transform: translateX(2px); background: rgba(94,240,255,0.08); border-color: rgba(94,240,255,0.35); }
        main { display: flex; flex-direction: column; gap: 18px; }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 18px;
        }
        .card {
            background: linear-gradient(135deg, rgba(255,255,255,0.02), rgba(255,255,255,0.01)), var(--panel);
            border: 1px solid rgba(255,255,255,0.08);
            border-radius: 18px;
            padding: 0;
            box-shadow: 0 10px 40px rgba(0,0,0,0.35);
            overflow: hidden;
            position: relative;
        }
        .card::before {
            content: '';
            position: absolute;
            inset: 0;
            background: linear-gradient(135deg, var(--accent) 0%, transparent 50%);
            opacity: 0.05;
            pointer-events: none;
        }
        .card header {
            padding: 14px 16px;
            font-weight: 700;
            letter-spacing: 0.05em;
            text-transform: uppercase;
            font-size: 0.9rem;
            background: linear-gradient(90deg, color-mix(in srgb, var(--accent) 35%, transparent), transparent);
            border-bottom: 1px solid rgba(255,255,255,0.06);
        }
        .card-body { padding: 16px; display: grid; gap: 12px; }
        form { display: grid; gap: 10px; }
        input, textarea, select, button {
            border-radius: 10px;
            border: 1px solid rgba(255,255,255,0.08);
            padding: 10px 12px;
            background: rgba(255,255,255,0.04);
            color: var(--text);
            font-size: 0.98rem;
            outline: none;
        }
        textarea { resize: vertical; min-height: 120px; font-family: 'Fira Code', monospace; }
        button {
            cursor: pointer;
            background: linear-gradient(135deg, var(--accent), var(--accent-2));
            border: none;
            color: #051225;
            font-weight: 700;
            transition: transform 0.1s ease;
        }
        button:hover { transform: translateY(-1px) scale(1.01); }
        .danger { background: linear-gradient(135deg, #ff6b6b, #ff9f7a); color: #220; }
        .muted { color: var(--muted); font-size: 0.9rem; }
        table { width: 100%; border-collapse: collapse; }
        th, td { padding: 10px; text-align: left; border-bottom: 1px solid rgba(255,255,255,0.06); }
        th { color: var(--muted); font-size: 0.85rem; letter-spacing: 0.06em; }
        .flex { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
        .badge { padding: 6px 10px; border-radius: 10px; background: rgba(94,240,255,0.1); color: var(--accent); font-weight: 700; font-size: 0.85rem; }
        .pill-row { display: flex; gap: 8px; flex-wrap: wrap; }
        .status-dot { width: 10px; height: 10px; border-radius: 50%; background: var(--success); box-shadow: 0 0 8px var(--success); }
        .dual { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
        .progress { height: 8px; background: rgba(255,255,255,0.06); border-radius: 999px; overflow: hidden; position: relative; }
        .progress span { display: block; height: 100%; background: linear-gradient(90deg,var(--accent),var(--accent-2)); }
        .stat-grid { display: grid; grid-template-columns: repeat(auto-fit,minmax(160px,1fr)); gap: 10px; }
        .stat { padding: 12px; border-radius: 12px; background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.06); }
        .timeline { display: grid; gap: 8px; }
        .timeline-item { padding: 10px; border-radius: 10px; background: rgba(255,255,255,0.02); border: 1px dashed rgba(255,255,255,0.08); }
        @media (max-width: 900px) { .layout { grid-template-columns: 1fr; } }
    </style>
</head>
<body>
<header class="top">
    <h1>GalaxyHost</h1>
    <div class="pill">
        <span class="status-dot"></span>
        <strong>SQLite Core</strong> · Secure file sandboxes
    </div>
    <?php if ($user): ?>
    <form method="post" style="margin:0;">
        <input type="hidden" name="action" value="logout">
        <button class="danger">Logout <?php echo htmlspecialchars($user['username']); ?></button>
    </form>
    <?php endif; ?>
</header>

<div class="layout">
    <aside>
        <div class="nav-item"><span>Dashboard</span><span class="badge">Live</span></div>
        <div class="nav-item"><span>File Manager</span><span class="badge" style="color:var(--accent-2);background:rgba(143,124,255,0.15);">Grid</span></div>
        <div class="nav-item"><span>Backups</span><span class="badge" style="color:var(--accent-3);background:rgba(255,209,102,0.18);">Auto</span></div>
        <div class="nav-item"><span>Users</span><span class="badge" style="background:rgba(255,255,255,0.12);color:#fff;">Isolated</span></div>
        <div class="nav-item"><span>Security</span><span class="badge" style="background:rgba(92,226,154,0.1);color:var(--success);">Lock</span></div>
        <?php if (!$user): ?>
            <div class="muted">Create an account to get your personal sandboxed hosting space.</div>
        <?php else: ?>
            <div class="muted">Root: <?php echo htmlspecialchars(user_root($user)); ?></div>
        <?php endif; ?>
    </aside>
    <main>
        <?php if ($msg = flash('error')): ?>
            <div class="card" style="border-color: rgba(255,107,107,0.4);">
                <header>Error</header>
                <div class="card-body" style="color: var(--danger); font-weight:700;">⚠️ <?php echo htmlspecialchars($msg); ?></div>
            </div>
        <?php endif; ?>
        <?php if ($msg = flash('success')): ?>
            <div class="card" style="border-color: rgba(92,226,154,0.4);">
                <header>Success</header>
                <div class="card-body" style="color: var(--success); font-weight:700;">✅ <?php echo htmlspecialchars($msg); ?></div>
            </div>
        <?php endif; ?>

        <?php if (!$user): ?>
            <div class="grid">
                <?php echo card('Create Account', '<form method="post"><input type="hidden" name="action" value="register"><input name="username" placeholder="Username" required><input type="password" name="password" placeholder="Password" required><button>Create Sandbox</button></form>', '#5ef0ff'); ?>
                <?php echo card('Login', '<form method="post"><input type="hidden" name="action" value="login"><input name="username" placeholder="Username" required><input type="password" name="password" placeholder="Password" required><button>Enter Portal</button></form>', '#8f7cff'); ?>
                <?php echo card('Platform Features',
                    '<ul style="margin:0 0 0 18px; color:var(--muted); display:grid; gap:6px;">'
                    .'<li>Isolated user directories with lock-in safeguards</li>'
                    .'<li>File manager with upload, edit, delete, and folder creation</li>'
                    .'<li>Instant SQLite provisioning and activity audit log</li>'
                    .'<li>One-click backup generator per account</li>'
                    .'<li>Dashboard metrics and dual-pane actions</li>'
                    .'</ul>',
                    '#ffd166'); ?>
            </div>
        <?php else: ?>
            <div class="grid">
                <div class="card">
                    <header>Dashboard</header>
                    <div class="card-body">
                        <div class="dual">
                            <div>
                                <div class="muted">Welcome back</div>
                                <h2 style="margin:6px 0;">Commander <?php echo htmlspecialchars($user['username']); ?></h2>
                                <div class="pill-row">
                                    <div class="badge">Created: <?php echo htmlspecialchars($user['created_at']); ?></div>
                                    <div class="badge" style="color:var(--success);background:rgba(92,226,154,0.12);">Files: <?php echo count($files); ?></div>
                                </div>
                            </div>
                            <div>
                                <form method="post" class="dual" enctype="multipart/form-data">
                                    <input type="hidden" name="action" value="create_folder">
                                    <input name="folder" placeholder="Create folder e.g. public_html" required>
                                    <button>Create</button>
                                </form>
                                <form method="post" class="dual" enctype="multipart/form-data">
                                    <input type="hidden" name="action" value="upload_file">
                                    <input name="target_folder" placeholder="Target folder (optional)">
                                    <input type="file" name="upload" required>
                                    <button>Upload</button>
                                </form>
                            </div>
                        </div>
                        <div style="display:grid; grid-template-columns: repeat(auto-fit,minmax(140px,1fr)); gap:8px;">
                            <div class="card" style="padding:12px;">
                                <header style="background:none;border:none;padding:0 0 6px 0;">Quota</header>
                                <div class="card-body" style="padding:0;">
                                    <div class="muted">Storage used</div>
                                    <?php $usedPct = $usage ? min(100, round(($usage['bytes'] / (1024*1024*50)) * 100, 1)) : 0; ?>
                                    <div class="progress"><span style="width: <?php echo $usedPct; ?>%"></span></div>
                                    <div style="font-size:0.85rem; color:var(--muted); margin-top:4px;">~<?php echo number_format($usage['bytes']/1024/1024,2); ?> MB used of 50 MB soft cap</div>
                                </div>
                            </div>
                            <div class="card" style="padding:12px;">
                                <header style="background:none;border:none;padding:0 0 6px 0;">Activity</header>
                                <div class="card-body" style="padding:0;">
                                    <div class="muted">Login entries and file edits are captured for auditing inside SQLite.</div>
                                    <div class="muted">Last login: <?php echo $loginEvents ? htmlspecialchars($loginEvents[0]['event_at']) : '—'; ?></div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
                <div class="card">
                    <header>Operations Lab</header>
                    <div class="card-body">
                        <div class="dual">
                            <form method="post">
                                <input type="hidden" name="action" value="rename_path">
                                <input name="path" placeholder="Path to rename" required>
                                <input name="new_name" placeholder="New name" required>
                                <button>Rename</button>
                            </form>
                            <form method="post">
                                <input type="hidden" name="action" value="clone_path">
                                <input name="path" placeholder="Path to clone" required>
                                <input name="copy_name" placeholder="Clone as" required>
                                <button>Duplicate</button>
                            </form>
                        </div>
                        <details>
                            <summary class="badge" style="cursor:pointer;">Generate starter template</summary>
                            <form method="post">
                                <input type="hidden" name="action" value="generate_template">
                                <input name="target_folder" placeholder="public_html or apps/demo">
                                <button>Provision Demo Site</button>
                            </form>
                        </details>
                        <div class="muted">Automations extend your sandbox with instant scaffolding and controlled clones without leaving GalaxyHost.</div>
                    </div>
                </div>
                <div class="card">
                    <header>File Manager</header>
                    <div class="card-body">
                        <table>
                            <thead><tr><th>Name</th><th>Type</th><th>Size</th><th>Modified</th><th></th></tr></thead>
                            <tbody>
                            <?php foreach ($files as $f): ?>
                                <tr>
                                    <td><?php echo htmlspecialchars($f['name']); ?></td>
                                    <td><?php echo $f['is_dir'] ? 'Directory' : 'File'; ?></td>
                                    <td><?php echo $f['is_dir'] ? '-' : number_format($f['size']/1024,2).' KB'; ?></td>
                                    <td><?php echo htmlspecialchars($f['modified']); ?></td>
                                    <td class="flex">
                                        <?php if (!$f['is_dir']): ?>
                                            <details>
                                                <summary class="badge" style="cursor:pointer;">Edit</summary>
                                                <form method="post">
                                                    <input type="hidden" name="action" value="save_file">
                                                    <input type="hidden" name="path" value="<?php echo htmlspecialchars($f['name']); ?>">
                                                    <textarea name="content"><?php echo htmlspecialchars(file_get_contents(user_root($user).'/'.$f['name'])); ?></textarea>
                                                    <button>Save</button>
                                                </form>
                                            </details>
                                        <?php endif; ?>
                                        <form method="post" onsubmit="return confirm('Delete <?php echo htmlspecialchars($f['name']); ?>?');">
                                            <input type="hidden" name="action" value="delete_path">
                                            <input type="hidden" name="path" value="<?php echo htmlspecialchars($f['name']); ?>">
                                            <button class="danger">Delete</button>
                                        </form>
                                    </td>
                                </tr>
                            <?php endforeach; ?>
                            </tbody>
                        </table>
                        <details>
                            <summary class="badge" style="cursor:pointer;">Create new file</summary>
                            <form method="post">
                                <input type="hidden" name="action" value="save_file">
                                <input name="path" placeholder="index.html" required>
                                <textarea name="content" placeholder="&lt;html&gt;Your site&lt;/html&gt;"></textarea>
                                <button>Save File</button>
                            </form>
                        </details>
                    </div>
                </div>
                <div class="card">
                    <header>Telemetry & Observability</header>
                    <div class="card-body">
                        <div class="stat-grid">
                            <div class="stat">
                                <div class="muted">Files</div>
                                <div style="font-size:1.2rem;font-weight:800;"><?php echo $usage ? $usage['files'] : 0; ?></div>
                            </div>
                            <div class="stat">
                                <div class="muted">Folders</div>
                                <div style="font-size:1.2rem;font-weight:800;"><?php echo $usage ? $usage['folders'] : 0; ?></div>
                            </div>
                            <div class="stat">
                                <div class="muted">Backups</div>
                                <div style="font-size:1.2rem;font-weight:800;"><?php echo count($backups); ?></div>
                            </div>
                            <div class="stat">
                                <div class="muted">Storage</div>
                                <div style="font-size:1.2rem;font-weight:800;">~<?php echo number_format(($usage['bytes'] ?? 0)/1024/1024,2); ?> MB</div>
                            </div>
                        </div>
                        <div class="timeline">
                            <div class="timeline-item">Sandbox root pinned to: <code><?php echo htmlspecialchars(user_root($user)); ?></code></div>
                            <div class="timeline-item">Audit trail latest: <?php echo $loginEvents ? htmlspecialchars($loginEvents[0]['event_at'].' · '.$loginEvents[0]['ip']) : 'No logins yet'; ?></div>
                        </div>
                    </div>
                </div>
                <div class="card">
                    <header>Backups</header>
                    <div class="card-body">
                        <form method="post">
                            <input type="hidden" name="action" value="create_backup">
                            <button>Create Backup</button>
                        </form>
                        <details>
                            <summary class="badge" style="cursor:pointer;">Restore backup into new folder</summary>
                            <form method="post">
                                <input type="hidden" name="action" value="restore_backup">
                                <select name="backup_file" required>
                                    <option value="">Select backup</option>
                                    <?php foreach ($backups as $b): ?>
                                        <option value="<?php echo htmlspecialchars($b['name']); ?>"><?php echo htmlspecialchars($b['name']); ?></option>
                                    <?php endforeach; ?>
                                </select>
                                <button>Restore</button>
                            </form>
                        </details>
                        <table>
                            <thead><tr><th>File</th><th>Size</th><th>Created</th><th>Download</th></tr></thead>
                            <tbody>
                            <?php foreach ($backups as $b): ?>
                                <tr>
                                    <td><?php echo htmlspecialchars($b['name']); ?></td>
                                    <td><?php echo number_format($b['size']/1024,2).' KB'; ?></td>
                                    <td><?php echo htmlspecialchars($b['modified']); ?></td>
                                    <td><a class="badge" href="<?php echo htmlspecialchars('users/'.$user['id'].'_'.preg_replace('/[^a-zA-Z0-9_-]/','_', $user['username']).'/backups/'.$b['name']); ?>" download>Download</a></td>
                                </tr>
                            <?php endforeach; ?>
                            </tbody>
                        </table>
                    </div>
                </div>
                <div class="card">
                    <header>Login Events · Audit</header>
                    <div class="card-body">
                        <div class="timeline">
                            <?php if (empty($loginEvents)): ?>
                                <div class="timeline-item">No login activity yet.</div>
                            <?php else: ?>
                                <?php foreach ($loginEvents as $event): ?>
                                    <div class="timeline-item">
                                        <div><strong><?php echo htmlspecialchars($event['event_at']); ?></strong></div>
                                        <div class="muted">IP: <?php echo htmlspecialchars($event['ip'] ?? 'n/a'); ?></div>
                                    </div>
                                <?php endforeach; ?>
                            <?php endif; ?>
                        </div>
                    </div>
                </div>
            </div>
        <?php endif; ?>
    </main>
</div>
</body>
</html>
