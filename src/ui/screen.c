/***************************************************************
 * screen.c: the windowed front end, on the 80x25 character screen.
 *
 * The board takes the left half of the screen: 8x8 squares of 5
 * columns and 3 rows each, row 1 at the top, with the rows' numbers in
 * column 0 and the columns' letters in row 24. A disc is a circle
 * drawn over the 15 characters of its square, black or white; a dot
 * marks each square where the player may move. The right half has the
 * players and their discs, the state of the game, the level, what the
 * engine is thinking, the moves, the move being typed, and the keys.
 *
 * The keys:
 *   Arrows          move the cursor on the board
 *   Enter, Space    move to the square under the cursor
 *   d3              type a move, then Enter. Backspace erases.
 *   M               the engine makes the player's move
 *   Esc             drop the typed move; while the engine thinks,
 *                   make it move now
 *   U               take back the player's last move (and the
 *                   engine's reply)
 *   S               switch sides: the engine plays the side to move
 *   N               a new game
 *   + -             a stronger or weaker engine: levels 1 to 5
 *                   (levels.h), 1 at first
 *   Q               quit, and clear the screen
 * The letters of commands are none of a to h, which start a move.
 *
 * The player is black at first, and moves first. A side with no move
 * passes by itself.
 *
 * The program copies the character ROM into the font RAM, with
 * Spleen's letters, writes the glyphs of the disc and the dot over
 * the codes from 0x80 on, and shows the font RAM (nice_cpu's
 * SYSTEM.md §1.8). On Q it shows the ROM again.
 *
 * Each move is also written to the console, as "move d3" or "move
 * pass", the end of the game as "result ...", and the position at the
 * end as "pos ...", as the text front end takes it. The program ends on
 * Q, or at the end of the console's input, which leaves the screen as
 * it is.
 *
 * The engine polls the keyboard during its search on a board, and in
 * niceemu only once a read of the console has found no byte waiting:
 * that is, once the input is a keyboard. Input from a file (niceemu
 * -i, in the tests) is then read only between moves, so a run is the
 * same every time.
 *
 * This front end is for NICE only. It does not use printf.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#include <stdint.h>
#include <nice.h>
#include "../engine/engine.h"
#include "../platform/platform.h"
#include "levels.h"
#include "version.h"

/***************************************************************
 * Colors: a screen word has the background in bits 31..20, the
 * foreground in bits 19..8, and the character in bits 7..0.
 ***************************************************************/
#define COLOR(bg, fg) ((uint32_t) (bg) << 20 | (uint32_t) (fg) << 8)

enum
{
    SQ_A    = 0x283, SQ_B    = 0x273,         // The squares, alternately
    LAST_A  = 0x6A4, LAST_B  = 0x594,         // The last move
    CUR_A   = 0x4B6, CUR_B   = 0x3A5,         // The cursor
    INK_BLACK = 0x111, INK_WHITE = 0xEEE,     // The discs
    INK_DOT = 0x152,                          // A legal move
    PANEL   = 0x123,                          // The background of the rest
    TEXT    = 0xCCC, BRIGHT = 0xFFF, DIM = 0x89A, WARN = 0xFA4
};

// The first column of the right half.
#define PX 42

// The rows of the right half.
enum
{
    ROW_TITLE = 0, ROW_BLACK = 1, ROW_WHITE = 2,
    ROW_STATUS = 4, ROW_LEVEL = 5,
    ROW_INFO = 7, ROW_PV = 8,
    ROW_MOVES = 10, MOVE_ROWS = 9,            // 9 rows of 7 moves
    ROW_INPUT = 20, ROW_MSG = 21, ROW_HELP = 22 // Help: three rows
};

/***************************************************************
 * The keys
 ***************************************************************/
enum
{
    K_END = -1, K_NONE = -2,
    K_ENTER = 13, K_ESC = 27, K_BS = 127,
    K_UP = 256, K_DOWN, K_RIGHT, K_LEFT, K_OTHER
};

/***************************************************************
 * The state of the front end
 ***************************************************************/
#define MAX_PLAYED 128             // A game has at most 60 moves and its passes

static int    played[MAX_PLAYED];  // The moves of the game
static int    n_played;
static int    human;               // The side the player has: ENGINE_BLACK or ENGINE_WHITE
static int    cur_col, cur_row;    // The cursor
static char   typed[3];            // The move being typed
static int    n_typed;
static const char *message = "";
static char   msg_buf[32];
static int    level = 1;

static int    legal[64];           // 1 on the squares where the side to move may move

static engine_info_t info;
static int    have_info;
static int    thinking;

/***************************************************************
 * Writing to the screen
 ***************************************************************/
static void put(int col, int row, uint32_t color, int c)
{
    nice_screen_put(col, row, (int) (color | (uint8_t) c));
}

// Writes s at col, row, and returns the column after it.
static int put_str(int col, int row, uint32_t color, const char *s)
{
    while (*s && col < 80)
        put(col++, row, color, *s++);
    return col;
}

// Fills a row of the right half from col to its end with blanks.
static void clear_to_end(int col, int row)
{
    while (col < 80)
        put(col++, row, COLOR(PANEL, TEXT), ' ');
}

// Writes the decimal digits of n to buf, and returns the end.
static char *fmt_uint(char *buf, uint32_t n)
{
    char tmp[10];
    int i = 0;
    do
        tmp[i++] = (char) ('0' + n % 10);
    while ((n /= 10) != 0);
    while (i > 0)
        *buf++ = tmp[--i];
    *buf = '\0';
    return buf;
}

static char *fmt_str(char *buf, const char *s)
{
    while (*s)
        *buf++ = *s++;
    *buf = '\0';
    return buf;
}

static void set_cursor(int show, int col, int row)
{
    nice_poke(0xFFFFFF81u, (unsigned) col);
    nice_poke(0xFFFFFF82u, (unsigned) row);
    nice_poke(0xFFFFFF80u, show ? 0x60u : 0u); // Shown, blinking
}

/***************************************************************
 * The font: a square is 5x3 characters of 16x32 pixels, 80x96 pixels.
 * The disc is a circle in it, over 15 glyphs, and the dot a small one,
 * in the glyph of the middle.
 ***************************************************************/
#define FONT_RAM  0xFFFE0000u
#define FONT_ROM  0xFFFE2000u
#define FONT_BASE 0xFFFFFF86u
#define GLYPH_DISC 0x80            // The 15 glyphs of the disc, row by row
#define GLYPH_DOT  0x8F

#define DISC_R 37                  // The radius of the disc, in pixels
#define DOT_R  9

// Whether pixel x, y of a square is within r of its middle.
static int in_circle(int x, int y, int r)
{
    int dx = 2 * x + 1 - 80, dy = 2 * y + 1 - 96;  // In half pixels
    return dx * dx + dy * dy <= 4 * r * r;
}

static void font_init(void)
{
    for (unsigned i = 0; i < 256 * 32; ++i)
        nice_poke(FONT_RAM + i, nice_peek(FONT_ROM + i));
    for (int g = 0; g < 16; ++g)
    {
        // The dot is the glyph of the middle, column 2 of row 1.
        int c = g < 15 ? g % 5 : 2, r = g < 15 ? g / 5 : 1;
        int radius = g < 15 ? DISC_R : DOT_R;
        for (int y = 0; y < 32; ++y)
        {
            unsigned bits = 0;
            for (int x = 0; x < 16; ++x)
                if (in_circle(16 * c + x, 32 * r + y, radius))
                    bits |= 0x8000u >> x;
            nice_poke(FONT_RAM + 32 * (unsigned) (GLYPH_DISC + g) + (unsigned) y, bits);
        }
    }
    nice_poke(FONT_BASE, FONT_RAM);
}

/***************************************************************
 * The board
 ***************************************************************/
// The screen column and row of the top left cell of a square.
static int sq_col(int col) { return 1 + 5 * col; }
static int sq_row(int row) { return 3 * row; }

static int game_over(void) { return engine_game_over(); }

static void update_legal(void)
{
    int list[ENGINE_MAX_MOVES];
    for (int i = 0; i < 64; ++i)
        legal[i] = 0;
    int n = engine_legal_moves(list);
    for (int i = 0; i < n; ++i)
        if (list[i] != ENGINE_PASS)
            legal[list[i]] = 1;
}

static void draw_square(int col, int row)
{
    int sq = col + 8 * row;
    int a = !((col + row) & 1);
    int bg = a ? SQ_A : SQ_B;
    if (n_played && played[n_played - 1] == sq)
        bg = a ? LAST_A : LAST_B;
    if (col == cur_col && row == cur_row && !thinking)
        bg = a ? CUR_A : CUR_B;

    int x = sq_col(col), y = sq_row(row);
    int disc = engine_square(sq);
    int dot = !disc && legal[sq] && engine_side_to_move() == human && !thinking;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 5; ++c)
        {
            if (disc)
                put(x + c, y + r, COLOR(bg, disc == ENGINE_BLACK ? INK_BLACK : INK_WHITE),
                    GLYPH_DISC + 5 * r + c);
            else if (dot && r == 1 && c == 2)
                put(x + c, y + r, COLOR(bg, INK_DOT), GLYPH_DOT);
            else
                put(x + c, y + r, COLOR(bg, 0), ' ');
        }
}

static void draw_board(void)
{
    uint32_t label = COLOR(PANEL, DIM);
    for (int row = 0; row < 8; ++row)
    {
        int y = sq_row(row);
        put(0, y, label, ' ');
        put(0, y + 1, label, '1' + row);
        put(0, y + 2, label, ' ');
        for (int col = 0; col < 8; ++col)
            draw_square(col, row);
    }
    for (int x = 0; x <= 41; ++x)
        put(x, 24, label, ' ');
    for (int col = 0; col < 8; ++col)
        put(sq_col(col) + 2, 24, label, 'a' + col);
    for (int y = 0; y < 24; ++y)
        put(41, y, label, ' ');
}

/***************************************************************
 * The right half
 ***************************************************************/
static void draw_line(int row, uint32_t color, const char *s)
{
    put(PX - 1, row, COLOR(PANEL, TEXT), ' ');
    clear_to_end(put_str(PX, row, color, s), row);
}

static const char *side_name(int side)
{
    return side == ENGINE_BLACK ? "Black" : "White";
}

static void draw_player(int side)
{
    int black, white;
    engine_count(&black, &white);
    char buf[40], *p = buf;
    p = fmt_str(p, side_name(side));
    p = fmt_str(p, side == human ? "  you          " : "  nice_othello ");
    p = fmt_uint(p, (uint32_t) (side == ENGINE_BLACK ? black : white));
    int bright = side == engine_side_to_move() && !game_over();
    draw_line(side == ENGINE_BLACK ? ROW_BLACK : ROW_WHITE,
              COLOR(PANEL, bright ? BRIGHT : TEXT), buf);
}

// The state of the game, as the status row shows it and the log's
// result line has it.
static void status_text(char *buf)
{
    int black, white;
    engine_count(&black, &white);
    if (game_over())
    {
        char *p = fmt_str(buf, black > white ? "Black wins " : white > black ? "White wins " : "Draw ");
        p = fmt_uint(p, (uint32_t) black);
        p = fmt_str(p, "-");
        fmt_uint(p, (uint32_t) white);
        return;
    }
    char *p = fmt_str(buf, side_name(engine_side_to_move()));
    fmt_str(p, thinking ? " to move: thinking" : " to move");
}

static void draw_status(void)
{
    char buf[40];
    status_text(buf);
    draw_line(ROW_STATUS, COLOR(PANEL, game_over() ? WARN : BRIGHT), buf);
}

static void draw_level(void)
{
    char buf[24];
    fmt_str(fmt_uint(fmt_str(buf, "Level "), (uint32_t) level), " of 5");
    draw_line(ROW_LEVEL, COLOR(PANEL, TEXT), buf);
}

// The score in discs, with two decimals, or a whole number if exact.
static char *fmt_score(char *p, int score, int exact)
{
    *p++ = score < 0 ? '-' : '+';
    uint32_t a = (uint32_t) (score < 0 ? -score : score);
    p = fmt_uint(p, a / 100);
    if (!exact)
    {
        *p++ = '.';
        *p++ = (char) ('0' + a / 10 % 10);
        *p++ = (char) ('0' + a % 10);
        *p = '\0';
    }
    return p;
}

static void draw_info(void)
{
    char buf[48], *p = buf;
    if (!have_info)
    {
        draw_line(ROW_INFO, COLOR(PANEL, TEXT), "");
        draw_line(ROW_PV, COLOR(PANEL, TEXT), "");
        return;
    }
    if (info.exact == ENGINE_INEXACT)
    {
        p = fmt_uint(fmt_str(p, "Depth "), (uint32_t) info.depth);
        p = fmt_score(fmt_str(p, "  Score "), info.score, 0);
    }
    else
    {
        p = fmt_uint(fmt_str(p, "Solved "), (uint32_t) info.depth);
        p = fmt_str(p, " empty  Score ");
        if (info.exact == ENGINE_WLD)
            p = fmt_str(p, info.score > 0 ? "win" : info.score < 0 ? "loss" : "draw");
        else
            p = fmt_score(p, info.score, 1);
    }
    fmt_uint(fmt_str(p, "  Nodes "), info.nodes);
    draw_line(ROW_INFO, COLOR(PANEL, TEXT), buf);

    p = fmt_str(buf, "PV");
    for (int i = 0; i < info.pv_len && p < buf + sizeof buf - 6; ++i)
    {
        char name[5];
        engine_move_name(info.pv[i], name);
        p = fmt_str(fmt_str(p, " "), name);
    }
    draw_line(ROW_PV, COLOR(PANEL, DIM), buf);
}

// The moves, 7 to a row, each row after the number of its first move:
// black's bright, and white's in the color of the text. A pass is a
// move, so black has the moves of even index. The last MOVE_ROWS rows.
#define MOVES_PER_ROW 7
static void draw_moves(void)
{
    draw_line(ROW_MOVES - 1, COLOR(PANEL, DIM), "Moves");
    int rows = (n_played + MOVES_PER_ROW - 1) / MOVES_PER_ROW;
    int first = rows > MOVE_ROWS ? rows - MOVE_ROWS : 0;
    for (int r = 0; r < MOVE_ROWS; ++r)
    {
        int y = ROW_MOVES + r;
        int i = (first + r) * MOVES_PER_ROW;
        int col = PX;
        put(PX - 1, y, COLOR(PANEL, TEXT), ' ');
        if (i < n_played)
        {
            char buf[8], *p = buf;
            if (i + 1 < 10)
                *p++ = ' ';
            fmt_str(fmt_uint(p, (uint32_t) i + 1), " ");
            col = put_str(col, y, COLOR(PANEL, DIM), buf);
        }
        for (int k = 0; k < MOVES_PER_ROW && i + k < n_played; ++k)
        {
            char name[5];
            engine_move_name(played[i + k], name);
            int end = col + 5;
            col = put_str(col, y, COLOR(PANEL, (i + k) % 2 ? TEXT : BRIGHT), name);
            while (col < end)
                put(col++, y, COLOR(PANEL, TEXT), ' ');
        }
        clear_to_end(col, y);
    }
}

static void draw_input(void)
{
    char buf[8];
    typed[n_typed] = '\0';
    fmt_str(fmt_str(buf, "> "), typed);
    draw_line(ROW_INPUT, COLOR(PANEL, BRIGHT), buf);
    set_cursor(engine_side_to_move() == human && !thinking && !game_over(), PX + 2 + n_typed,
               ROW_INPUT);
    draw_line(ROW_MSG, COLOR(PANEL, WARN), message);
}

static void draw_all(void)
{
    draw_board();
    int col = put_str(PX, ROW_TITLE, COLOR(PANEL, BRIGHT), "nice_othello ");
    put(PX - 1, ROW_TITLE, COLOR(PANEL, TEXT), ' ');
    clear_to_end(put_str(col, ROW_TITLE, COLOR(PANEL, BRIGHT), nice_othello_version), ROW_TITLE);
    draw_player(ENGINE_BLACK);
    draw_player(ENGINE_WHITE);
    draw_line(ROW_WHITE + 1, COLOR(PANEL, TEXT), "");
    draw_status();
    draw_level();
    draw_line(ROW_LEVEL + 1, COLOR(PANEL, TEXT), "");
    draw_info();
    draw_line(ROW_PV + 1, COLOR(PANEL, TEXT), "");
    draw_moves();
    draw_line(ROW_INPUT - 1, COLOR(PANEL, TEXT), "");
    draw_input();
    draw_line(ROW_HELP,     COLOR(PANEL, DIM), "Arrows+Enter, or d3 Enter: move");
    draw_line(ROW_HELP + 1, COLOR(PANEL, DIM), "M the engine moves for you  U undo");
    draw_line(ROW_HELP + 2, COLOR(PANEL, DIM), "S switch  N new  +/- level  Q quit");
}

/***************************************************************
 * The console: the keys, and the log of the moves
 ***************************************************************/
static int pushed_back = K_NONE;
static int live;                   // The input is a keyboard (see the top)

// The log goes to the console's register itself, not through
// nice_putc: under the monitor on the board, nice_putc prints on the
// screen too, where the board is (NiceMonMEGA65's monitor/README.md).
static void console_putc(int c)
{
    nice_poke(NICE_CONSOLE, (unsigned) c);
}

static void console_print(const char *s)
{
    while (*s)
        console_putc(*s++);
}

static int read_byte(void)
{
    int c = pushed_back;
    if (c != K_NONE)
    {
        pushed_back = K_NONE;
        return c;
    }
    c = platform_pollc();
    if (c == K_NONE)
        live = 1;
    return c;
}

// The byte after an ESC: the rest of a key's bytes may still be on
// their way from a keyboard, so this waits up to 20 ms for one.
static int read_byte_soon(void)
{
    uint32_t t0 = platform_micros();
    int c;
    while ((c = read_byte()) == K_NONE && platform_micros() - t0 < 20000)
        ;
    return c;
}

// The next key, K_NONE if none waits, or K_END.
static int poll_key(void)
{
    int c = read_byte();
    if (c == '\n')
        return K_ENTER;
    if (c == 8)
        return K_BS;
    if (c != K_ESC)
        return c;

    // Esc alone, or the start of the bytes of a key: ESC [ A for Up,
    // ESC [ 1 5 ~ for F5, ESC O P for F1.
    int d = read_byte_soon();
    if (d != '[' && d != 'O')
    {
        pushed_back = d;
        return K_ESC;
    }
    int e = read_byte_soon();
    switch (e)
    {
        case 'A': return K_UP;
        case 'B': return K_DOWN;
        case 'C': return K_RIGHT;
        case 'D': return K_LEFT;
        default: break;
    }
    while (e >= '0' && e <= '9')
        e = read_byte_soon();
    return K_OTHER;
}

// The keys typed while the engine thought, for after its move.
#define AHEAD 16
static int ahead[AHEAD];
static int n_ahead;

static int wait_key(void)
{
    if (n_ahead)
    {
        int k = ahead[0];
        for (int i = 1; i < n_ahead; ++i)
            ahead[i - 1] = ahead[i];
        --n_ahead;
        return k;
    }
    int k;
    while ((k = poll_key()) == K_NONE)
        ;
    return k;
}

/***************************************************************
 * The game
 ***************************************************************/
static void log_result(void)
{
    char buf[40];
    status_text(buf);
    console_print("result ");
    console_print(buf);
    console_putc('\n');
}

static void play(int move)
{
    if (n_played >= MAX_PLAYED || !engine_make_move(move))
    {
        message = "Not a legal move.";
        return;
    }
    played[n_played++] = move;
    char name[5];
    engine_move_name(move, name);
    console_print("move ");
    console_print(name);
    console_putc('\n');
    update_legal();
    if (game_over())
        log_result();
}

static void new_game(void)
{
    engine_new_game();
    n_played = 0;
    have_info = 0;
    update_legal();
}

static void undo(void)
{
    if (n_played == 0)
    {
        message = "Nothing to take back.";
        return;
    }
    // Back to the player's move before the last, past the engine's
    // moves and the passes.
    do
    {
        engine_undo();
        --n_played;
    } while (n_played > 0
             && (engine_side_to_move() != human || played[n_played] == ENGINE_PASS));
    have_info = 0;
    update_legal();
}

/***************************************************************
 * The engine's move
 ***************************************************************/
static int stop_search(void)
{
    if (!live)
        return 0;
    int k = poll_key();
    if (k == K_ESC)
        return 1;
    if (k != K_NONE && n_ahead < AHEAD)
        ahead[n_ahead++] = k;
    return 0;
}

static int on_info(const engine_info_t *in)
{
    info = *in;
    have_info = 1;
    draw_info();
    return 0;
}

// The engine finds a move for the side to move, at the level, and
// plays it.
static void think(void)
{
    have_info = 0;
    thinking = 1;
    draw_all();
    engine_limits_t limits = {0, 0, stop_search, 0, 0};
    level_limits(level, &limits);
    int move = engine_search(&limits, on_info);
    thinking = 0;
    if (move != ENGINE_NO_MOVE)
        play(move);
}

/***************************************************************
 * The keys
 ***************************************************************/
static void enter_typed(void)
{
    typed[n_typed] = '\0';
    int move = engine_parse_move(typed);
    n_typed = 0;
    if (move >= 0 && move < 64 && legal[move] && engine_side_to_move() == human)
        play(move);
    else
    {
        fmt_str(fmt_str(msg_buf, "Not a legal move: "), typed);
        message = msg_buf;
    }
}

static void enter_square(void)
{
    int sq = cur_col + 8 * cur_row;
    if (game_over() || engine_side_to_move() != human)
        return;
    if (legal[sq])
        play(sq);
    else
        message = "No move there.";
}

static void move_cursor(int k)
{
    switch (k)
    {
        case K_UP:    cur_row -= cur_row > 0; break;
        case K_DOWN:  cur_row += cur_row < 7; break;
        case K_RIGHT: cur_col += cur_col < 7; break;
        case K_LEFT:  cur_col -= cur_col > 0; break;
        default: break;
    }
}

static int fits_typed(int c)
{
    switch (n_typed)
    {
        case 0:  return c >= 'a' && c <= 'h';
        case 1:  return c >= '1' && c <= '8';
        default: return 0;
    }
}

// Acts on the key k. Returns 0 to quit.
static int handle_key(int k)
{
    message = "";
    if (k >= 'A' && k <= 'Z')
        k += 'a' - 'A';

    if (k >= K_UP && k <= K_LEFT)
        move_cursor(k);
    else if (k == K_ESC)
        n_typed = 0;
    else if (k == K_BS)
    {
        if (n_typed)
            --n_typed;
    }
    else if (k == K_ENTER || k == ' ')
    {
        if (n_typed == 2)
            enter_typed();
        else if (n_typed)
            message = "Type a move such as d3.";
        else
            enter_square();
    }
    else if (fits_typed(k))
        typed[n_typed++] = (char) k;
    else if (n_typed)
        message = "Type a move such as d3.";
    else switch (k)
    {
        case 'q':
            nice_poke(FONT_BASE, 0);
            nice_screen_clear();
            return 0;
        case 'm':
            if (!game_over() && engine_side_to_move() == human)
                think();
            break;
        case 'u': undo(); break;
        case 'n': new_game(); break;
        case 's':
            human = human == ENGINE_BLACK ? ENGINE_WHITE : ENGINE_BLACK;
            break;
        case '+':
        case '-':
            level += k == '+' ? level < LEVELS : -(level > 1);
            break;
        default: break;
    }
    return 1;
}

int main(void)
{
    human = ENGINE_BLACK;
    cur_col = 3;
    cur_row = 2;
    live = platform_on_board();
    font_init();
    new_game();

    for (;;)
    {
        if (!game_over())
        {
            int list[ENGINE_MAX_MOVES];
            if (engine_side_to_move() != human)
            {
                think();
                continue;
            }
            if (engine_legal_moves(list) == 1 && list[0] == ENGINE_PASS)
            {
                play(ENGINE_PASS);
                message = "You have no move, and pass.";
                continue;
            }
        }
        draw_all();
        int k = wait_key();
        if (k == K_END || !handle_key(k))
            break;
    }
    set_cursor(0, 0, 0);
    char pos[ENGINE_POS_LEN + 1];
    engine_get_position(pos);
    console_print("pos ");
    console_print(pos);
    console_putc('\n');
    return 0;
}
