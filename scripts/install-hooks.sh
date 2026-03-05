#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

git config core.hooksPath .githooks
chmod +x .githooks/pre-commit
chmod +x .githooks/pre-push

echo "Git hooks installed successfully."
echo "hooksPath=$(git config --get core.hooksPath)"
