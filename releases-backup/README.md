# Backup for the pruned releases

The release notes for **v1.10.31 – v1.10.45** — the run of instrumentation
releases that chased the picture stutter — kept here before the releases and
tags were removed from GitHub.

| | |
| --- | --- |
| `notes/` | each release's notes, with its id, publish date and URL |
| `tag-commits.txt` | tag, the commit it pointed at, and its release id |
| `all-releases.json` | the whole release list from the API, as it was |
| `image-digests.txt` | the container image digest for each of the fifteen |

**No history is lost by the pruning.** Every one of those commits is on
`master` — checked, all fifteen — so the code for each version is still there
and still reachable by its commit. What goes is the entry in the release list.

The container images go too. They are a package rather than part of the
repository, so the packages API does them, and `image-digests.txt` records the
digest of each. It was checked that no tag being kept — `latest`, `edge`,
`master`, `1.10`, `1`, or the versions either side — shares a digest with any of
the fifteen, so removing them removes nothing else. The script leaves any
version alone that also answers to a tag outside the range, and says so.

Run `../prune-releases.sh` to do the removal; it prints what it would do and
changes nothing without `--yes`.
