#ifndef GAME_H
#define GAME_H

typedef enum {
    kGamePlaying = 0,
    kGameWon,
    kGameLost
} GameStatus;

typedef enum {
    kDifficultyBeginner = 0,
    kDifficultyIntermediate,
    kDifficultyExpert
} GameDifficulty;

typedef struct {
    short width;
    short height;
    short mineCount;
    short flagCount;
    short revealedCount;
    short elapsedSeconds;
    unsigned long startTicks;
    GameStatus status;
    GameDifficulty difficulty;
    short firstClickDone;

    unsigned char* mines;
    unsigned char* flags;
    unsigned char* unsure;
    unsigned char* revealed;
    unsigned char* counts;

    short changedX[480];
    short changedY[480];
    short changedCount;
} GameState;

void GameInit(GameState* game);
void GameDispose(GameState* game);
void GameNewDifficulty(GameState* game, GameDifficulty difficulty);
void GameRestart(GameState* game);

short GameGetWidth(const GameState* game);
short GameGetHeight(const GameState* game);
short GameGetMineCount(const GameState* game);
short GameGetFlagCount(const GameState* game);
short GameGetRemainingMines(const GameState* game);
short GameGetElapsedSeconds(const GameState* game);
GameStatus GameGetStatus(const GameState* game);
GameDifficulty GameGetDifficulty(const GameState* game);

short GameIsRevealed(const GameState* game, short x, short y);
short GameIsFlagged(const GameState* game, short x, short y);
short GameIsUnsure(const GameState* game, short x, short y);
short GameIsMine(const GameState* game, short x, short y);
short GameGetCount(const GameState* game, short x, short y);

void GameUpdateTimer(GameState* game);
void GameClearChanges(GameState* game);
short GameGetChangedCount(const GameState* game);
void GameGetChangedCell(const GameState* game, short index, short* x, short* y);
void GameReveal(GameState* game, short x, short y);
void GameCycleMark(GameState* game, short x, short y);

#endif
