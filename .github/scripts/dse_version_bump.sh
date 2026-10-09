#!/usr/bin/env bash

set -euo pipefail

makefile="Makefile"
dry_run="${DRY_RUN:-0}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --file)
            if [[ $# -lt 2 ]]; then
                echo "error: --file requires a path" >&2
                exit 2
            fi
            makefile="$2"
            shift 2
            ;;
        --dry-run)
            dry_run=1
            shift
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

if [[ ! -f "$makefile" ]]; then
    echo "error: Makefile not found: $makefile" >&2
    exit 1
fi

repo_stems=$(
    grep -E '^[[:space:]]*(export[[:space:]]+)?DSE_[A-Z0-9_]+_REPO[[:space:]]*(\?=|:=|=)[[:space:]]*https?://' "$makefile" |
        sed -E 's#^[[:space:]]*(export[[:space:]]+)?(DSE_[A-Z0-9_]+)_REPO[[:space:]]*(\?=|:=|=)[[:space:]]*https?://.*#\2#' |
        sort -u || true
)

declare -A updates

while IFS= read -r stem; do
    [[ -z "$stem" ]] && continue

    version_pattern="^[[:space:]]*(export[[:space:]]+)?${stem}_VERSION[[:space:]]*(\\?=|:=|=)[[:space:]]*v?[0-9]+(\\.[0-9]+){1,3}([[:space:]]*(#.*)?)$"
    if ! grep -Eq "$version_pattern" "$makefile"; then
        continue
    fi

    repo_url=$(
        grep -E "^[[:space:]]*(export[[:space:]]+)?${stem}_REPO[[:space:]]*(\\?=|:=|=)[[:space:]]*https?://" "$makefile" |
            sed -nE "s#^[[:space:]]*(export[[:space:]]+)?${stem}_REPO[[:space:]]*(\\?=|:=|=)[[:space:]]*(https?://[^[:space:]#]+).*#\\3#p" |
            head -n 1
    )
    current_version=$(
        grep -E "$version_pattern" "$makefile" |
            sed -E 's|^[^=]*=[[:space:]]*v?([0-9]+(\.[0-9]+){1,3}).*|\1|' |
            head -n 1
    )

    remote_tags=$(git ls-remote --tags --refs "${repo_url%/}")
    latest_version=$(
        printf '%s\n' "$remote_tags" |
            sed -E 's|.*refs/tags/||' |
            grep -E '^v?[0-9]+(\.[0-9]+){1,3}$' |
            sed -E 's/^v//' |
            sort -V |
            tail -n 1 || true
    )

    if [[ -z "$latest_version" ]]; then
        echo "skip: $stem ($repo_url) has no numeric semantic-version tags"
        continue
    fi

    highest_version=$(printf '%s\n%s\n' "$current_version" "$latest_version" | sort -V | tail -n 1)
    if [[ "$highest_version" != "$latest_version" || "$current_version" == "$latest_version" ]]; then
        echo "up to date: $stem $current_version"
        continue
    fi

    echo "update: $stem $current_version -> $latest_version ($repo_url)"
    updates["$stem"]="$latest_version"
done <<< "$repo_stems"

if [[ "$dry_run" == "1" ]]; then
    exit 0
fi

for stem in "${!updates[@]}"; do
    latest_version="${updates[$stem]}"
    sed -i -E \
        "s#^([[:space:]]*(export[[:space:]]+)?${stem}_VERSION[[:space:]]*(\\?=|:=|=)[[:space:]]*)v?[0-9]+(\\.[0-9]+){1,3}#\\1${latest_version}#" \
        "$makefile"
done