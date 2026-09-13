---
title: Pavement structures
summary: "A BuildingType with `ToTile=` writes a terrain tile into the cells of its foundation when placed and then deletes itself."
category: buildings-economy
keys:
  - Morphable
  - Buildable
related:
  - type: system
    id: base-adjacency
  - type: system
    id: walls-and-gates
---

A pavement structure is a BuildingType whose rules section names a terrain tile in `ToTile=`. It is priced, queued and placed like any structure, but it never stands on the map: placing it writes that tile into qualifying cells of its foundation and deletes the structure.

```ini title="rules.ini"
[PAVEMENT]     ; a BuildingType
ToTile=pave01  ; the first tile of a tile set whose FileName is "pave"
```

`ToTile=` names a single tile: its tile set's `FileName` followed by the tile's two-digit number in that set, compared without regard to case.

## Placement

A pavement placement is legal when at least one cell of its foundation passes the cell test below, where other structures need every cell to pass. The local player's placement must still pass the [proximity check](/systems/base-adjacency/), but unlike other structures it may cover shrouded ground.

A cell passes when all of:

- its tile, if it has one, belongs to a tile set with [`Morphable=yes`](/keys/morphable/);
- no structure stands on it, or the structure there belongs to the house placing the pavement;
- it lies inside the playable area;
- it holds no overlay, so walls, Tiberium and veins all block it;
- it is not a bridge, a cell that was under a bridge, or a ramp;
- its land type is [`Buildable=yes`](/keys/buildable/).

Vehicles, infantry and terrain objects such as trees do not fail the test. An ally's structure blocks the cell like an enemy's. In the map editor the test uses the playfield instead of the playable area and only a wall overlay blocks the cell.

## Laying the tiles

Every foundation cell that passes the cell test receives the tile, except a cell that already carries that tile or whose land type is Road. A structure standing on the cell is left in place with the tile written beneath it. Laying the tile removes any smudge that overlaps the cell, including the parts of that smudge on other cells. If no cell receives the tile, the placement is refused.
