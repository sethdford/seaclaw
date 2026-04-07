#!/bin/sh
# human install script — run with: curl -fsSL https://h-uman.ai/install.sh | sh
# POSIX-compatible, no bash required

# ANSI colors (work without tput where unavailable)
# shellcheck disable=SC2039
green() { [ -t 1 ] && printf '\033[32m%s\033[0m\n' "$1" || printf '%s\n' "$1"; }
# shellcheck disable=SC2039
red()   { [ -t 1 ] && printf '\033[31m%s\033[0m\n' "$1" || printf '%s\n' "$1"; }
# shellcheck disable=SC2039
bold()  { [ -t 1 ] && printf '\033[1m%s\033[0m' "$1" || printf '%s' "$1"; }

REPO="sethdford/h-uman"
INSTALL_URL="https://h-uman.ai/install.sh"

print_help() {
    bold "human "
    printf "— install script\n\n"
    printf "Usage: curl -fsSL %s | sh\n" "$INSTALL_URL"
    printf "       curl -fsSL %s | sh -s -- --help\n\n" "$INSTALL_URL"
    printf "Options:\n"
    printf "  --help    Show this help and exit\n\n"
    printf "Detects OS (Linux/macOS) and arch (x86_64/aarch64), downloads the\n"
    printf "appropriate binary from GitHub releases, and installs to\n"
    printf "/usr/local/bin (or ~/.local/bin if not writable).\n"
}

detect_arch() {
    a=$(uname -m 2>/dev/null)
    case "$a" in
        x86_64|amd64)  echo "x86_64" ;;
        aarch64|arm64)  echo "aarch64" ;;
        *) red "Unsupported architecture: $a" ; exit 1 ;;
    esac
}

detect_os() {
    os=$(uname -s 2>/dev/null)
    case "$os" in
        Linux)  echo "linux" ;;
        Darwin) echo "macos" ;;
        *)      red "Unsupported OS: $os" ; exit 1 ;;
    esac
}

download() {
    url="$1"
    out="$2"
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL -o "$out" "$url"
        return $?
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$out" "$url"
        return $?
    else
        red "Neither curl nor wget found. Please install one of them."
        exit 1
    fi
}

main() {
    for arg in "$@"; do
        case "$arg" in
            --help|-h) print_help; exit 0 ;;
        esac
    done

    arch=$(detect_arch)
    os=$(detect_os)

    case "${os}-${arch}" in
        linux-x86_64)   bin_name="human-linux-x86_64.bin" ;;
        linux-aarch64)  bin_name="human-linux-aarch64.bin" ;;
        macos-aarch64)  bin_name="human-macos-aarch64.bin" ;;
        *)
            red "Unsupported platform: $os $arch"
            exit 1
            ;;
    esac

    # Resolve version from latest GitHub release
    api_url="https://api.github.com/repos/$REPO/releases/latest"
    version=""
    if command -v curl >/dev/null 2>&1; then
        version=$(curl -fsSL "$api_url" 2>/dev/null | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -1)
    elif command -v wget >/dev/null 2>&1; then
        version=$(wget -q -O - "$api_url" 2>/dev/null | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -1)
    fi

    if [ -z "$version" ]; then
        red "Could not fetch latest release version. Check network or try again later."
        exit 1
    fi

    url="https://github.com/$REPO/releases/download/$version/$bin_name"
    tmpdir="${TMPDIR:-/tmp}"
    tmp="$tmpdir/human-$$.bin"

    # Debian/Ubuntu: prefer .deb package when dpkg is available
    used_deb=false
    if [ "$os" = "linux" ] && [ "$arch" = "x86_64" ] && command -v dpkg >/dev/null 2>&1; then
        deb_ver="${version#v}"
        deb_name="human_${deb_ver}_amd64.deb"
        deb_url="https://github.com/$REPO/releases/download/$version/$deb_name"
        deb_tmp="$tmpdir/$deb_name"
        printf "Debian/Ubuntu detected — trying .deb package...\n"
        if download "$deb_url" "$deb_tmp" 2>/dev/null; then
            if sudo dpkg -i "$deb_tmp" 2>/dev/null; then
                green "human v$version installed via .deb package"
                rm -f "$deb_tmp" 2>/dev/null
                install_path="$(command -v human 2>/dev/null || echo /usr/bin/human)"
                used_deb=true
            else
                printf "dpkg install failed, falling back to binary install...\n"
                rm -f "$deb_tmp" 2>/dev/null
            fi
        else
            printf ".deb not available for this release, using binary install...\n"
        fi
    fi

    if [ "$used_deb" = "false" ]; then
        printf "Installing human %s (%s-%s)...\n" "$version" "$os" "$arch"
        if ! download "$url" "$tmp"; then
            red "Download failed: $url"
            rm -f "$tmp" 2>/dev/null
            exit 1
        fi

        install_dir="/usr/local/bin"
        if [ ! -w "/usr/local/bin" ] 2>/dev/null; then
            install_dir="$HOME/.local/bin"
            mkdir -p "$install_dir" 2>/dev/null
            if [ ! -d "$install_dir" ] || [ ! -w "$install_dir" ]; then
                red "Cannot write to /usr/local/bin or $install_dir. Run with sudo or fix permissions."
                rm -f "$tmp" 2>/dev/null
                exit 1
            fi
        fi

        install_path="$install_dir/human"
        if ! mv "$tmp" "$install_path" 2>/dev/null; then
            red "Failed to move binary to $install_path"
            rm -f "$tmp" 2>/dev/null
            exit 1
        fi

        chmod +x "$install_path"

        green "human v$version installed to $install_path"

        if [ "$install_dir" = "$HOME/.local/bin" ]; then
            printf "\nEnsure %s is in your PATH:\n" "$install_dir"
            printf "  export PATH=\"\$PATH:%s\"\n" "$install_dir"
        fi
    fi

    "$install_path" --version 2>/dev/null

    # macOS: build and install human-ondevice for on-device Apple Intelligence
    if [ "$os" = "macos" ]; then
        printf "\n"
        bold "Apple Intelligence (on-device, free)"
        printf "\n"
        printf "Your Mac has a built-in LLM via Apple Intelligence.\n"
        printf "Build human-ondevice server for zero-dependency on-device inference? [Y/n] "
        read -r answer </dev/tty 2>/dev/null || answer="y"
        case "$answer" in
            [nN]*) printf "Skipping on-device server build.\n" ;;
            *)
                if command -v swift >/dev/null 2>&1; then
                    printf "Building human-ondevice...\n"
                    ondevice_dir="$(mktemp -d)"
                    if git clone --depth 1 https://github.com/h-uman/human.git "$ondevice_dir" 2>/dev/null; then
                        if (cd "$ondevice_dir/apps/tools/human-ondevice" && swift build -c release 2>/dev/null); then
                            cp "$ondevice_dir/apps/tools/human-ondevice/.build/release/human-ondevice" "$(dirname "$install_path")/"
                            green "human-ondevice installed — Apple Intelligence is ready."
                        else
                            red "Build failed. You can build it later from the source tree:"
                            printf "  cd apps/tools/human-ondevice && swift build -c release\n"
                        fi
                    fi
                    rm -rf "$ondevice_dir"
                else
                    printf "Swift toolchain not found. Install Xcode Command Line Tools then build:\n"
                    printf "  cd apps/tools/human-ondevice && swift build -c release\n\n"
                fi
                ;;
        esac
    fi

    # Run onboard if this is a fresh install (no existing config)
    if [ ! -f "$HOME/.human/config.json" ]; then
        printf "\n"
        bold "First-time setup"
        printf "\n"
        if [ "$os" = "macos" ]; then
            printf "Configuring with Apple Intelligence defaults...\n"
            "$install_path" onboard --apple 2>/dev/null || "$install_path" onboard 2>/dev/null || true
        else
            printf "Run 'human onboard' to configure your provider and API key.\n"
        fi
    fi

    printf "\n"
    green "You're ready! Run 'human agent' to start chatting."
}

main "$@"
