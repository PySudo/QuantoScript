# Security Policy

## Supported versions

QuantoScript is pre-1.0 software. Security fixes are applied to the latest
release and the `main` branch only.

| Version | Supported |
|---------|-----------|
| 1.0.x   | ✅        |
| < 1.0   | ❌        |

## Reporting a vulnerability

**Please do not report security vulnerabilities through public GitHub issues.**

Instead, use GitHub's private reporting:

1. Go to the [Security tab](https://github.com/PySudo/QuantoScript/security) of the
   repository.
2. Click **Report a vulnerability** to open a private advisory.

Alternatively, open a minimal private channel with the maintainers via the repository's
contact options.

When reporting, please include:

- A description of the vulnerability and its impact.
- A minimal QuantoScript program or input that reproduces it.
- The `qs version` output and your operating system.
- Any suggested remediation, if you have one.

## Scope

QuantoScript executes untrusted-looking scripts by design, but it is **not** a security
sandbox by default. Areas where reports are especially valuable:

- Memory-safety issues in the C runtime (buffer overflows, use-after-free, etc.),
  including malformed `.qvm` bytecode files.
- Bypasses of the `--sandbox` file-access restriction.
- Vulnerabilities in the HTTP / WebSocket / TLS handling.

## Response

We aim to acknowledge a report within a few days, agree on a disclosure timeline, and
credit reporters who wish to be named once a fix is released.
