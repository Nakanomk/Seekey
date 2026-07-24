# AUR Packaging

This directory contains templates for two independent AUR packages:

- `seekey`: stable releases from GitHub tags such as `v0.2.0`
- `seekey-git`: the latest commit from the `main` branch

Each subdirectory becomes its own AUR Git repository. Do not push the whole
upstream Seekey repository to AUR.

## Prerequisites

1. Create an account at <https://aur.archlinux.org>.
2. Add your SSH public key under **My Account > SSH Public Key**.
3. Install the packaging tools:

   ```sh
   sudo pacman -S --needed base-devel git namcap pacman-contrib
   ```

## Publish the stable `seekey` package

The stable PKGBUILD downloads `v0.2.0`. Commit and push the release code first,
then create an immutable tag:

```sh
git tag -a v0.2.0 -m "Seekey 0.2.0"
git push origin main v0.2.0
```

After GitHub exposes the tag archive, replace the temporary `SKIP` checksum,
build, test, and regenerate `.SRCINFO`:

```sh
cd packaging/aur/seekey
updpkgsums
makepkg --cleanbuild
namcap PKGBUILD seekey-*.pkg.tar.zst
makepkg --printsrcinfo > .SRCINFO
```

Create the AUR package repository on first push:

```sh
git clone ssh://aur@aur.archlinux.org/seekey.git /tmp/seekey-aur
cp PKGBUILD .SRCINFO 70-seekey.rules seekey.install /tmp/seekey-aur/
cd /tmp/seekey-aur
git add PKGBUILD .SRCINFO 70-seekey.rules seekey.install
git commit -m "Initial import: seekey 0.2.0"
git push
```

Never publish the stable PKGBUILD while its GitHub archive checksum is `SKIP`.

## Publish the rolling `seekey-git` package

The VCS source intentionally uses `SKIP`; checksums are not meaningful for a
Git repository. Build once so `pkgver()` writes the current generated version,
then regenerate `.SRCINFO`:

```sh
cd packaging/aur/seekey-git
makepkg --cleanbuild
namcap PKGBUILD seekey-git-*.pkg.tar.zst
makepkg --printsrcinfo > .SRCINFO
```

Push it to a separate AUR repository:

```sh
git clone ssh://aur@aur.archlinux.org/seekey-git.git /tmp/seekey-git-aur
cp PKGBUILD .SRCINFO 70-seekey.rules seekey.install /tmp/seekey-git-aur/
cd /tmp/seekey-git-aur
git add PKGBUILD .SRCINFO 70-seekey.rules seekey.install
git commit -m "Initial import: seekey-git"
git push
```

You do not need to update the AUR repository for every upstream commit:
`seekey-git` users fetch `main` and run `pkgver()` locally. Update the AUR
package only when dependencies, build steps, install files, or packaging
metadata change.

## New stable releases

For a later release, for example `0.3.0`:

1. Update `SEEKEY_VERSION` and other version metadata upstream.
2. Commit, push, and create the `v0.3.0` tag.
3. Set `pkgver=0.3.0` and reset `pkgrel=1` in `seekey/PKGBUILD`.
4. Run `updpkgsums`, `makepkg --cleanbuild`, `namcap`, and regenerate
   `.SRCINFO`.
5. Copy the four packaging files into the existing AUR repo, commit, and push.

Increment `pkgrel` only for packaging-only changes to the same upstream
version.
