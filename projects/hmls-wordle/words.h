#ifndef WORDS_H
#define WORDS_H

enum {
    kWordLength = 5
};

void WordsInit(void);
void WordsPickAnswer(char out[kWordLength]);
short WordsIsValidGuess(const char word[kWordLength]);

#endif
