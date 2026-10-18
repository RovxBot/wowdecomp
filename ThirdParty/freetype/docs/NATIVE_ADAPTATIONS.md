# Native font service adaptations

This software is based in part on the work of the FreeType Team. The original
FreeType Project license is retained in `FTL.txt` beside this document.

For the native 12340 font preview, changes guarded by
`WOWDECOMP_PORTABLE_RUNTIME` adapt these original files:

- `src/truetype/ttdriver.c`: correctly typed module and driver callbacks.
- `src/raster/ftrend1.c`, `src/smooth/ftsmooth.c`: typed renderer constructors.
- `src/raster/ftraster.c`, `src/smooth/ftgrays.c`: typed raster callbacks;
  grayscale outline/span callbacks; defined scaling of negative coordinates.
- `src/base/ftoutln.c`, `src/base/ftcalc.c`: defined signed coordinate scaling.
- `src/base/ftobjs.c`: preserve null glyph-loader arrays when adjusting an empty
  loader; no pointer arithmetic before allocation.

The Windows paths retain their original expressions and callback selection.
The native target's separate module list selects TrueType/SFNT and grayscale/
monochrome rasterization. Auto-hinting, Type1 and CFF are unavailable there.
The source version, attribution and original notices are retained. See
`docs/m3/FONTS.md` in the product repository for scope and validation.
