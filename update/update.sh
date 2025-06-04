#!/bin/sh

REPO_URL="https://github.com/username/WebBrowser.git"
BRANCH="main"

if [ ! -d .git ]; then
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
