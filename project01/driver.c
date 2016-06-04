#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "library.c"

/*
 * Prints the heroic epic of Mel, a Real Programmer, using the /dev/fb0 frame buffer,
 * and the functions we had to write for the first project.
 *
 * Intended as a 'driver program', for demonstration purposes, to show that the program
 * is written to spec.
 *
 *    Text from:  https://www.cs.utah.edu/~elb/folklore/mel.html
 *
 */
int main() {

    // init the graphics
    init_graphics();

    FILE *story_text;
    char *currentLine = NULL;
    size_t len = 0;
    ssize_t read;
    int color_index = 0;

    color_t textColors[100] = {
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        0x0000, 0x0001F, 0x7800, 0x7BEF,
                        };

    story_text = fopen("./story_text.txt", "r");
    if (story_text == NULL) {
        printf("Please make sure that the file named 'story_text.txt' is in the current directory\n");
        exit(-1);
    }

    // start by clearing the screen
    clear_screen();

    // explanation
    printf("A story about Real Programmers. Press 'q' to quit at any time.\n");

    int x1 = 60;
    int y1 = 100;
    // then, begin the story
    while ((read = getline(&currentLine, &len, story_text)) != -1) {

        // break out if user wants to
        char key = getkey();
        if (key == 'q') break;

        // pause at paragraph endings, signified by a ';', at the beginning of each line
        if (currentLine[0] == ';') {
            sleep_ms(900);
            sleep_ms(900);
            sleep_ms(900);

            // break out if user wants to
            char key = getkey();
            if (key == 'q') break;


            sleep_ms(900);
            sleep_ms(900);
            sleep_ms(900);
            sleep_ms(900);
            // clear the old screen
            clear_screen();
            // change screen color for next set
            color_entire_screen(textColors[++color_index]);
            // reset x1, y1
            x1 = 60;
            y1 = 100;

            // draw first rectangle, line marker
            draw_rect(x1-25, y1+20, 20, 20, textColors[color_index + 2]);

            continue;
        }

        // otherwise, pause for less time, and add a new line the current screen
        sleep_ms(900);
        sleep_ms(900);
        sleep_ms(900);

        // make sure current line is null terminated, then draw it on the screen
        char *nullTerm = "\0";
        char *string_to_write = strcat(currentLine, nullTerm);
        draw_text(x1, y1+=20, string_to_write, textColors[color_index-1]);
        draw_rect(x1-25, y1+20, 20, 20, textColors[color_index+1]);
        //printf("currentLine = %s\n", currentLine);

    }

    // cleanup and exit
    fclose(story_text);
    if (currentLine) free(currentLine);

    // clear the screen
    clear_screen();

    // exit graphics
    exit_graphics();
}
