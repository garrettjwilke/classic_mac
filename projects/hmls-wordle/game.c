#include "game.h"

#include <string.h>

static void SetMessage(GameState* game, const char* msg)
{
    short i;

    for (i = 0; i < 47 && msg[i] != '\0'; ++i) {
        game->message[i] = msg[i];
    }
    game->message[i] = '\0';
}

void GameInit(GameState* game)
{
    memset(game, 0, sizeof(*game));
    WordsInit();
    GameNew(game);
}

void GameNew(GameState* game)
{
    short row;
    short col;

    WordsPickAnswer(game->answer);
    for (row = 0; row < kMaxGuesses; ++row) {
        for (col = 0; col < kWordLength; ++col) {
            game->grid[row][col].letter = 0;
            game->grid[row][col].state = kCellEmpty;
        }
    }
    game->currentRow = 0;
    game->currentCol = 0;
    game->status = kGamePlaying;
    SetMessage(game, "Type a 5-letter word");
}

GameStatus GameGetStatus(const GameState* game)
{
    return game->status;
}

short GameGetCurrentRow(const GameState* game)
{
    return game->currentRow;
}

short GameGetCurrentCol(const GameState* game)
{
    return game->currentCol;
}

const TileCell* GameGetCell(const GameState* game, short row, short col)
{
    return &game->grid[row][col];
}

void GameGetAnswer(const GameState* game, char out[kWordLength])
{
    short i;
    for (i = 0; i < kWordLength; ++i) {
        out[i] = game->answer[i];
    }
}

const char* GameGetMessage(const GameState* game)
{
    return game->message;
}

void GameClearMessage(GameState* game)
{
    game->message[0] = '\0';
}

short GameTypeLetter(GameState* game, char letter)
{
    if (game->status != kGamePlaying) {
        return 0;
    }
    if (letter < 'A' || letter > 'Z') {
        return 0;
    }
    if (game->currentCol >= kWordLength) {
        return 0;
    }

    game->grid[game->currentRow][game->currentCol].letter = letter;
    game->grid[game->currentRow][game->currentCol].state = kCellFilled;
    game->currentCol += 1;
    GameClearMessage(game);
    return 1;
}

short GameBackspace(GameState* game)
{
    if (game->status != kGamePlaying) {
        return 0;
    }
    if (game->currentCol <= 0) {
        return 0;
    }

    game->currentCol -= 1;
    game->grid[game->currentRow][game->currentCol].letter = 0;
    game->grid[game->currentRow][game->currentCol].state = kCellEmpty;
    GameClearMessage(game);
    return 1;
}

static void ScoreRow(GameState* game)
{
    short col;
    short answerUsed[kWordLength];
    char guess[kWordLength];

    for (col = 0; col < kWordLength; ++col) {
        guess[col] = game->grid[game->currentRow][col].letter;
        answerUsed[col] = 0;
        game->grid[game->currentRow][col].state = kCellAbsent;
    }

    /* First pass: exact matches. */
    for (col = 0; col < kWordLength; ++col) {
        if (guess[col] == game->answer[col]) {
            game->grid[game->currentRow][col].state = kCellCorrect;
            answerUsed[col] = 1;
        }
    }

    /* Second pass: present but wrong position. */
    for (col = 0; col < kWordLength; ++col) {
        short a;
        if (game->grid[game->currentRow][col].state == kCellCorrect) {
            continue;
        }
        for (a = 0; a < kWordLength; ++a) {
            if (!answerUsed[a] && guess[col] == game->answer[a]) {
                game->grid[game->currentRow][col].state = kCellPresent;
                answerUsed[a] = 1;
                break;
            }
        }
    }
}

SubmitResult GameSubmit(GameState* game)
{
    char guess[kWordLength];
    short col;
    short allCorrect;

    if (game->status != kGamePlaying) {
        return kSubmitGameOver;
    }
    if (game->currentCol < kWordLength) {
        SetMessage(game, "Need 5 letters");
        return kSubmitTooShort;
    }

    for (col = 0; col < kWordLength; ++col) {
        guess[col] = game->grid[game->currentRow][col].letter;
    }
    if (!WordsIsValidGuess(guess)) {
        SetMessage(game, "Not in word list");
        return kSubmitNotInList;
    }

    ScoreRow(game);

    allCorrect = 1;
    for (col = 0; col < kWordLength; ++col) {
        if (game->grid[game->currentRow][col].state != kCellCorrect) {
            allCorrect = 0;
            break;
        }
    }

    if (allCorrect) {
        game->status = kGameWon;
        SetMessage(game, "You win!");
        return kSubmitOk;
    }

    game->currentRow += 1;
    game->currentCol = 0;

    if (game->currentRow >= kMaxGuesses) {
        char msg[48];
        short i;

        game->status = kGameLost;
        msg[0] = 'A';
        msg[1] = 'n';
        msg[2] = 's';
        msg[3] = 'w';
        msg[4] = 'e';
        msg[5] = 'r';
        msg[6] = ':';
        msg[7] = ' ';
        for (i = 0; i < kWordLength; ++i) {
            msg[8 + i] = game->answer[i];
        }
        msg[8 + kWordLength] = '\0';
        SetMessage(game, msg);
        return kSubmitOk;
    }

    SetMessage(game, "Guess again");
    return kSubmitOk;
}
