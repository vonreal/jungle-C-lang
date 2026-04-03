#include <stdio.h>

#define IN 1
#define OUT 0

int main()
{
    int state, c, line_count, word_count, letter_count;

    state = OUT;
    line_count = word_count = letter_count = 0;
    while ((c = getchar()) != EOF)
    {
        letter_count++;
        if (c == '\n')
            line_count++;
        if (c == ' ' || c == '\t' || c == '\n')
            state = OUT;
        else if (state == OUT)
        {
            state = IN;
            word_count++;
        }
    }

    printf("%d %d %d\n", line_count, word_count, letter_count);

    return 0;
}