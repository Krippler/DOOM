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
# Container images go too, unless --keep-images is given. Those live outside the
# repository, under the packages API, which this session cannot reach at all:
#
#   403 {"message":"This GitHub API path is not available: sessions are bound
#        to their configured repositories."}
#
# so the same token that does the releases does them. It needs delete:packages
# on top of repo access. The digests of the fifteen are recorded in
# releases-backup/image-digests.txt, and it has been checked that no tag being
# kept -- latest, edge, 1.10, 1, or the versions either side -- shares a digest
# with any of them, so removing these removes nothing else.
#
set -euo pipefail

REPO="${REPO:-Krippler/DOOM}"
FIRST="${FIRST:-31}"
LAST="${LAST:-45}"
MINOR="${MINOR:-1.10}"

PKG="${PKG:-doom}"
OWNER_LOWER="$(printf '%s' "${REPO%%/*}" | tr '[:upper:]' '[:lower:]')"

TOKEN="${GH_TOKEN:-${GITHUB_TOKEN:-}}"

if [ -z "$TOKEN" ] && command -v gh >/dev/null 2>&1; then
    TOKEN="$(gh auth token 2>/dev/null || true)"
fi

if [ -z "$TOKEN" ]; then
    echo "No token. Set GH_TOKEN, or run 'gh auth login' first." >&2
    echo "It needs write access to $REPO -- a classic PAT with 'repo' -- and," >&2
    echo "for the container images, read:packages and delete:packages too." >&2
    exit 1
fi

api() { curl -fsS -H "Authorization: Bearer $TOKEN" \
             -H "Accept: application/vnd.github+json" "$@"; }

GO=0
IMAGES=1

for arg in "$@"; do
    case "$arg" in
        --yes)         GO=1 ;;
        --keep-images) IMAGES=0 ;;
        *) echo "unknown option: $arg" >&2; exit 1 ;;
    esac
done

[ "$GO" = "1" ] || echo "DRY RUN -- nothing will be changed. Add --yes to go ahead."
echo "repository: $REPO"
echo "range:      v$MINOR.$FIRST .. v$MINOR.$LAST"
echo

gone=0
kept=0

export FIRST LAST MINOR PKG TOKEN GO

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

#
# The container images, which are a package rather than part of the repository.
#
# Matched on the tags each version carries, so a version is only removed when
# every tag on it is one of ours -- an image that also answers to something we
# are keeping is left alone and said so, rather than taking the kept tag with it.
#
if [ "$IMAGES" = "1" ]; then
    echo
    echo "container images (ghcr.io/$OWNER_LOWER/$PKG):"

    versions="$(api "https://api.github.com/user/packages/container/$PKG/versions?per_page=100" 2>/dev/null || true)"

    if [ -z "$versions" ]; then
        echo "  cannot read the package versions with this token."
        echo "  It needs read:packages and delete:packages as well as repo."
    else
        printf '%s' "$versions" | python3 -c '
import json, os, sys, subprocess

first = int(os.environ["FIRST"]); last = int(os.environ["LAST"])
minor = os.environ["MINOR"]; pkg = os.environ["PKG"]
token = os.environ["TOKEN"]; go = os.environ["GO"] == "1"

want = {"%s.%d" % (minor, n) for n in range(first, last + 1)}

for v in json.load(sys.stdin):
    tags = set((v.get("metadata", {}).get("container", {}) or {}).get("tags", []))
    if not tags & want:
        continue

    extra = tags - want
    if extra:
        print("  %-9s left alone -- also tagged %s"
              % (",".join(sorted(tags & want)), ",".join(sorted(extra))))
        continue

    label = ",".join(sorted(tags))
    if not go:
        print("  %-9s would remove version %s" % (label, v["id"]))
        continue

    r = subprocess.run(["curl", "-fsS", "-o", "/dev/null", "-X", "DELETE",
                        "-H", "Authorization: Bearer " + token,
                        "https://api.github.com/user/packages/container/%s/versions/%s"
                        % (pkg, v["id"])])
    print("  %-9s %s" % (label, "image deleted" if r.returncode == 0
                                else "COULD NOT delete the image"))
' || echo "  (could not process the package versions)"
    fi
fi

echo
if [ "$GO" = "1" ]; then
    echo "removed $gone release/tag pairs, left alone $kept"
    echo "Locally:  git fetch --prune --prune-tags origin"
else
    echo "Nothing was changed. Re-run with --yes to do it."
fi
