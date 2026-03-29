#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* ─── Configuration ─── */
#define MAX_ROWS 20
#define MAX_COLS 26  /* columns labeled A-Z */

typedef enum { HIDDEN, REVEALED, FLAGGED } CellState;

typedef struct {
    int is_mine;
    int adjacent;    /* count of neighboring mines */
    CellState state;
} Cell;

typedef struct {
    Cell grid[MAX_ROWS][MAX_COLS];
    int rows;
    int cols;
    int mines;
    int flags;
    int revealed;
    int total_safe;
    int game_over;   /* 0 = playing, 1 = won, -1 = lost */
    int first_move;
} Game;

/* ─── Prototypes ─── */
void game_init(Game *g, int rows, int cols, int mines);
void place_mines(Game *g, int safe_r, int safe_c);
void calc_adjacent(Game *g);
void reveal(Game *g, int r, int c);
void toggle_flag(Game *g, int r, int c);
void print_board(const Game *g, int show_all);
int  parse_input(const char *buf, int *action, int *row, int *col, int max_row, int max_col);
void clear_screen(void);

/* ─── Helpers ─── */
static int in_bounds(const Game *g, int r, int c) {
    return r >= 0 && r < g->rows && c >= 0 && c < g->cols;
}

/* 8-directional neighbor offsets */
static const int dr[] = {-1,-1,-1, 0, 0, 1, 1, 1};
static const int dc[] = {-1, 0, 1,-1, 1,-1, 0, 1};

/* ─── Core Logic ─── */

void game_init(Game *g, int rows, int cols, int mines) {
    memset(g, 0, sizeof(*g));
    g->rows  = rows;
    g->cols  = cols;
    g->mines = mines;
    g->total_safe = rows * cols - mines;
    g->first_move = 1;
}

/* Place mines randomly, keeping a 3×3 safe zone around (safe_r, safe_c) */
void place_mines(Game *g, int safe_r, int safe_c) {
    int placed = 0;
    while (placed < g->mines) {
        int r = rand() % g->rows;
        int c = rand() % g->cols;
        if (g->grid[r][c].is_mine) continue;
        if (abs(r - safe_r) <= 1 && abs(c - safe_c) <= 1) continue;
        g->grid[r][c].is_mine = 1;
        placed++;
    }
}

void calc_adjacent(Game *g) {
    for (int r = 0; r < g->rows; r++) {
        for (int c = 0; c < g->cols; c++) {
            if (g->grid[r][c].is_mine) continue;
            int count = 0;
            for (int d = 0; d < 8; d++) {
                int nr = r + dr[d], nc = c + dc[d];
                if (in_bounds(g, nr, nc) && g->grid[nr][nc].is_mine)
                    count++;
            }
            g->grid[r][c].adjacent = count;
        }
    }
}

/* Flood-fill reveal */
void reveal(Game *g, int r, int c) {
    if (!in_bounds(g, r, c)) return;
    Cell *cell = &g->grid[r][c];
    if (cell->state != HIDDEN) return;

    cell->state = REVEALED;
    g->revealed++;

    if (cell->is_mine) {
        g->game_over = -1;
        return;
    }

    if (g->revealed == g->total_safe) {
        g->game_over = 1;
        return;
    }

    /* If no adjacent mines, reveal neighbors recursively */
    if (cell->adjacent == 0) {
        for (int d = 0; d < 8; d++)
            reveal(g, r + dr[d], c + dc[d]);
    }
}

void toggle_flag(Game *g, int r, int c) {
    Cell *cell = &g->grid[r][c];
    if (cell->state == HIDDEN) {
        cell->state = FLAGGED;
        g->flags++;
    } else if (cell->state == FLAGGED) {
        cell->state = HIDDEN;
        g->flags--;
    }
}

/* ─── Display ─── */

void clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[H\033[J");
#endif
}

/* Color codes for the numbers */
static const char *num_colors[] = {
    "",           /* 0 – unused */
    "\033[34m",   /* 1 – blue */
    "\033[32m",   /* 2 – green */
    "\033[31m",   /* 3 – red */
    "\033[35m",   /* 4 – purple */
    "\033[33m",   /* 5 – yellow */
    "\033[36m",   /* 6 – cyan */
    "\033[37m",   /* 7 – white */
    "\033[90m",   /* 8 – gray */
};
#define RESET "\033[0m"
#define BOLD  "\033[1m"

void print_board(const Game *g, int show_all) {
    /* Header */
    printf("\n  %s MINESWEEPER %s   Mines: %d   Flags: %d\n\n",
           BOLD, RESET, g->mines, g->flags);

    /* Column labels */
    printf("      ");
    for (int c = 0; c < g->cols; c++)
        printf(" %c", 'A' + c);
    printf("\n");

    printf("    +");
    for (int c = 0; c < g->cols; c++)
        printf("--");
    printf("-+\n");

    for (int r = 0; r < g->rows; r++) {
        printf("  %2d |", r + 1);
        for (int c = 0; c < g->cols; c++) {
            const Cell *cell = &g->grid[r][c];
            char ch;
            const char *color = "";

            if (show_all && cell->is_mine) {
                color = "\033[31;1m";
                ch = '*';
            } else if (cell->state == HIDDEN) {
                ch = '.';
                color = "\033[90m";
            } else if (cell->state == FLAGGED) {
                ch = 'F';
                color = "\033[33;1m";
            } else { /* REVEALED */
                if (cell->is_mine) {
                    ch = '*';
                    color = "\033[31;1m";
                } else if (cell->adjacent == 0) {
                    ch = ' ';
                } else {
                    ch = '0' + cell->adjacent;
                    color = num_colors[cell->adjacent];
                }
            }
            printf(" %s%c%s", color, ch, RESET);
        }
        printf(" |\n");
    }

    printf("    +");
    for (int c = 0; c < g->cols; c++)
        printf("--");
    printf("-+\n");
}

/* ─── Input Parsing ─── */
/* Accepts:  "A3" or "3A" to reveal,  "xA3" or "x3A" to flag/unflag
   Returns 1 on success, 0 on bad input.
   *action: 0 = reveal, 1 = flag */
int parse_input(const char *buf, int *action, int *row, int *col, int max_row, int max_col) {
    /* Skip whitespace */
    while (*buf == ' ' || *buf == '\t') buf++;

    *action = 0;
    if (toupper((unsigned char)*buf) == 'X') {
        *action = 1;
        buf++;
        while (*buf == ' ') buf++;
    }

    int got_row = 0, got_col = 0;
    int r = 0, c = 0;

    /* Try letter then number, or number then letter */
    const char *p = buf;

    if (isalpha((unsigned char)*p)) {
        c = toupper((unsigned char)*p) - 'A';
        got_col = 1;
        p++;
        while (*p == ' ') p++;
        if (isdigit((unsigned char)*p)) {
            r = (int)strtol(p, NULL, 10) - 1;
            got_row = 1;
        }
    } else if (isdigit((unsigned char)*p)) {
        char *end;
        r = (int)strtol(p, &end, 10) - 1;
        got_row = 1;
        p = end;
        while (*p == ' ') p++;
        if (isalpha((unsigned char)*p)) {
            c = toupper((unsigned char)*p) - 'A';
            got_col = 1;
        }
    }

    if (!got_row || !got_col) return 0;
    if (r < 0 || r >= max_row || c < 0 || c >= max_col) return 0;

    *row = r;
    *col = c;
    return 1;
}

/* ─── Difficulty Selection ─── */
typedef struct { const char *name; int rows, cols, mines; } Preset;

static const Preset presets[] = {
    { "Beginner",     9,  9, 10 },
    { "Intermediate", 16, 16, 40 },
    { "Expert",       16, 26, 99 },
};

/* ─── Main ─── */
int main(void) {
    srand((unsigned)time(NULL));
    Game game;

    clear_screen();
    printf("\n");
    printf("  +------------------------------+\n");
    printf("  |       M I N E S W E E P E R  |\n");
    printf("  +------------------------------+\n\n");
    printf("  Select difficulty:\n");
    printf("    1) Beginner      ( 9x9,  10 mines)\n");
    printf("    2) Intermediate  (16x16, 40 mines)\n");
    printf("    3) Expert        (16x26, 99 mines)\n\n");
    printf("  Choice [1-3]: ");
    fflush(stdout);

    char buf[64];
    int choice = 1;
    if (fgets(buf, sizeof(buf), stdin)) {
        int v = atoi(buf);
        if (v >= 1 && v <= 3) choice = v;
    }

    const Preset *p = &presets[choice - 1];
    game_init(&game, p->rows, p->cols, p->mines);

    /* Game loop */
    while (game.game_over == 0) {
        clear_screen();
        print_board(&game, 0);
        printf("\n  Enter move (e.g. B3 to reveal, xB3 to flag): ");
        fflush(stdout);

        if (!fgets(buf, sizeof(buf), stdin)) break;

        /* Quit shortcut */
        if (toupper((unsigned char)buf[0]) == 'Q') break;

        int action, r, c;
        if (!parse_input(buf, &action, &r, &c, game.rows, game.cols)) {
            printf("  Invalid input. Press Enter...");
            fgets(buf, sizeof(buf), stdin);
            continue;
        }

        if (action == 1) {
            if (game.grid[r][c].state == REVEALED) {
                printf("  Cell is already revealed. Press Enter...");
                fgets(buf, sizeof(buf), stdin);
                continue;
            }
            toggle_flag(&game, r, c);
        } else {
            if (game.grid[r][c].state == FLAGGED) {
                printf("  Cell is flagged. Unflag first (x%c%d). Press Enter...",
                       'A' + c, r + 1);
                fgets(buf, sizeof(buf), stdin);
                continue;
            }
            /* On first move, place mines away from chosen cell */
            if (game.first_move) {
                place_mines(&game, r, c);
                calc_adjacent(&game);
                game.first_move = 0;
            }
            reveal(&game, r, c);
        }
    }

    /* End screen */
    clear_screen();
    print_board(&game, 1);

    if (game.game_over == 1) {
        printf("\n  ** YOU WIN! Cleared all %d safe cells. **\n\n", game.total_safe);
    } else if (game.game_over == -1) {
        printf("\n  BOOM! You hit a mine. Better luck next time.\n\n");
    } else {
        printf("\n  Game quit.\n\n");
    }

    return 0;
}
