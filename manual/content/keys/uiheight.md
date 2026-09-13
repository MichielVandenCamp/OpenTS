---
key: UIHeight
summary: The height in pixels of the screen that the sidebar, menus, and dialogs are laid out on.
---

The resolution set by [`ScreenWidth`](/keys/screenwidth/) and [`ScreenHeight`](/keys/screenheight/) decides how much of the map fits on the display, because the tactical map is drawn at one pixel per pixel of the resolution. This key decides the size of everything else: the sidebar, the tab strip and mission timer, the messages and tooltips over the map, the menus, and the dialogs. They are laid out on a screen of this height with the resolution's shape, and that screen is stretched over the same area of the display as the map. A smaller value gives a larger interface.

The interface's size therefore does not follow the resolution. At `600` it covers the display as it did at a resolution of 800 by 600, whether the game renders at 1280 by 720 or at 3840 by 2160. At `1080` it covers the display as it did at 1920 by 1080.

```ini title="sun.ini"
[Video]
ScreenWidth=1920
ScreenHeight=1080
; The map at full resolution, the interface at the size it had at 800 by 600.
UIHeight=600
```

The display options screen offers Match resolution, Large, Normal, and Small, which store `0`, `480`, `600`, and `1080`, and lists any other stored value as a custom size. A value of zero or below, or the resolution's own height, lays the interface out at the resolution. Other heights are kept between 400 and 4,096 pixels, and moved further when needed so that the interface's screen is between 640 and 4,096 pixels wide at the resolution's shape.

While the interface has a height of its own, [`ScaleMode`](/keys/scalemode/) filters it on the way up as it filters the map, and a [`CursorScale`](/keys/cursorscale/) of `0` sizes the pointer to match the interface. Everything drawn as part of the map keeps the map's scale, including health bars, waypoint markers, and the caption a mission prints over the map. Screen shake moves the map and leaves the interface in place.
