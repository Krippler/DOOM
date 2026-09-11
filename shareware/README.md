# Shareware game data

`doom1.wad` — the shareware DOOM IWAD, episode 1 ("Knee-Deep in the Dead"),
nine levels. It is baked into the container image so the game runs with
nothing mounted.

```
size    4196020 bytes
md5     f0cefca49926d00903cf57551d901abe
sha256  1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771
```

That is the v1.9 shareware IWAD, the same file every source port recognises.
The entrypoint checks the size and checksum at startup and refuses to use it if
either is wrong.

## Why it is committed rather than downloaded

A build-time download would add a URL that has to stay up for as long as the
image is built — including the nightly rebuild, which nobody is watching. The
file is 4 MB and never changes. Committing it makes the build hermetic: no
network, no mirror to rot, and the bytes are the ones that were tested.

## Licence

This file is **not** covered by the GPL that applies to the engine sources in
this repository. It is id Software's shareware data, distributed under their
shareware terms: it may be copied and shared freely, unmodified, and not sold.
That is why it is mirrored widely and why Debian ships it as
`doom-wad-shareware` in `non-free`.

Nothing else is included. `DOOM.WAD`, `DOOM2.WAD`, `TNT.WAD` and
`PLUTONIA.WAD` are commercial data and come from your own copy of the game —
mount them at `/wads` and they take precedence over this file.

The ignore rules in `.gitignore` and `.dockerignore` exclude every `*.wad`
except this one by name, so a commercial IWAD sitting in the working tree
cannot be committed or baked into an image by accident.
