import sys

IN = 1
OUT = 0

state = OUT
line_count = word_count = letter_count = 0

for line in sys.stdin:
    for c in line:
        if c == ' ' or c == '\t' or c == '\n':
            state = OUT
        elif not state:
            state = IN
            word_count += 1
        letter_count += 1
    line_count += 1

print(line_count, word_count, letter_count)