#include <Memory.h>
#include <Quickdraw.h>
#include <Resources.h>

#include "tiles.h"

enum {
    kFirstTileID = 128,
    kLastTileID = 146,
    kTileCacheCount = kLastTileID - kFirstTileID + 1,
    kMaxBoardPixelWidth = 30 * kTileSize,
    kMaxBoardPixelHeight = 16 * kTileSize
};

static Handle gTileCache[kTileCacheCount];
static Ptr gBoardBits;
static short gBoardRowBytes;
static BitMap gBoardMap;
static short gComposeActive;
static short gComposeWidth;
static short gComposeHeight;

static Handle TileHandle(short tileID)
{
    if (tileID < kFirstTileID || tileID > kLastTileID) {
        return NULL;
    }
    return gTileCache[tileID - kFirstTileID];
}

static void CopyTileBits(Handle tile, BitMap* destMap, const Rect* destRect)
{
    BitMap source;

    if (tile == NULL || *tile == NULL || destMap == NULL) {
        return;
    }

    HLock(tile);

    /* Opaque 1-bit tiles: one srcCopy is enough (no mask/bic/or pair). */
    source.baseAddr = *tile;
    source.rowBytes = 2;
    SetRect(&source.bounds, 0, 0, kTileSize, kTileSize);

    CopyBits(&source, destMap, &source.bounds, destRect, srcCopy, nil);

    HUnlock(tile);
}

void InitTiles(void)
{
    short tileID;
    Handle tile;
    short i;

    for (i = 0; i < kTileCacheCount; ++i) {
        gTileCache[i] = NULL;
    }

    for (tileID = kFirstTileID; tileID <= kLastTileID; ++tileID) {
        tile = Get1Resource('TILE', tileID);
        if (tile != NULL) {
            LoadResource(tile);
            HNoPurge(tile);
            gTileCache[tileID - kFirstTileID] = tile;
        }
    }

    gBoardRowBytes = (short)(((kMaxBoardPixelWidth + 15) / 16) * 2);
    gBoardBits = NewPtrClear((Size)gBoardRowBytes * kMaxBoardPixelHeight);
    if (gBoardBits != NULL) {
        gBoardMap.baseAddr = gBoardBits;
        gBoardMap.rowBytes = gBoardRowBytes;
        SetRect(&gBoardMap.bounds, 0, 0, kMaxBoardPixelWidth, kMaxBoardPixelHeight);
    }

    gComposeActive = 0;
    gComposeWidth = 0;
    gComposeHeight = 0;
}

void DisposeTiles(void)
{
    short i;

    gComposeActive = 0;

    if (gBoardBits != NULL) {
        DisposePtr(gBoardBits);
        gBoardBits = NULL;
        gBoardMap.baseAddr = NULL;
    }

    for (i = 0; i < kTileCacheCount; ++i) {
        if (gTileCache[i] != NULL) {
            HPurge(gTileCache[i]);
            gTileCache[i] = NULL;
        }
    }
}

void DrawTile(short tileID, const Rect* destRect)
{
    Handle tile = TileHandle(tileID);
    GrafPtr port;
    BitMap destination;

    if (tile == NULL) {
        tile = Get1Resource('TILE', tileID);
    }
    if (tile == NULL) {
        FrameRect(destRect);
        return;
    }

    if (gComposeActive) {
        CopyTileBits(tile, &gBoardMap, destRect);
        return;
    }

    GetPort(&port);
    destination = port->portBits;
    CopyTileBits(tile, &destination, destRect);
}

short BeginBoardCompose(short pixelWidth, short pixelHeight)
{
    if (gBoardBits == NULL
        || pixelWidth <= 0
        || pixelHeight <= 0
        || pixelWidth > kMaxBoardPixelWidth
        || pixelHeight > kMaxBoardPixelHeight) {
        return 0;
    }

    gComposeActive = 1;
    gComposeWidth = pixelWidth;
    gComposeHeight = pixelHeight;
    return 1;
}

void EndBoardCompose(GrafPtr destPort, const Rect* destRect)
{
    Rect srcRect;

    if (!gComposeActive || gBoardBits == NULL || destPort == NULL || destRect == NULL) {
        gComposeActive = 0;
        return;
    }

    SetRect(&srcRect, 0, 0, gComposeWidth, gComposeHeight);
    CopyBits(&gBoardMap, &destPort->portBits, &srcRect, destRect, srcCopy, nil);

    gComposeActive = 0;
    gComposeWidth = 0;
    gComposeHeight = 0;
}
