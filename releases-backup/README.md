# Backup for the pruned releases

The release notes for **v1.10.31 – v1.10.45** — the run of instrumentation
releases that chased the picture stutter — kept here before the releases and
tags were removed from GitHub.

| | |
| --- | --- |
| `notes/` | each release's notes, with its id, publish date and URL |
| `tag-commits.txt` | tag, the commit it pointed at, and its release id |
| `all-releases.json` | the whole release list from the API, as it was |

**No history is lost by the pruning.** Every one of those commits is on
`master` — checked, all fifteen — so the code for each version is still there
and still reachable by its commit. What goes is the entry in the release list.

Container images are untouched: `ghcr.io/krippler/doom:1.10.31` and the rest
stay pullable. Deleting those would break anyone pinned to one, which is a
different decision from tidying a list.

Run `../prune-releases.sh` to do the removal; it prints what it would do and
changes nothing without `--yes`.
