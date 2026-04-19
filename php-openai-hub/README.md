# OpenAI Hub (Single File PHP)

This implementation is now fully self-contained in **one file**:

- `php-openai-hub/index.php`

It includes:
- Registration/login with strong password rules.
- User-owned agent creation.
- User-owned model training job queue records.
- API token generation (stored hashed).
- User audit trail.
- Built-in security controls (CSRF, secure sessions, login rate limit, prepared SQL, output escaping, CSP headers).

## Run locally

```bash
cd php-openai-hub
php -S 127.0.0.1:8080 index.php
```

Then open <http://127.0.0.1:8080>.

> SQLite data is stored at `php-openai-hub/storage/hub.sqlite`.
