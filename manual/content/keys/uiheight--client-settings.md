---
key: UIHeight
scope: client-settings
label: Stored interface height
see_also: [ScreenHeight]
when_omitted:
  kind: unchanged
  note: The read passes through the height the display was already opened with, and nothing later overwrites it.
---

This is the later of the two reads of the assignment, made with the rest of the client settings once the display is already open. It cannot change the screen the interface is drawn on; what it settles is the size the display options screen starts from, and the value written back to `sun.ini` when the settings are saved.
