---
title: Tactical view zoom
summary: "How much of the map the tactical view shows, and how the mouse wheel zooms it."
category: interface-controls
keys:
  - ScreenWidth
  - ScreenHeight
  - UIHeight
  - ScaleMode
related:
  - type: system
    id: sidebar
  - type: key
    id: ZoomInFactor
---

The zoom is the height of the screen measured in pixels of the map. The view opens at `600`, as tall a slice of the map as an 800 by 600 screen shows, and zooms out as far as `1080`, as tall a slice as a 1920 by 1080 screen shows. The width follows the resolution's shape, so a wider screen shows more of the map to the sides at the same zoom. The sidebar and the tab strip cover part of the screen, and a smaller interface, set by a larger [`UIHeight`](/keys/uiheight/), leaves more of it to the view.

The resolution sets only the shape of the map's picture, not how much of the map it holds. The map is drawn at the zoom's size and stretched over the view, filtered by [`ScaleMode`](/keys/scalemode/).

## The mouse wheel

Turning the wheel away from the player zooms in, and turning it toward the player zooms out. Eight steps lie between the two ends, and each widens the view by the same ratio, about 7.6 percent. A wheel or touchpad that reports finer movement moves one step for each whole notch its movement adds up to. The spot on the map under the pointer stays under it, unless that would carry the view past the edge of the playable area.

The wheel zooms only while all of these hold:

- **All of:**
  - a scenario is running;
  - the pointer is over the tactical view;
  - no dialog is open.

Otherwise it scrolls the sidebar's build strips, as the `SidebarUp` and `SidebarDown` commands do.

The zoom lasts until the game is closed. Neither `sun.ini` nor a saved game records it, so every session opens at the nearest zoom.

The magnification of the [Zoom in](/mapping/actions/taction-zoom-in/) trigger action, set by [`ZoomInFactor`](/keys/zoominfactor/), applies on top of the zoom.
