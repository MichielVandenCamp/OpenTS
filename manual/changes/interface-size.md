---
title: Size the interface apart from the resolution
category: feature
release: 0.2.0
targets:
- type: key
  id: UIHeight
  effect: added
credit: [Michiel Van den Camp]
---

The display options screen has an interface size, stored as `UIHeight`, that sets how large the sidebar, tab strip, radar, menus, and dialogs are drawn whatever the resolution. The tactical map stays at the resolution, so a high resolution shows more of the map while the interface keeps the size it had at 800 by 600, or any other size chosen.
