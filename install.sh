#!/usr/bin/env bash
# install.sh - install seekey for any Wayland environment
#
# What it does:
#   1. Detect the distro and ensure build dependencies (gtk4, libevdev,
#      ncursesw, json-glib, gettext, pkg-config, gcc, make) are installed.
#   2. Build seekey with `make`.
#   3. Install the binary to ~/.local/bin (or /usr/local/bin with --system).
#   4. Install the example config and desktop application entry.
#   5. Install a udev rule that grants the active local session access to
#      /dev/input/event*; persistent `input` group access is opt-in fallback.
#
# It does NOT install autostart entries; see the wiki (Autostart page) for
# compositor-specific startup instructions.

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "${SCRIPT_DIR}"

# ---------- Defaults & argument parsing --------------------------------

PREFIX="${HOME}/.local"
BINDIR=""
DATADIR=""
APPLICATIONSDIR=""
LOCALEDIR=""
SYSTEM_INSTALL=false
SETUP_INPUT=true
UNINSTALL=false
FORCE=false
DRY_RUN=false
INSTALL_DEPS=true
USE_INPUT_GROUP=false

usage() {
    cat <<EOF
Usage: ./install.sh [OPTIONS]

Options:
  --user            Install to \$HOME/.local (default)
  --system          Install to /usr/local (requires sudo)
  --no-input        Skip input-device access setup
  --input-group     Also grant persistent access through the input group
  --force           Overwrite an existing Seekey udev rule
  --uninstall       Reverse a previous install
  --dry-run         Print what would be done, do nothing
  --no-deps         Don't try to install build dependencies
  -h, --help        Show this help

After install:
  - Run 'seekey' to start
  - Run 'seekey --config-gui' to open graphical settings
  - Run 'seekey --config-tui' to edit settings
  - Active local sessions get input access through a udev/logind ACL
  - If that is unavailable, rerun with --input-group and log out/in
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user)        PREFIX="${HOME}/.local"; SYSTEM_INSTALL=false ;;
        --system)      PREFIX="/usr/local"; SYSTEM_INSTALL=true ;;
        --no-input)    SETUP_INPUT=false ;;
        --input-group)   USE_INPUT_GROUP=true ;;
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
APPLICATIONSDIR="${PREFIX}/share/applications"
LOCALEDIR="${PREFIX}/share/locale"
ROOT_CMD=()
if (( EUID != 0 )); then
    ROOT_CMD=(sudo)
fi
INSTALL_CMD=()
if $SYSTEM_INSTALL; then
    if (( EUID != 0 )) && ! $DRY_RUN && ! command -v sudo >/dev/null 2>&1; then
        echo "sudo is required for --system" >&2
        exit 1
    fi
    INSTALL_CMD=("${ROOT_CMD[@]}")
fi
CURRENT_USER="${SUDO_USER:-${USER:-$(id -un)}}"
RUNNING_USER="$(id -un)"
UDEV_RULE_PATH="/etc/udev/rules.d/70-seekey-input.rules"
LEGACY_UDEV_RULE_PATH="/etc/udev/rules.d/99-seekey.rules"

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

require_root_tool() {
    if (( EUID != 0 )) && ! $DRY_RUN && ! command -v sudo >/dev/null 2>&1; then
        die "sudo is required for system package, udev, or group changes"
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
        PM_INSTALL_DEPS_ARGS=(pacman -S --needed --noconfirm
                              gtk4 libevdev ncurses json-glib
                              gettext pkgconf gcc make)
        ;;
    dnf)
        PM_INSTALL_DEPS_ARGS=(dnf install -y
                              gtk4-devel libevdev-devel ncurses-devel
                              json-glib-devel pkgconf-pkg-config
                              gettext gcc make)
        ;;
    apt)
        PM_INSTALL_DEPS_ARGS=(apt install -y
                              libgtk-4-dev libevdev-dev libncurses-dev
                              libjson-glib-dev gettext pkg-config build-essential)
        ;;
    zypper)
        PM_INSTALL_DEPS_ARGS=(zypper install -y
                              gtk4-devel libevdev-devel ncurses-devel
                              json-glib-devel gettext-tools pkg-config gcc make)
        ;;
    apk)
        PM_INSTALL_DEPS_ARGS=(apk add gtk4-dev libevdev-dev
                              ncurses-dev json-glib-dev pkgconf
                              gettext gcc make musl-dev)
        ;;
    xbps-install)
        PM_INSTALL_DEPS_ARGS=(xbps-install -y
                              gtk4-devel libevdev-devel ncurses-devel
                              json-glib-devel gettext pkg-config gcc make)
        ;;
    *)
        warn "Unknown package manager. Please install these manually:"
        warn "  - gtk4 (development files)"
        warn "  - libevdev (development files)"
        warn "  - ncursesw (development files)"
        warn "  - json-glib (development files)"
        warn "  - gettext, pkg-config, gcc, make"
        ;;
esac

# ---------- Dependency check ------------------------------------------

check_build_deps() {
    local missing=()
    for tool in pkg-config make gcc xgettext msgmerge msgfmt; do
        command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
    done
    if command -v pkg-config >/dev/null 2>&1; then
        for pkg in gtk4 libevdev ncursesw json-glib-1.0; do
            pkg-config --exists "$pkg" 2>/dev/null || missing+=("$pkg")
        done
    fi
    printf '%s\n' "${missing[@]}"
}

MISSING=()
if ! $UNINSTALL; then
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
    require_root_tool
    run "${ROOT_CMD[@]}" "${PM_INSTALL_DEPS_ARGS[@]}"
    if ! $DRY_RUN; then
        MISSING=()
        while IFS= read -r line; do
            [[ -n "$line" ]] && MISSING+=("$line")
        done < <(check_build_deps)
        if [[ ${#MISSING[@]} -gt 0 ]]; then
            die "Dependencies still missing after install: ${MISSING[*]}"
        fi
    fi
fi

# ---------- Compositor hints -------------------------------------------

print_compositor_hints() {
    local desktop="${XDG_CURRENT_DESKTOP:-unknown}"
    local desktop_lower="${desktop,,}"
    local wayland="${WAYLAND_DISPLAY:-}"
    echo
    echo "  Detected environment:"
    echo "    Desktop: ${desktop}"
    if [[ -n "${wayland}" ]]; then
        echo "    Wayland display: ${wayland}"
    else
        warn "  WAYLAND_DISPLAY is not set. seekey needs a Wayland session."
    fi
    case "${desktop_lower}" in
        *niri*|*hyprland*|*sway*|*river*|*wayfire*|*labwc*)
            if pkg-config --exists gtk4-layer-shell-0 2>/dev/null ||
               pkg-config --exists gtk4-layer-shell 2>/dev/null; then
                ok "  Compositor and build support wlr-layer-shell."
            else
                warn "  Compositor supports wlr-layer-shell, but gtk4-layer-shell"
                warn "  was not detected. Install it, then rebuild Seekey."
            fi ;;
        *gnome*)
            warn "GNOME does not support wlr-layer-shell. In the TUI set"
            warn "  layer-shell=off (or pass --no-layer-shell) to use a fallback window." ;;
        *kde*|*plasma*)
            warn "KDE Plasma does not support wlr-layer-shell. In the TUI set"
            warn "  layer-shell=off to use a fallback window." ;;
        *)
            warn "Unknown compositor. Try running seekey and check the output." ;;
    esac
}

# ---------- Undo a previous install -----------------------------------

do_uninstall() {
    log "Uninstalling seekey from ${PREFIX}"
    run "${INSTALL_CMD[@]}" rm -f "${BINDIR}/seekey"
    run "${INSTALL_CMD[@]}" rm -rf "${DATADIR}"
    run "${INSTALL_CMD[@]}" rm -f "${APPLICATIONSDIR}/dev.seekey.desktop"
    local mo
    for mo in "${LOCALEDIR}"/*/LC_MESSAGES/seekey.mo; do
        [[ -f "${mo}" ]] || continue
        run "${INSTALL_CMD[@]}" rm -f "${mo}"
    done
    if $SETUP_INPUT; then
        remove_udev_rules
    fi
    ok "Uninstalled binary and data files"
    ok "Note: input group membership and your config were left untouched"
    exit 0
}

# ---------- Input-device access ----------------------------------------

is_seekey_udev_rule() {
    local path="$1"
    [[ -f "${path}" ]] && grep -q '^# [Ss]eekey:' "${path}"
}

reload_input_udev_rules() {
    run "${ROOT_CMD[@]}" udevadm control --reload-rules
    run "${ROOT_CMD[@]}" udevadm trigger --subsystem-match=input
}

remove_udev_rules() {
    local removed=false rule_path
    for rule_path in "${UDEV_RULE_PATH}" "${LEGACY_UDEV_RULE_PATH}"; do
        if ! is_seekey_udev_rule "${rule_path}"; then
            continue
        fi
        require_root_tool
        log "Removing Seekey udev rule ${rule_path}"
        run "${ROOT_CMD[@]}" rm -f "${rule_path}"
        removed=true
    done
    if $removed; then
        reload_input_udev_rules
    fi
}

install_udev_rule() {
    require_root_tool
    local source_path="${SCRIPT_DIR}/data/70-seekey-input.rules"
    [[ -f "${source_path}" ]] || die "Missing udev rule: ${source_path}"

    if is_seekey_udev_rule "${LEGACY_UDEV_RULE_PATH}"; then
        log "Removing legacy udev rule ${LEGACY_UDEV_RULE_PATH}"
        run "${ROOT_CMD[@]}" rm -f "${LEGACY_UDEV_RULE_PATH}"
    fi

    if [[ -f "${UDEV_RULE_PATH}" ]] && ! $FORCE; then
        log "udev rule already present at ${UDEV_RULE_PATH} (use --force to overwrite)"
    else
        log "Installing active-session udev rule to ${UDEV_RULE_PATH}"
        run "${ROOT_CMD[@]}" install -Dm644 "${source_path}" "${UDEV_RULE_PATH}"
    fi
    reload_input_udev_rules
}

user_can_read_input_event() {
    local path="$1"
    if [[ "${RUNNING_USER}" == "${CURRENT_USER}" ]]; then
        [[ -r "${path}" ]]
    elif command -v runuser >/dev/null 2>&1; then
        runuser -u "${CURRENT_USER}" -- test -r "${path}"
    elif command -v sudo >/dev/null 2>&1; then
        sudo -u "${CURRENT_USER}" test -r "${path}"
    else
        return 1
    fi
}

input_events_are_readable() {
    local found=false path
    for path in /dev/input/event*; do
        [[ -e "${path}" ]] || continue
        found=true
        user_can_read_input_event "${path}" || return 1
    done
    $found
}

setup_input_group() {
    require_root_tool
    if ! getent group input >/dev/null; then
        log "Creating 'input' group"
        run "${ROOT_CMD[@]}" groupadd input
    fi
    if id -nG "${CURRENT_USER}" 2>/dev/null | tr ' ' '\n' | grep -qx input; then
        ok "User ${CURRENT_USER} is already in the 'input' group"
    else
        log "Adding ${CURRENT_USER} to the 'input' group"
        run "${ROOT_CMD[@]}" usermod -aG input "${CURRENT_USER}"
        warn "*** Log out and back in for the input group to take effect ***"
    fi
}

setup_input_access() {
    if $USE_INPUT_GROUP; then
        setup_input_group
        return
    fi
    if $DRY_RUN; then
        log "Input access will use the active local session ACL"
        return
    fi
    if input_events_are_readable; then
        ok "Active local user can read input event devices"
        return
    fi
    warn "Active-session input ACL is unavailable on this system."
    warn "  Rerun with --input-group for the persistent group fallback."
}

# ---------- Build ------------------------------------------------------

build_seekey() {
    log "Building seekey"
    run make -B PREFIX="${PREFIX}"
}

# ---------- Install binary & data -------------------------------------

install_files() {
    log "Installing binary to ${BINDIR}/seekey"
    run "${INSTALL_CMD[@]}" install -Dm755 seekey "${BINDIR}/seekey"

    if [[ -f seekey.ini.example ]]; then
        log "Installing example config to ${DATADIR}/seekey.ini.example"
        run "${INSTALL_CMD[@]}" install -Dm644 seekey.ini.example \
            "${DATADIR}/seekey.ini.example"
    fi

    if [[ -f data/dev.seekey.desktop ]]; then
        log "Installing desktop entry to ${APPLICATIONSDIR}/dev.seekey.desktop"
        run "${INSTALL_CMD[@]}" install -Dm644 data/dev.seekey.desktop \
            "${APPLICATIONSDIR}/dev.seekey.desktop"
    fi

    local mo lang
    for mo in locale/*/LC_MESSAGES/seekey.mo; do
        [[ -f "${mo}" ]] || continue
        lang="${mo#locale/}"
        lang="${lang%%/*}"
        log "Installing ${lang} translation"
        run "${INSTALL_CMD[@]}" install -Dm644 "${mo}" \
            "${LOCALEDIR}/${lang}/LC_MESSAGES/seekey.mo"
    done

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
echo "  Desktop:  ${APPLICATIONSDIR}/dev.seekey.desktop"
echo "  Locale:   ${LOCALEDIR}"
$DRY_RUN && echo "  (dry-run: no changes will be made)"
echo

build_seekey
install_files

if $SETUP_INPUT; then
    install_udev_rule
    setup_input_access
fi

print_compositor_hints

echo
ok "Done."
echo "  Run:    ${BINDIR}/seekey"
echo "  GUI:    ${BINDIR}/seekey --config-gui"
echo "  TUI:    ${BINDIR}/seekey --config-tui"
if ! command -v fuzzel >/dev/null 2>&1; then
    warn "fuzzel is not installed. The settings menu (--config-gui) will use"
    warn "  the built-in fallback menu; install fuzzel for a native, themed"
    warn "  menu that follows your fuzzel.ini."
fi
echo
echo "  To uninstall: ${BINDIR}/seekey  ->  ./install.sh --uninstall"
