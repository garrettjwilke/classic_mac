#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

int print_txt(char* input)
{
    int c;
    FILE *input_file;

    input_file = fopen(input, "r");

    if (input_file == 0)
    {
        //fopen returns 0, the NULL pointer, on failure
        perror("Canot open input file\n");
        getchar();
        exit(-1);
    }
    else
    {
        int found_word = 0;

        while ((c =fgetc(input_file)) != EOF )
        {
            //if it's an alpha, convert it to lower case
            if (isalpha(c))
            {
                found_word = 1;
                c = tolower(c);
                putchar(c);
            }
            else {
                if (found_word) {
                    putchar('\n');
                    found_word=0;
                }
            }

        }
    }

    fclose(input_file);

    printf("\n");

    getchar();
    return 0;
}

int main (int)
{
    //char *input = argv[1];
    printf("input file name:\n");
    char input[100];
    gets(input);
    print_txt(input);
}
