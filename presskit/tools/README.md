# presskit/tools — how the images were generated

The screenshots and logos in `../images/` are **rendered from the same CP437
keep art the DOS binaries draw** (`src/common/LOGO.C` in the repository), in the
authentic VGA 16-colour palette, so they match what the hardware actually paints.

## render-screens.html

A self-contained page that reconstructs every screen (boot banner, rescue disk,
installer welcome, "installed", main menu, About) and the logo lockups on an
80×25 VGA cell grid. Open it in any browser to view them; each screen/logo sits
in a `div` with a `shot-<name>` id.

## Regenerating the PNGs

The PNG screenshots and logo rasters were captured by taking element-level
screenshots of `render-screens.html` with a headless Chromium (Playwright),
at a 2× device scale factor for crisp output. Each `#shot-*` element maps to one
output file.

## Regenerating the SVGs

The scalable SVG logos are emitted from the same art data (each CP437 cell maps
to a vector rect / square / line), auto-cropped to the mark's bounding box.

Both steps are deterministic and need no assets outside this repository.
