#!/bin/sh

REPO_URL="https://github.com/Kgkkjjj/WebBrowser.git"
# default branch to pull updates from
BRANCH="codex/build-web-browser-with-gtk-3-gui"

# ensure we operate from the project root even if the script is called from
# the update directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/.." || exit 1

if [ ! -d .git ]; then
    if [ -n "$(ls -A 2>/dev/null)" ]; then
        echo "No git repository found and directory is not empty." >&2
        echo "Please run this script in an existing git repository or an empty directory." >&2
        exit 1
    fi

    echo "No git repository found. Cloning..."
    git clone "$REPO_URL" . || {
        echo "Clone failed" >&2
        exit 1
    }
else
    echo "Fetching updates from $REPO_URL..."
    git pull "$REPO_URL" "$BRANCH" || {
        echo "Update failed" >&2
        exit 1
    }
fi

exit 0
