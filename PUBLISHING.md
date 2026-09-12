# Publishing Guide

How this container's images and releases are published, and how the Unraid
template gets listed. Relevant if you maintain the repo — not needed to *play*
(see [DOCKER.md](DOCKER.md)).

---

## Cutting a release

Releases are automatic. Open a PR that flips `CHANGELOG.md`'s `[Unreleased]`
section to `[X.Y.Z] — YYYY-MM-DD`, and merge it. CI notices the version flip
and, in the same run, publishes the images, pushes the `vX.Y.Z` git tag, and
creates the GitHub Release using that CHANGELOG section as the body. Nothing
to push from your machine.

There is no version string baked into the engine to bump: the version tracks
linuxdoom-1.10, so releases move the patch component.

## Tagging policy

Defined in `.github/workflows/docker-publish.yml`:

| Trigger | Images | Git tag | GitHub Release |
|---|---|---|---|
| Plain merge to `master` (CHANGELOG top is `[Unreleased]`) | `edge` | — | — |
| **Release-PR merge** (CHANGELOG top is `[X.Y.Z] — DATE`) | `edge` + `X.Y.Z`, `X.Y`, `X`, `latest` | `vX.Y.Z` (auto) | auto |
| Manual `git push origin vX.Y.Z` | `X.Y.Z`, `X.Y`, `X`, `latest` | (already pushed) | auto |
| Nightly schedule | rebuilds current tags | — | — |

`latest` is always the newest **released** version; `edge` tracks the tip of
`master`. The workflow also builds `claude/**`, `fix/**` and `feat/**`
branches, so an image exists to test before anything is merged; each is
tagged with its branch name.

`sha-<short>` is written only by default-branch builds. The same commit is
built twice when a branch is pushed and then merged, and since images carry a
version stamp those two differ — so both writing `sha-<short>` left it
pointing at whichever build happened to finish last.

## Registry

GHCR only, at `ghcr.io/krippler/doom`. Unlike the other repos here, this one
does not publish to Docker Hub, so there are no secrets to set up: the
workflow authenticates with the built-in `GITHUB_TOKEN` and a fork publishes
with no configuration at all.

Images are signed with cosign on every push.

The GHCR package inherits this repository's visibility, so on a public repo it
comes out publicly pullable and Community Applications can fetch it — that is
how `ghcr.io/krippler/lighthue` and `ghcr.io/krippler/fresharr` behave today.
It is worth confirming once after the first run rather than assuming, because
it does not always hold: `ghcr.io/krippler/starr` is private, which is
invisible until somebody without credentials tries to pull it.

```bash
# 200 means anyone can pull it; DENIED means the package is private.
tok=$(curl -s "https://ghcr.io/token?scope=repository:krippler/doom:pull&service=ghcr.io" \
      | sed -nE 's/.*"token":"([^"]+)".*/\1/p')
curl -s -o /dev/null -w '%{http_code}\n' -H "Authorization: Bearer $tok" \
     https://ghcr.io/v2/krippler/doom/tags/list
```

If it is private, the package's own settings page has the switch —
repository visibility does not change it retroactively.

---

## Unraid Community Apps

`templates/unraid.xml` is the template, `templates/doom-icon.png` the icon,
and `ca_profile.xml` the repository card CA shows for the maintainer.

To get it indexed:

1. Fork <https://github.com/selfhosters/unRAID-CA-templates>
2. Copy `templates/unraid.xml` into the fork
3. Open a PR

Or host your own template repo and add it in Unraid under
**Apps → Settings → Add templates repository URL**.

Before submitting, check that the raw URLs in the template actually resolve on
the branch you merged to — `<TemplateURL>` and `<Icon>` both assume `master`,
and CA fetches them directly. A 404 icon is a common reason for a template to
come back.

I have not put the template through CA's own validator; the acceptance
criteria are set by the CA moderators and change over time.

### Testing the template without CA

In the Unraid web UI go to **Docker → Add Container** and paste the raw URL of
`templates/unraid.xml` into the *Template* field. That fills in the form
exactly as a CA user would get it.

Put an IWAD where the template expects one first:

```
mkdir -p /mnt/user/appdata/doom/wads
cp /path/to/DOOM1.WAD /mnt/user/appdata/doom/wads/
```

Then start it and click **WebUI**. With no IWAD the container stops straight
away and the log lists the filenames it looked for.

### Notes specific to Unraid

**Permissions.** The container starts as root only long enough to take
ownership of its state directory as `PUID:PGID`, then drops to that user. The
template defaults to `99:100` (`nobody:users`), which is what appdata is owned
by, so nothing needs chowning by hand.

**Sound.** Normally there is none, and that is expected — a server has no
audio device. If you have a PulseAudio server on the network that accepts TCP
connections, putting its address in `PULSE_SERVER` gives you effects and music.

**Game data is not distributed.** The image contains no WAD and the build
context excludes them, so no copyrighted game data can end up in a published
image by accident. The shareware `DOOM1.WAD` is freely redistributable and is
the easy way to try it.
