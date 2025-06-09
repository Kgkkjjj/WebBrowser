<?php
header('Content-Type: application/json');
$target = 'https://linuxksdteam.site' . $_SERVER['REQUEST_URI'];
$options = ['http' => ['timeout' => 5]];
$context = stream_context_create($options);
$data = @file_get_contents($target, false, $context);
if ($data === false) {
    http_response_code(502);
    echo json_encode(['error' => 'Backend unavailable']);
} else {
    echo $data;
}
?>

