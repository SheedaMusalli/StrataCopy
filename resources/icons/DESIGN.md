# Forward Strata

An original geometric logo designed for StrataCopy. Three separated layers represent a transfer queue and the name “Strata”. The upper layer contains a forward arrow cut out of the surface. Cyan marks the active layer; blue and violet give the lower layers depth. The silhouette and high contrast remain legible at taskbar sizes.

The SVG is the editable source of truth. The 1024-pixel PNG and Windows ICO (16, 20, 24, 32, 40, 48, 64, 128 and 256 pixels) are derived from this drawing. No stock icon, external brand mark or image generator was used.

`scripts/render-icon.cpp` rasterizes the SVG through Qt SVG. Pillow encodes its raster output into the multiresolution ICO. The icon is embedded in the executable and used in the Qt window and both interface modes.
