---
title: Shift-click a cameo to order five
category: feature
release: 0.2.0
targets:
- type: system
  id: sidebar
  effect: changed
breaking: false
credit:
- Michiel Van den Camp
---

Holding Shift while left-clicking a vehicle, infantry or aircraft cameo orders five of it; with nothing of its kind on order, one starts and four join the queue. Orders that exceed `MaximumQueuedObjects` or the type's build limit are refused as a single click's would be. Structure cameos still take one order.
