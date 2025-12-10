<?php
// MIC Pro: Apple Music-inspired streaming portal powered by SQLite and Audius API.
// Single-file experience with authentication, playlists, discovery, and immersive UI.

session_start();

const DB_FILE = __DIR__ . '/portal.sqlite';

function init_db(): PDO {
    $isNew = !file_exists(DB_FILE);
    $pdo = new PDO('sqlite:' . DB_FILE);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    $pdo->exec('PRAGMA foreign_keys = ON');
    if ($isNew) {
        $pdo->exec('CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE NOT NULL, password_hash TEXT NOT NULL, created_at TEXT NOT NULL)');
        $pdo->exec('CREATE TABLE login_events (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL, event_at TEXT NOT NULL, ip TEXT, FOREIGN KEY(user_id) REFERENCES users(id))');
        $pdo->exec('CREATE TABLE playlists (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL, name TEXT NOT NULL, created_at TEXT NOT NULL, FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE)');
        $pdo->exec('CREATE TABLE playlist_tracks (id INTEGER PRIMARY KEY AUTOINCREMENT, playlist_id INTEGER NOT NULL, track_id TEXT NOT NULL, title TEXT NOT NULL, artist TEXT NOT NULL, artwork TEXT, stream_url TEXT NOT NULL, duration INTEGER DEFAULT 0, added_at TEXT NOT NULL, FOREIGN KEY(playlist_id) REFERENCES playlists(id) ON DELETE CASCADE)');
    } else {
        $pdo->exec('CREATE TABLE IF NOT EXISTS playlists (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL, name TEXT NOT NULL, created_at TEXT NOT NULL, FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE)');
        $pdo->exec('CREATE TABLE IF NOT EXISTS playlist_tracks (id INTEGER PRIMARY KEY AUTOINCREMENT, playlist_id INTEGER NOT NULL, track_id TEXT NOT NULL, title TEXT NOT NULL, artist TEXT NOT NULL, artwork TEXT, stream_url TEXT NOT NULL, duration INTEGER DEFAULT 0, added_at TEXT NOT NULL, FOREIGN KEY(playlist_id) REFERENCES playlists(id) ON DELETE CASCADE)');
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

function require_login(PDO $pdo): array {
    $user = current_user($pdo);
    if (!$user) {
        header('Location: ?');
        exit;
    }
    return $user;
}

function ensure_default_playlist(PDO $pdo, int $userId): void {
    $stmt = $pdo->prepare('SELECT id FROM playlists WHERE user_id = :u LIMIT 1');
    $stmt->execute([':u' => $userId]);
    if (!$stmt->fetchColumn()) {
        $pdo->prepare('INSERT INTO playlists (user_id, name, created_at) VALUES (:u, :n, :c)')
            ->execute([':u' => $userId, ':n' => 'Favorites', ':c' => date('c')]);
    }
}

function load_playlists(PDO $pdo, int $userId): array {
    $stmt = $pdo->prepare('SELECT * FROM playlists WHERE user_id = :u ORDER BY created_at DESC');
    $stmt->execute([':u' => $userId]);
    $playlists = $stmt->fetchAll(PDO::FETCH_ASSOC);
    foreach ($playlists as &$pl) {
        $tracks = $pdo->prepare('SELECT * FROM playlist_tracks WHERE playlist_id = :p ORDER BY added_at DESC');
        $tracks->execute([':p' => $pl['id']]);
        $pl['tracks'] = $tracks->fetchAll(PDO::FETCH_ASSOC);
    }
    return $playlists;
}

function sanitize(string $value): string {
    return trim(filter_var($value, FILTER_SANITIZE_STRING, FILTER_FLAG_NO_ENCODE_QUOTES));
}

function audius_request(string $endpoint, array $params = []): array {
    $base = 'https://api.audius.co/v1';
    $params['app_name'] = 'mic-pro';
    $query = http_build_query($params);
    $url = rtrim($base, '/') . '/' . ltrim($endpoint, '/') . '?' . $query;
    $context = stream_context_create([
        'http' => ['timeout' => 6, 'ignore_errors' => true],
        'https' => ['timeout' => 6, 'ignore_errors' => true],
    ]);
    $raw = @file_get_contents($url, false, $context);
    if ($raw === false) {
        return [];
    }
    $decoded = json_decode($raw, true);
    if (!isset($decoded['data'])) {
        return [];
    }
    $tracks = [];
    foreach ($decoded['data'] as $t) {
        $art = $t['artwork']['150x150'] ?? $t['artwork']['480x480'] ?? '';
        $tracks[] = [
            'track_id' => $t['id'] ?? uniqid('audius_', true),
            'title' => $t['title'] ?? 'Unknown Title',
            'artist' => $t['user']['name'] ?? $t['user']['handle'] ?? 'Unknown Artist',
            'artwork' => $art,
            'duration' => (int)($t['duration'] ?? 0),
            'stream_url' => ($t['stream_url'] ?? '') . '?app_name=mic-pro',
        ];
    }
    return $tracks;
}

function sample_fallback_tracks(): array {
    return [
        ['track_id' => 'sample-helix-1', 'title' => 'SoundHelix Suite No.1', 'artist' => 'SoundHelix', 'artwork' => 'https://images.unsplash.com/photo-1511379938547-c1f69419868d?auto=format&fit=crop&w=480&q=80', 'duration' => 393, 'stream_url' => 'https://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3'],
        ['track_id' => 'sample-helix-2', 'title' => 'SoundHelix Suite No.2', 'artist' => 'SoundHelix', 'artwork' => 'https://images.unsplash.com/photo-1511671782779-c97d3d27a1d4?auto=format&fit=crop&w=480&q=80', 'duration' => 370, 'stream_url' => 'https://www.soundhelix.com/examples/mp3/SoundHelix-Song-2.mp3'],
        ['track_id' => 'sample-helix-3', 'title' => 'SoundHelix Suite No.3', 'artist' => 'SoundHelix', 'artwork' => 'https://images.unsplash.com/photo-1507838153414-b4b713384a76?auto=format&fit=crop&w=480&q=80', 'duration' => 405, 'stream_url' => 'https://www.soundhelix.com/examples/mp3/SoundHelix-Song-3.mp3'],
    ];
}

function hero_tracks(): array {
    $tracks = audius_request('tracks/trending', ['limit' => 8]);
    if (empty($tracks)) {
        $tracks = sample_fallback_tracks();
    }
    return array_slice($tracks, 0, 8);
}

// API proxy for front-end fetches.
if (isset($_GET['api'])) {
    header('Content-Type: application/json');
    $type = $_GET['api'];
    if ($type === 'trending') {
        $tracks = audius_request('tracks/trending', ['limit' => 24]);
        if (empty($tracks)) { $tracks = sample_fallback_tracks(); }
        echo json_encode(['tracks' => $tracks]);
    } elseif ($type === 'search') {
        $q = sanitize($_GET['q'] ?? '');
        $tracks = $q ? audius_request('tracks/search', ['query' => $q, 'limit' => 18]) : [];
        echo json_encode(['tracks' => $tracks]);
    } else {
        echo json_encode(['tracks' => sample_fallback_tracks()]);
    }
    exit;
}

$action = $_POST['action'] ?? null;

if ($action === 'register') {
    $username = sanitize($_POST['username'] ?? '');
    $password = $_POST['password'] ?? '';
    if ($username === '' || $password === '') {
        flash('error', 'Username and password are required.');
    } else {
        $hash = password_hash($password, PASSWORD_DEFAULT);
        try {
            $pdo->prepare('INSERT INTO users (username, password_hash, created_at) VALUES (:u, :p, :c)')
                ->execute([':u' => $username, ':p' => $hash, ':c' => date('c')]);
            $userId = (int)$pdo->lastInsertId();
            ensure_default_playlist($pdo, $userId);
            flash('success', 'Account created. Sign in to start listening.');
        } catch (PDOException $e) {
            flash('error', 'Username already exists.');
        }
    }
    header('Location: ?');
    exit;
}

if ($action === 'login') {
    $username = sanitize($_POST['username'] ?? '');
    $password = $_POST['password'] ?? '';
    $stmt = $pdo->prepare('SELECT * FROM users WHERE username = :u');
    $stmt->execute([':u' => $username]);
    $user = $stmt->fetch(PDO::FETCH_ASSOC);
    if ($user && password_verify($password, $user['password_hash'])) {
        $_SESSION['user_id'] = $user['id'];
        ensure_default_playlist($pdo, (int)$user['id']);
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

if ($action && in_array($action, ['create_playlist', 'delete_playlist', 'save_track', 'remove_track'], true)) {
    $user = require_login($pdo);
    try {
        switch ($action) {
            case 'create_playlist':
                $name = sanitize($_POST['name'] ?? '');
                if ($name === '') { throw new RuntimeException('Playlist name required.'); }
                $pdo->prepare('INSERT INTO playlists (user_id, name, created_at) VALUES (:u, :n, :c)')
                    ->execute([':u' => $user['id'], ':n' => $name, ':c' => date('c')]);
                flash('success', 'Playlist "' . htmlspecialchars($name) . '" created.');
                break;
            case 'delete_playlist':
                $pid = (int)($_POST['playlist_id'] ?? 0);
                if ($pid <= 0) { throw new RuntimeException('Invalid playlist.'); }
                $owned = $pdo->prepare('SELECT id FROM playlists WHERE id = :p AND user_id = :u');
                $owned->execute([':p' => $pid, ':u' => $user['id']]);
                if (!$owned->fetchColumn()) { throw new RuntimeException('Not allowed.'); }
                $pdo->prepare('DELETE FROM playlists WHERE id = :p')->execute([':p' => $pid]);
                flash('success', 'Playlist removed.');
                break;
            case 'save_track':
                $pid = (int)($_POST['playlist_id'] ?? 0);
                $trackId = sanitize($_POST['track_id'] ?? '');
                $title = sanitize($_POST['title'] ?? '');
                $artist = sanitize($_POST['artist'] ?? '');
                $artwork = sanitize($_POST['artwork'] ?? '');
                $stream = filter_var($_POST['stream_url'] ?? '', FILTER_SANITIZE_URL);
                $duration = (int)($_POST['duration'] ?? 0);
                if ($pid <= 0 || $trackId === '' || $title === '' || $artist === '' || $stream === '') {
                    throw new RuntimeException('Incomplete track details.');
                }
                $owned = $pdo->prepare('SELECT id FROM playlists WHERE id = :p AND user_id = :u');
                $owned->execute([':p' => $pid, ':u' => $user['id']]);
                if (!$owned->fetchColumn()) { throw new RuntimeException('Not allowed.'); }
                $pdo->prepare('INSERT INTO playlist_tracks (playlist_id, track_id, title, artist, artwork, stream_url, duration, added_at) VALUES (:p,:t,:tt,:a,:aw,:s,:d,:c)')
                    ->execute([':p' => $pid, ':t' => $trackId, ':tt' => $title, ':a' => $artist, ':aw' => $artwork, ':s' => $stream, ':d' => $duration, ':c' => date('c')]);
                flash('success', 'Added "' . htmlspecialchars($title) . '" to your playlist.');
                break;
            case 'remove_track':
                $tid = (int)($_POST['track_row'] ?? 0);
                if ($tid <= 0) { throw new RuntimeException('Invalid track reference.'); }
                $playlistCheck = $pdo->prepare('SELECT playlist_id FROM playlist_tracks WHERE id = :id');
                $playlistCheck->execute([':id' => $tid]);
                $plid = (int)($playlistCheck->fetchColumn() ?: 0);
                if ($plid === 0) { throw new RuntimeException('Track not found.'); }
                $owned = $pdo->prepare('SELECT id FROM playlists WHERE id = :p AND user_id = :u');
                $owned->execute([':p' => $plid, ':u' => $user['id']]);
                if (!$owned->fetchColumn()) { throw new RuntimeException('Not allowed.'); }
                $pdo->prepare('DELETE FROM playlist_tracks WHERE id = :id')->execute([':id' => $tid]);
                flash('success', 'Track removed.');
                break;
        }
    } catch (Throwable $e) {
        flash('error', $e->getMessage());
    }
    header('Location: ?');
    exit;
}

$user = current_user($pdo);
$playlists = $user ? load_playlists($pdo, (int)$user['id']) : [];
$loginEvents = [];
if ($user) {
    $stmt = $pdo->prepare('SELECT * FROM login_events WHERE user_id = :u ORDER BY event_at DESC LIMIT 6');
    $stmt->execute([':u' => $user['id']]);
    $loginEvents = $stmt->fetchAll(PDO::FETCH_ASSOC);
}
$hero = hero_tracks();
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>MIC Pro · Music Intelligence Cloud</title>
    <style>
        :root {
            --bg: #050507;
            --panel: #0c0d11;
            --glass: rgba(255,255,255,0.04);
            --glass-2: rgba(255,255,255,0.08);
            --muted: #9aa0ad;
            --accent: #ff375f;
            --accent-2: #7b61ff;
            --accent-3: #00e7ff;
            --text: #f8f9fc;
            --shadow: 0 20px 60px rgba(0,0,0,0.4);
            --radius: 16px;
        }
        * { box-sizing: border-box; }
        body {
            margin: 0; padding: 0;
            font-family: 'SF Pro Display', 'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif;
            background: radial-gradient(circle at 20% 20%, rgba(255,55,95,0.08), transparent 26%),
                        radial-gradient(circle at 80% 10%, rgba(0,231,255,0.08), transparent 24%),
                        linear-gradient(180deg, #040308, #080a12 60%, #050507);
            color: var(--text);
            min-height: 100vh;
        }
        a { color: var(--text); text-decoration: none; }
        header.top {
            position: sticky; top: 0; z-index: 10;
            backdrop-filter: blur(12px);
            background: rgba(5,5,7,0.8);
            border-bottom: 1px solid rgba(255,255,255,0.06);
            padding: 14px 24px;
            display: flex; align-items: center; gap: 16px;
        }
        header.top h1 { margin: 0; font-size: 1.1rem; letter-spacing: 0.08em; text-transform: uppercase; }
        .tag { padding: 6px 12px; border-radius: 999px; background: linear-gradient(120deg, var(--accent), var(--accent-2)); font-weight: 700; font-size: 0.85rem; box-shadow: var(--shadow); }
        .layout { display: grid; grid-template-columns: 280px 1fr 360px; gap: 18px; padding: 18px; }
        aside.left, aside.right { background: rgba(255,255,255,0.02); border: 1px solid rgba(255,255,255,0.06); border-radius: var(--radius); padding: 16px; box-shadow: var(--shadow); backdrop-filter: blur(8px); }
        main { display: grid; gap: 18px; }
        .section { background: rgba(255,255,255,0.02); border: 1px solid rgba(255,255,255,0.06); border-radius: var(--radius); padding: 18px; box-shadow: var(--shadow); }
        .section header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 10px; }
        .section h2 { margin: 0; font-size: 1.1rem; letter-spacing: 0.02em; }
        .muted { color: var(--muted); }
        .hero-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px,1fr)); gap: 14px; }
        .hero-card { position: relative; border-radius: var(--radius); overflow: hidden; background: linear-gradient(135deg, rgba(123,97,255,0.3), rgba(0,231,255,0.2)); padding: 14px; min-height: 200px; display: flex; flex-direction: column; justify-content: space-between; box-shadow: var(--shadow); }
        .hero-card img { width: 100%; height: 120px; object-fit: cover; border-radius: 12px; }
        .hero-card button { margin-top: 10px; }
        button, input, select { border-radius: 12px; border: 1px solid rgba(255,255,255,0.08); background: rgba(255,255,255,0.06); color: var(--text); padding: 10px 12px; font-weight: 600; letter-spacing: 0.01em; }
        button { cursor: pointer; background: linear-gradient(120deg, var(--accent), var(--accent-2)); border: none; box-shadow: var(--shadow); }
        button.ghost { background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.14); box-shadow: none; }
        form { display: grid; gap: 10px; }
        .stack { display: grid; gap: 10px; }
        .pill-row { display: flex; gap: 8px; flex-wrap: wrap; }
        .badge { padding: 6px 10px; border-radius: 10px; background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.14); font-weight: 600; font-size: 0.85rem; }
        .playlist-card { border: 1px solid rgba(255,255,255,0.06); border-radius: var(--radius); padding: 14px; background: rgba(255,255,255,0.03); box-shadow: var(--shadow); display: grid; gap: 12px; }
        .playlist-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 12px; }
        table { width: 100%; border-collapse: collapse; }
        th, td { border-bottom: 1px solid rgba(255,255,255,0.08); padding: 8px 6px; text-align: left; }
        .track-row { display: grid; grid-template-columns: auto 1fr auto; gap: 10px; align-items: center; padding: 10px; border-radius: 12px; background: rgba(255,255,255,0.04); border: 1px solid rgba(255,255,255,0.06); }
        .avatar { width: 48px; height: 48px; border-radius: 12px; object-fit: cover; background: #111; }
        .queue { display: grid; gap: 12px; }
        .queue .track-row { grid-template-columns: auto 1fr auto; }
        .player { position: sticky; bottom: 18px; border-radius: var(--radius); background: linear-gradient(120deg, rgba(255,55,95,0.22), rgba(123,97,255,0.26)); padding: 14px; box-shadow: var(--shadow); display: grid; gap: 10px; }
        .timeline { width: 100%; height: 8px; border-radius: 999px; background: rgba(255,255,255,0.16); overflow: hidden; }
        .timeline span { display: block; height: 100%; background: linear-gradient(90deg, var(--accent), var(--accent-2)); width: 0%; }
        .flex { display: flex; align-items: center; gap: 10px; }
        .two-col { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
        .search-bar { display: grid; grid-template-columns: 1fr auto; gap: 8px; }
        .flash { padding: 10px 12px; border-radius: 12px; background: rgba(0,255,140,0.12); border: 1px solid rgba(0,255,140,0.3); color: #caffd7; }
        .flash.error { background: rgba(255,55,95,0.12); border-color: rgba(255,55,95,0.4); color: #ffd8e3; }
        @media(max-width: 1100px) { .layout { grid-template-columns: 1fr; } aside.right { order: 3; } }
    </style>
</head>
<body>
<header class="top">
    <div class="tag">MIC Pro</div>
    <h1>Music Intelligence Cloud · Apple Music-inspired immersion</h1>
    <div style="margin-left:auto;" class="pill-row">
        <span class="badge">Audius full-track API</span>
        <span class="badge">Spatial dashboard</span>
    </div>
</header>
<div class="layout">
    <aside class="left">
        <div class="stack">
            <div style="font-weight:700; font-size:1.05rem;">Control Desk</div>
            <?php if (!$user): ?>
                <form method="post">
                    <input type="hidden" name="action" value="login">
                    <input name="username" placeholder="Username" required>
                    <input type="password" name="password" placeholder="Password" required>
                    <button>Login</button>
                </form>
                <details>
                    <summary class="muted">Create account</summary>
                    <form method="post">
                        <input type="hidden" name="action" value="register">
                        <input name="username" placeholder="Username" required>
                        <input type="password" name="password" placeholder="Password" required>
                        <button class="ghost">Register</button>
                    </form>
                </details>
            <?php else: ?>
                <div class="pill-row">
                    <span class="badge">Hi, <?php echo htmlspecialchars($user['username']); ?></span>
                    <span class="badge">Since <?php echo htmlspecialchars(substr($user['created_at'],0,10)); ?></span>
                </div>
                <form method="post">
                    <input type="hidden" name="action" value="logout">
                    <button class="ghost">Log out</button>
                </form>
                <div class="section" style="padding:12px;">
                    <header style="margin-bottom:6px;"><h2 style="font-size:0.95rem;">New Playlist</h2></header>
                    <form method="post">
                        <input type="hidden" name="action" value="create_playlist">
                        <input name="name" placeholder="Midnight Drive" required>
                        <button>Create</button>
                    </form>
                </div>
                <div class="section" style="padding:12px;">
                    <header style="margin-bottom:6px;"><h2 style="font-size:0.95rem;">Recent logins</h2></header>
                    <div class="stack">
                        <?php if (empty($loginEvents)): ?>
                            <div class="muted">No activity yet.</div>
                        <?php else: ?>
                            <?php foreach ($loginEvents as $event): ?>
                                <div class="badge"><?php echo htmlspecialchars($event['event_at']); ?> · <?php echo htmlspecialchars($event['ip'] ?? 'n/a'); ?></div>
                            <?php endforeach; ?>
                        <?php endif; ?>
                    </div>
                </div>
            <?php endif; ?>
            <?php if ($msg = flash('success')): ?><div class="flash"><?php echo $msg; ?></div><?php endif; ?>
            <?php if ($msg = flash('error')): ?><div class="flash error"><?php echo $msg; ?></div><?php endif; ?>
        </div>
    </aside>
    <main>
        <div class="section">
            <header>
                <div>
                    <div class="muted" style="letter-spacing:0.08em; text-transform:uppercase; font-size:0.8rem;">Immersive listening</div>
                    <h2>Galaxy hero picks</h2>
                </div>
                <div class="pill-row">
                    <span class="badge">Spatial gradient UI</span>
                    <span class="badge">Play + Queue + Save</span>
                </div>
            </header>
            <div class="hero-grid">
                <?php foreach ($hero as $h): ?>
                <div class="hero-card" data-track='<?php echo json_encode($h); ?>'>
                    <div class="pill-row">
                        <span class="badge">Trending</span>
                        <span class="badge"><?php echo htmlspecialchars($h['artist']); ?></span>
                    </div>
                    <?php if (!empty($h['artwork'])): ?><img src="<?php echo htmlspecialchars($h['artwork']); ?>" alt="art"><?php endif; ?>
                    <div>
                        <div style="font-weight:800;font-size:1rem;"><?php echo htmlspecialchars($h['title']); ?></div>
                        <div class="muted">Duration: <?php echo $h['duration'] ? gmdate('i:s', (int)$h['duration']) : '~'; ?></div>
                    </div>
                    <div class="pill-row">
                        <button type="button" class="play-now" data-title="<?php echo htmlspecialchars($h['title']); ?>" data-artist="<?php echo htmlspecialchars($h['artist']); ?>" data-artwork="<?php echo htmlspecialchars($h['artwork']); ?>" data-stream="<?php echo htmlspecialchars($h['stream_url']); ?>" data-id="<?php echo htmlspecialchars($h['track_id']); ?>" data-duration="<?php echo (int)($h['duration'] ?? 0); ?>">Play now</button>
                        <?php if ($user): ?>
                            <form method="post" class="inline-add">
                                <input type="hidden" name="action" value="save_track">
                                <input type="hidden" name="track_id" value="<?php echo htmlspecialchars($h['track_id']); ?>">
                                <input type="hidden" name="title" value="<?php echo htmlspecialchars($h['title']); ?>">
                                <input type="hidden" name="artist" value="<?php echo htmlspecialchars($h['artist']); ?>">
                                <input type="hidden" name="artwork" value="<?php echo htmlspecialchars($h['artwork']); ?>">
                                <input type="hidden" name="stream_url" value="<?php echo htmlspecialchars($h['stream_url']); ?>">
                                <input type="hidden" name="duration" value="<?php echo (int)($h['duration'] ?? 0); ?>">
                                <select name="playlist_id" required>
                                    <?php foreach ($playlists as $pl): ?><option value="<?php echo (int)$pl['id']; ?>">Save to <?php echo htmlspecialchars($pl['name']); ?></option><?php endforeach; ?>
                                </select>
                                <button class="ghost">Save</button>
                            </form>
                        <?php endif; ?>
                    </div>
                </div>
                <?php endforeach; ?>
            </div>
        </div>
        <div class="section">
            <header>
                <div>
                    <div class="muted" style="letter-spacing:0.08em; text-transform:uppercase; font-size:0.8rem;">Discovery rail</div>
                    <h2>Search the Audius galaxy</h2>
                </div>
                <div class="pill-row"><span class="badge">Full-length streams</span><span class="badge">Live search</span></div>
            </header>
            <div class="search-bar">
                <input id="search" placeholder="Search artists, moods, genres..." aria-label="Search Audius">
                <button id="searchBtn">Search</button>
            </div>
            <div class="muted" id="searchStatus">Try "dream pop" or "lofi"</div>
            <div id="searchResults" class="stack"></div>
        </div>
        <?php if ($user): ?>
        <div class="section">
            <header>
                <div>
                    <div class="muted" style="letter-spacing:0.08em; text-transform:uppercase; font-size:0.8rem;">Your universe</div>
                    <h2>Playlists & queues</h2>
                </div>
                <div class="pill-row"><span class="badge">Curate</span><span class="badge">Story</span></div>
            </header>
            <div class="playlist-grid">
                <?php foreach ($playlists as $pl): ?>
                    <div class="playlist-card">
                        <div class="flex" style="justify-content:space-between;">
                            <div>
                                <div style="font-weight:800; font-size:1.05rem;"><?php echo htmlspecialchars($pl['name']); ?></div>
                                <div class="muted"><?php echo count($pl['tracks']); ?> tracks</div>
                            </div>
                            <form method="post" onsubmit="return confirm('Delete playlist?');">
                                <input type="hidden" name="action" value="delete_playlist">
                                <input type="hidden" name="playlist_id" value="<?php echo (int)$pl['id']; ?>">
                                <button class="ghost">Delete</button>
                            </form>
                        </div>
                        <div class="stack">
                            <?php if (empty($pl['tracks'])): ?>
                                <div class="muted">Empty. Add something from hero or search.</div>
                            <?php else: ?>
                                <?php foreach ($pl['tracks'] as $t): ?>
                                    <div class="track-row">
                                        <img class="avatar" src="<?php echo htmlspecialchars($t['artwork'] ?: 'https://placehold.co/64x64'); ?>" alt="art">
                                        <div>
                                            <div style="font-weight:700;"><?php echo htmlspecialchars($t['title']); ?></div>
                                            <div class="muted"><?php echo htmlspecialchars($t['artist']); ?> · <?php echo $t['duration'] ? gmdate('i:s',(int)$t['duration']) : '~'; ?></div>
                                        </div>
                                        <div class="pill-row">
                                            <button type="button" class="play-now" data-title="<?php echo htmlspecialchars($t['title']); ?>" data-artist="<?php echo htmlspecialchars($t['artist']); ?>" data-artwork="<?php echo htmlspecialchars($t['artwork']); ?>" data-stream="<?php echo htmlspecialchars($t['stream_url']); ?>" data-id="<?php echo htmlspecialchars($t['track_id']); ?>" data-duration="<?php echo (int)$t['duration']; ?>">Play</button>
                                            <form method="post">
                                                <input type="hidden" name="action" value="remove_track">
                                                <input type="hidden" name="track_row" value="<?php echo (int)$t['id']; ?>">
                                                <button class="ghost">Remove</button>
                                            </form>
                                        </div>
                                    </div>
                                <?php endforeach; ?>
                            <?php endif; ?>
                        </div>
                    </div>
                <?php endforeach; ?>
            </div>
        </div>
        <?php endif; ?>
    </main>
    <aside class="right">
        <div class="section" style="height:100%; display:grid; gap:12px;">
            <header>
                <div>
                    <div class="muted" style="letter-spacing:0.08em; text-transform:uppercase; font-size:0.8rem;">Now playing</div>
                    <h2>Spatial player</h2>
                </div>
                <span class="badge">Queue</span>
            </header>
            <div class="player" id="playerPanel">
                <div class="flex">
                    <img id="playerArt" class="avatar" src="https://placehold.co/64x64" alt="artwork">
                    <div>
                        <div id="playerTitle" style="font-weight:800;">Select a track</div>
                        <div id="playerArtist" class="muted">MIC Pro</div>
                    </div>
                </div>
                <div class="timeline"><span id="progressBar"></span></div>
                <div class="flex" style="justify-content:space-between;">
                    <div class="pill-row">
                        <button id="prevBtn" class="ghost" type="button">Prev</button>
                        <button id="playPause" type="button">Play</button>
                        <button id="nextBtn" class="ghost" type="button">Next</button>
                    </div>
                    <div class="muted" id="timecode">0:00 / 0:00</div>
                </div>
            </div>
            <div>
                <div class="muted" style="margin-bottom:6px;">Up next</div>
                <div class="queue" id="queue"></div>
            </div>
        </div>
    </aside>
</div>
<script>
const player = new Audio();
player.crossOrigin = 'anonymous';
let queue = [];
let currentIndex = -1;
const queueEl = document.getElementById('queue');
const progressEl = document.getElementById('progressBar');
const timecode = document.getElementById('timecode');
const playerArt = document.getElementById('playerArt');
const playerTitle = document.getElementById('playerTitle');
const playerArtist = document.getElementById('playerArtist');

function renderQueue(){
    queueEl.innerHTML = '';
    queue.forEach((t, idx)=>{
        const row = document.createElement('div');
        row.className = 'track-row';
        row.innerHTML = `<img class="avatar" src="${t.artwork || 'https://placehold.co/64x64'}" alt="art"><div><div style="font-weight:700;">${t.title}</div><div class="muted">${t.artist}</div></div><button type="button" class="ghost" data-jump="${idx}">${idx===currentIndex?'Playing':'Play'}</button>`;
        row.querySelector('button').onclick = ()=>{ playAt(idx); };
        queueEl.appendChild(row);
    });
}

function playAt(idx){
    if(idx < 0 || idx >= queue.length) return;
    currentIndex = idx;
    const t = queue[idx];
    player.src = t.stream_url;
    player.play().catch(()=>{});
    playerArt.src = t.artwork || 'https://placehold.co/64x64';
    playerTitle.textContent = t.title;
    playerArtist.textContent = t.artist;
    renderQueue();
}

function addToQueue(track){
    queue.push(track);
    renderQueue();
    if(currentIndex === -1){ playAt(0); }
}

player.ontimeupdate = ()=>{
    if(!player.duration || isNaN(player.duration)) return;
    const pct = (player.currentTime / player.duration) * 100;
    progressEl.style.width = pct + '%';
    const format = (s)=>{ if(!isFinite(s)) return '0:00'; const m = Math.floor(s/60); const sec = Math.floor(s%60).toString().padStart(2,'0'); return `${m}:${sec}`; };
    timecode.textContent = `${format(player.currentTime)} / ${format(player.duration)}`;
};
player.onended = ()=>{ playAt(currentIndex+1); };

document.querySelectorAll('.play-now').forEach(btn=>{
    btn.onclick = ()=>{
        const track = {
            track_id: btn.dataset.id,
            title: btn.dataset.title,
            artist: btn.dataset.artist,
            artwork: btn.dataset.artwork,
            stream_url: btn.dataset.stream,
            duration: parseInt(btn.dataset.duration||'0',10)
        };
        addToQueue(track);
        playAt(queue.length-1);
    };
});

const searchBtn = document.getElementById('searchBtn');
const searchInput = document.getElementById('search');
const searchResults = document.getElementById('searchResults');
const searchStatus = document.getElementById('searchStatus');

async function search(){
    const term = searchInput.value.trim();
    if(!term){ searchStatus.textContent = 'Type something to search.'; return; }
    searchStatus.textContent = 'Searching Audius...';
    const res = await fetch(`?api=search&q=${encodeURIComponent(term)}`);
    const data = await res.json();
    const tracks = data.tracks || [];
    searchResults.innerHTML = '';
    if(tracks.length === 0){ searchResults.innerHTML = '<div class="muted">Nothing found.</div>'; searchStatus.textContent=''; return; }
    searchStatus.textContent = `${tracks.length} results`;
    tracks.forEach(t=>{
        const row = document.createElement('div');
        row.className = 'track-row';
        row.innerHTML = `<img class="avatar" src="${t.artwork || 'https://placehold.co/64x64'}" alt="art"><div><div style="font-weight:800;">${t.title}</div><div class="muted">${t.artist}</div></div><div class="pill-row"></div>`;
        const playBtn = document.createElement('button');
        playBtn.textContent = 'Play';
        playBtn.type = 'button';
        playBtn.onclick = ()=>{ addToQueue(t); playAt(queue.length-1); };
        row.querySelector('.pill-row').appendChild(playBtn);
        <?php if ($user): ?>
        const form = document.createElement('form');
        form.method = 'post';
        form.innerHTML = `
            <input type="hidden" name="action" value="save_track">
            <input type="hidden" name="track_id" value="${t.track_id}">
            <input type="hidden" name="title" value="${t.title}">
            <input type="hidden" name="artist" value="${t.artist}">
            <input type="hidden" name="artwork" value="${t.artwork}">
            <input type="hidden" name="stream_url" value="${t.stream_url}">
            <input type="hidden" name="duration" value="${t.duration||0}">
            <select name="playlist_id" required>
                <?php foreach ($playlists as $pl): ?>
                    <option value="<?php echo (int)$pl['id']; ?>">Save to <?php echo htmlspecialchars($pl['name']); ?></option>
                <?php endforeach; ?>
            </select>
            <button class="ghost">Save</button>`;
        row.querySelector('.pill-row').appendChild(form);
        <?php endif; ?>
        searchResults.appendChild(row);
    });
}
searchBtn.onclick = search;
searchInput.onkeydown = (e)=>{ if(e.key==='Enter'){ e.preventDefault(); search(); } };

(async function preloadTrending(){
    const res = await fetch('?api=trending');
    const data = await res.json();
    const tracks = data.tracks||[];
    tracks.slice(0,5).forEach(t=>queue.push(t));
    renderQueue();
})();
</script>
</body>
</html>
