#include <Quickdraw.h>
#include <Resources.h>

#include "tiles.h"

static void DrawBWTile(Handle tile, const Rect* destRect)
{
    BitMap source;
    BitMap destination;
    GrafPtr port;

    if (tile == NULL) {
        return;
    }

    HLock(tile);

    source.baseAddr = *tile + 32;
    source.rowBytes = 2;
    SetRect(&source.bounds, 0, 0, kTileSize, kTileSize);

    GetPort(&port);
    destination = port->portBits;

    CopyBits(&source, &destination, &source.bounds, destRect, srcBic, nil);

    source.baseAddr = *tile;
    CopyBits(&source, &destination, &source.bounds, destRect, srcOr, nil);

    HUnlock(tile);
}

void DrawTile(short tileID, const Rect* destRect)
{
    Handle tile = Get1Resource('TILE', tileID);
    if (tile == NULL) {
        FrameRect(destRect);
        return;
    }

    DrawBWTile(tile, destRect);
}
