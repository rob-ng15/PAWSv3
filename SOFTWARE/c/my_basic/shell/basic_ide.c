#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include "my_basic.h"

#define MAX_LINES 128
#define MAX_COLS  128

static char lines[MAX_LINES][MAX_COLS];
static int line_count = 1;
static int cur_y = 0;
static int cur_x = 0;

/* Flatten 2D line array into a single null-terminated string */
static char *flatten_buffer(void) {
    static char script[MAX_LINES * (MAX_COLS + 1)];
    script[0] = '\0';
    for (int i = 0; i < line_count; i++) {
        strcat(script, lines[i]);
        strcat(script, "\n");
    }
    return script;
}

/* Suspend curses, run my_basic interpreter, wait, and resume curses */
static void run_script(void) {
    def_prog_mode();   /* Save ncurses terminal states */
    endwin();          /* Temporarily exit curses mode */

    printf("\n================ Running BASIC ================\n");

    struct mb_interpreter_t *bas = NULL;
    mb_init();
    mb_open(&bas);

    char *code = flatten_buffer();
    if (mb_load_string(bas, code, true) == MB_FUNC_OK) {
        mb_run(bas);
    } else {
        printf("[Parse Error]: Check your BASIC syntax.\n");
    }

    mb_close(&bas);
    mb_dispose();

    printf("\n===============================================\n");
    printf("Press [ENTER] to return to the editor...");
    fflush(stdout);

    /* Consume leftover inputs and wait for Enter */
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);

    reset_prog_mode(); /* Restore curses terminal states */
    refresh();
}

/* Render editor lines, status bar, and position cursor */
static void draw_editor(void) {
    erase();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    /* Draw text lines */
    for (int i = 0; i < line_count && i < rows - 1; i++) {
        mvprintw(i, 0, "%s", lines[i]);
    }

    /* Draw status bar */
    attron(A_REVERSE);
    mvhline(rows - 1, 0, ' ', cols);
    mvprintw(rows - 1, 1, "[F5] Run | [F10] Quit | Line: %d Col: %d", cur_y + 1, cur_x + 1);
    attroff(A_REVERSE);

    move(cur_y, cur_x);
    refresh();
}

int main(void) {
    /* Pre-populate with sample script */
    strcpy(lines[0], "PRINT \"Hello from embedded my_basic!\"");
    strcpy(lines[1], "FOR i = 1 TO 5");
    strcpy(lines[2], "    PRINT \"Count: \", i");
    strcpy(lines[3], "NEXT");
    line_count = 4;

    /* Initialize ncurses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    int ch;
    draw_editor();

    while ((ch = getch()) != KEY_F(10)) {
        int len = (int)strlen(lines[cur_y]);

        switch (ch) {
            case KEY_F(5):
                run_script();
                break;

            case KEY_UP:
                if (cur_y > 0) {
                    cur_y--;
                    int prev_len = (int)strlen(lines[cur_y]);
                    if (cur_x > prev_len) cur_x = prev_len;
                }
                break;

            case KEY_DOWN:
                if (cur_y < line_count - 1) {
                    cur_y++;
                    int next_len = (int)strlen(lines[cur_y]);
                    if (cur_x > next_len) cur_x = next_len;
                }
                break;

            case KEY_LEFT:
                if (cur_x > 0) cur_x--;
                break;

            case KEY_RIGHT:
                if (cur_x < len) cur_x++;
                break;

            case KEY_BACKSPACE:
            case 127:
            case '\b':
                if (cur_x > 0) {
                    memmove(&lines[cur_y][cur_x - 1], &lines[cur_y][cur_x], len - cur_x + 1);
                    cur_x--;
                } else if (cur_y > 0) {
                    /* Merge with previous line */
                    int prev_len = (int)strlen(lines[cur_y - 1]);
                    if (prev_len + len < MAX_COLS - 1) {
                        strcat(lines[cur_y - 1], lines[cur_y]);
                        for (int i = cur_y; i < line_count - 1; i++) {
                            strcpy(lines[i], lines[i + 1]);
                        }
                        line_count--;
                        cur_y--;
                        cur_x = prev_len;
                    }
                }
                break;

            case '\n':
            case '\r':
            case KEY_ENTER:
                if (line_count < MAX_LINES) {
                    /* Split line at cursor */
                    for (int i = line_count; i > cur_y + 1; i--) {
                        strcpy(lines[i], lines[i - 1]);
                    }
                    strcpy(lines[cur_y + 1], &lines[cur_y][cur_x]);
                    lines[cur_y][cur_x] = '\0';
                    line_count++;
                    cur_y++;
                    cur_x = 0;
                }
                break;

            default:
                /* Printable characters */
                if (ch >= 32 && ch <= 126 && len < MAX_COLS - 2) {
                    memmove(&lines[cur_y][cur_x + 1], &lines[cur_y][cur_x], len - cur_x + 1);
                    lines[cur_y][cur_x] = (char)ch;
                    cur_x++;
                }
                break;
        }

        draw_editor();
    }

    endwin();
    return 0;
}
