#!/usr/bin/env bash
#
# Remove the GitHub releases and git tags for a run of versions.
#
# Why this is a script you run rather than something already done: creating,
# editing and deleting releases is refused for the session type this repository
# is worked on from --
#
#   403 {"message":"Creating, editing, or deleting releases is not
#        permitted for this session type."}
#
# -- which is a deliberate guardrail rather than a missing permission, so it is
# not worked around. It needs a token of yours.
#
# Safe by default: prints what it would do and changes nothing until --yes.
# The release goes first and the tag second, so a release is never left
# pointing at a tag that has gone. Anything already absent is skipped, so it
# can be re-run after an interruption.
#
#   ./prune-releases.sh                 # show what would go
#   ./prune-releases.sh --yes           # actually do it
#   FIRST=31 LAST=45 ./prune-releases.sh --yes
#
# The release notes for the default range are kept in releases-backup/notes/,
# and releases-backup/tag-commits.txt records the commit each tag pointed at.
# Every one of those commits is on master, so no history is lost here: the
# versions stop being listed, they do not stop existing.
#
# Container images are NOT touched. ghcr.io/krippler/doom:1.10.31 and the rest
# stay pullable, because deleting them would break anyone pinned to one and
# that is a different decision from tidying a release list.
#
set -euo pipefail

REPO="${REPO:-Krippler/DOOM}"
FIRST="${FIRST:-31}"
LAST="${LAST:-45}"
MINOR="${MINOR:-1.10}"

TOKEN="${GH_TOKEN:-${GITHUB_TOKEN:-}}"

if [ -z "$TOKEN" ] && command -v gh >/dev/null 2>&1; then
    TOKEN="$(gh auth token 2>/dev/null || true)"
fi

if [ -z "$TOKEN" ]; then
    echo "No token. Set GH_TOKEN, or run 'gh auth login' first." >&2
    echo "It needs write access to $REPO -- a classic PAT with 'repo', or a" >&2
    echo "fine-grained one with Contents: read and write." >&2
    exit 1
fi

api() { curl -fsS -H "Authorization: Bearer $TOKEN" \
             -H "Accept: application/vnd.github+json" "$@"; }

GO=0
[ "${1:-}" = "--yes" ] && GO=1

[ "$GO" = "1" ] || echo "DRY RUN -- nothing will be changed. Add --yes to go ahead."
echo "repository: $REPO"
echo "range:      v$MINOR.$FIRST .. v$MINOR.$LAST"
echo

gone=0
kept=0

for n in $(seq "$FIRST" "$LAST"); do
    tag="v$MINOR.$n"

    id="$(api "https://api.github.com/repos/$REPO/releases/tags/$tag" 2>/dev/null \
          | python3 -c 'import sys,json
try: print(json.load(sys.stdin)["id"])
except Exception: pass' || true)"

    has_tag=0
    if api -o /dev/null "https://api.github.com/repos/$REPO/git/refs/tags/$tag" 2>/dev/null; then
        has_tag=1
    fi

    if [ -z "$id" ] && [ "$has_tag" = "0" ]; then
        printf '  %-10s already gone\n' "$tag"
        kept=$((kept + 1))
        continue
    fi

    if [ "$GO" = "0" ]; then
        printf '  %-10s would remove%s%s\n' "$tag" \
            "${id:+ release $id}" "$([ "$has_tag" = 1 ] && echo ' and the tag')"
        continue
    fi

    # The release first: a release whose tag has gone is worse than both.
    if [ -n "$id" ]; then
        if api -o /dev/null -X DELETE "https://api.github.com/repos/$REPO/releases/$id"; then
            printf '  %-10s release deleted\n' "$tag"
        else
            printf '  %-10s COULD NOT delete the release -- leaving the tag alone\n' "$tag"
            kept=$((kept + 1))
            continue
        fi
    fi

    if [ "$has_tag" = "1" ]; then
        if api -o /dev/null -X DELETE \
               "https://api.github.com/repos/$REPO/git/refs/tags/$tag"; then
            printf '  %-10s tag deleted\n' "$tag"
        else
            printf '  %-10s tag could NOT be deleted\n' "$tag"
        fi
    fi

    gone=$((gone + 1))
done

echo
if [ "$GO" = "1" ]; then
    echo "removed $gone, left alone $kept"
    echo "Locally:  git fetch --prune --prune-tags origin"
else
    echo "Nothing was changed. Re-run with --yes to do it."
fi
