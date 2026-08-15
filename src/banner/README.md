# src/banner — BANNER.EXE

The animated boot banner (`wmake banner`). It wears the shared fortress
keep and the block-letter wordmark from [`../common/LOGO.C`](../common/LOGO.C),
so the installed boot, the rescue/install floppy, and the installer all
show the same face.

## Modes

| Invocation      | Where                                    | Shows                                            |
|-----------------|------------------------------------------|--------------------------------------------------|
| `BANNER`        | installed `AUTOEXEC.BAT` `:BANNER`       | keep + `CASTALIA DOS` wordmark + edition/profile |
| `BANNER /Q`     | slow terminals                           | the same, drawn instantly (no animation)         |
| `BANNER /R`     | the rescue/install floppy `AUTOEXEC.BAT` | keep + wordmark + a framed rescue command list   |
| `BANNER /R /Q`  | slow rescue boot                         | the rescue splash, drawn instantly               |

## The show

The keep rises row by row from the ground, the **CASTALIA DOS** wordmark
lights up dark→steel→amber, and the tower pennants flicker amber/red — all
inside ~3 seconds. **Any key skips instantly**, honouring the performance
rule that nothing may block the boot. When installed the banner clears to a
black field so the Castalia menu draws clean; the rescue splash holds
briefly, then clears to the command prompt.

## Art

Keep art and the 5-row block font live in `../common/LOGO.C`. The keep is
stored as printable ASCII (`#` steel wall, `M` amber merlon, `o` lit window,
`G` dark gate, `:` base shadow, `_` ground, `|`/`P` flag) and translated to
CP437 blocks at draw time, keeping the source printable and diff-friendly.
Links `ui` and `logo`.
