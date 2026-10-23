/*
 * clex_editor.c  -  A live Lexical Analyser for C, in a GUI code editor (GTK 3)
 *
 * Features
 *   - Multi-file editor (tabs): open, save, save as, close, several files at once
 *   - Real-time lexical analysis while you type (debounced 150 ms)
 *   - Syntax colouring (keywords, numbers, strings, comments, preprocessor, operators)
 *   - Red highlight + squiggly underline on errors:
 *        unclosed / unmatched / mismatched  ( ) [ ] { }
 *        unterminated string, char constant, block comment
 *        malformed numbers (09, 12abc, 10uu), integer literals too large,
 *        bad escape sequences ("\q", '\x'), invalid characters, stray comment terminator,
 *        malformed #include <file> / "file"
 *   - Yellow warnings: nested slash-star in comments, multi-character constants,
 *        decimal literals too big for a signed type, float literals out of range
 *   - "Problems" list (click an entry to jump to the error)
 *   - "Tokens" list  (the actual output of the lexer: line, column, type, lexeme)
 *   - Line-number gutter (lines with errors are shown in red)
 *
 * Build:
 *   gcc -Wall -o clex_editor clex_editor.c $(pkg-config --cflags --libs gtk+-3.0)
 * Run:
 *   ./clex_editor [file1.c file2.c ...]
 */

#include <gtk/gtk.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <float.h>
#include <math.h>

#define MAX_STACK      4096
#define MAX_TOKEN_ROWS 20000
#define ANALYZE_DELAY  150      /* ms after the last key press */

/* ------------------------------------------------------------------ */
/*  Data structures                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    GtkWidget     *root;          /* the page widget stored in the notebook */
    GtkWidget     *view;          /* GtkTextView                            */
    GtkTextBuffer *buf;
    GtkWidget     *tab_label;
    GtkWidget     *probs_label, *toks_label;
    GtkWidget     *probs_view,  *toks_view;
    GtkListStore  *probs, *toks;
    GHashTable    *errlines;      /* line numbers (1-based) that have errors */
    char          *path;          /* NULL => untitled */
    int            untitled_no;
    gboolean       modified;
    guint          timer;
    int            ntokens, nerrors, nwarns;
} Doc;

typedef struct { char ch; int line, col; } Open;

static GtkWidget *g_win, *g_nb, *g_info;
static int g_untitled_counter = 0;

static const char *KEYWORDS[] = {
    "auto","break","case","char","const","continue","default","do","double",
    "else","enum","extern","float","for","goto","if","inline","int","long",
    "register","restrict","return","short","signed","sizeof","static","struct",
    "switch","typedef","union","unsigned","void","volatile","while",
    "_Bool","_Complex","_Imaginary","_Alignas","_Alignof","_Atomic","_Generic",
    "_Noreturn","_Static_assert","_Thread_local", NULL
};

/* ------------------------------------------------------------------ */
/*  Small helpers                                                      */
/* ------------------------------------------------------------------ */

static int isdig(int c) { return c >= '0' && c <= '9'; }
static int isalp(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int ishex(int c) { return isdig(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

static int is_keyword(const char *w) {
    for (int i = 0; KEYWORDS[i]; i++)
        if (strcmp(w, KEYWORDS[i]) == 0) return 1;
    return 0;
}

static Doc *doc_from_page(GtkWidget *page) {
    return page ? g_object_get_data(G_OBJECT(page), "doc") : NULL;
}

static Doc *doc_current(void) {
    int n = gtk_notebook_get_current_page(GTK_NOTEBOOK(g_nb));
    if (n < 0) return NULL;
    return doc_from_page(gtk_notebook_get_nth_page(GTK_NOTEBOOK(g_nb), n));
}

static char *doc_name(Doc *d) {
    if (d->path) return g_path_get_basename(d->path);
    return g_strdup_printf("Untitled-%d.c", d->untitled_no);
}

static void show_error(const char *msg) {
    GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(g_win), GTK_DIALOG_MODAL,
                        GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", msg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

/* ------------------------------------------------------------------ */
/*  Status line / labels                                               */
/* ------------------------------------------------------------------ */

static void update_info(Doc *d) {
    if (!d) { gtk_label_set_text(GTK_LABEL(g_info), ""); return; }
    GtkTextIter it;
    gtk_text_buffer_get_iter_at_mark(d->buf, &it, gtk_text_buffer_get_insert(d->buf));
    char *s = g_strdup_printf("  Ln %d, Col %d     |     Tokens: %d     |     Errors: %d     |     Warnings: %d",
                              gtk_text_iter_get_line(&it) + 1,
                              gtk_text_iter_get_line_offset(&it) + 1,
                              d->ntokens, d->nerrors, d->nwarns);
    gtk_label_set_text(GTK_LABEL(g_info), s);
    g_free(s);
}

static void update_labels(Doc *d) {
    char *name = doc_name(d);
    char *txt  = g_strdup_printf("%s%s", d->modified ? "* " : "", name);
    gtk_label_set_text(GTK_LABEL(d->tab_label), txt);
    gtk_widget_set_tooltip_text(d->tab_label, d->path ? d->path : "Not saved yet");
    if (d == doc_current()) {
        char *title = g_strdup_printf("%s - C Lexical Analyser", txt);
        gtk_window_set_title(GTK_WINDOW(g_win), title);
        g_free(title);
    }
    g_free(txt); g_free(name);
}

/* ------------------------------------------------------------------ */
/*  Lexer output helpers                                               */
/* ------------------------------------------------------------------ */

static void mark(Doc *d, const char *tag, int l1, int c1, int l2, int c2) {
    GtkTextIter a, b;
    gtk_text_buffer_get_iter_at_line_index(d->buf, &a, l1, c1);
    gtk_text_buffer_get_iter_at_line_index(d->buf, &b, l2, c2);
    gtk_text_buffer_apply_tag_by_name(d->buf, tag, &a, &b);
}

static void issue_v(Doc *d, gboolean warn, int l1, int c1, int l2, int c2,
                    const char *fmt, va_list ap) {
    char *m = g_strdup_vprintf(fmt, ap);
    char *full = warn ? g_strdup_printf("Warning: %s", m) : g_strdup(m);
    gtk_list_store_insert_with_values(d->probs, NULL, -1, 0, l1 + 1, 1, c1 + 1, 2, full, -1);
    g_free(m); g_free(full);
    mark(d, warn ? "warning" : "error", l1, c1, l2, c2);
    if (warn) d->nwarns++;
    else { g_hash_table_add(d->errlines, GINT_TO_POINTER(l1 + 1)); d->nerrors++; }
}

static void add_error(Doc *d, int l1, int c1, int l2, int c2, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    issue_v(d, FALSE, l1, c1, l2, c2, fmt, ap);
    va_end(ap);
}

static void add_warning(Doc *d, int l1, int c1, int l2, int c2, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    issue_v(d, TRUE, l1, c1, l2, c2, fmt, ap);
    va_end(ap);
}

static void add_token(Doc *d, const char *type, int line, int col, const char *p, size_t len) {
    d->ntokens++;
    if (d->ntokens > MAX_TOKEN_ROWS) return;
    if (len > 60) len = 60;
    char *raw = g_strndup(p, len);
    char *lex = g_utf8_make_valid(raw, -1);
    g_strdelimit(lex, "\n\r\t", ' ');
    gtk_list_store_insert_with_values(d->toks, NULL, -1,
                                      0, line + 1, 1, col + 1, 2, type, 3, lex, -1);
    g_free(raw); g_free(lex);
}

/* Checks a numeric literal. Returns a message (or NULL if fine).
 * *warn is set to 1 when the message is only a warning. */
static const char *check_number(const char *p, size_t len, int *warn) {
    char buf[128];
    *warn = 0;
    if (len >= sizeof buf) return NULL;
    memcpy(buf, p, len); buf[len] = 0;
    const char *q = buf, *digits = buf;
    int isfloat = 0, base = 10;

    if (q[0] == '0' && (q[1] == 'x' || q[1] == 'X')) {
        base = 16; q += 2; digits = q;
        if (!ishex(*q)) return "Invalid hexadecimal constant";
        while (ishex(*q)) q++;
        if (*q == '.') { isfloat = 1; q++; while (ishex(*q)) q++; }
        if (*q == 'p' || *q == 'P') {
            isfloat = 1; q++;
            if (*q == '+' || *q == '-') q++;
            if (!isdig(*q)) return "Malformed exponent in numeric constant";
            while (isdig(*q)) q++;
        } else if (isfloat) return "Hexadecimal floating constant needs an exponent (p)";
    } else if (q[0] == '0' && (q[1] == 'b' || q[1] == 'B')) {
        base = 2; q += 2; digits = q;
        if (*q != '0' && *q != '1') return "Invalid binary constant";
        while (*q == '0' || *q == '1') q++;
        if (isdig(*q)) return "Invalid digit in binary constant";
    } else {
        const char *st = q;
        while (isdig(*q)) q++;
        if (*q == '.') { isfloat = 1; q++; while (isdig(*q)) q++; }
        if (*q == 'e' || *q == 'E') {
            isfloat = 1; q++;
            if (*q == '+' || *q == '-') q++;
            if (!isdig(*q)) return "Malformed exponent in numeric constant";
            while (isdig(*q)) q++;
        }
        if (!isfloat && st[0] == '0' && q - st > 1) {
            base = 8;
            for (const char *t = st; t < q; t++)
                if (*t > '7') return "Invalid digit in octal constant";
        }
    }

    if (isfloat) {                                   /* suffix: f F l L (only one) */
        if (*q && !((*q == 'f' || *q == 'F' || *q == 'l' || *q == 'L') && q[1] == 0))
            return "Invalid suffix on floating constant";
        double v = strtod(buf, NULL);
        if (isinf(v) || ((*q == 'f' || *q == 'F') && v > FLT_MAX)) {
            *warn = 1;
            return "Floating constant is out of range for its type";
        }
        return NULL;
    }

    /* integer suffix: u, l, ll, ul, ull, lu, llu (any case, but ll must match) */
    int nu = 0, nl = 0, prevl = 0;
    char lc = 0;
    for (const char *t = q; *t; t++) {
        if (*t == 'u' || *t == 'U') { nu++; prevl = 0; }
        else if (*t == 'l' || *t == 'L') {
            if (nl && (!prevl || *t != lc)) return "Invalid suffix on integer constant";
            lc = *t; nl++; prevl = 1;
        } else return "Invalid suffix on integer constant";
    }
    if (nu > 1 || nl > 2) return "Invalid suffix on integer constant";

    /* range check */
    char dig[128];
    size_t dl = (size_t)(q - digits);
    memcpy(dig, digits, dl); dig[dl] = 0;
    errno = 0;
    unsigned long long v = strtoull(dig, NULL, base);
    if (errno == ERANGE)
        return "Integer constant is too large for any integer type (max 18446744073709551615)";
    if (base == 10 && nu == 0 && v > (unsigned long long)LLONG_MAX) {
        *warn = 1;
        return "Decimal constant is too large for a signed type, it will be treated as unsigned";
    }
    return NULL;
}

/* Validates one escape sequence. s[*pj] is the backslash. Advances *pj past it. */
static void scan_escape(Doc *d, const char *s, size_t n, size_t *pj, int line, size_t ls, int wide) {
    size_t j = *pj, k = j + 2;
    int col = (int)(j - ls);
    unsigned char c = (unsigned char)s[j + 1];

    if (c && strchr("'\"?\\abfnrtv", c)) {
        /* simple escape: fine */
    } else if (c == '\r' && k < n && s[k] == '\n') {
        /* CRLF line continuation: ignore */
    } else if (c >= '0' && c <= '7') {
        int cnt = 1;
        while (k < n && cnt < 3 && s[k] >= '0' && s[k] <= '7') { k++; cnt++; }
    } else if (c == 'x') {
        unsigned long v = 0; size_t st = k;
        while (k < n && ishex((unsigned char)s[k])) {
            int x = (unsigned char)s[k];
            if (v < 0x1000000UL) v = v * 16 + (isdig(x) ? x - '0' : (x | 32) - 'a' + 10);
            k++;
        }
        if (k == st)
            add_error(d, line, col, line, (int)(k - ls), "\\x used with no following hex digits");
        else if (!wide && v > 0xFF)
            add_error(d, line, col, line, (int)(k - ls), "Hex escape sequence out of range (max \\xFF)");
    } else if (c == 'u' || c == 'U') {
        int need = (c == 'u') ? 4 : 8, got = 0;
        while (k < n && got < need && ishex((unsigned char)s[k])) { k++; got++; }
        if (got < need)
            add_error(d, line, col, line, (int)(k - ls),
                      "Incomplete universal character name \\%c (needs %d hex digits)", c, need);
    } else {
        while (k < n && (((unsigned char)s[k]) & 0xC0) == 0x80) k++;   /* whole UTF-8 char */
        add_warning(d, line, col, line, (int)(k - ls), "Unknown escape sequence '\\%.*s'",
                    (int)(k - j - 1), s + j + 1);
    }
    *pj = k;
}

static int op_len(const char *p) {
    static const char *o3[] = { "<<=", ">>=", "...", NULL };
    static const char *o2[] = { "++","--","->","<<",">>","<=",">=","==","!=","&&","||",
                                "+=","-=","*=","/=","%=","&=","|=","^=","##", NULL };
    for (int k = 0; o3[k]; k++) if (strncmp(p, o3[k], 3) == 0) return 3;
    for (int k = 0; o2[k]; k++) if (strncmp(p, o2[k], 2) == 0) return 2;
    if (*p && strchr("+-*/%=<>!&|^~?:.", *p)) return 1;
    return 0;
}

static char closer_of(char o) { return o == '(' ? ')' : o == '[' ? ']' : '}'; }

/* ------------------------------------------------------------------ */
/*  THE LEXICAL ANALYSER                                               */
/* ------------------------------------------------------------------ */

static gboolean analyze(gpointer data) {
    Doc *d = data;
    d->timer = 0;

    GtkTextIter s0, e0;
    gtk_text_buffer_get_bounds(d->buf, &s0, &e0);
    gtk_text_buffer_remove_all_tags(d->buf, &s0, &e0);
    char *text = gtk_text_buffer_get_text(d->buf, &s0, &e0, FALSE);

    gtk_tree_view_set_model(GTK_TREE_VIEW(d->toks_view), NULL);
    gtk_list_store_clear(d->toks);
    gtk_list_store_clear(d->probs);
    g_hash_table_remove_all(d->errlines);
    d->ntokens = d->nerrors = d->nwarns = 0;

    const char *s = text;
    size_t n = strlen(s), i = 0, ls = 0;   /* ls = byte offset where current line starts */
    int line = 0;
    Open st[MAX_STACK];
    int sp = 0;

    while (i < n) {
        unsigned char c = (unsigned char)s[i];

        if (c == '\n') { line++; i++; ls = i; continue; }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') { i++; continue; }

        int col = (int)(i - ls);

        /* ---------- // line comment ---------- */
        if (c == '/' && s[i + 1] == '/') {
            size_t j = i;
            while (j < n && s[j] != '\n') j++;
            add_token(d, "Comment", line, col, s + i, j - i);
            mark(d, "comment", line, col, line, (int)(j - ls));
            i = j; continue;
        }

        /* ---------- block comment ---------- */
        if (c == '/' && s[i + 1] == '*') {
            size_t j = i + 2, els = ls;
            int el = line, closed = 0;
            while (j < n) {
                if (s[j] == '*' && s[j + 1] == '/') { j += 2; closed = 1; break; }
                if (s[j] == '/' && s[j + 1] == '*')
                    add_warning(d, el, (int)(j - els), el, (int)(j - els) + 2,
                                "\"/*\" found inside a comment (comments do not nest)");
                if (s[j] == '\n') { el++; els = j + 1; }
                j++;
            }
            add_token(d, "Comment", line, col, s + i, j - i);
            mark(d, "comment", line, col, el, (int)(j - els));
            if (!closed)
                add_error(d, line, col, line, col + 2, "Unterminated comment - missing closing */");
            line = el; ls = els; i = j; continue;
        }

        /* ---------- preprocessor directive ---------- */
        if (c == '#') {
            size_t k = ls;
            while (k < i && (s[k] == ' ' || s[k] == '\t')) k++;
            if (k == i) {                       /* '#' is first thing on the line */
                size_t j = i, els = ls;
                int el = line;
                for (;;) {
                    while (j < n && s[j] != '\n' && !(s[j] == '/' && (s[j + 1] == '/' || s[j + 1] == '*'))) j++;
                    if (j < n && s[j] == '\n' && j > i &&
                        (s[j - 1] == '\\' || (s[j - 1] == '\r' && j > i + 1 && s[j - 2] == '\\'))) {
                        el++; j++; els = j; continue;
                    }
                    break;
                }
                if (el == line) {                       /* check #include "file" / <file> */
                    size_t p = i + 1;
                    while (p < j && (s[p] == ' ' || s[p] == '\t')) p++;
                    size_t q = p;
                    while (q < j && isalp((unsigned char)s[q])) q++;
                    if (q - p == 7 && strncmp(s + p, "include", 7) == 0) {
                        while (q < j && (s[q] == ' ' || s[q] == '\t')) q++;
                        size_t e = j;
                        while (e > q && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) e--;
                        int c1 = (int)(p + 7 - ls);
                        if (q >= e || (s[q] != '<' && s[q] != '"' && !isalp((unsigned char)s[q]))) {
                            add_error(d, line, col, line, c1, "#include expects \"FILE\" or <FILE>");
                        } else if (s[q] == '<' || s[q] == '"') {
                            char endc = (s[q] == '<') ? '>' : '"';
                            size_t r = q + 1;
                            while (r < e && s[r] != endc) r++;
                            if (r >= e)
                                add_error(d, line, (int)(q - ls), line, (int)(e - ls),
                                          "Missing terminating %c in #include", endc);
                            else if (r == q + 1)
                                add_error(d, line, (int)(q - ls), line, (int)(r + 1 - ls),
                                          "Empty file name in #include");
                        }
                    }
                }
                add_token(d, "Preprocessor", line, col, s + i, j - i);
                mark(d, "preproc", line, col, el, (int)(j - els));
                line = el; ls = els; i = j; continue;
            }
        }

        /* ---------- string / char literal ---------- */
        if (c == '"' || c == '\'') {
            char q = (char)c;
            int wide = (i > 0 && (s[i - 1] == 'L' || s[i - 1] == 'u' || s[i - 1] == 'U' || s[i - 1] == '8'));
            size_t j = i + 1;
            int cnt = 0;
            while (j < n && s[j] != q && s[j] != '\n') {
                if (s[j] == '\\' && j + 1 < n && s[j + 1] != '\n')
                    scan_escape(d, s, n, &j, line, ls, wide);
                else j++;
                cnt++;
            }
            int closed = (j < n && s[j] == q);
            if (closed) j++;
            add_token(d, q == '"' ? "String" : "Char", line, col, s + i, j - i);
            mark(d, "string", line, col, line, (int)(j - ls));
            if (!closed)
                add_error(d, line, col, line, (int)(j - ls),
                          q == '"' ? "Unterminated string literal - missing closing \""
                                   : "Unterminated character constant - missing closing '");
            else if (q == '\'' && cnt == 0)
                add_error(d, line, col, line, (int)(j - ls), "Empty character constant");
            else if (q == '\'' && cnt > 1)
                add_warning(d, line, col, line, (int)(j - ls),
                            "Multi-character constant (value is implementation-defined)");
            i = j; continue;
        }

        /* ---------- number ---------- */
        if (isdig(c) || (c == '.' && isdig((unsigned char)s[i + 1]))) {
            int hex = (c == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X'));
            size_t j = i + 1;
            while (j < n) {
                unsigned char x = (unsigned char)s[j];
                if (isalp(x) || isdig(x) || x == '.') j++;
                else if ((x == '+' || x == '-') && !hex && (s[j - 1] == 'e' || s[j - 1] == 'E')) j++;
                else if ((x == '+' || x == '-') && hex && (s[j - 1] == 'p' || s[j - 1] == 'P')) j++;
                else break;
            }
            add_token(d, "Number", line, col, s + i, j - i);
            int wflag = 0;
            const char *err = check_number(s + i, j - i, &wflag);
            if (!err || wflag) mark(d, "number", line, col, line, (int)(j - ls));
            if (err) {
                char *w = g_strndup(s + i, j - i);
                if (wflag) add_warning(d, line, col, line, (int)(j - ls), "%s: %s", err, w);
                else       add_error(d, line, col, line, (int)(j - ls), "%s: %s", err, w);
                g_free(w);
            }
            i = j; continue;
        }

        /* ---------- identifier / keyword ---------- */
        if (isalp(c)) {
            size_t j = i + 1;
            while (j < n && (isalp((unsigned char)s[j]) || isdig((unsigned char)s[j]))) j++;
            char *w = g_strndup(s + i, j - i);
            int kw = is_keyword(w);
            g_free(w);
            add_token(d, kw ? "Keyword" : "Identifier", line, col, s + i, j - i);
            if (kw) mark(d, "keyword", line, col, line, (int)(j - ls));
            i = j; continue;
        }

        /* ---------- opening brackets ---------- */
        if (c == '(' || c == '[' || c == '{') {
            if (sp < MAX_STACK) { st[sp].ch = (char)c; st[sp].line = line; st[sp].col = col; sp++; }
            add_token(d, "Bracket", line, col, s + i, 1);
            i++; continue;
        }

        /* ---------- closing brackets ---------- */
        if (c == ')' || c == ']' || c == '}') {
            char want = (c == ')') ? '(' : (c == ']') ? '[' : '{';
            add_token(d, "Bracket", line, col, s + i, 1);
            int k = sp - 1;
            while (k >= 0 && st[k].ch != want) k--;
            if (k < 0) {
                add_error(d, line, col, line, col + 1,
                          "Unmatched '%c' - there is no opening '%c' for it", c, want);
            } else {
                for (int m = sp - 1; m > k; m--)       /* things left open inside */
                    add_error(d, st[m].line, st[m].col, st[m].line, st[m].col + 1,
                              "Unclosed '%c' - missing '%c' (a '%c' was found at line %d first)",
                              st[m].ch, closer_of(st[m].ch), c, line + 1);
                sp = k;                                  /* pop the matching one too */
            }
            i++; continue;
        }

        /* ---------- punctuation ---------- */
        if (c == ';' || c == ',') {
            add_token(d, "Punctuation", line, col, s + i, 1);
            i++; continue;
        }

        /* ---------- stray comment terminator ---------- */
        if (c == '*' && s[i + 1] == '/' && s[i + 2] != '/') {
            add_token(d, "INVALID", line, col, s + i, 2);
            add_error(d, line, col, line, col + 2, "Stray '*/' - there is no matching /*");
            i += 2; continue;
        }

        /* ---------- operators ---------- */
        int ol = op_len(s + i);
        if (ol > 0) {
            add_token(d, "Operator", line, col, s + i, ol);
            mark(d, "operator", line, col, line, col + ol);
            i += ol; continue;
        }

        /* ---------- anything else is an invalid character ---------- */
        {
            size_t j = i + 1;
            while (j < n && (((unsigned char)s[j]) & 0xC0) == 0x80) j++;   /* whole UTF-8 char */
            add_token(d, "INVALID", line, col, s + i, j - i);
            if (c == '#')
                add_error(d, line, col, line, col + 1,
                          "Stray '#' - preprocessor directives must start a line");
            else if (c < 32 || c == 127)
                add_error(d, line, col, line, col + 1, "Invalid control character (0x%02X)", c);
            else
                add_error(d, line, col, line, (int)(j - ls),
                          "Invalid character '%.*s' - not part of any C token", (int)(j - i), s + i);
            i = j; continue;
        }
    }

    /* whatever is still on the stack was never closed */
    for (int m = 0; m < sp; m++)
        add_error(d, st[m].line, st[m].col, st[m].line, st[m].col + 1,
                  "Unclosed '%c' - missing '%c'", st[m].ch, closer_of(st[m].ch));

    gtk_tree_view_set_model(GTK_TREE_VIEW(d->toks_view), GTK_TREE_MODEL(d->toks));

    char *t = g_strdup_printf("Problems (%d)", d->nerrors + d->nwarns);
    gtk_label_set_text(GTK_LABEL(d->probs_label), t); g_free(t);
    t = g_strdup_printf("Tokens (%d)", d->ntokens);
    gtk_label_set_text(GTK_LABEL(d->toks_label), t); g_free(t);

    gtk_widget_queue_draw(d->view);
    if (d == doc_current()) update_info(d);
    g_free(text);
    return G_SOURCE_REMOVE;
}

/* ------------------------------------------------------------------ */
/*  Editor widget callbacks                                            */
/* ------------------------------------------------------------------ */

static void on_changed(GtkTextBuffer *b, gpointer data) {
    (void)b;
    Doc *d = data;
    if (!d->modified) { d->modified = TRUE; update_labels(d); }
    if (d->timer) g_source_remove(d->timer);
    d->timer = g_timeout_add(ANALYZE_DELAY, analyze, d);
    gtk_widget_queue_draw(d->view);
}

static void on_cursor_moved(GObject *o, GParamSpec *p, gpointer data) {
    (void)o; (void)p;
    Doc *d = data;
    if (d == doc_current()) update_info(d);
}

/* line-number gutter */
static gboolean on_view_draw(GtkWidget *w, cairo_t *cr, gpointer data) {
    Doc *d = data;
    GtkTextView *tv = GTK_TEXT_VIEW(w);
    GdkWindow *win = gtk_text_view_get_window(tv, GTK_TEXT_WINDOW_LEFT);
    if (!win || !gtk_cairo_should_draw_window(cr, win)) return FALSE;

    cairo_save(cr);
    gtk_cairo_transform_to_window(cr, w, win);
    int W = gdk_window_get_width(win), H = gdk_window_get_height(win);
    cairo_set_source_rgb(cr, 0.93, 0.93, 0.93);
    cairo_rectangle(cr, 0, 0, W, H);
    cairo_fill(cr);

    GdkRectangle vis;
    gtk_text_view_get_visible_rect(tv, &vis);
    GtkTextIter it;
    gtk_text_view_get_line_at_y(tv, &it, vis.y, NULL);

    PangoLayout *lay = gtk_widget_create_pango_layout(w, NULL);
    pango_layout_set_width(lay, (W - 10) * PANGO_SCALE);
    pango_layout_set_alignment(lay, PANGO_ALIGN_RIGHT);

    for (;;) {
        int y, h, wx, wy;
        gtk_text_view_get_line_yrange(tv, &it, &y, &h);
        gtk_text_view_buffer_to_window_coords(tv, GTK_TEXT_WINDOW_LEFT, 0, y, &wx, &wy);
        if (wy > H) break;
        int ln = gtk_text_iter_get_line(&it) + 1;
        char b[16];
        g_snprintf(b, sizeof b, "%d", ln);
        pango_layout_set_text(lay, b, -1);
        if (g_hash_table_contains(d->errlines, GINT_TO_POINTER(ln)))
            cairo_set_source_rgb(cr, 0.85, 0.0, 0.0);
        else
            cairo_set_source_rgb(cr, 0.45, 0.45, 0.45);
        cairo_move_to(cr, 0, wy);
        pango_cairo_show_layout(cr, lay);
        if (!gtk_text_iter_forward_line(&it)) break;
    }
    g_object_unref(lay);
    cairo_restore(cr);
    return FALSE;
}

/* double-click a row in Problems / Tokens => jump to that place */
static void on_row_activated(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *c, gpointer data) {
    (void)c;
    Doc *d = data;
    GtkTreeModel *m = gtk_tree_view_get_model(tv);
    GtkTreeIter it;
    if (!m || !gtk_tree_model_get_iter(m, &it, path)) return;
    int line, col;
    gtk_tree_model_get(m, &it, 0, &line, 1, &col, -1);
    GtkTextIter ti;
    gtk_text_buffer_get_iter_at_line_index(d->buf, &ti, line - 1, col - 1);
    gtk_text_buffer_place_cursor(d->buf, &ti);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(d->view), &ti, 0.1, TRUE, 0.5, 0.5);
    gtk_widget_grab_focus(d->view);
}

static void doc_free(gpointer p) {
    Doc *d = p;
    if (d->timer) g_source_remove(d->timer);
    g_hash_table_destroy(d->errlines);
    g_object_unref(d->probs);
    g_object_unref(d->toks);
    g_free(d->path);
    g_free(d);
}

static void close_doc(Doc *d);

static void on_tab_close_clicked(GtkButton *b, gpointer root) {
    (void)b;
    close_doc(doc_from_page(GTK_WIDGET(root)));
}

static GtkWidget *make_tree(GtkListStore *store, const char **titles, int n) {
    GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    for (int i = 0; i < n; i++) {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(titles[i], r, "text", i, NULL);
        gtk_tree_view_column_set_resizable(c, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tv), c);
    }
    return tv;
}

static GtkWidget *scrolled(GtkWidget *child) {
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sw), child);
    return sw;
}

/* ------------------------------------------------------------------ */
/*  Create a new editor tab                                            */
/* ------------------------------------------------------------------ */

static Doc *doc_new(const char *path, const char *text) {
    Doc *d = g_new0(Doc, 1);
    d->path = path ? g_strdup(path) : NULL;
    if (!path) d->untitled_no = ++g_untitled_counter;
    d->errlines = g_hash_table_new(g_direct_hash, g_direct_equal);

    /* buffer + tags (the 'error' tag is created last => highest priority) */
    d->buf = gtk_text_buffer_new(NULL);
    gtk_text_buffer_create_tag(d->buf, "keyword",  "foreground", "#0033CC", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(d->buf, "number",   "foreground", "#B35900", NULL);
    gtk_text_buffer_create_tag(d->buf, "string",   "foreground", "#067D17", NULL);
    gtk_text_buffer_create_tag(d->buf, "comment",  "foreground", "#808080", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_text_buffer_create_tag(d->buf, "preproc",  "foreground", "#8A2BE2", NULL);
    gtk_text_buffer_create_tag(d->buf, "operator", "foreground", "#555555", NULL);
    gtk_text_buffer_create_tag(d->buf, "warning",
                               "background", "#FFE08A", "underline", PANGO_UNDERLINE_SINGLE, NULL);
    gtk_text_buffer_create_tag(d->buf, "error",
                               "background", "#FF9E9E", "foreground", "#6B0000",
                               "underline", PANGO_UNDERLINE_ERROR, NULL);

    d->view = gtk_text_view_new_with_buffer(d->buf);
    g_object_unref(d->buf);                         /* the view now owns it */
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(d->view), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(d->view), 8);
    gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(d->view), GTK_TEXT_WINDOW_LEFT, 56);
    g_signal_connect_after(d->view, "draw", G_CALLBACK(on_view_draw), d);

    /* bottom panel: Problems + Tokens */
    d->probs = gtk_list_store_new(3, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(d->probs), 0, GTK_SORT_ASCENDING);
    d->toks  = gtk_list_store_new(4, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
    const char *pt[] = { "Line", "Col", "Message" };
    const char *tt[] = { "Line", "Col", "Token type", "Lexeme" };
    d->probs_view = make_tree(d->probs, pt, 3);
    d->toks_view  = make_tree(d->toks,  tt, 4);
    g_signal_connect(d->probs_view, "row-activated", G_CALLBACK(on_row_activated), d);
    g_signal_connect(d->toks_view,  "row-activated", G_CALLBACK(on_row_activated), d);

    GtkWidget *bottom = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(bottom), GTK_POS_TOP);
    d->probs_label = gtk_label_new("Problems (0)");
    d->toks_label  = gtk_label_new("Tokens (0)");
    gtk_notebook_append_page(GTK_NOTEBOOK(bottom), scrolled(d->probs_view), d->probs_label);
    gtk_notebook_append_page(GTK_NOTEBOOK(bottom), scrolled(d->toks_view),  d->toks_label);

    d->root = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_pack1(GTK_PANED(d->root), scrolled(d->view), TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(d->root), bottom, FALSE, FALSE);
    gtk_widget_set_size_request(bottom, -1, 170);
    g_object_set_data_full(G_OBJECT(d->root), "doc", d, doc_free);

    /* tab header: label + close button */
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    d->tab_label = gtk_label_new("");
    GtkWidget *x = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(x), GTK_RELIEF_NONE);
    g_signal_connect(x, "clicked", G_CALLBACK(on_tab_close_clicked), d->root);
    gtk_box_pack_start(GTK_BOX(hb), d->tab_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hb), x, FALSE, FALSE, 0);
    gtk_widget_show_all(hb);

    /* fill text BEFORE connecting 'changed' so a freshly opened file is not "modified" */
    gtk_text_buffer_set_text(d->buf, text ? text : "", -1);
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(d->buf, &start);
    gtk_text_buffer_place_cursor(d->buf, &start);
    g_signal_connect(d->buf, "changed", G_CALLBACK(on_changed), d);
    g_signal_connect(d->buf, "notify::cursor-position", G_CALLBACK(on_cursor_moved), d);

    int idx = gtk_notebook_append_page(GTK_NOTEBOOK(g_nb), d->root, hb);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(g_nb), d->root, TRUE);
    gtk_widget_show_all(d->root);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(g_nb), idx);

    update_labels(d);
    analyze(d);
    update_info(d);
    gtk_widget_grab_focus(d->view);
    return d;
}

/* ------------------------------------------------------------------ */
/*  File operations                                                    */
/* ------------------------------------------------------------------ */

static gboolean doc_save(Doc *d, gboolean save_as) {
    char *path = g_strdup(d->path);

    if (save_as || !path) {
        GtkWidget *dlg = gtk_file_chooser_dialog_new("Save File", GTK_WINDOW(g_win),
                GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL,
                "_Save", GTK_RESPONSE_ACCEPT, NULL);
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);
        if (path) gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), path);
        else      gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), "untitled.c");
        int r = gtk_dialog_run(GTK_DIALOG(dlg));
        g_free(path); path = NULL;
        if (r == GTK_RESPONSE_ACCEPT)
            path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        gtk_widget_destroy(dlg);
        if (!path) return FALSE;
    }

    GtkTextIter a, b;
    gtk_text_buffer_get_bounds(d->buf, &a, &b);
    char *text = gtk_text_buffer_get_text(d->buf, &a, &b, FALSE);
    GError *err = NULL;
    gboolean ok = g_file_set_contents(path, text, -1, &err);
    g_free(text);
    if (!ok) {
        show_error(err->message);
        g_error_free(err);
        g_free(path);
        return FALSE;
    }
    g_free(d->path);
    d->path = path;
    d->modified = FALSE;
    update_labels(d);
    return TRUE;
}

/* TRUE = ok to throw this document away */
static gboolean confirm_discard(Doc *d) {
    if (!d->modified) return TRUE;
    char *name = doc_name(d);
    GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(g_win), GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE, "Save changes to \"%s\" before closing?", name);
    g_free(name);
    gtk_dialog_add_buttons(GTK_DIALOG(dlg), "Close _without saving", GTK_RESPONSE_REJECT,
                           "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    int r = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    if (r == GTK_RESPONSE_ACCEPT) return doc_save(d, FALSE);
    return r == GTK_RESPONSE_REJECT;
}

static void close_doc(Doc *d) {
    if (!d || !confirm_discard(d)) return;
    int pg = gtk_notebook_page_num(GTK_NOTEBOOK(g_nb), d->root);
    gtk_notebook_remove_page(GTK_NOTEBOOK(g_nb), pg);          /* frees d */
    if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(g_nb)) == 0) doc_new(NULL, "");
}

static void open_path(const char *path) {
    int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(g_nb));
    for (int i = 0; i < n; i++) {                              /* already open? */
        Doc *d = doc_from_page(gtk_notebook_get_nth_page(GTK_NOTEBOOK(g_nb), i));
        if (d && d->path && strcmp(d->path, path) == 0) {
            gtk_notebook_set_current_page(GTK_NOTEBOOK(g_nb), i);
            return;
        }
    }
    char *contents = NULL; gsize len = 0; GError *err = NULL;
    if (!g_file_get_contents(path, &contents, &len, &err)) {
        show_error(err->message);
        g_error_free(err);
        return;
    }
    char *valid = g_utf8_make_valid(contents, (gssize)len);
    g_free(contents);

    /* re-use an empty, untouched "Untitled" tab */
    Doc *cur = doc_current();
    GtkWidget *reuse = NULL;
    if (cur && !cur->path && !cur->modified && gtk_text_buffer_get_char_count(cur->buf) == 0)
        reuse = cur->root;

    doc_new(path, valid);
    g_free(valid);
    if (reuse) gtk_notebook_remove_page(GTK_NOTEBOOK(g_nb), gtk_notebook_page_num(GTK_NOTEBOOK(g_nb), reuse));
}

/* ------------------------------------------------------------------ */
/*  Menu callbacks                                                     */
/* ------------------------------------------------------------------ */

static void on_new(GtkMenuItem *m, gpointer u)    { (void)m; (void)u; doc_new(NULL, ""); }
static void on_save(GtkMenuItem *m, gpointer u)   { (void)m; (void)u; Doc *d = doc_current(); if (d) doc_save(d, FALSE); }
static void on_saveas(GtkMenuItem *m, gpointer u) { (void)m; (void)u; Doc *d = doc_current(); if (d) doc_save(d, TRUE); }
static void on_close(GtkMenuItem *m, gpointer u)  { (void)m; (void)u; close_doc(doc_current()); }

static void on_open(GtkMenuItem *m, gpointer u) {
    (void)m; (void)u;
    GtkWidget *dlg = gtk_file_chooser_dialog_new("Open C File(s)", GTK_WINDOW(g_win),
            GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL,
            "_Open", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dlg), TRUE);

    GtkFileFilter *f1 = gtk_file_filter_new();
    gtk_file_filter_set_name(f1, "C files (*.c, *.h)");
    gtk_file_filter_add_pattern(f1, "*.c");
    gtk_file_filter_add_pattern(f1, "*.h");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), f1);
    GtkFileFilter *f2 = gtk_file_filter_new();
    gtk_file_filter_set_name(f2, "All files");
    gtk_file_filter_add_pattern(f2, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), f2);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        GSList *files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dlg));
        for (GSList *l = files; l; l = l->next) open_path(l->data);
        g_slist_free_full(files, g_free);
    }
    gtk_widget_destroy(dlg);
}

static gboolean on_delete(GtkWidget *w, GdkEvent *e, gpointer u) {
    (void)w; (void)e; (void)u;
    int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(g_nb));
    for (int i = 0; i < n; i++) {
        Doc *d = doc_from_page(gtk_notebook_get_nth_page(GTK_NOTEBOOK(g_nb), i));
        if (d && d->modified) {
            gtk_notebook_set_current_page(GTK_NOTEBOOK(g_nb), i);
            if (!confirm_discard(d)) return TRUE;               /* stay open */
        }
    }
    return FALSE;
}

static void on_quit(GtkMenuItem *m, gpointer u) {
    (void)m; (void)u;
    if (!on_delete(NULL, NULL, NULL)) gtk_main_quit();
}

static void on_switch_page(GtkNotebook *nb, GtkWidget *page, guint num, gpointer u) {
    (void)nb; (void)num; (void)u;
    Doc *d = doc_from_page(page);
    if (!d) return;
    update_info(d);
    char *name = doc_name(d);
    char *title = g_strdup_printf("%s%s - C Lexical Analyser", d->modified ? "* " : "", name);
    gtk_window_set_title(GTK_WINDOW(g_win), title);
    g_free(name); g_free(title);
}

static void add_item(GtkWidget *menu, GtkAccelGroup *ag, const char *label,
                     guint key, GdkModifierType mod, GCallback cb) {
    GtkWidget *mi = gtk_menu_item_new_with_mnemonic(label);
    if (key) gtk_widget_add_accelerator(mi, "activate", ag, key, mod, GTK_ACCEL_VISIBLE);
    g_signal_connect(mi, "activate", cb, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "textview { font-family: Monospace; font-size: 12pt; }"
        "treeview { font-size: 10pt; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_win), "C Lexical Analyser");
    gtk_window_set_default_size(GTK_WINDOW(g_win), 1100, 750);
    g_signal_connect(g_win, "delete-event", G_CALLBACK(on_delete), NULL);
    g_signal_connect(g_win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkAccelGroup *ag = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(g_win), ag);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(g_win), vbox);

    GtkWidget *bar = gtk_menu_bar_new();
    GtkWidget *file_item = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *file_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), file_item);
    add_item(file_menu, ag, "_New",        GDK_KEY_n, GDK_CONTROL_MASK, G_CALLBACK(on_new));
    add_item(file_menu, ag, "_Open...",    GDK_KEY_o, GDK_CONTROL_MASK, G_CALLBACK(on_open));
    add_item(file_menu, ag, "_Save",       GDK_KEY_s, GDK_CONTROL_MASK, G_CALLBACK(on_save));
    add_item(file_menu, ag, "Save _As...", GDK_KEY_s, GDK_CONTROL_MASK | GDK_SHIFT_MASK, G_CALLBACK(on_saveas));
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    add_item(file_menu, ag, "_Close Tab",  GDK_KEY_w, GDK_CONTROL_MASK, G_CALLBACK(on_close));
    add_item(file_menu, ag, "_Quit",       GDK_KEY_q, GDK_CONTROL_MASK, G_CALLBACK(on_quit));
    gtk_box_pack_start(GTK_BOX(vbox), bar, FALSE, FALSE, 0);

    g_nb = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(g_nb), TRUE);
    g_signal_connect(g_nb, "switch-page", G_CALLBACK(on_switch_page), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), g_nb, TRUE, TRUE, 0);

    g_info = gtk_label_new("");
    gtk_widget_set_halign(g_info, GTK_ALIGN_START);
    gtk_widget_set_margin_top(g_info, 3);
    gtk_widget_set_margin_bottom(g_info, 3);
    gtk_box_pack_start(GTK_BOX(vbox), g_info, FALSE, FALSE, 0);

    gtk_widget_show_all(g_win);

    for (int i = 1; i < argc; i++) open_path(argv[i]);
    if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(g_nb)) == 0)
        doc_new(NULL,
            "#include <stdio.h>\n\n"
            "int main(void) {\n"
            "    int count = 10;\n"
            "    printf(\"Hello, World! %d\\n\", count);\n"
            "    return 0;\n"
            "}\n");

    gtk_main();
    return 0;
}
