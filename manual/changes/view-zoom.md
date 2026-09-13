---
title: Zoom the tactical view with the mouse wheel
category: feature
release: 0.2.0
targets:
- type: system
  id: view-zoom
  effect: added
- type: key
  id: ScreenWidth
  effect: changed
- type: key
  id: ScreenHeight
  effect: changed
credit: [Michiel Van den Camp]
---

The tactical view shows as much of the map as an 800 by 600 screen at any resolution, where a higher resolution used to show more. Turning the mouse wheel over the map zooms out in eight steps to as much as a 1920 by 1080 screen showed and back, keeping the spot under the pointer in place. Off the map, the wheel scrolls the sidebar's build strips as it did everywhere before.
