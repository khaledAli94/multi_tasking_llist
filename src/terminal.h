#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* ================================================================
 * Parsed terminal event — one per logical action.
 * The parser emits these; sinks consume them.
 * ================================================================ */

enum term_event_type_t {
    TERM_CHAR,          /* printable character or C0 control     */
    TERM_CSI,           /* ESC [ Pn ; Pn ... final_byte          */
    TERM_OSC,           /* ESC ]  (ignored for now)              */
    TERM_ESCAPE,        /* bare ESC + single char (ESC c etc.)   */
};

#define TERM_MAX_PARAMS  8

struct term_event_t {
    enum term_event_type_t type;

    union {
        /* TERM_CHAR */
        struct {
            unsigned char ch;   /* the byte (includes \r \n \t \b) */
        } ch;

        /* TERM_CSI */
        struct {
            unsigned char final;              /* e.g. 'm', 'H', 'J' */
            unsigned char intermediate;       /* e.g. '?' or 0       */
            int  params[TERM_MAX_PARAMS];
            int  nparams;
        } csi;

        /* TERM_ESCAPE */
        struct {
            unsigned char ch;   /* the char after ESC               */
        } esc;
    } u;
};

/* ================================================================
 * Sink — a function that receives parsed events.
 * Register up to TERM_MAX_SINKS sinks.
 * ================================================================ */

#define TERM_MAX_SINKS  4

typedef void (*term_sink_fn)(const struct term_event_t *ev, void *prv);

/* ================================================================
 * Parser state (one per logical terminal stream)
 * ================================================================ */

enum term_parse_state_t {
    TSTATE_GROUND,
    TSTATE_ESC,
    TSTATE_CSI_ENTRY,
    TSTATE_CSI_PARAM,
    TSTATE_CSI_INTER,
    TSTATE_OSC,
};

struct terminal_t {
    enum term_parse_state_t state;

    /* CSI accumulator */
    int          params[TERM_MAX_PARAMS];
    int          nparams;
    int          cur_param;       /* being built digit by digit  */
    unsigned char intermediate;  /* stores '?' etc.             */

    /* sinks */
    term_sink_fn  sinks[TERM_MAX_SINKS];
    void         *sink_prv[TERM_MAX_SINKS];
    int           nsinks;
};

/* ================================================================
 * API
 * ================================================================ */

/* Initialise a terminal instance (zero it first or use static storage) */
void terminal_init(struct terminal_t *t);

/* Feed one raw byte — may emit 0..N events to all registered sinks */
void terminal_feed(struct terminal_t *t, unsigned char byte);

/* Feed a string of raw bytes */
void terminal_feed_buf(struct terminal_t *t, const unsigned char *buf, size_t n);

/* Register a sink — returns 0 on success, -1 if full */
int terminal_add_sink(struct terminal_t *t, term_sink_fn fn, void *prv);

/* Remove a sink */
void terminal_remove_sink(struct terminal_t *t, term_sink_fn fn);

/* ================================================================
 * Pre-built sinks
 * ================================================================ */

/*
 * console_sink — drives the framebuffer console.
 * prv = NULL (uses the global screen[] directly).
 *
 * Replaces the old ANSI parser inside console_putc().
 * console_putc() becomes a raw cell-writer with no escape logic.
 */
void console_sink(const struct term_event_t *ev, void *prv);

/*
 * uart_echo_sink — echoes the event back to the UART as raw bytes.
 * prv = NULL.
 *
 * Used so the remote terminal (PuTTY) sees what was typed.
 * For printable chars → send the char.
 * For backspace      → send "\b \b".
 * For CSI 'm'        → re-emit the ANSI colour escape verbatim.
 * Everything else is ignored (we do not re-send cursor moves etc.).
 */
void uart_echo_sink(const struct term_event_t *ev, void *prv);

#ifdef __cplusplus
}
#endif

#endif /* __TERMINAL_H__ */
