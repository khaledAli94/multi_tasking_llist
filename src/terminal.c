/* terminal.c
 *
 * Single-parse terminal multiplexer.
 *
 * Implements a subset of the VT100/VT220/xterm state machine
 * sufficient for:
 *   - Printable ASCII + C0 controls (\r \n \t \b)
 *   - SGR colour sequences  ESC [ Pn ; ... m
 *   - Cursor motion         ESC [ Pn ; Pn H / A / B / C / D
 *   - Erase                 ESC [ Pn J / K
 *   - Mode sets             ESC [ ? Pn h / l
 *   - Bare ESC c  (reset)
 *
 * Parse-once, fan-out-to-N sinks.  No malloc.
 */

#include "terminal.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* helpers                                                              */
/* ------------------------------------------------------------------ */

static void emit(struct terminal_t *t, struct term_event_t *ev)
{
    int i;
    for (i = 0; i < t->nsinks; i++) {
        if (t->sinks[i])
            t->sinks[i](ev, t->sink_prv[i]);
    }
}

static void emit_char(struct terminal_t *t, unsigned char ch)
{
    struct term_event_t ev;
    ev.type   = TERM_CHAR;
    ev.u.ch.ch = ch;
    emit(t, &ev);
}

/* flush the current CSI sequence */
static void emit_csi(struct terminal_t *t, unsigned char final_byte)
{
    struct term_event_t ev;
    int i;

    ev.type = TERM_CSI;
    ev.u.csi.final        = final_byte;
    ev.u.csi.intermediate = t->intermediate;

    /* commit the last parameter being built */
    if (t->nparams < TERM_MAX_PARAMS)
        t->params[t->nparams++] = t->cur_param;

    ev.u.csi.nparams = t->nparams;
    for (i = 0; i < t->nparams && i < TERM_MAX_PARAMS; i++)
        ev.u.csi.params[i] = t->params[i];

    emit(t, &ev);
}

static void reset_csi(struct terminal_t *t)
{
    int i;
    for (i = 0; i < TERM_MAX_PARAMS; i++)
        t->params[i] = 0;
    t->nparams       = 0;
    t->cur_param     = 0;
    t->intermediate  = 0;
}

/* ------------------------------------------------------------------ */
/* public API                                                           */
/* ------------------------------------------------------------------ */

void terminal_init(struct terminal_t *t)
{
    memset(t, 0, sizeof(*t));
    t->state = TSTATE_GROUND;
}

int terminal_add_sink(struct terminal_t *t, term_sink_fn fn, void *prv)
{
    if (t->nsinks >= TERM_MAX_SINKS)
        return -1;
    t->sinks[t->nsinks]   = fn;
    t->sink_prv[t->nsinks] = prv;
    t->nsinks++;
    return 0;
}

void terminal_remove_sink(struct terminal_t *t, term_sink_fn fn)
{
    int i, j;
    for (i = 0; i < t->nsinks; i++) {
        if (t->sinks[i] == fn) {
            for (j = i + 1; j < t->nsinks; j++) {
                t->sinks[j-1]   = t->sinks[j];
                t->sink_prv[j-1] = t->sink_prv[j];
            }
            t->nsinks--;
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* core byte-by-byte parser                                             */
/* ------------------------------------------------------------------ */

void terminal_feed(struct terminal_t *t, unsigned char b)
{
    switch (t->state) {

    /* ── GROUND ────────────────────────────────────────────────── */
    case TSTATE_GROUND:
        if (b == 0x1B) {
            t->state = TSTATE_ESC;
            return;
        }
        /* C0 controls and printable ASCII all go straight through */
        emit_char(t, b);
        return;

    /* ── ESC received ───────────────────────────────────────────── */
    case TSTATE_ESC:
        if (b == '[') {
            reset_csi(t);
            t->state = TSTATE_CSI_ENTRY;
            return;
        }
        if (b == ']') {
            t->state = TSTATE_OSC;
            return;
        }
        /* bare ESC + char  (ESC c = full reset, ESC 7/8 = save/restore) */
        {
            struct term_event_t ev;
            ev.type    = TERM_ESCAPE;
            ev.u.esc.ch = b;
            emit(t, &ev);
        }
        t->state = TSTATE_GROUND;
        return;

    /* ── CSI entry (got ESC [, first real byte) ─────────────────── */
    case TSTATE_CSI_ENTRY:
        t->state = TSTATE_CSI_PARAM;

        /* 
         * Private-use marker: '?' '!' '>' '<' 
         * Store as intermediate and stay in param collection.
         */
        if (b == '?' || b == '!' || b == '>' || b == '<') {
            t->intermediate = b;
            return;
        }
        /* fall through — treat as first param byte */
        /* FALLTHROUGH */

    /* ── CSI param collection ───────────────────────────────────── */
    case TSTATE_CSI_PARAM:
        if (b >= '0' && b <= '9') {
            t->cur_param = t->cur_param * 10 + (b - '0');
            return;
        }
        if (b == ';') {
            /* end of one parameter, start next */
            if (t->nparams < TERM_MAX_PARAMS)
                t->params[t->nparams++] = t->cur_param;
            t->cur_param = 0;
            return;
        }
        if (b >= 0x20 && b <= 0x2F) {
            /* intermediate byte (space ! " # $ % & ' ( ) * + , - . /) */
            if (!t->intermediate)   /* keep first only */
                t->intermediate = b;
            t->state = TSTATE_CSI_INTER;
            return;
        }
        /* Final byte 0x40-0x7E */
        if (b >= 0x40 && b <= 0x7E) {
            emit_csi(t, b);
            t->state = TSTATE_GROUND;
            return;
        }
        /* Anything else: bail */
        t->state = TSTATE_GROUND;
        return;

    /* ── CSI intermediate (after an intermediate byte) ─────────── */
    case TSTATE_CSI_INTER:
        if (b >= 0x40 && b <= 0x7E) {
            emit_csi(t, b);
            t->state = TSTATE_GROUND;
            return;
        }
        /* another intermediate — ignore extras */
        return;

    /* ── OSC string (ESC ]) ─────────────────────────────────────── */
    case TSTATE_OSC:
        /*
         * Terminated by BEL (0x07) or ST (ESC $.
         * We do not need OSC content — just absorb until terminator.
         */
        if (b == 0x07) {
            t->state = TSTATE_GROUND;
            return;
        }
        if (b == 0x1B) {
            /*
             * Could be start of ST = ESC \.
             * The '\' will arrive as the next byte.
             * For simplicity absorb the ESC here and
             * let the next byte (if '\') also be absorbed.
             * Re-entering ESC state would break things,
             * so we just stay in OSC and drop it.
             */
            return;
        }
        if (b == '\\') {
            t->state = TSTATE_GROUND;
            return;
        }
        /* absorb OSC content silently */
        return;

    default:
        t->state = TSTATE_GROUND;
        return;
    }
}

void terminal_feed_buf(struct terminal_t *t,
                       const unsigned char *buf, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        terminal_feed(t, buf[i]);
}

/* ================================================================
 * console_sink
 *
 * Receives parsed events and drives the framebuffer text buffer.
 * All ANSI parsing has already been done by the time we get here.
 * This replaces the old parser inside console_putc().
 * ================================================================ */

#include "console.h"    /* struct cell_t, screen[][], cursor, colors  */
#include <stdlib.h>     /* malloc                                      */

/* ---- color state ---- */
static unsigned char con_fg = COLOR_DEFAULT;
static unsigned char con_bg = COLOR_DEFAULT;

/* ---- cursor ---- */
static int con_cx = 0;
static int con_cy = 0;

/* map ANSI color index → XRGB (used at render time; stored as index here) */

static void con_scroll_up(void)
{
    int r, c;
    for (r = 1; r < CONSOLE_ROWS; r++)
        for (c = 0; c < CONSOLE_COLS; c++)
            screen[r-1][c] = screen[r][c];

    for (c = 0; c < CONSOLE_COLS; c++) {
        screen[CONSOLE_ROWS-1][c].ch = ' ';
        screen[CONSOLE_ROWS-1][c].fg = COLOR_DEFAULT;
        screen[CONSOLE_ROWS-1][c].bg = COLOR_DEFAULT;
    }
    con_cy = CONSOLE_ROWS - 1;
}

static void con_newline(void)
{
    con_cx = 0;
    con_cy++;
    if (con_cy >= CONSOLE_ROWS)
        con_scroll_up();
}

/* Write one raw character to current cursor position */
static void con_put_raw(unsigned char ch)
{
    if (con_cx >= CONSOLE_COLS)
        con_newline();
    screen[con_cy][con_cx].ch = ch;
    screen[con_cy][con_cx].fg = con_fg;
    screen[con_cy][con_cx].bg = con_bg;
    con_cx++;
}

/* Clear a rectangular region */
static void con_clear_rect(int x0, int y0, int x1, int y1)
{
    int r, c;
    for (r = y0; r <= y1 && r < CONSOLE_ROWS; r++)
        for (c = x0; c <= x1 && c < CONSOLE_COLS; c++) {
            screen[r][c].ch = ' ';
            screen[r][c].fg = COLOR_DEFAULT;
            screen[r][c].bg = COLOR_DEFAULT;
        }
}

void console_sink(const struct term_event_t *ev, void *prv)
{
    (void)prv;

    switch (ev->type) {

    /* ── printable + C0 ─────────────────────────────────────────── */
    case TERM_CHAR: {
        unsigned char ch = ev->u.ch.ch;

        switch (ch) {
        case '\r':
            con_cx = 0;
            break;
        case '\n':
            con_newline();
            break;
        case '\t':
            con_cx = (con_cx + 8) & ~7;
            if (con_cx >= CONSOLE_COLS) con_newline();
            break;
        case 0x08:  /* BS */
        case 0x7F:  /* DEL */
            if (con_cx > 0) {
                con_cx--;
                screen[con_cy][con_cx].ch = ' ';
                screen[con_cy][con_cx].fg = COLOR_DEFAULT;
                screen[con_cy][con_cx].bg = COLOR_DEFAULT;
            }
            break;
        default:
            if (ch >= 32 && ch < 127)
                con_put_raw(ch);
            break;
        }
        break;
    }

    /* ── CSI sequences ──────────────────────────────────────────── */
    case TERM_CSI: {
        const int *p      = ev->u.csi.params;
        int        np     = ev->u.csi.nparams;
        unsigned char fin = ev->u.csi.final;
        unsigned char mid = ev->u.csi.intermediate;
        int p0 = (np > 0) ? p[0] : 0;
        int p1 = (np > 1) ? p[1] : 0;

        switch (fin) {

        case 'm':   /* SGR — Select Graphic Rendition */
            if (np == 0) {
                con_fg = COLOR_DEFAULT;
                con_bg = COLOR_DEFAULT;
            } else {
                int i;
                for (i = 0; i < np; i++) {
                    int v = p[i];
                    if      (v == 0)              { con_fg = COLOR_DEFAULT;
                                                    con_bg = COLOR_DEFAULT; }
                    else if (v >= 30 && v <= 37)  con_fg = (unsigned char)(v - 30);
                    else if (v >= 40 && v <= 47)  con_bg = (unsigned char)(v - 40);
                    else if (v >= 90 && v <= 97)  con_fg = (unsigned char)(v - 90);
                    else if (v >= 100 && v <= 107) con_bg = (unsigned char)(v - 100);
                    /* bold/italic/underline: ignored */
                }
            }
            break;

        case 'H':   /* CUP — Cursor Position  ESC [ row ; col H */
        case 'f': {
            int row = (p0 > 0 ? p0 : 1) - 1;
            int col = (p1 > 0 ? p1 : 1) - 1;
            con_cy = (row < CONSOLE_ROWS) ? row : CONSOLE_ROWS - 1;
            con_cx = (col < CONSOLE_COLS) ? col : CONSOLE_COLS - 1;
            break;
        }

        case 'A':   /* CUU — Cursor Up */
            con_cy -= (p0 > 0 ? p0 : 1);
            if (con_cy < 0) con_cy = 0;
            break;

        case 'B':   /* CUD — Cursor Down */
            con_cy += (p0 > 0 ? p0 : 1);
            if (con_cy >= CONSOLE_ROWS) con_cy = CONSOLE_ROWS - 1;
            break;

        case 'C':   /* CUF — Cursor Forward */
            con_cx += (p0 > 0 ? p0 : 1);
            if (con_cx >= CONSOLE_COLS) con_cx = CONSOLE_COLS - 1;
            break;

        case 'D':   /* CUB — Cursor Backward */
            con_cx -= (p0 > 0 ? p0 : 1);
            if (con_cx < 0) con_cx = 0;
            break;

        case 'J':   /* ED — Erase in Display */
            switch (p0) {
            case 0: /* cursor to end */
                con_clear_rect(con_cx, con_cy, CONSOLE_COLS-1, con_cy);
                con_clear_rect(0, con_cy+1, CONSOLE_COLS-1, CONSOLE_ROWS-1);
                break;
            case 1: /* start to cursor */
                con_clear_rect(0, 0, CONSOLE_COLS-1, con_cy-1);
                con_clear_rect(0, con_cy, con_cx, con_cy);
                break;
            case 2: /* entire screen */
                con_clear_rect(0, 0, CONSOLE_COLS-1, CONSOLE_ROWS-1);
                con_cy = 0;
                con_cx = 0;
                break;
            }
            break;

        case 'K':   /* EL — Erase in Line */
            switch (p0) {
            case 0: con_clear_rect(con_cx, con_cy, CONSOLE_COLS-1, con_cy); break;
            case 1: con_clear_rect(0,      con_cy, con_cx,          con_cy); break;
            case 2: con_clear_rect(0,      con_cy, CONSOLE_COLS-1,  con_cy); break;
            }
            break;

        case 'h':   /* SM / private set */
        case 'l':   /* RM / private reset */
            /*
             * ESC [ ? 1 l  → application cursor keys off
             * ESC [ 12 h   → local echo on
             * We do not need to act on these — they are hints for
             * the remote terminal (PuTTY already does local echo).
             * Silently ignore.
             */
            (void)mid;
            break;

        default:
            /* unrecognised final byte — silently ignore */
            break;
        }
        break;
    }

    /* ── bare ESC sequences ─────────────────────────────────────── */
    case TERM_ESCAPE:
        switch (ev->u.esc.ch) {
        case 'c':   /* RIS — Reset to Initial State */
            con_fg = COLOR_DEFAULT;
            con_bg = COLOR_DEFAULT;
            con_cx = 0;
            con_cy = 0;
            con_clear_rect(0, 0, CONSOLE_COLS-1, CONSOLE_ROWS-1);
            break;
        case '7':   /* DECSC — save cursor (stub) */
        case '8':   /* DECRC — restore cursor (stub) */
            break;
        default:
            break;
        }
        break;

    case TERM_OSC:
        /* ignored */
        break;
    }
}

/* ================================================================
 * uart_echo_sink
 *
 * Re-emits bytes back to the serial line so the remote terminal
 * (PuTTY / minicom) stays in sync.
 *
 * Rules:
 *   TERM_CHAR printable  → echo the char
 *   TERM_CHAR \r         → echo \r\n  (canonical newline)
 *   TERM_CHAR \b / 0x7F  → echo "\b \b"  (destructive backspace)
 *   TERM_CHAR \t         → echo \t
 *   TERM_CSI  'm'        → re-emit the SGR escape verbatim
 *   everything else      → silence
 *
 * prv = NULL.
 * ================================================================ */

#include "uart.h"

/* Re-build and send an SGR sequence for the echo sink */
static void uart_emit_sgr(const struct term_event_t *ev)
{
    char buf[32];
    int  pos = 0;
    int  i;

    buf[pos++] = 0x1B;
    buf[pos++] = '[';

    for (i = 0; i < ev->u.csi.nparams; i++) {
        /* tiny itoa — no libc */
        int v = ev->u.csi.params[i];
        if (i > 0) buf[pos++] = ';';
        if (v == 0) {
            buf[pos++] = '0';
        } else {
            char tmp[8];
            int  tlen = 0;
            while (v > 0) { tmp[tlen++] = '0' + (v % 10); v /= 10; }
            /* reverse */
            int j;
            for (j = tlen - 1; j >= 0; j--)
                buf[pos++] = tmp[j];
        }
    }

    buf[pos++] = 'm';
    buf[pos]   = '\0';

    uart_puts(buf);
}

void uart_echo_sink(const struct term_event_t *ev, void *prv)
{
    (void)prv;

    switch (ev->type) {

    case TERM_CHAR: {
        unsigned char ch = ev->u.ch.ch;
        switch (ch) {
        case '\r':
        case '\n':
            uart_puts("\r\n");
            break;
        case 0x08:
        case 0x7F:
            uart_puts("\b \b");
            break;
        case 0x15:   /* ctrl+U — line kill: caller must redraw prompt */
            break;
        default:
            if (ch >= 32 && ch < 127)
                uart_putc((char)ch);
            break;
        }
        break;
    }

    case TERM_CSI:
        /*
         * Only re-emit SGR (colour) sequences.
         * Cursor-movement sequences generated by the application
         * (e.g. the prompt) need to reach the remote terminal,
         * but we do NOT echo cursor sequences typed by the user
         * (users do not type ESC sequences manually in normal use).
         *
         * Distinguish: sequences that were generated by the application
         * output path arrive here via terminal_feed() called from
         * printf → console_putc → terminal_feed.
         * Sequences typed by the user arrive via the RX path.
         *
         * The cleanest split is two separate terminal_t instances:
         *   - one for application OUTPUT (printf → fb + uart echo)
         *   - one for user INPUT  (uart rx → line buffer)
         * See the integration notes below.
         */
        if (ev->u.csi.final == 'm')
            uart_emit_sgr(ev);
        break;

    case TERM_ESCAPE:
    case TERM_OSC:
        break;
    }
}
