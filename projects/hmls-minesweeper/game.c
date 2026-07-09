#include <Memory.h>
#include <OSUtils.h>
#include <string.h>

#include "game.h"

enum {
    kMaxBoardCells = 480
};

static short GameIndex(const GameState* game, short x, short y)
{
    return (short)(y * game->width + x);
}

static short GameInBounds(const GameState* game, short x, short y)
{
    return x >= 0 && y >= 0 && x < game->width && y < game->height;
}

static void GameNoteChange(GameState* game, short x, short y)
{
    if (game->changedCount < kMaxBoardCells) {
        game->changedX[game->changedCount] = x;
        game->changedY[game->changedCount] = y;
        ++game->changedCount;
    }
}

static void GameNoteUnflaggedMines(GameState* game)
{
    short x;
    short y;

    for (y = 0; y < game->height; ++y) {
        for (x = 0; x < game->width; ++x) {
            if (game->mines[GameIndex(game, x, y)] && !game->flags[GameIndex(game, x, y)]) {
                GameNoteChange(game, x, y);
            }
        }
    }
}

static void GameClearBoard(GameState* game)
{
    short i;
    short total = (short)(game->width * game->height);

    for (i = 0; i < total; ++i) {
        game->mines[i] = 0;
        game->flags[i] = 0;
        game->unsure[i] = 0;
        game->revealed[i] = 0;
        game->counts[i] = 0;
    }

    game->flagCount = 0;
    game->revealedCount = 0;
    game->elapsedSeconds = 0;
    game->startTicks = 0;
    game->status = kGamePlaying;
    game->firstClickDone = 0;
    game->changedCount = 0;
}

static void GameGetDifficultySize(GameDifficulty difficulty, short* width, short* height, short* mines)
{
    switch (difficulty) {
        case kDifficultyIntermediate:
            *width = 16;
            *height = 16;
            *mines = 40;
            break;
        case kDifficultyExpert:
            *width = 30;
            *height = 16;
            *mines = 99;
            break;
        case kDifficultyBeginner:
        default:
            *width = 9;
            *height = 9;
            *mines = 10;
            break;
    }
}

static void GameCountNeighbors(GameState* game)
{
    short x;
    short y;
    short dx;
    short dy;
    short nx;
    short ny;
    short count;

    for (y = 0; y < game->height; ++y) {
        for (x = 0; x < game->width; ++x) {
            if (game->mines[GameIndex(game, x, y)]) {
                game->counts[GameIndex(game, x, y)] = 0;
                continue;
            }

            count = 0;
            for (dy = -1; dy <= 1; ++dy) {
                for (dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    nx = (short)(x + dx);
                    ny = (short)(y + dy);
                    if (GameInBounds(game, nx, ny) && game->mines[GameIndex(game, nx, ny)]) {
                        ++count;
                    }
                }
            }
            game->counts[GameIndex(game, x, y)] = (unsigned char)count;
        }
    }
}

static short GameIsSafePlacement(const GameState* game, short index, short safeIndex)
{
    short dx;
    short dy;
    short x;
    short y;
    short sx;
    short sy;
    short nx;
    short ny;

    if (index == safeIndex) {
        return 0;
    }

    sx = (short)(safeIndex % game->width);
    sy = (short)(safeIndex / game->width);
    x = (short)(index % game->width);
    y = (short)(index / game->width);

    for (dy = -1; dy <= 1; ++dy) {
        for (dx = -1; dx <= 1; ++dx) {
            nx = (short)(sx + dx);
            ny = (short)(sy + dy);
            if (nx == x && ny == y) {
                return 0;
            }
        }
    }

    return 1;
}

static void GamePlaceMines(GameState* game, short safeX, short safeY)
{
    short total = (short)(game->width * game->height);
    short placed = 0;
    short safeIndex = GameIndex(game, safeX, safeY);
    short index;

    for (index = 0; index < total; ++index) {
        game->mines[index] = 0;
    }

    while (placed < game->mineCount) {
        index = (short)(((unsigned short)Random()) % (unsigned short)total);
        if (!GameIsSafePlacement(game, index, safeIndex)) {
            continue;
        }
        if (game->mines[index]) {
            continue;
        }
        game->mines[index] = 1;
        ++placed;
    }

    GameCountNeighbors(game);
}

static void GameCheckWin(GameState* game)
{
    short total = (short)(game->width * game->height);
    short safeCells = (short)(total - game->mineCount);

    if (game->revealedCount >= safeCells) {
        game->status = kGameWon;
    }
}

static void GameRevealCell(GameState* game, short startX, short startY)
{
    short* pending;
    short top;
    short x;
    short y;
    short dx;
    short dy;
    short nx;
    short ny;
    short index;

    if (!GameInBounds(game, startX, startY)) {
        return;
    }

    index = GameIndex(game, startX, startY);
    if (game->revealed[index] || game->flags[index] || game->unsure[index]) {
        return;
    }

    pending = (short*)NewPtrClear((Size)(kMaxBoardCells * 2 * sizeof(short)));
    if (!pending) {
        return;
    }

    top = 0;
    pending[top++] = startX;
    pending[top++] = startY;

    while (top > 0) {
        y = pending[--top];
        x = pending[--top];

        if (!GameInBounds(game, x, y)) {
            continue;
        }

        index = GameIndex(game, x, y);
        if (game->revealed[index] || game->flags[index] || game->unsure[index]) {
            continue;
        }

        game->revealed[index] = 1;
        ++game->revealedCount;
        GameNoteChange(game, x, y);

        if (game->mines[index]) {
            game->status = kGameLost;
            DisposePtr((Ptr)pending);
            return;
        }

        if (game->counts[index] != 0) {
            continue;
        }

        for (dy = -1; dy <= 1; ++dy) {
            for (dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                nx = (short)(x + dx);
                ny = (short)(y + dy);
                if (GameInBounds(game, nx, ny)
                    && !game->revealed[GameIndex(game, nx, ny)]
                    && !game->flags[GameIndex(game, nx, ny)]
                    && !game->unsure[GameIndex(game, nx, ny)]) {
                    pending[top++] = nx;
                    pending[top++] = ny;
                }
            }
        }
    }

    DisposePtr((Ptr)pending);
    GameCheckWin(game);
}

void GameInit(GameState* game)
{
    memset(game, 0, sizeof(*game));
    game->mines = NULL;
    game->flags = NULL;
    game->unsure = NULL;
    game->revealed = NULL;
    game->counts = NULL;
    game->difficulty = kDifficultyBeginner;
}

void GameDispose(GameState* game)
{
    if (game->mines) {
        DisposePtr((Ptr)game->mines);
        game->mines = NULL;
    }
    if (game->flags) {
        DisposePtr((Ptr)game->flags);
        game->flags = NULL;
    }
    if (game->unsure) {
        DisposePtr((Ptr)game->unsure);
        game->unsure = NULL;
    }
    if (game->revealed) {
        DisposePtr((Ptr)game->revealed);
        game->revealed = NULL;
    }
    if (game->counts) {
        DisposePtr((Ptr)game->counts);
        game->counts = NULL;
    }
}

void GameNewDifficulty(GameState* game, GameDifficulty difficulty)
{
    short width;
    short height;
    short mines;
    short total;

    GameGetDifficultySize(difficulty, &width, &height, &mines);
    game->difficulty = difficulty;
    game->width = width;
    game->height = height;
    game->mineCount = mines;
    total = (short)(width * height);

    GameDispose(game);

    game->mines = (unsigned char*)NewPtrClear(total);
    game->flags = (unsigned char*)NewPtrClear(total);
    game->unsure = (unsigned char*)NewPtrClear(total);
    game->revealed = (unsigned char*)NewPtrClear(total);
    game->counts = (unsigned char*)NewPtrClear(total);

    GameClearBoard(game);
}

void GameRestart(GameState* game)
{
    GameNewDifficulty(game, game->difficulty);
}

short GameGetWidth(const GameState* game)
{
    return game->width;
}

short GameGetHeight(const GameState* game)
{
    return game->height;
}

short GameGetMineCount(const GameState* game)
{
    return game->mineCount;
}

short GameGetFlagCount(const GameState* game)
{
    return game->flagCount;
}

short GameGetRemainingMines(const GameState* game)
{
    return (short)(game->mineCount - game->flagCount);
}

short GameGetElapsedSeconds(const GameState* game)
{
    return game->elapsedSeconds;
}

GameStatus GameGetStatus(const GameState* game)
{
    return game->status;
}

GameDifficulty GameGetDifficulty(const GameState* game)
{
    return game->difficulty;
}

short GameIsRevealed(const GameState* game, short x, short y)
{
    if (!GameInBounds(game, x, y)) {
        return 0;
    }
    return game->revealed[GameIndex(game, x, y)] != 0;
}

short GameIsFlagged(const GameState* game, short x, short y)
{
    if (!GameInBounds(game, x, y)) {
        return 0;
    }
    return game->flags[GameIndex(game, x, y)] != 0;
}

short GameIsUnsure(const GameState* game, short x, short y)
{
    if (!GameInBounds(game, x, y)) {
        return 0;
    }
    return game->unsure[GameIndex(game, x, y)] != 0;
}

short GameIsMine(const GameState* game, short x, short y)
{
    if (!GameInBounds(game, x, y)) {
        return 0;
    }
    return game->mines[GameIndex(game, x, y)] != 0;
}

short GameGetCount(const GameState* game, short x, short y)
{
    if (!GameInBounds(game, x, y)) {
        return 0;
    }
    return game->counts[GameIndex(game, x, y)];
}

void GameClearChanges(GameState* game)
{
    game->changedCount = 0;
}

short GameGetChangedCount(const GameState* game)
{
    return game->changedCount;
}

void GameGetChangedCell(const GameState* game, short index, short* x, short* y)
{
    *x = game->changedX[index];
    *y = game->changedY[index];
}

void GameUpdateTimer(GameState* game)
{
    unsigned long ticks;

    if (game->status != kGamePlaying || !game->firstClickDone) {
        return;
    }

    ticks = TickCount();
    game->elapsedSeconds = (short)((ticks - game->startTicks) / 60);
    if (game->elapsedSeconds > 999) {
        game->elapsedSeconds = 999;
    }
}

void GameReveal(GameState* game, short x, short y)
{
    if (game->status != kGamePlaying || !GameInBounds(game, x, y)) {
        return;
    }

    if (game->flags[GameIndex(game, x, y)] || game->unsure[GameIndex(game, x, y)]) {
        return;
    }

    GameClearChanges(game);

    if (!game->firstClickDone) {
        game->firstClickDone = 1;
        game->startTicks = TickCount();
        GamePlaceMines(game, x, y);
    }

    GameRevealCell(game, x, y);
    if (game->status == kGameLost) {
        GameNoteUnflaggedMines(game);
    }
}

void GameCycleMark(GameState* game, short x, short y)
{
    short index;

    if (game->status != kGamePlaying || !GameInBounds(game, x, y)) {
        return;
    }

    index = GameIndex(game, x, y);
    if (game->revealed[index]) {
        return;
    }

    GameClearChanges(game);

    if (game->flags[index]) {
        game->flags[index] = 0;
        --game->flagCount;
        game->unsure[index] = 1;
    } else if (game->unsure[index]) {
        game->unsure[index] = 0;
    } else {
        game->flags[index] = 1;
        ++game->flagCount;
    }

    GameNoteChange(game, x, y);
}
