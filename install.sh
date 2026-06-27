#!/usr/bin/env bash
# install.sh - install seekey for any Wayland environment
#
# What it does:
#   1. Detect the distro and ensure build dependencies (gtk4, libevdev,
#      ncursesw, json-glib, pkg-config, gcc, make) are installed.
#   2. Build seekey with `make`.
#   3. Install the binary to ~/.local/bin (or /usr/local/bin with --system).
#   4. Install the example config to ~/.local/share/seekey (or /usr/local).
#   5. Install a udev rule so /dev/input/event* is readable by the `input`
#      group, and add the current user to that group if needed.
#
# It does NOT install autostart entries; see the wiki (Autostart page) for
# compositor-specific startup instructions.

set -euo pipefail

# ---------- Defaults & argument parsing --------------------------------

PREFIX="${HOME}/.local"
BINDIR=""
DATADIR=""
SETUP_INPUT=true
UNINSTALL=false
FORCE=false
DRY_RUN=false
INSTALL_DEPS=true

usage() {
    cat <<EOF
Usage: ./install.sh [OPTIONS]

Options:
  --user            Install to \$HOME/.local (default)
  --system          Install to /usr/local (requires sudo)
  --no-input        Skip udev rule + input group setup
  --force           Overwrite existing files
  --uninstall       Reverse a previous install
  --dry-run         Print what would be done, do nothing
  --no-deps         Don't try to install build dependencies
  -h, --help        Show this help

After install:
  - Run 'seekey' to start
  - Run 'seekey --config-tui' to edit settings
  - Log out and back in for input group changes to take effect
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user)        PREFIX="${HOME}/.local" ;;
        --system)      PREFIX="/usr/local" ;;
        --no-input)    SETUP_INPUT=false ;;
        --force)       FORCE=true ;;
        --uninstall)   UNINSTALL=true ;;
        --dry-run)     DRY_RUN=true ;;
        --no-deps)     INSTALL_DEPS=false ;;
        -h|--help)     usage; exit 0 ;;
        *)             echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
    shift
done

BINDIR="${PREFIX}/bin"
DATADIR="${PREFIX}/share/seekey"
SUDO=""
if [[ ! -w "${PREFIX}" ]]; then
    SUDO="sudo"
fi

# ---------- Logging helpers --------------------------------------------

log()  { printf '  \033[1;34m*\033[0m %s\n' "$*"; }
ok()   { printf '  \033[1;32m✓\033[0m %s\n' "$*"; }
warn() { printf '  \033[1;33m!\033[0m %s\n' "$*" >&2; }
die()  { printf '  \033[1;31m✗\033[0m %s\n' "$*" >&2; exit 1; }

run() {
    if $DRY_RUN; then
        printf '    $ %s\n' "$*"
    else
        "$@"
    fi
}

# ---------- Distro / package manager detection -------------------------

detect_pkg_manager() {
    if command -v pacman >/dev/null 2>&1; then
        echo "pacman"
    elif command -v dnf >/dev/null 2>&1; then
        echo "dnf"
    elif command -v apt >/dev/null 2>&1; then
        echo "apt"
    elif command -v zypper >/dev/null 2>&1; then
        echo "zypper"
    elif command -v apk >/dev/null 2>&1; then
        echo "apk"
    elif command -v xbps-install >/dev/null 2>&1; then
        echo "xbps-install"
    else
        echo "unknown"
    fi
}

PM="$(detect_pkg_manager)"
log "Detected package manager: ${PM}"

PM_INSTALL_DEPS_ARGS=()
case "$PM" in
    pacman)
        PM_INSTALL_DEPS_ARGS=(sudo pacman -S --needed --noconfirm
                              gtk4 libevdev ncurses json-glib
                              pkgconf gcc make)
        PM_PRESENT() { pacman -Qi "$1" >/dev/null 2>&1; }
        ;;
    dnf)
        PM_INSTALL_DEPS_ARGS=(sudo dnf install -y
                              gtk4-devel libevdev-devel ncurses-devel
                              json-glib-devel pkgconf-pkg-config
                              gcc make)
        PM_PRESENT() { rpm -q "$1" >/dev/null 2>&1; }
        ;;
    apt)
        PM_INSTALL_DEPS_ARGS=(sudo apt install -y
                              libgtk-4-dev libevdev-dev libncursesw5-dev
                              libjson-glib-dev pkg-config build-essential)
        PM_PRESENT() { dpkg -s "$1" >/dev/null 2>&1; }
        ;;
    zypper)
        PM_INSTALL_DEPS_ARGS=(sudo zypper install -y
                              gtk4-devel libevdev-devel ncurses-devel
                              json-glib-devel pkg-config gcc make)
        PM_PRESENT() { rpm -q "$1" >/dev/null 2>&1; }
        ;;
    apk)
        PM_INSTALL_DEPS_ARGS=(sudo apk add gtk4-dev libevdev-dev
                              ncurses-dev json-glib-dev pkgconf
                              gcc make musl-dev)
        PM_PRESENT() { apk info -e "$1" >/dev/null 2>&1; }
        ;;
    xbps-install)
        PM_INSTALL_DEPS_ARGS=(sudo xbps-install -y
                              gtk4-devel libevdev-devel ncurses-devel
                              json-glib-devel pkg-config gcc make)
        PM_PRESENT() { xbps-query "$1" >/dev/null 2>&1; }
        ;;
    *)
        warn "Unknown package manager. Please install these manually:"
        warn "  - gtk4 (development files)"
        warn "  - libevdev (development files)"
        warn "  - ncursesw (development files)"
        warn "  - json-glib (development files)"
        warn "  - pkg-config, gcc, make"
        INSTALL_DEPS=false
        ;;
esac

# ---------- Dependency check ------------------------------------------

check_build_deps() {
    local missing=()
    for tool in pkg-config make gcc; do
        command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
    done
    for pkg in gtk4 libevdev ncursesw json-glib-1.0; do
        pkg-config --exists "$pkg" 2>/dev/null || missing+=("$pkg")
    done
    printf '%s\n' "${missing[@]}"
}

MISSING=()
if $INSTALL_DEPS; then
    while IFS= read -r line; do
        [[ -n "$line" ]] && MISSING+=("$line")
    done < <(check_build_deps)
fi

if [[ ${#MISSING[@]} -gt 0 ]]; then
    log "Missing build dependencies: ${MISSING[*]}"
    if ! $INSTALL_DEPS; then
        die "Cannot proceed without dependencies (--no-deps given)."
    fi
    if [[ "$PM" == "unknown" ]]; then
        die "No known package manager. Install dependencies manually."
    fi
    log "Installing via ${PM}..."
    run "${PM_INSTALL_DEPS_ARGS[@]}"
    MISSING=()
    while IFS= read -r line; do
        [[ -n "$line" ]] && MISSING+=("$line")
    done < <(check_build_deps)
    if [[ ${#MISSING[@]} -gt 0 ]]; then
        die "Dependencies still missing after install: ${MISSING[*]}"
    fi
fi

# ---------- Compositor hints -------------------------------------------

print_compositor_hints() {
    local desktop="${XDG_CURRENT_DESKTOP:-unknown}"
    local wayland="${WAYLAND_DISPLAY:-}"
    echo
    echo "  Detected environment:"
    echo "    Desktop: ${desktop}"
    if [[ -n "${wayland}" ]]; then
        echo "    Wayland display: ${wayland}"
    else
        warn "  WAYLAND_DISPLAY is not set. seekey needs a Wayland session."
    fi
    case "${desktop}" in
        *niri*|*Hyprland*|*sway*|*river*)
            ok "  Compositor supports wlr-layer-shell: default config should work." ;;
        *GNOME*)
            warn "GNOME does not support wlr-layer-shell. In the TUI set"
            warn "  layer-shell=off (or pass --no-layer-shell) to use a fallback window." ;;
        *KDE*|*Plasma*)
            warn "KDE Plasma does not support wlr-layer-shell. In the TUI set"
            warn "  layer-shell=off to use a fallback window." ;;
        *)
            warn "Unknown compositor. Try running seekey and check the output." ;;
    esac
}

# ---------- Undo a previous install -----------------------------------

do_uninstall() {
    log "Uninstalling seekey from ${PREFIX}"
    run ${SUDO} rm -f "${BINDIR}/seekey"
    run ${SUDO} rm -rf "${DATADIR}"
    if [[ -f "${HOME}/.config/autostart/seekey.desktop" ]]; then
        run rm -f "${HOME}/.config/autostart/seekey.desktop"
    fi
    if [[ -f /etc/udev/rules.d/99-seekey.rules ]]; then
        warn "Leaving /etc/udev/rules.d/99-seekey.rules in place"
        warn "  (remove it manually if you no longer need it: sudo rm /etc/udev/rules.d/99-seekey.rules)"
    fi
    ok "Uninstalled binary and data files"
    ok "Note: input group membership and your config were left untouched"
    exit 0
}

# ---------- udev rule + input group -----------------------------------

install_udev_rule() {
    local rule_path="/etc/udev/rules.d/99-seekey.rules"
    if [[ -f "${rule_path}" ]] && ! $FORCE; then
        log "udev rule already present at ${rule_path} (use --force to overwrite)"
        return
    fi
    local rule_content='# seekey: allow members of the input group to read /dev/input/event*
KERNEL=="event*", SUBSYSTEM=="input", GROUP="input", MODE="0640"'
    log "Installing udev rule to ${rule_path}"
    if $DRY_RUN; then
        printf '    $ sudo tee %s >/dev/null <<< <heredoc>\n' "${rule_path}"
    else
        printf '%s\n' "${rule_content}" | sudo tee "${rule_path}" >/dev/null
    fi
    run sudo udevadm control --reload-rules
    run sudo udevadm trigger
}

setup_input_group() {
    if ! getent group input >/dev/null; then
        log "Creating 'input' group"
        run sudo groupadd input
    fi
    if id -nG "${USER}" 2>/dev/null | tr ' ' '\n' | grep -qx input; then
        ok "User ${USER} is already in the 'input' group"
    else
        log "Adding ${USER} to the 'input' group"
        run sudo usermod -aG input "${USER}"
        warn "*** Log out and back in for the new group to take effect ***"
    fi
}

# ---------- Build ------------------------------------------------------

build_seekey() {
    log "Building seekey"
    run make
}

# ---------- Install binary & data -------------------------------------

install_files() {
    log "Installing binary to ${BINDIR}/seekey"
    run ${SUDO} install -Dm755 seekey "${BINDIR}/seekey"

    if [[ -f seekey.ini.example ]]; then
        log "Installing example config to ${DATADIR}/seekey.ini.example"
        run ${SUDO} install -Dm644 seekey.ini.example \
            "${DATADIR}/seekey.ini.example"
    fi

    if [[ ":${PATH}:" != *":${BINDIR}:"* ]]; then
        warn "${BINDIR} is not on your PATH."
        warn "  Add this to your shell rc (e.g. ~/.bashrc):"
        warn "    export PATH=\"${BINDIR}:\$PATH\""
    fi
}

# ---------- Main -------------------------------------------------------

if $UNINSTALL; then
    do_uninstall
fi

echo
echo "Installing seekey"
echo "  Prefix:   ${PREFIX}"
echo "  Bindir:   ${BINDIR}"
echo "  Datadir:  ${DATADIR}"
$DRY_RUN && echo "  (dry-run: no changes will be made)"
echo

build_seekey
install_files

if $SETUP_INPUT; then
    install_udev_rule
    setup_input_group
fi

print_compositor_hints

echo
ok "Done."
echo "  Run:    ${BINDIR}/seekey"
echo "  Edit:   ${BINDIR}/seekey --config-tui"
if $SETUP_INPUT && ! id -nG "${USER}" 2>/dev/null | tr ' ' '\n' | grep -qx input; then
    echo
    warn "Don't forget to log out and back in so the 'input' group takes effect."
fi
echo
echo "  To uninstall: ${BINDIR}/seekey  ->  ./install.sh --uninstall"
