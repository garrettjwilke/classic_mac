#ifndef GAME_H
#define GAME_H

#include "words.h"

enum {
    kMaxGuesses = 6
};

typedef enum {
    kCellEmpty = 0,
    kCellFilled,
    kCellCorrect,
    kCellPresent,
    kCellAbsent
} CellState;

typedef enum {
    kGamePlaying = 0,
    kGameWon,
    kGameLost
} GameStatus;

typedef enum {
    kSubmitOk = 0,
    kSubmitTooShort,
    kSubmitNotInList,
    kSubmitGameOver
} SubmitResult;

typedef struct {
    char letter;
    CellState state;
} TileCell;

typedef struct {
    char answer[kWordLength];
    TileCell grid[kMaxGuesses][kWordLength];
    short currentRow;
    short currentCol;
    GameStatus status;
    char message[48];
} GameState;

void GameInit(GameState* game);
void GameNew(GameState* game);

GameStatus GameGetStatus(const GameState* game);
short GameGetCurrentRow(const GameState* game);
short GameGetCurrentCol(const GameState* game);
const TileCell* GameGetCell(const GameState* game, short row, short col);
void GameGetAnswer(const GameState* game, char out[kWordLength]);
const char* GameGetMessage(const GameState* game);

void GameClearMessage(GameState* game);
short GameTypeLetter(GameState* game, char letter);
short GameBackspace(GameState* game);
SubmitResult GameSubmit(GameState* game);

#endif
