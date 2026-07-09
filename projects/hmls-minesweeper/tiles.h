#ifndef TILES_H
#define TILES_H

#include <Quickdraw.h>

enum {
    kTileSize = 16,

    kTileCovered = 128,
    kTileFlag,
    kTileMine,
    kTileMineHit,
    kTileEmpty,
    kTile1,
    kTile2,
    kTile3,
    kTile4,
    kTile5,
    kTile6,
    kTile7,
    kTile8,
    kTileFaceNormal,
    kTileFaceSurprised,
    kTileFaceDead,
    kTileFaceCool
};

void DrawTile(short tileID, const Rect* destRect);

#endif
