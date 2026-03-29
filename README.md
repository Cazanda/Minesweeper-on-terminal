# Minesweeper — Terminal Edition (C)

A fully playable Minesweeper clone written in pure C, designed to run in any terminal. No external libraries required — just the C standard library.

![Gameplay Screenshot](screenshot.png)

---

## How to Build & Play

```bash
gcc -o minesweeper minesweeper.c
./minesweeper
```

On Windows with MinGW:
```bash
gcc -o minesweeper.exe minesweeper.c
minesweeper.exe
```

### Controls

| Input   | Action                        |
|---------|-------------------------------|
| `B3`    | Reveal column B, row 3        |
| `3B`    | Same thing (order is flexible)|
| `xB3`   | Flag/unflag column B, row 3   |
| `Q`     | Quit the game                 |

### Difficulty Presets

| Level        | Grid   | Mines |
|--------------|--------|-------|
| Beginner     | 9x9    | 10    |
| Intermediate | 16x16  | 40    |
| Expert       | 16x26  | 99    |

---

## Timer & High Scores

### Timer

The timer starts on your **first move** (not when the game launches), so browsing the menu doesn't count against your time. While playing, the elapsed time is shown live in the board header:

```
  MINESWEEPER   Mines: 10   Flags: 2   Time: 0:42
```

On a win, your final time is displayed on the end screen.

### High Scores

The top 5 fastest winning times are tracked **per difficulty level** and saved to `highscores.dat` in the game directory. Scores persist between sessions.

- The leaderboard is shown on the **main menu** every time you launch.
- If your winning time enters the top 5, the game announces `** NEW HIGH SCORE! **` and shows the updated leaderboard.
- Times are sorted ascending (fastest first) and displayed in `M:SS` format.

The `highscores.dat` file is plain text and can be deleted to reset all scores.

---

## How This Project Was Built

This game was developed iteratively, starting from a working prototype and refining it through real playtesting. Here's the full timeline of decisions and fixes.

### Step 1: Data Model

The first decision was how to represent the game state. Each cell needs to track three things: whether it contains a mine, how many neighboring mines it has, and its visibility state (hidden, revealed, or flagged). This led to two structs:

```c
typedef enum { HIDDEN, REVEALED, FLAGGED } CellState;

typedef struct {
    int is_mine;
    int adjacent;
    CellState state;
} Cell;
```

The entire game state (grid, dimensions, counters, game-over flag) lives in a single `Game` struct. This keeps everything in one place and makes it easy to pass around to functions without globals.

### Step 2: Mine Placement — The "Safe First Click" Problem

In real Minesweeper, your first click never hits a mine. The naive approach of placing mines before the game starts can't guarantee this. The solution: **defer mine placement until after the first move**, then exclude a 3x3 zone around the clicked cell.

```c
if (abs(r - safe_r) <= 1 && abs(c - safe_c) <= 1) continue;
```

This means the board is empty until you click. Mines are placed, adjacency counts are calculated, and then the reveal happens — all in response to that first input.

### Step 3: Flood-Fill Reveal

When you reveal a cell with zero adjacent mines, the game should automatically reveal all connected empty cells (and their numbered borders). This is a classic flood-fill, implemented with recursion:

```c
void reveal(Game *g, int r, int c) {
    if (!in_bounds(g, r, c)) return;
    if (cell->state != HIDDEN) return;

    cell->state = REVEALED;

    if (cell->adjacent == 0) {
        for (int d = 0; d < 8; d++)
            reveal(g, r + dr[d], c + dc[d]);
    }
}
```

The 8-directional neighbor offsets are stored in parallel arrays (`dr[]` and `dc[]`) to keep the loop clean. The base cases (out of bounds, already revealed) prevent infinite recursion.

### Step 4: Terminal Rendering

The board is drawn using `printf` with ANSI escape codes for color. Each number (1-8) gets its own color matching the classic Minesweeper palette:

- 1 = blue, 2 = green, 3 = red, 4 = purple, etc.

The screen is cleared between frames using `\033[H\033[J` (ANSI escape) on Unix or `system("cls")` on Windows, selected at compile time with `#ifdef _WIN32`.

### Step 5: Input Parsing

The parser accepts flexible coordinate formats — `B3`, `3B`, `b3` all work. It needed to handle:

- Leading whitespace
- Case insensitivity
- A flag prefix character
- Column letters A-P and row numbers 1-16

This is done by scanning for the first alpha and first numeric characters, regardless of order.

---

## Bugs Found During Playtesting

### Bug 1: Unicode Characters Broken on Windows

**Problem:** The original title screen used Unicode box-drawing characters (`╔═║╚╝`) and symbols (`×`, `★`, `✖`). On Windows, these rendered as garbled text (`ÔÇö`, `ÔÇÖ`, etc.) because the default console code page doesn't support UTF-8.

**Fix:** Replaced all Unicode with plain ASCII: `+`, `-`, `|`, `x`, `*`.

### Bug 2: Column Headers Misaligned

**Problem:** The column letter labels (A B C D...) were shifted one space to the left relative to the actual grid data, because the header didn't account for the full width of the row number prefix (`%2d |`).

**Fix:** Added one extra space to the column header `printf` to align with the 6-character row prefix.

### Bug 3: Can't Reveal Column F (Flag Prefix Conflict)

**Problem:** Typing `F12` to reveal column F, row 12 was interpreted as "flag cell at column 1, row 2" because the parser consumed `F` as the flag command prefix before parsing the coordinate.

**Initial fix attempt:** Only treat `F` as a flag prefix when followed by another letter. This worked but was fragile.

**Final fix:** Changed the flag prefix from `f` to `x`, since `X` isn't used as a column name (the grid only goes up to column P on Expert). Clean separation, no ambiguity.

### Bug 4: Flagging a Revealed Cell Silently Does Nothing

**Problem:** Typing `xB13` on an already-revealed cell caused the screen to redraw with no visible change and no error message. The `toggle_flag` function correctly ignored revealed cells, but the game loop didn't tell the player why nothing happened.

**Fix:** Added a guard before `toggle_flag` that checks if the cell is already revealed and prints a message: "Cell is already revealed."

---

## C Concepts Covered

This project exercises a good cross-section of C fundamentals:

- **Structs and enums** for game state modeling
- **2D arrays** for the grid
- **Recursion** for flood-fill
- **Pointer arithmetic** and string parsing for input handling
- **Preprocessor directives** (`#ifdef`) for cross-platform support
- **ANSI escape codes** for terminal colors and screen control
- **`rand()` and `srand()`** for mine placement randomization
- **`time.h`** for the live game timer and high score timestamps
- **File I/O** (`fopen`/`fprintf`/`fscanf`) for persistent high score storage

