# Unraid Community Applications

`doom.xml` is the Community Applications template and `icon.png` is the icon
it points at. Both are ready; what remains are the steps only the repository
owner can take, listed below.

## What still has to happen before submitting

CA lists an application by pointing at a template in a GitHub repository, and
the template points at a published image. Neither of those can be done from
inside this repo, so:

1. **Publish the image.** The template's `<Repository>` is
   `ghcr.io/krippler/doom:latest`, which does not exist yet. Build and push
   it, or change the line to wherever you publish:

   ```
   docker build -t ghcr.io/krippler/doom:latest .
   docker push ghcr.io/krippler/doom:latest
   ```

   Make the package public afterwards — GHCR packages default to private, and
   CA cannot pull a private image. A `.github/workflows/` build that pushes on
   tag is the usual way to keep it current, and CA moderators prefer an image
   that is rebuilt rather than pushed once by hand.

2. **Check the branch in the URLs.** `<TemplateURL>` and `<Icon>` both point
   at `.../krippler/DOOM/master/unraid/...`. They have to resolve to the raw
   files on whichever branch you actually merge this to. Open both URLs in a
   browser before submitting; CA fetches them directly and a 404 icon is a
   common reason for a template to be bounced back.

3. **Decide on the support link.** It is currently the repository's issue
   tracker. CA wants somewhere users can get help, and a dedicated thread in
   the Unraid forums' *Docker Containers* section is the conventional choice
   — some moderators ask for one specifically. If you create a thread, put its
   URL in `<Support>`.

4. **Submit the repository to CA.** Post in the
   [Community Applications support thread](https://forums.unraid.net/topic/38582-plug-in-community-applications/)
   asking for the repository to be added, or follow whatever the current
   submission process is — it has changed over the years, so check before
   posting.

I have not verified the template against CA's own validator, and the exact
acceptance criteria are set by the CA moderators and change over time. Treat
the list above as the shape of the work rather than a guarantee.

## Testing it on Unraid before submitting

You do not need CA to try the template. In the Unraid web UI, go to
**Docker → Add Container**, paste the raw URL of `doom.xml` into the
*Template* field at the top, and it will fill the form in. That exercises
exactly what CA users will get.

Before starting it, put an IWAD where the template expects one:

```
mkdir -p /mnt/user/appdata/doom/wads
cp /path/to/DOOM1.WAD /mnt/user/appdata/doom/wads/
```

Then start the container and click **WebUI**. If no IWAD is found the
container stops immediately and the log says which filenames it looked for.

## Notes specific to Unraid

**Permissions.** The container starts as root only long enough to take
ownership of its state directory as `PUID:PGID`, then drops to that user. The
template defaults to `99:100`, Unraid's `nobody:users`, which is what appdata
is owned by, so nothing needs chowning by hand.

**Sound.** There is normally none, and that is expected — an Unraid server has
no audio device and the game runs silently. If you do have a PulseAudio server
somewhere on the network that accepts TCP connections, putting its address in
the `PULSE_SERVER` variable will give you effects and music.

**Size.** The image is around 600 MB unpacked, most of it the FluidR3
soundfont used for music. Building with
`--build-arg SOUNDFONT_PACKAGE=fluidr3mono-gm-soundfont` uses the same
instrument set Ogg-compressed and brings that down to about 365 MB, at some
cost in fidelity.

**Game data is not distributed.** The image contains no WAD and the build
context excludes them, so no copyrighted game data can end up in a published
image by accident. Users supply their own; the shareware `DOOM1.WAD` is freely
redistributable and is the easy way to try it.
