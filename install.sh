#!/bin/sh
# QuantoScript installer for macOS and Linux.
#
#   curl -fsSL https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.sh | sh
#
# Environment variables:
#   QUANTO_INSTALL   install prefix              (default: $HOME/.quanto)
#   QS_VERSION       version to install, e.g. 1.0.0  (default: latest release)
#   QS_REPO          owner/repo                  (default: PySudo/QuantoScript)
#   QS_ARCHIVE       install from a local .tar.gz instead of downloading
#   NO_MODIFY_PATH   set to 1 to skip editing your shell profile
#
# Flags:
#   --uninstall      remove a previous installation
#   --help           show this message

set -eu

# ── Configuration ──────────────────────────────────────────────────
REPO="${QS_REPO:-PySudo/QuantoScript}"
PREFIX="${QUANTO_INSTALL:-$HOME/.quanto}"
BIN_NAME="qs"
DATA_SUBDIR="share/quantoscript"

RED=''; GREEN=''; BLUE=''; BOLD=''; RESET=''
if [ -t 1 ]; then
    RED=$(printf '\033[31m'); GREEN=$(printf '\033[32m')
    BLUE=$(printf '\033[34m'); BOLD=$(printf '\033[1m'); RESET=$(printf '\033[0m')
fi

say()  { printf '%s%s%s\n' "$BLUE" "$*" "$RESET"; }
ok()   { printf '%s%s%s\n' "$GREEN" "$*" "$RESET"; }
warn() { printf '%s%s%s\n' "$RED" "$*" "$RESET" >&2; }
die()  { printf '%serror:%s %s\n' "$RED" "$RESET" "$*" >&2; exit 1; }

need() { command -v "$1" >/dev/null 2>&1; }

# ── Uninstall ──────────────────────────────────────────────────────
uninstall() {
    say "Removing QuantoScript from $PREFIX"
    rm -f  "$PREFIX/bin/$BIN_NAME"
    rm -rf "$PREFIX/$DATA_SUBDIR"
    rmdir "$PREFIX/bin" "$PREFIX/share" 2>/dev/null || true
    rmdir "$PREFIX" 2>/dev/null || true
    remove_profile_block
    ok "Uninstalled. Open a new terminal for the PATH change to take effect."
    exit 0
}

# ── Detect target triple ───────────────────────────────────────────
detect_target() {
    os=$(uname -s)
    arch=$(uname -m)
    case "$os" in
        Darwin) os_part="apple-darwin" ;;
        Linux)  os_part="unknown-linux-gnu" ;;
        *)      die "unsupported OS: $os (Windows users: use install.ps1)" ;;
    esac
    case "$arch" in
        x86_64|amd64)   arch_part="x86_64" ;;
        arm64|aarch64)  arch_part="aarch64" ;;
        *)              die "unsupported architecture: $arch" ;;
    esac
    printf '%s-%s' "$arch_part" "$os_part"
}

# ── HTTP helpers ───────────────────────────────────────────────────
fetch() {  # fetch <url> <output-file>
    if need curl; then
        curl -fsSL "$1" -o "$2"
    elif need wget; then
        wget -qO "$2" "$1"
    else
        die "need curl or wget to download files"
    fi
}

fetch_stdout() {  # fetch <url> to stdout
    if need curl; then
        curl -fsSL "$1"
    elif need wget; then
        wget -qO- "$1"
    else
        die "need curl or wget to download files"
    fi
}

latest_version() {
    # Parse the tag_name from the GitHub "latest release" API without jq.
    fetch_stdout "https://api.github.com/repos/$REPO/releases/latest" \
        | grep -m1 '"tag_name"' \
        | sed -E 's/.*"tag_name" *: *"v?([^"]+)".*/\1/'
}

verify_checksum() {  # verify_checksum <file> <sha-file>
    [ -f "$2" ] || { warn "no checksum file; skipping verification"; return 0; }
    expected=$(awk '{print $1}' "$2")
    if need sha256sum; then
        actual=$(sha256sum "$1" | awk '{print $1}')
    elif need shasum; then
        actual=$(shasum -a 256 "$1" | awk '{print $1}')
    else
        warn "no sha256 tool; skipping checksum verification"; return 0
    fi
    [ "$expected" = "$actual" ] || die "checksum mismatch for $(basename "$1")"
    ok "  checksum verified"
}

# ── Profile / PATH management ──────────────────────────────────────
BEGIN_MARK="# >>> quantoscript >>>"
END_MARK="# <<< quantoscript <<<"

profile_files() {
    # Emit the shell rc files we should update.
    case "${SHELL:-}" in
        *zsh)  printf '%s\n' "${ZDOTDIR:-$HOME}/.zshrc" ;;
        *bash) printf '%s\n' "$HOME/.bashrc" ;;
    esac
    printf '%s\n' "$HOME/.profile"
}

remove_profile_block() {
    profile_files | sort -u | while IFS= read -r rc; do
        [ -f "$rc" ] || continue
        grep -qF "$BEGIN_MARK" "$rc" 2>/dev/null || continue
        tmp="$rc.qs.tmp"
        awk -v b="$BEGIN_MARK" -v e="$END_MARK" '
            $0==b {skip=1} skip==0 {print} $0==e {skip=0}
        ' "$rc" > "$tmp" && mv "$tmp" "$rc"
    done
}

update_profile() {
    [ "${NO_MODIFY_PATH:-0}" = "1" ] && return 0
    remove_profile_block
    profile_files | sort -u | while IFS= read -r rc; do
        {
            printf '\n%s\n' "$BEGIN_MARK"
            printf 'export QUANTO_HOME="%s"\n' "$PREFIX/$DATA_SUBDIR"
            printf 'case ":$PATH:" in *":%s/bin:"*) ;; *) export PATH="%s/bin:$PATH" ;; esac\n' "$PREFIX" "$PREFIX"
            printf '%s\n' "$END_MARK"
        } >> "$rc"
        say "  updated $rc"
    done
}

# ── Main install ───────────────────────────────────────────────────
do_install() {
    target=$(detect_target)
    say "${BOLD}Installing QuantoScript${RESET}"
    say "  target:  $target"
    say "  prefix:  $PREFIX"

    tmp=$(mktemp -d 2>/dev/null || mktemp -d -t quanto)
    trap 'rm -rf "$tmp"' EXIT INT TERM

    if [ -n "${QS_ARCHIVE:-}" ]; then
        [ -f "$QS_ARCHIVE" ] || die "QS_ARCHIVE not found: $QS_ARCHIVE"
        say "  source:  $QS_ARCHIVE (local)"
        cp "$QS_ARCHIVE" "$tmp/pkg.tar.gz"
    else
        version="${QS_VERSION:-}"
        if [ -z "$version" ]; then
            say "  resolving latest release..."
            version=$(latest_version)
            [ -n "$version" ] || die "could not determine the latest release. Set QS_VERSION or check that $REPO has published releases."
        fi
        say "  version: $version"
        asset="quantoscript-${version}-${target}.tar.gz"
        base="https://github.com/$REPO/releases/download/v${version}"
        say "  downloading $asset ..."
        if ! fetch "$base/$asset" "$tmp/pkg.tar.gz"; then
            die "no prebuilt binary for $target (v$version). Build from source: https://github.com/$REPO#building-from-source"
        fi
        fetch "$base/$asset.sha256" "$tmp/pkg.sha256" 2>/dev/null || true
        verify_checksum "$tmp/pkg.tar.gz" "$tmp/pkg.sha256"
    fi

    say "  extracting..."
    tar -xzf "$tmp/pkg.tar.gz" -C "$tmp"
    pkgdir=$(find "$tmp" -maxdepth 1 -type d -name 'quantoscript-*' | head -n1)
    [ -n "$pkgdir" ] || pkgdir=$(find "$tmp" -maxdepth 2 -name "$BIN_NAME" -type f -exec dirname {} \; | head -n1 | sed 's#/bin$##')
    [ -d "$pkgdir" ] || die "unexpected archive layout"
    [ -f "$pkgdir/bin/$BIN_NAME" ] || die "binary not found in archive"

    # Install into: PREFIX/bin/qs and PREFIX/share/quantoscript/{stdlib,examples}
    mkdir -p "$PREFIX/bin" "$PREFIX/$DATA_SUBDIR"
    command install -m 0755 "$pkgdir/bin/$BIN_NAME" "$PREFIX/bin/$BIN_NAME" 2>/dev/null \
        || { cp "$pkgdir/bin/$BIN_NAME" "$PREFIX/bin/$BIN_NAME"; chmod 0755 "$PREFIX/bin/$BIN_NAME"; }
    rm -rf "$PREFIX/$DATA_SUBDIR/stdlib" "$PREFIX/$DATA_SUBDIR/examples"
    [ -d "$pkgdir/stdlib" ]   && cp -R "$pkgdir/stdlib"   "$PREFIX/$DATA_SUBDIR/stdlib"
    [ -d "$pkgdir/examples" ] && cp -R "$pkgdir/examples" "$PREFIX/$DATA_SUBDIR/examples"
    ok "  installed $BIN_NAME to $PREFIX/bin/$BIN_NAME"

    update_profile

    # Verify
    if QUANTO_HOME="$PREFIX/$DATA_SUBDIR" "$PREFIX/bin/$BIN_NAME" version >/dev/null 2>&1; then
        installed_ver=$(QUANTO_HOME="$PREFIX/$DATA_SUBDIR" "$PREFIX/bin/$BIN_NAME" version)
        printf '\n'
        ok "${BOLD}$installed_ver installed successfully!${RESET}"
    else
        die "installation completed but '$BIN_NAME version' failed to run"
    fi

    printf '\n'
    say "Next steps:"
    printf '  Restart your terminal, or run:  %sexport PATH="%s/bin:$PATH"%s\n' "$BOLD" "$PREFIX" "$RESET"
    printf '  Then try:                       %sqs --help%s\n' "$BOLD" "$RESET"
    printf '                                  %sqs %s/examples/full_tour.qs%s\n' "$BOLD" "$PREFIX/$DATA_SUBDIR" "$RESET"
}

# ── Entry point ────────────────────────────────────────────────────
case "${1:-}" in
    --uninstall) uninstall ;;
    --help|-h)
        sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
        exit 0 ;;
    --version)
        [ -n "${2:-}" ] || die "--version requires an argument"
        QS_VERSION="$2"; do_install ;;
    "") do_install ;;
    *) die "unknown option: $1 (try --help)" ;;
esac
