---
key: UIHeight
scope: client-settings-2
label: Interface height the display opens at
see_also: [ScreenHeight, CursorScale]
when_omitted:
  kind: value
  value: "0"
  note: The interface is laid out at the resolution.
---

This is the earlier of the two reads of the assignment, made with the resolution before the main window exists. The window opens at the resolution, and the interface is laid out on its own screen from the first frame. With `ScreenWidth=1920`, `ScreenHeight=1080`, and `UIHeight=600`, the interface is laid out on a screen of 1067 by 600, and the tactical map is drawn at 1618 by 1051 pixels, the part of 1920 by 1080 left beside the sidebar and below the tab strip.
