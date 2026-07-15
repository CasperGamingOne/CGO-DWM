## 2024-05-15 - Cached Text Widths in dwm bar
**Learning:** DWM's `drawbar` is called very frequently (e.g., on `expose`, `enternotify`, window movements) and `TEXTW` is extremely slow because it does string measurement using Xft (`XftTextExtentsUtf8`).
**Action:** When strings are mostly static (like `tags`) or change infrequently (like `stext`), compute their width once and cache it in a global or per-instance variable (`tagw[]`, `stextw`).
