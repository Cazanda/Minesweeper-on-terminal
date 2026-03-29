#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <conio.h>

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
    int game_over;      /* 0 = playing, 1 = won, -1 = lost */
    int first_move;
    time_t start_time;
    int timer_started;
} Game;

/* ─── Prototypes ─── */
void game_init(Game *g, int rows, int cols, int mines);
void place_mines(Game *g, int safe_r, int safe_c);
void calc_adjacent(Game *g);
void reveal(Game *g, int r, int c);
void toggle_flag(Game *g, int r, int c);
void print_board(const Game *g, int show_all, int cur_r, int cur_c);
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
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define CURSOR  "\033[7m"   /* reverse video for cursor highlight */

void print_board(const Game *g, int show_all, int cur_r, int cur_c) {
    /* Header */
    if (g->timer_started) {
        long elapsed = (long)(time(NULL) - g->start_time);
        printf("\n  %s MINESWEEPER %s   Mines: %d   Flags: %d   Time: %ld:%02ld\n\n",
               BOLD, RESET, g->mines, g->flags, elapsed / 60, elapsed % 60);
    } else {
        printf("\n  %s MINESWEEPER %s   Mines: %d   Flags: %d\n\n",
               BOLD, RESET, g->mines, g->flags);
    }

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

            if (!show_all && r == cur_r && c == cur_c)
                printf(" %s%s%c%s", CURSOR, color, ch, RESET);
            else
                printf(" %s%c%s", color, ch, RESET);
        }
        printf(" |\n");
    }

    printf("    +");
    for (int c = 0; c < g->cols; c++)
        printf("--");
    printf("-+\n");
}

/* ─── High Score System ─── */
#define NUM_DIFFICULTIES 3
#define MAX_SCORES       5
#define SCORES_FILE      "highscores.dat"

typedef struct {
    long times[NUM_DIFFICULTIES][MAX_SCORES]; /* seconds; 0 = empty slot */
    int  counts[NUM_DIFFICULTIES];
} ScoreBoard;

void scores_load(ScoreBoard *sb) {
    memset(sb, 0, sizeof(*sb));
    FILE *f = fopen(SCORES_FILE, "r");
    if (!f) return;
    for (int d = 0; d < NUM_DIFFICULTIES; d++) {
        if (fscanf(f, "%d", &sb->counts[d]) != 1) break;
        if (sb->counts[d] > MAX_SCORES) sb->counts[d] = MAX_SCORES;
        for (int i = 0; i < sb->counts[d]; i++) {
            if (fscanf(f, "%ld", &sb->times[d][i]) != 1) {
                sb->counts[d] = i;
                break;
            }
        }
    }
    fclose(f);
}

void scores_save(const ScoreBoard *sb) {
    FILE *f = fopen(SCORES_FILE, "w");
    if (!f) return;
    for (int d = 0; d < NUM_DIFFICULTIES; d++) {
        fprintf(f, "%d\n", sb->counts[d]);
        for (int i = 0; i < sb->counts[d]; i++)
            fprintf(f, "%ld\n", sb->times[d][i]);
    }
    fclose(f);
}

/* Returns 1 if the time qualifies as a new top-5, 0 otherwise */
int scores_add(ScoreBoard *sb, int d, long seconds) {
    int n = sb->counts[d];
    if (n >= MAX_SCORES && seconds >= sb->times[d][n - 1]) return 0;
    /* Find insertion position */
    int pos = (n < MAX_SCORES) ? n : MAX_SCORES - 1;
    for (int i = 0; i < n && i < MAX_SCORES; i++) {
        if (seconds < sb->times[d][i]) { pos = i; break; }
    }
    /* Shift entries down */
    int limit = (n < MAX_SCORES) ? n : MAX_SCORES - 1;
    for (int i = limit; i > pos; i--)
        sb->times[d][i] = sb->times[d][i - 1];
    sb->times[d][pos] = seconds;
    if (n < MAX_SCORES) sb->counts[d]++;
    return 1;
}

void scores_print(const ScoreBoard *sb, int d) {
    static const char *names[] = { "Beginner", "Intermediate", "Expert" };
    printf("  %s%-13s%s", BOLD, names[d], RESET);
    if (sb->counts[d] == 0) {
        printf("  --\n");
        return;
    }
    for (int i = 0; i < sb->counts[d]; i++) {
        long t = sb->times[d][i];
        if (i == 0) printf("  ");
        else        printf("               ");
        printf("%d. %ld:%02ld\n", i + 1, t / 60, t % 60);
    }
}

void scores_print_all(const ScoreBoard *sb) {
    printf("\n  %s--- High Scores (Top 5 per difficulty) ---%s\n", BOLD, RESET);
    for (int d = 0; d < NUM_DIFFICULTIES; d++)
        scores_print(sb, d);
    printf("\n");
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
    ScoreBoard scoreboard;
    scores_load(&scoreboard);

    clear_screen();
    printf("\n");
    printf("  +------------------------------+\n");
    printf("  |       M I N E S W E E P E R  |\n");
    printf("  +------------------------------+\n");
    scores_print_all(&scoreboard);
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

    int cursor_r = 0, cursor_c = 0;

    /* Game loop */
    while (game.game_over == 0) {
        clear_screen();
        print_board(&game, 0, cursor_r, cursor_c);
        printf("\n  Arrow keys: move  |  Z: reveal  |  X: flag  |  Q: quit\n");
        fflush(stdout);

        int key = _getch();

        /* Arrow keys send two bytes: 0 or 224, then a direction code */
        if (key == 0 || key == 224) {
            int key2 = _getch();
            switch (key2) {
                case 72: cursor_r = (cursor_r - 1 + game.rows) % game.rows; break; /* Up    */
                case 80: cursor_r = (cursor_r + 1) % game.rows;             break; /* Down  */
                case 75: cursor_c = (cursor_c - 1 + game.cols) % game.cols; break; /* Left  */
                case 77: cursor_c = (cursor_c + 1) % game.cols;             break; /* Right */
            }
            continue;
        }

        key = toupper(key);

        if (key == 'Q') break;

        int r = cursor_r, c = cursor_c;

        if (key == 'X') {
            /* Flag / unflag */
            if (game.grid[r][c].state == REVEALED) continue;
            toggle_flag(&game, r, c);
        } else if (key == 'Z') {
            /* Reveal */
            if (game.grid[r][c].state == FLAGGED) continue;
            if (game.first_move) {
                place_mines(&game, r, c);
                calc_adjacent(&game);
                game.first_move = 0;
                game.start_time = time(NULL);
                game.timer_started = 1;
            }
            reveal(&game, r, c);
        }
    }

    /* Compute final time */
    long final_time = 0;
    int  is_high_score = 0;
    if (game.game_over == 1 && game.timer_started) {
        final_time = (long)(time(NULL) - game.start_time);
        is_high_score = scores_add(&scoreboard, choice - 1, final_time);
        if (is_high_score) scores_save(&scoreboard);
    }

    /* End screen */
    clear_screen();
    print_board(&game, 1, -1, -1);

    if (game.game_over == 1) {
        long m = final_time / 60, s = final_time % 60;
        printf("\n  %s** YOU WIN! **%s  Cleared all %d safe cells in %ld:%02ld\n",
               BOLD, RESET, game.total_safe, m, s);
        if (is_high_score)
            printf("  %s** NEW HIGH SCORE! **%s\n", BOLD, RESET);
        scores_print_all(&scoreboard);
    } else if (game.game_over == -1) {
        printf("\n  BOOM! You hit a mine. Better luck next time.\n\n");
    } else {
        printf("\n  Game quit.\n\n");
    }

    return 0;
}
