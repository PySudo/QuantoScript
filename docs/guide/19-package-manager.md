[« Previous: Command-Line Interface](18-command-line.md) · [Guide Index](README.md) · [Next: Style and Limits »](20-style-and-limits.md)

# 19. Package Manager

QuantoScript includes a small, git-backed package manager. Packages are cloned
from GitHub into a local `packages/` directory, and their `quanto.json`
dependencies are resolved recursively.

---

## Installing a package

```bash
qs install https://github.com/owner/repo
```

The repository is cloned into `packages/`. Its manifest (`quanto.json`)
dependencies are fetched as well.

---

## Using an installed package

Import from the cloned path:

```qscript
from "packages/owner_repo/main.qs" import something
```

---

## Listing and removing

```bash
qs list              # list installed packages
qs remove repo       # remove a package
```

---

## Creating a project

`qs init` scaffolds a new project:

```bash
qs init
```

It creates:

- `main.qs` — the entry point
- `test/` — a directory for tests
- `quanto.json` — the project manifest

Run your project with:

```bash
qs main.qs
```

---

## Where packages live

Like the standard library, packages are resolved relative to `QUANTO_HOME`. Run
`qs home` to see the configured location. See
[Imports](13-imports.md) for how paths are resolved.

---

## Next steps

[Style and Limits](20-style-and-limits.md) closes the guide with conventions and
known limitations.

---

[« Previous: Command-Line Interface](18-command-line.md) · [Guide Index](README.md) · [Next: Style and Limits »](20-style-and-limits.md)
