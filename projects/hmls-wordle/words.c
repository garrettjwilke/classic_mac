#include "words.h"

#include <OSUtils.h>
#include <Quickdraw.h>

extern const long gAnswerCount;
extern const long gDictionaryCount;
extern const char gAnswers[][5];
extern const char gDictionary[][5];

static short gSeeded;

static void CopyWord(const char src[kWordLength], char dst[kWordLength])
{
    short i;
    for (i = 0; i < kWordLength; ++i) {
        dst[i] = src[i];
    }
}

void WordsInit(void)
{
    if (!gSeeded) {
        qd.randSeed = TickCount();
        gSeeded = 1;
    }
}

void WordsPickAnswer(char out[kWordLength])
{
    unsigned long index;

    WordsInit();
    if (gAnswerCount <= 0) {
        out[0] = 'A';
        out[1] = 'A';
        out[2] = 'A';
        out[3] = 'A';
        out[4] = 'A';
        return;
    }

    index = ((unsigned long)(unsigned short)Random()
        | ((unsigned long)(unsigned short)Random() << 16))
        % (unsigned long)gAnswerCount;
    CopyWord(gAnswers[index], out);
}

static short CompareWord(const char a[kWordLength], const char b[kWordLength])
{
    short i;
    for (i = 0; i < kWordLength; ++i) {
        if (a[i] < b[i]) {
            return -1;
        }
        if (a[i] > b[i]) {
            return 1;
        }
    }
    return 0;
}

short WordsIsValidGuess(const char word[kWordLength])
{
    long lo = 0;
    long hi = gDictionaryCount - 1;

    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2;
        short cmp = CompareWord(word, gDictionary[mid]);
        if (cmp == 0) {
            return 1;
        }
        if (cmp < 0) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return 0;
}
