# OpenAI Hub (PHP)

A secure-by-default PHP hub where users can:

- Register/login with strong passwords.
- Create their own agents with purpose, system prompt, and safety policy.
- Queue their own model training jobs.
- Generate API tokens (stored hashed).
- Review a personal audit trail.

## Security features

- CSRF protection on every form.
- Session hardening (`HttpOnly`, `SameSite=Strict`, periodic session ID regeneration).
- Password hashing with Argon2id.
- Login rate limiting by IP over rolling window.
- SQL injection protection via prepared statements.
- XSS mitigation with output escaping.
- Security headers including CSP, X-Frame-Options, and X-Content-Type-Options.
- Audit logging for key security-sensitive actions.

## Run locally

```bash
cd php-openai-hub
php -S 127.0.0.1:8080 -t public
```

Then open <http://127.0.0.1:8080>.

> Data is stored in `storage/hub.sqlite`.
