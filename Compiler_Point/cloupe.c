/*
 * CLoupe - a C compiler phase visualiser (GTK 3, classic-Mac look)
 *
 * Build : gcc cloupe.c -o cloupe $(pkg-config --cflags --libs gtk+-3.0) -lm
 * Run   : ./cloupe
 *
 * Phases : 1 Lexical  2 Syntax  3 Semantic  4 Intermediate code
 *          5 Optimiser  6 Target code (x86-style assembly)
 * Supports a C subset: int/float/char/void, functions, if/else, while, for,
 * return, calls (printf/scanf built in), + - * / %, comparisons, && || !, ++ --,
 * = += -= *= /=.  No arrays, pointers or structs.
 */
#include <gtk/gtk.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXT 4096
#define MAXI 4096
#define MAXL 8192
#define MAXN 20000
#define MAXS 512

/* ------------------------------------------------------------ logging */
static GString *lgs[6];
static void lg(int ph, const char *fmt, ...) {
    char b[400]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    g_string_append(lgs[ph], b); g_string_append_c(lgs[ph], '\n');
}
static void cp(char *d, int n, const char *s) { size_t l = strlen(s); if (l > (size_t)n - 1) l = n - 1; memmove(d, s, l); d[l] = 0; }

/* ------------------------------------------------------- 1. lexical */
enum { T_COM, T_PRE, T_STR, T_CHR, T_INTL, T_FLTL, T_KW, T_ID, T_OP, T_PUN, T_ERR };
static const char *TYN[] = {"comment", "preprocessor", "string", "char", "int-literal", "float-literal",
                            "keyword", "identifier", "operator", "punctuation", "error"};
enum { R_NONE, R_TYPE, R_CTRL, R_FN, R_CALL, R_DECL };
typedef struct { char v[128]; int ty, s, e, line, role, st, bad, warn; } Tok;
static Tok T[MAXT]; static int nt;
static int P[MAXT], np;                       /* tokens the parser sees */

static int isKW(const char *s) {
    static const char *k[] = {"int", "float", "char", "void", "if", "else", "while", "for", "return", 0};
    for (int i = 0; k[i]; i++) if (!strcmp(s, k[i])) return 1;
    return 0;
}
static void addTok(const char *src, int s, int e, int ty, int line) {
    if (nt >= MAXT) return;
    Tok *t = &T[nt++]; memset(t, 0, sizeof *t);
    int n = e - s; if (n > 127) n = 127;
    memcpy(t->v, src + s, n); t->v[n] = 0;
    t->ty = ty; t->s = s; t->e = e; t->line = line;
}
static void lexer(const char *src) {
    nt = 0; int i = 0, line = 1, n = strlen(src);
    static const char *ops2[] = {"==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=", "-=", "*=", "/=", 0};
    while (i < n) {
        char c = src[i]; int s = i;
        if (c == '\n') { line++; i++; continue; }
        if (isspace((unsigned char)c)) { i++; continue; }
        if (c == '/' && src[i + 1] == '/') { while (i < n && src[i] != '\n') i++; addTok(src, s, i, T_COM, line); continue; }
        if (c == '/' && src[i + 1] == '*') {
            int l0 = line; i += 2;
            while (i < n && !(src[i] == '*' && src[i + 1] == '/')) { if (src[i] == '\n') line++; i++; }
            i = i < n ? i + 2 : n; addTok(src, s, i, T_COM, l0); continue;
        }
        if (c == '#') { while (i < n && src[i] != '\n') i++; addTok(src, s, i, T_PRE, line); continue; }
        if (c == '"' || c == '\'') {
            char q = c; i++;
            while (i < n && src[i] != q && src[i] != '\n') { if (src[i] == '\\' && i + 1 < n) i++; i++; }
            if (i < n && src[i] == q) { i++; addTok(src, s, i, q == '"' ? T_STR : T_CHR, line); }
            else addTok(src, s, i, T_ERR, line);
            continue;
        }
        if (isdigit((unsigned char)c)) {
            int f = 0;
            while (i < n && isdigit((unsigned char)src[i])) i++;
            if (src[i] == '.' && isdigit((unsigned char)src[i + 1])) { f = 1; i++; while (i < n && isdigit((unsigned char)src[i])) i++; }
            addTok(src, s, i, f ? T_FLTL : T_INTL, line); continue;
        }
        if (isalpha((unsigned char)c) || c == '_') {
            while (i < n && (isalnum((unsigned char)src[i]) || src[i] == '_')) i++;
            char b[128]; int l = i - s > 127 ? 127 : i - s; memcpy(b, src + s, l); b[l] = 0;
            addTok(src, s, i, isKW(b) ? T_KW : T_ID, line); continue;
        }
        int m = 0;
        for (int k = 0; ops2[k]; k++) if (src[i] == ops2[k][0] && src[i + 1] == ops2[k][1]) { m = 1; break; }
        if (m) { i += 2; addTok(src, s, i, T_OP, line); continue; }
        if (strchr("+-*/%=<>!&|", c)) { i++; addTok(src, s, i, T_OP, line); continue; }
        if (strchr("{}()[];,", c)) { i++; addTok(src, s, i, T_PUN, line); continue; }
        i++; while (i < n && ((unsigned char)src[i] & 0xC0) == 0x80) i++;
        addTok(src, s, i, T_ERR, line);
    }
}

/* -------------------------------------------------------- 2. syntax */
enum { K_PROG, K_FUNC, K_PARAM, K_BLOCK, K_DECL, K_VAR, K_IF, K_WHILE, K_FOR, K_RET, K_EXPR,
       K_NUM, K_STR, K_ID, K_BIN, K_UN, K_INC, K_ASG, K_CALL };
enum { Y_VOID, Y_CHAR, Y_INT, Y_FLOAT, Y_STR };
static const char *YN[] = {"void", "char", "int", "float", "char*"};
typedef struct Node {
    int k, ty, tok, line, pre; char name[64], op[4], val[128], label[200];
    struct Node **c; int nc, cap;
} Node;
static Node *pool[MAXN]; static int npool;
static Node *mk(int k, const char *label) {
    Node *n = calloc(1, sizeof *n); n->k = k; n->tok = -1; cp(n->label, sizeof n->label, label);
    if (npool < MAXN) pool[npool++] = n;
    return n;
}
static Node *mkf(int k, const char *fmt, const char *a) { char b[200]; snprintf(b, sizeof b, fmt, a); return mk(k, b); }
static void add(Node *p, Node *c) {
    if (p->nc == p->cap) { p->cap = p->cap ? p->cap * 2 : 4; p->c = realloc(p->c, p->cap * sizeof(Node *)); }
    p->c[p->nc++] = c;
}
static void freeAst(void) { for (int i = 0; i < npool; i++) { free(pool[i]->c); free(pool[i]); } npool = 0; }
static int lo(Node *n) {
    if (!n) return 0;
    if (n->line) return n->line;
    if (n->tok >= 0) return T[n->tok].line;
    for (int i = 0; i < n->nc; i++) { int l = lo(n->c[i]); if (l) return l; }
    return 0;
}

static jmp_buf jb; static char perr[200]; static int perrTok = -1, pp;
static void fail(const char *fmt, const char *a) __attribute__((noreturn));
static void fail(const char *fmt, const char *a) {
    snprintf(perr, sizeof perr, fmt, a);
    perrTok = pp < np ? P[pp] : (np ? P[np - 1] : -1);
    longjmp(jb, 1);
}
static int is(const char *v) { return pp < np && !strcmp(T[P[pp]].v, v); }
static int isTy(int i) {
    if (i >= np) return 0;
    const char *s = T[P[i]].v;
    return !strcmp(s, "int") || !strcmp(s, "float") || !strcmp(s, "char") || !strcmp(s, "void");
}
static int tyOf(const char *s) { return !strcmp(s, "int") ? Y_INT : !strcmp(s, "float") ? Y_FLOAT : !strcmp(s, "char") ? Y_CHAR : Y_VOID; }
static int need(const char *v) {
    if (!is(v)) fail("expected '%s'", v);
    return P[pp++];
}
static int typeTok(void) { if (!isTy(pp)) fail("%s", "expected a type (int, float, char, void)"); T[P[pp]].role = R_TYPE; return P[pp++]; }
static int identTok(int role) { if (pp >= np || T[P[pp]].ty != T_ID) fail("%s", "expected a name"); T[P[pp]].role = role; return P[pp++]; }

static Node *expr(void), *stmt(void), *block(void);
static int bp(const char *o) {
    static const char *L[] = {"||", "&&", "==", "!=", "<", ">", "<=", ">=", "+", "-", "*", "/", "%"};
    static const int B[] = {1, 2, 3, 3, 4, 4, 4, 4, 5, 5, 6, 6, 6};
    for (int i = 0; i < 13; i++) if (!strcmp(o, L[i])) return B[i];
    return 0;
}
static Node *prim(void) {
    if (pp >= np) fail("%s", "unexpected end of file");
    int ti = P[pp]; Tok *t = &T[ti];
    if (t->ty == T_INTL || t->ty == T_FLTL) {
        pp++; Node *n = mkf(K_NUM, "Number %s", t->v); cp(n->val, sizeof n->val, t->v);
        n->ty = t->ty == T_INTL ? Y_INT : Y_FLOAT; return n;
    }
    if (t->ty == T_CHR) {
        pp++; Node *n = mkf(K_NUM, "Char %s", t->v); const char *s = t->v + 1;
        int v = s[0] == '\\' ? (s[1] == 'n' ? 10 : s[1] == 't' ? 9 : s[1] == 'r' ? 13 : s[1] == '0' ? 0 : s[1]) : s[0];
        snprintf(n->val, sizeof n->val, "%d", v); n->ty = Y_CHAR; return n;
    }
    if (t->ty == T_STR) { pp++; Node *n = mkf(K_STR, "String %s", t->v); cp(n->val, sizeof n->val, t->v); n->ty = Y_STR; return n; }
    if (t->ty == T_ID) {
        pp++;
        if (is("(")) {
            t->role = R_CALL; pp++; Node *n = mkf(K_CALL, "Call %s", t->v);
            cp(n->name, sizeof n->name, t->v); n->tok = ti; n->line = t->line;
            if (!is(")")) { do { add(n, expr()); } while (is(",") && ++pp); }
            need(")"); return n;
        }
        Node *n = mkf(K_ID, "Id %s", t->v); cp(n->name, sizeof n->name, t->v); n->tok = ti; n->line = t->line; return n;
    }
    if (!strcmp(t->v, "(")) { pp++; Node *e = expr(); need(")"); return e; }
    fail("unexpected '%s'", t->v);
}
static Node *post(void) {
    Node *e = prim();
    while (is("++") || is("--")) {
        Tok *o = &T[P[pp++]]; Node *n = mkf(K_INC, "Postfix %s", o->v); cp(n->op, 4, o->v); add(n, e); e = n;
    }
    return e;
}
static Node *un(void) {
    if (pp < np && T[P[pp]].ty == T_OP && (is("-") || is("!") || is("++") || is("--"))) {
        Tok *o = &T[P[pp++]]; Node *n;
        if (!strcmp(o->v, "++") || !strcmp(o->v, "--")) { n = mkf(K_INC, "Prefix %s", o->v); n->pre = 1; }
        else n = mkf(K_UN, "Unary %s", o->v);
        cp(n->op, 4, o->v); add(n, un()); return n;
    }
    return post();
}
static Node *bin(int m) {
    Node *l = un();
    while (pp < np && T[P[pp]].ty == T_OP && bp(T[P[pp]].v) >= m) {
        int oi = P[pp++]; Tok *o = &T[oi];
        Node *r = bin(bp(o->v) + 1), *n = mkf(K_BIN, "Binary %s", o->v);
        cp(n->op, 4, o->v); n->tok = oi; n->line = o->line; add(n, l); add(n, r); l = n;
    }
    return l;
}
static Node *expr(void) {
    Node *l = bin(1);
    if (pp < np && (is("=") || is("+=") || is("-=") || is("*=") || is("/="))) {
        if (l->k != K_ID) fail("%s", "left side of assignment must be a variable");
        Tok *o = &T[P[pp++]]; Node *n = mkf(K_ASG, "Assign %s", o->v);
        cp(n->op, 4, o->v); n->line = o->line; add(n, l); add(n, expr()); return n;
    }
    return l;
}
static Node *declRest(int tyTok, int id) {
    Node *d = mkf(K_DECL, "Decl %s", T[tyTok].v); int ty = tyOf(T[tyTok].v);
    for (;;) {
        Node *v = mkf(K_VAR, "Var %s", T[id].v);
        cp(v->name, 64, T[id].v); v->ty = ty; v->tok = id; v->line = T[id].line;
        if (is("=")) { pp++; add(v, expr()); strcat(v->label, " ="); }
        add(d, v); lg(1, "declaration: %s %s", T[tyTok].v, T[id].v);
        if (is(",")) { pp++; id = identTok(R_DECL); } else break;
    }
    need(";"); return d;
}
static Node *block(void) {
    need("{"); Node *b = mk(K_BLOCK, "Block");
    while (!is("}")) { if (pp >= np) fail("%s", "missing '}' before end of file"); add(b, stmt()); }
    pp++; return b;
}
static Node *stmt(void) {
    if (pp >= np) fail("%s", "unexpected end of file");
    Tok *t = &T[P[pp]];
    if (!strcmp(t->v, "{")) return block();
    if (isTy(pp)) { int ty = typeTok(); int id = identTok(R_DECL); return declRest(ty, id); }
    if (!strcmp(t->v, "if")) {
        t->role = R_CTRL; pp++; need("("); Node *n = mk(K_IF, "If"); add(n, expr()); need(")"); add(n, stmt());
        if (is("else")) { T[P[pp]].role = R_CTRL; pp++; add(n, stmt()); }
        lg(1, "selection: %s", n->nc == 3 ? "if-else" : "if"); return n;
    }
    if (!strcmp(t->v, "while")) {
        t->role = R_CTRL; pp++; need("("); Node *n = mk(K_WHILE, "While"); add(n, expr()); need(")"); add(n, stmt());
        lg(1, "iteration: while"); return n;
    }
    if (!strcmp(t->v, "for")) {
        t->role = R_CTRL; pp++; need("("); Node *n = mk(K_FOR, "For"), *in = NULL, *c = NULL, *u = NULL;
        if (isTy(pp)) { int ty = typeTok(); int id = identTok(R_DECL); in = declRest(ty, id); }
        else { if (!is(";")) { in = mk(K_EXPR, "Init"); add(in, expr()); } need(";"); }
        add(n, in);
        if (!is(";")) c = expr();
        need(";"); add(n, c);
        if (!is(")")) u = expr();
        need(")"); add(n, u); add(n, stmt());
        lg(1, "iteration: for"); return n;
    }
    if (!strcmp(t->v, "return")) {
        t->role = R_CTRL; pp++; Node *n = mk(K_RET, "Return"); n->line = t->line;
        if (!is(";")) add(n, expr());
        need(";"); lg(1, "jump: return"); return n;
    }
    Node *n = mk(K_EXPR, "ExprStmt"); add(n, expr()); need(";"); return n;
}
static Node *program(void) {
    Node *p = mk(K_PROG, "Program"); pp = 0;
    while (pp < np) {
        int ty = typeTok(), id = identTok(R_DECL);
        if (is("(")) {
            T[id].role = R_FN; pp++;
            Node *f = mkf(K_FUNC, "Function %s", T[id].v);
            strcat(f->label, " : "); strcat(f->label, T[ty].v);
            cp(f->name, 64, T[id].v); f->ty = tyOf(T[ty].v); f->tok = id;
            if (!is(")")) {
                do {
                    int pt = typeTok(), pn = identTok(R_DECL); Node *q = mk(K_PARAM, "");
                    snprintf(q->label, sizeof q->label, "Param %s %s", T[pt].v, T[pn].v);
                    cp(q->name, 64, T[pn].v); q->ty = tyOf(T[pt].v); q->tok = pn; add(f, q);
                } while (is(",") && ++pp);
            }
            need(")"); lg(1, "function definition: %s (%d params)", f->name, f->nc);
            add(f, block()); add(p, f);
        } else add(p, declRest(ty, id));
    }
    return p;
}

/* ------------------------------------------------------ 3. semantic */
typedef struct { char name[64]; int ty, kind, scope, line, alive, nparam; } Sym;   /* kind 0 var 1 param 2 func */
static Sym S[MAXS]; static int ns, depth;
typedef struct { char m[200]; int line, warn; } Err;
static Err E[256]; static int ne; static Node *curf;
static void err(int tok, int warn, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    if (ne < 256) { vsnprintf(E[ne].m, sizeof E[ne].m, fmt, ap); E[ne].line = tok >= 0 ? T[tok].line : 0; E[ne].warn = warn; ne++; }
    va_end(ap);
    if (tok >= 0) { if (warn) T[tok].warn = 1; else T[tok].bad = 1; }
}
static Sym *find(const char *n, int fn) {
    for (int i = ns - 1; i >= 0; i--) if (S[i].alive && !strcmp(S[i].name, n) && ((S[i].kind == 2) == fn)) return &S[i];
    return NULL;
}
static void declare(const char *name, int ty, int kind, int tok) {
    for (int i = 0; i < ns; i++)
        if (S[i].alive && S[i].scope == depth && S[i].kind != 2 && !strcmp(S[i].name, name)) { err(tok, 0, "redeclaration of '%s'", name); return; }
    if (ns >= MAXS) return;
    Sym *s = &S[ns++]; memset(s, 0, sizeof *s);
    cp(s->name, 64, name); s->ty = ty; s->kind = kind; s->scope = depth; s->line = T[tok].line; s->alive = 1;
    T[tok].st = ty + 1; lg(2, "scope %d: insert %s : %s", depth, name, YN[ty]);
}
static void popScope(void) { for (int i = 0; i < ns; i++) if (S[i].alive && S[i].scope == depth && S[i].kind != 2) S[i].alive = 0; depth--; }
static int rk(int t) { return t == Y_CHAR ? 1 : t == Y_INT ? 2 : t == Y_FLOAT ? 3 : 0; }
static void conv(int to, int from, int tok) {
    if (from == Y_STR && to != Y_STR) err(tok, 0, "cannot convert a string to %s", YN[to]);
    else if (to != Y_VOID && rk(from) > rk(to)) err(tok, 1, "'%s' to '%s' may lose precision", YN[from], YN[to]);
}
static int chk(Node *n) {
    int a, b;
    switch (n->k) {
    case K_NUM: case K_STR: return n->ty;
    case K_ID: {
        Sym *s = find(n->name, 0);
        if (!s) { err(n->tok, 0, "'%s' is not declared", n->name); return n->ty = Y_INT; }
        T[n->tok].st = s->ty + 1; lg(2, "lookup %s : %s (scope %d)", n->name, YN[s->ty], s->scope); return n->ty = s->ty;
    }
    case K_BIN: {
        a = chk(n->c[0]); b = chk(n->c[1]);
        if (a == Y_VOID || b == Y_VOID) err(n->tok, 0, "void value used in expression");
        if (n->op[0] == '%' && (a == Y_FLOAT || b == Y_FLOAT)) err(n->tok, 0, "'%%' needs integer operands");
        if (n->op[0] == '/' && n->c[1]->k == K_NUM && atof(n->c[1]->val) == 0) err(n->tok, 0, "division by zero");
        int cmp = strchr("<>=!&|", n->op[0]) != NULL;
        return n->ty = cmp ? Y_INT : (rk(a) >= rk(b) ? a : b);
    }
    case K_UN: a = chk(n->c[0]); return n->ty = n->op[0] == '!' ? Y_INT : a;
    case K_INC: if (n->c[0]->k != K_ID) err(-1, 0, "++/-- needs a variable"); return n->ty = chk(n->c[0]);
    case K_ASG: a = chk(n->c[0]); b = chk(n->c[1]); conv(a, b, n->c[0]->tok); return n->ty = a;
    case K_CALL: {
        for (int i = 0; i < n->nc; i++) chk(n->c[i]);
        Sym *f = find(n->name, 1); int bi = !strcmp(n->name, "printf") || !strcmp(n->name, "scanf");
        if (!f && !bi) err(n->tok, 0, "call to undeclared function '%s'", n->name);
        else if (f && f->nparam != n->nc) err(n->tok, 0, "'%s' expects %d argument(s), got %d", n->name, f->nparam, n->nc);
        lg(2, "call %s with %d argument(s)", n->name, n->nc);
        return n->ty = f ? f->ty : Y_INT;
    }
    }
    return Y_INT;
}
static int hasRet(Node *n) { if (!n) return 0; if (n->k == K_RET) return 1; for (int i = 0; i < n->nc; i++) if (hasRet(n->c[i])) return 1; return 0; }
static void st(Node *n, int noscope) {
    if (!n) return;
    switch (n->k) {
    case K_BLOCK:
        if (!noscope) depth++;
        for (int i = 0; i < n->nc; i++) st(n->c[i], 0);
        if (!noscope) popScope();
        break;
    case K_DECL:
        for (int i = 0; i < n->nc; i++) {
            Node *v = n->c[i]; declare(v->name, v->ty, 0, v->tok);
            if (v->ty == Y_VOID) err(v->tok, 0, "variable '%s' declared void", v->name);
            if (v->nc) conv(v->ty, chk(v->c[0]), v->tok);
        }
        break;
    case K_IF: chk(n->c[0]); st(n->c[1], 0); if (n->nc > 2) st(n->c[2], 0); break;
    case K_WHILE: chk(n->c[0]); st(n->c[1], 0); break;
    case K_FOR:
        depth++; st(n->c[0], 0); if (n->c[1]) chk(n->c[1]); if (n->c[2]) chk(n->c[2]); st(n->c[3], 0); popScope(); break;
    case K_RET:
        if (n->nc) {
            int t = chk(n->c[0]);
            if (curf->ty == Y_VOID) err(curf->tok, 0, "void function returns a value"); else conv(curf->ty, t, curf->tok);
        } else if (curf->ty != Y_VOID) err(curf->tok, 1, "non-void function returns nothing");
        break;
    case K_EXPR: chk(n->c[0]); break;
    default: chk(n);
    }
}
static void semantic(Node *ast) {
    ne = ns = depth = 0;
    for (int i = 0; i < ast->nc; i++) {
        Node *n = ast->c[i];
        if (n->k != K_FUNC) { st(n, 0); continue; }
        if (find(n->name, 1)) err(n->tok, 0, "redefinition of function '%s'", n->name);
        if (ns < MAXS) {
            Sym *s = &S[ns++]; memset(s, 0, sizeof *s);
            cp(s->name, 64, n->name); s->ty = n->ty; s->kind = 2; s->line = T[n->tok].line; s->alive = 1; s->nparam = n->nc - 1;
        }
        lg(2, "function %s: open scope", n->name);
        curf = n; depth = 1;
        for (int k = 0; k < n->nc - 1; k++) declare(n->c[k]->name, n->c[k]->ty, 1, n->c[k]->tok);
        Node *body = n->c[n->nc - 1]; st(body, 1); popScope();
        if (n->ty != Y_VOID && !hasRet(body)) err(n->tok, 1, "'%s' may not return a value", n->name);
    }
}

/* ------------------------------------------- 4. intermediate (3-address) */
enum { I_BIN, I_UN, I_MOV, I_LBL, I_JMP, I_JZ, I_PARAM, I_CALL, I_RET, I_FUNC, I_ARG, I_END, I_NOP };
typedef struct { int k, line, n; char d[64], a[128], b[128], o[4], l[64]; } Ins;
static Ins IR[MAXI], OP[MAXI]; static int nir, nop_, tc, lc, curLn;
static Ins *mkI(int k, const char *d, const char *a, const char *o, const char *b) {
    Ins *i = &IR[nir < MAXI - 1 ? nir++ : nir]; memset(i, 0, sizeof *i); i->k = k; i->line = curLn;
    if (d) cp(i->d, 64, d);
    if (a) cp(i->a, 128, a);
    if (o) cp(i->o, 4, o);
    if (b) cp(i->b, 128, b);
    return i;
}
static void newT(char *o) { snprintf(o, 128, "t%d", ++tc); }
static void newL(char *o) { snprintf(o, 16, "L%d", ++lc); }
static void ex(Node *n, char *out) {
    char a[128], b[128], o[2];
    switch (n->k) {
    case K_NUM: case K_STR: cp(out, 128, n->val); break;
    case K_ID: cp(out, 128, n->name); break;
    case K_BIN: ex(n->c[0], a); ex(n->c[1], b); newT(out); mkI(I_BIN, out, a, n->op, b); break;
    case K_UN: ex(n->c[0], a); newT(out); mkI(I_UN, out, a, n->op, 0); break;
    case K_INC: {
        const char *v = n->c[0]->name; o[0] = n->op[0]; o[1] = 0;
        if (n->pre) { mkI(I_BIN, v, v, o, "1"); cp(out, 128, v); }
        else { newT(out); mkI(I_MOV, out, v, 0, 0); mkI(I_BIN, v, v, o, "1"); }
        break;
    }
    case K_ASG: {
        const char *v = n->c[0]->name; ex(n->c[1], a); o[0] = n->op[0]; o[1] = 0;
        if (n->op[0] == '=') mkI(I_MOV, v, a, 0, 0); else mkI(I_BIN, v, v, o, a);
        cp(out, 128, v); break;
    }
    case K_CALL: {
        char as[16][128]; int na = n->nc > 16 ? 16 : n->nc;
        for (int k = 0; k < na; k++) ex(n->c[k], as[k]);
        for (int k = 0; k < na; k++) mkI(I_PARAM, 0, as[k], 0, 0);
        newT(out); Ins *i = mkI(I_CALL, out, 0, 0, 0); cp(i->l, 64, n->name); i->n = na; break;
    }
    }
}
static void gst(Node *n) {
    if (!n) return;
    int l = lo(n); if (l) curLn = l;
    char a[128], l1[16], l2[16];
    switch (n->k) {
    case K_BLOCK: for (int i = 0; i < n->nc; i++) gst(n->c[i]); break;
    case K_DECL:
        for (int i = 0; i < n->nc; i++) { Node *v = n->c[i]; curLn = v->line; if (v->nc) { ex(v->c[0], a); mkI(I_MOV, v->name, a, 0, 0); } }
        break;
    case K_EXPR: ex(n->c[0], a); break;
    case K_RET: { a[0] = 0; if (n->nc) ex(n->c[0], a); mkI(I_RET, 0, a, 0, 0); break; }
    case K_IF:
        ex(n->c[0], a); newL(l1); Ins *j = mkI(I_JZ, 0, a, 0, 0); cp(j->l, 64, l1); gst(n->c[1]);
        if (n->nc == 3) {
            newL(l2); j = mkI(I_JMP, 0, 0, 0, 0); cp(j->l, 64, l2);
            j = mkI(I_LBL, 0, 0, 0, 0); cp(j->l, 64, l1); gst(n->c[2]);
            j = mkI(I_LBL, 0, 0, 0, 0); cp(j->l, 64, l2);
        } else { j = mkI(I_LBL, 0, 0, 0, 0); cp(j->l, 64, l1); }
        break;
    case K_WHILE: {
        newL(l1); newL(l2); Ins *j = mkI(I_LBL, 0, 0, 0, 0); cp(j->l, 64, l1);
        ex(n->c[0], a); j = mkI(I_JZ, 0, a, 0, 0); cp(j->l, 64, l2); gst(n->c[1]);
        j = mkI(I_JMP, 0, 0, 0, 0); cp(j->l, 64, l1); j = mkI(I_LBL, 0, 0, 0, 0); cp(j->l, 64, l2); break;
    }
    case K_FOR: {
        gst(n->c[0]); newL(l1); newL(l2); Ins *j = mkI(I_LBL, 0, 0, 0, 0); cp(j->l, 64, l1);
        if (n->c[1]) { ex(n->c[1], a); j = mkI(I_JZ, 0, a, 0, 0); cp(j->l, 64, l2); }
        gst(n->c[3]); if (n->c[2]) ex(n->c[2], a);
        j = mkI(I_JMP, 0, 0, 0, 0); cp(j->l, 64, l1); j = mkI(I_LBL, 0, 0, 0, 0); cp(j->l, 64, l2); break;
    }
    }
}
static void generate(Node *ast) {
    nir = tc = lc = 0;
    for (int i = 0; i < ast->nc; i++) {
        Node *f = ast->c[i]; if (f->k != K_FUNC) continue;
        curLn = T[f->tok].line; mkI(I_FUNC, f->name, 0, 0, 0);
        for (int k = 0; k < f->nc - 1; k++) mkI(I_ARG, f->c[k]->name, 0, 0, 0)->n = k;
        gst(f->c[f->nc - 1]); mkI(I_END, 0, 0, 0, 0);
    }
}
static void fmtI(const Ins *i, char *o, int n) {
    switch (i->k) {
    case I_BIN: snprintf(o, n, "%s = %s %s %s", i->d, i->a, i->o, i->b); break;
    case I_UN: snprintf(o, n, "%s = %s%s", i->d, i->o, i->a); break;
    case I_MOV: snprintf(o, n, "%s = %s", i->d, i->a); break;
    case I_LBL: snprintf(o, n, "%s:", i->l); break;
    case I_JMP: snprintf(o, n, "goto %s", i->l); break;
    case I_JZ: snprintf(o, n, "ifFalse %s goto %s", i->a, i->l); break;
    case I_PARAM: snprintf(o, n, "param %s", i->a); break;
    case I_CALL: snprintf(o, n, "%s = call %s, %d", i->d, i->l, i->n); break;
    case I_RET: if (i->a[0]) snprintf(o, n, "return %s", i->a); else snprintf(o, n, "return"); break;
    case I_FUNC: snprintf(o, n, "func %s:", i->d); break;
    case I_ARG: snprintf(o, n, "%s = arg%d", i->d, i->n); break;
    case I_END: snprintf(o, n, "endfunc"); break;
    default: snprintf(o, n, "nop");
    }
}

/* --------------------------------------------------------- 5. optimiser */
static int isN(const char *s) {
    const char *p = s; int dot = 0;
    if (*p == '-') p++;
    if (!isdigit((unsigned char)*p)) return 0;
    for (; *p; p++) { if (*p == '.') { if (dot) return 0; dot = 1; } else if (!isdigit((unsigned char)*p)) return 0; }
    return 1;
}
static int isT(const char *s) { if (s[0] != 't' || !s[1]) return 0; for (int i = 1; s[i]; i++) if (!isdigit((unsigned char)s[i])) return 0; return 1; }
static void numStr(double r, char *out) { if (r == floor(r) && fabs(r) < 1e15) snprintf(out, 128, "%.0f", r); else snprintf(out, 128, "%g", r); }
static int ev(const char *o, double a, double b, char *out) {
    double r; int ia = floor(a) == a && floor(b) == b;
    if (!strcmp(o, "+")) r = a + b; else if (!strcmp(o, "-")) r = a - b; else if (!strcmp(o, "*")) r = a * b;
    else if (!strcmp(o, "/")) { if (b == 0) return 0; r = ia ? trunc(a / b) : a / b; }
    else if (!strcmp(o, "%")) { if (b == 0) return 0; r = fmod(a, b); }
    else if (!strcmp(o, "<")) r = a < b; else if (!strcmp(o, ">")) r = a > b;
    else if (!strcmp(o, "<=")) r = a <= b; else if (!strcmp(o, ">=")) r = a >= b;
    else if (!strcmp(o, "==")) r = a == b; else if (!strcmp(o, "!=")) r = a != b;
    else if (!strcmp(o, "&&")) r = (a != 0) && (b != 0); else if (!strcmp(o, "||")) r = (a != 0) || (b != 0);
    else return 0;
    numStr(r, out); return 1;
}
static char mk_[64][64], mv_[64][128]; static int nm;
static void mkill(const char *d) {
    for (int i = 0; i < nm;) {
        if (!strcmp(mk_[i], d) || !strcmp(mv_[i], d)) { cp(mk_[i], 64, mk_[nm - 1]); cp(mv_[i], 128, mv_[nm - 1]); nm--; } else i++;
    }
}
static const char *mget(const char *k) { for (int i = 0; i < nm; i++) if (!strcmp(mk_[i], k)) return mv_[i]; return NULL; }
static void mset(const char *k, const char *v) {
    for (int i = 0; i < nm; i++) if (!strcmp(mk_[i], k)) { cp(mv_[i], 128, v); return; }
    if (nm < 64) { cp(mk_[nm], 64, k); cp(mv_[nm], 128, v); nm++; }
}
static void toMov(Ins *i) { i->k = I_MOV; i->b[0] = 0; i->o[0] = 0; }
static void optimize(void) {
    memcpy(OP, IR, sizeof(Ins) * nir); nop_ = nir;
    int ch = 1, pass = 0; char buf[256], r[128];
    while (ch && pass < 8) {
        ch = 0; pass++; nm = 0;
        for (int x = 0; x < nop_; x++) {
            Ins *i = &OP[x];
            if (i->k == I_LBL || i->k == I_FUNC) { nm = 0; continue; }
            char *fs[2] = {i->a, i->b};
            for (int f = 0; f < 2; f++) {
                char *o = fs[f]; if (!*o) continue;
                const char *v = mget(o);
                if (v && strcmp(v, o)) { fmtI(i, buf, sizeof buf); lg(4, "propagate %s = %s into \"%s\"", o, v, buf); cp(o, 128, v); ch = 1; }
            }
            if (i->k == I_BIN && isN(i->a) && isN(i->b)) {
                if (ev(i->o, atof(i->a), atof(i->b), r)) { lg(4, "fold %s %s %s = %s", i->a, i->o, i->b, r); cp(i->a, 128, r); toMov(i); ch = 1; }
            } else if (i->k == I_BIN && ((!strcmp(i->o, "*") && !strcmp(i->b, "1")) || (!strcmp(i->o, "+") && !strcmp(i->b, "0")) ||
                                         (!strcmp(i->o, "-") && !strcmp(i->b, "0")) || (!strcmp(i->o, "/") && !strcmp(i->b, "1")))) {
                fmtI(i, buf, sizeof buf); lg(4, "identity: simplify \"%s\"", buf); toMov(i); ch = 1;
            } else if (i->k == I_BIN && !strcmp(i->o, "*") && !strcmp(i->b, "2")) {
                lg(4, "strength reduction: %s*2 becomes %s+%s", i->a, i->a, i->a); strcpy(i->o, "+"); cp(i->b, 128, i->a); ch = 1;
            } else if (i->k == I_UN && isN(i->a)) {
                double v = atof(i->a); numStr(i->o[0] == '-' ? -v : (v == 0), r);
                lg(4, "fold %s%s = %s", i->o, i->a, r); cp(i->a, 128, r); i->k = I_MOV; i->o[0] = 0; ch = 1;
            }
            if (i->k == I_JZ && isN(i->a)) {
                fmtI(i, buf, sizeof buf);
                if (atof(i->a) != 0) { lg(4, "branch never taken: drop \"%s\"", buf); i->k = I_NOP; }
                else { lg(4, "branch always taken: goto %s", i->l); i->k = I_JMP; }
                ch = 1;
            }
            if (i->k == I_MOV && !strcmp(i->d, i->a)) { lg(4, "remove self-assignment of %s", i->d); i->k = I_NOP; ch = 1; }
            if (i->k == I_BIN || i->k == I_UN || i->k == I_MOV || i->k == I_CALL || i->k == I_ARG) {
                mkill(i->d);
                if (i->k == I_MOV && (isN(i->a) || isalpha((unsigned char)i->a[0]) || i->a[0] == '_') && strcmp(i->a, i->d)) mset(i->d, i->a);
            }
        }
        for (int x = 0; x < nop_; x++) {
            Ins *i = &OP[x];
            if ((i->k == I_BIN || i->k == I_UN || i->k == I_MOV) && isT(i->d)) {
                int u = 0;
                for (int y = 0; y < nop_ && !u; y++) if (y != x && (!strcmp(OP[y].a, i->d) || !strcmp(OP[y].b, i->d))) u = 1;
                if (!u) { fmtI(i, buf, sizeof buf); lg(4, "dead code: remove \"%s\"", buf); i->k = I_NOP; ch = 1; }
            }
        }
        int w = 0; for (int x = 0; x < nop_; x++) if (OP[x].k != I_NOP) OP[w++] = OP[x]; nop_ = w;
    }
}

/* ------------------------------------------------------ 6. target code */
typedef struct { char t[160]; int line; } AL;
static AL B[MAXL], AS[MAXL]; static int nb, nas;
static char DATA[256][64], STRS[64][130]; static int nd, nstr;
static void W(int line, const char *fmt, ...) {
    if (nb >= MAXL) return;
    va_list ap; va_start(ap, fmt); vsnprintf(B[nb].t, sizeof B[nb].t, fmt, ap); va_end(ap); B[nb++].line = line;
}
static void opd(const char *s, char *o) {
    int k;
    if (!*s) { strcpy(o, "0"); return; }
    if (isN(s)) { cp(o, 70, s); return; }
    if (s[0] == '"') {
        for (k = 0; k < nstr; k++) if (!strcmp(STRS[k], s)) break;
        if (k == nstr && nstr < 64) cp(STRS[nstr++], 130, s);
        snprintf(o, 70, "str%d", k); return;
    }
    for (k = 0; k < nd; k++) if (!strcmp(DATA[k], s)) break;
    if (k == nd && nd < 256) cp(DATA[nd++], 64, s);
    snprintf(o, 70, "[%s]", s);
}
static void asmgen(void) {
    nb = nas = nd = nstr = 0;
    for (int x = 0; x < nop_; x++) {
        Ins *i = &OP[x]; int L = i->line; char A[70], Bo[70], D[70] = "0";
        opd(i->a, A); opd(i->b, Bo); if (i->d[0] && i->k != I_FUNC) opd(i->d, D);
        switch (i->k) {
        case I_FUNC: W(L, "%s:", i->d); W(L, "  push ebp"); W(L, "  mov ebp, esp"); break;
        case I_ARG: W(L, "  mov eax, [ebp+%d]", 8 + 4 * i->n); W(L, "  mov %s, eax", D); break;
        case I_MOV: W(L, "  mov eax, %s", A); W(L, "  mov %s, eax", D); break;
        case I_BIN: {
            W(L, "  mov eax, %s", A); W(L, "  mov ebx, %s", Bo);
            const char *o = i->o, *m = !strcmp(o, "+") ? "add" : !strcmp(o, "-") ? "sub" : !strcmp(o, "*") ? "imul" : !strcmp(o, "&&") ? "and" : !strcmp(o, "||") ? "or" : NULL;
            const char *c = !strcmp(o, "<") ? "l" : !strcmp(o, ">") ? "g" : !strcmp(o, "<=") ? "le" : !strcmp(o, ">=") ? "ge" : !strcmp(o, "==") ? "e" : !strcmp(o, "!=") ? "ne" : NULL;
            if (m) W(L, "  %s eax, ebx", m);
            else if (c) { W(L, "  cmp eax, ebx"); W(L, "  set%s al", c); W(L, "  movzx eax, al"); }
            else { W(L, "  cdq"); W(L, "  idiv ebx"); if (!strcmp(o, "%")) W(L, "  mov eax, edx"); }
            W(L, "  mov %s, eax", D); break;
        }
        case I_UN:
            W(L, "  mov eax, %s", A);
            if (i->o[0] == '-') W(L, "  neg eax"); else { W(L, "  cmp eax, 0"); W(L, "  sete al"); W(L, "  movzx eax, al"); }
            W(L, "  mov %s, eax", D); break;
        case I_LBL: W(L, "%s:", i->l); break;
        case I_JMP: W(L, "  jmp %s", i->l); break;
        case I_JZ: W(L, "  mov eax, %s", A); W(L, "  cmp eax, 0"); W(L, "  je %s", i->l); break;
        case I_PARAM: W(L, "  push %s", A); break;
        case I_CALL: W(L, "  call %s", i->l); if (i->n) W(L, "  add esp, %d", 4 * i->n); W(L, "  mov %s, eax", D); break;
        case I_RET: if (i->a[0]) W(L, "  mov eax, %s", A); W(L, "  pop ebp"); W(L, "  ret"); break;
        }
    }
    #define H(...) do { snprintf(AS[nas].t, sizeof AS[nas].t, __VA_ARGS__); AS[nas++].line = 0; } while (0)
    H("section .data");
    for (int k = 0; k < nd; k++) H("  %s dd 0", DATA[k]);
    for (int k = 0; k < nstr; k++) H("  str%d db %s, 0", k, STRS[k]);
    H("section .text");
    for (int k = 0; k < nb && nas < MAXL; k++) AS[nas++] = B[k];
}

/* =================================================================== GUI */
static const char *SAMPLES[3] = {
"#include <stdio.h>\n\nint square(int n) {\n    return n * n;\n}\n\nint main() {\n    int total = 0;\n    int i;\n"
"    for (i = 1; i <= 5; i++) {\n        total = total + square(i);\n    }\n    int secs = 60 * 60 * 24;   // folded at compile time\n"
"    if (total > 50) {\n        printf(\"big %d\\n\", total);\n    }\n    return 0;\n}\n",
"int main() {\n    int a = 5;\n    float f = 2.5;\n    a = f;\n    b = a + 1;\n    int a = 3;\n    return a;\n}\n",
"int main() {\n    int x = 10;\n    while (x > 0 {\n        x = x - 1;\n    }\n    return x\n}\n"};
static const char *SN[6] = {"Lexical", "Syntax", "Semantic", "Intermediate", "Optimiser", "Target"};
static const char *PCOL[6] = {"#ff5a5a", "#5a8cff", "#3fbf6a", "#ffa53f", "#b06cff", "#c79a5e"};
static const char *PD[6] = {
 "LEXICAL ANALYZER\nReads the characters left to right and groups them into tokens: keywords, identifiers, numbers, strings, operators. Whitespace and comments are dropped. Every token is marked in the editor with a red highlighter. Hover a row to find its lexeme.",
 "SYNTAX ANALYZER\nChecks the tokens against the grammar of C (recursive descent) and builds the Abstract Syntax Tree. Blue: types and keywords (strong), functions and calls (outlined shade), declared names (light).",
 "SEMANTIC ANALYZER\nWalks the tree with a symbol table and scope stack: declarations, types, calls. Green: names resolved to a type. Red wavy: error. Yellow underline: warning.",
 "INTERMEDIATE CODE GENERATOR\nFlattens the tree into three-address code: one operator per instruction, temporaries t1, t2... and labels L1, L2... for jumps. Orange: source lines that produced code.",
 "CODE OPTIMISER\nImproves the code without changing what it does: constant folding, constant/copy propagation, strength reduction and dead-code removal. Purple: source lines that still produce code.",
 "TARGET CODE GENERATION\nMaps each optimised instruction to x86-style assembly. Simplified: variables live in memory, eax and ebx are scratch registers, call arguments are pushed left to right."};
static const char *SM_[3] = {"\xE2\x9C\x93", "\xE2\x9C\x97", "\xE2\x80\x93"};   /* check, cross, dash */

static GtkWidget *win, *btn[6], *status; static GtkTextView *edv, *outv, *procv;
static GtkTextBuffer *eb, *ob, *pb; static GtkTextTag *tm1, *tm2, *tm3, *tbad, *twrn, *tfoc, *te, *tw, *th, *thov;
static char *src; static int slen, LO[MAXL], nlines, cur, S6[6], synErr, hovrow = -1, focS = -1, focE = -1;
static Node *ast;
static GString *og; static int rs_[MAXL], re_[MAXL], rf[MAXL], nrow;

static void lineRange(int ln, int *s, int *e) {
    if (ln < 1 || ln > nlines) { *s = *e = -1; return; }
    *s = LO[ln - 1]; *e = ln < nlines ? LO[ln] : slen;
}
static void row(int s, int e, int f, const char *fmt, ...) {
    char b[512]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    g_string_append(og, b); g_string_append_c(og, '\n');
    if (nrow < MAXL) { rs_[nrow] = s; re_[nrow] = e; rf[nrow] = f; nrow++; }
}
#define ROWL(ln, f, ...) do { int s_, e_; lineRange(ln, &s_, &e_); row(s_, e_, f, __VA_ARGS__); } while (0)

static void setColors(int ph) {
    GdkRGBA c, m; gdk_rgba_parse(&c, PCOL[ph]);
    double f[3] = {0.4, 0.72, 0.55}; GtkTextTag *tg[3] = {tm1, tm2, tm3};
    for (int i = 0; i < 3; i++) {
        m.red = c.red * (1 - f[i]) + f[i]; m.green = c.green * (1 - f[i]) + f[i]; m.blue = c.blue * (1 - f[i]) + f[i]; m.alpha = 1;
        g_object_set(tg[i], "background-rgba", &m, NULL);
    }
}
static void tagTok(Tok *t, GtkTextTag *tag) {
    GtkTextIter a, b;
    gtk_text_buffer_get_iter_at_offset(eb, &a, g_utf8_pointer_to_offset(src, src + t->s));
    gtk_text_buffer_get_iter_at_offset(eb, &b, g_utf8_pointer_to_offset(src, src + t->e));
    gtk_text_buffer_apply_tag(eb, tag, &a, &b);
}
static void paint(void) {
    GtkTextIter a, b; static char lh[MAXL];
    gtk_text_buffer_get_bounds(eb, &a, &b); gtk_text_buffer_remove_all_tags(eb, &a, &b);
    setColors(cur); memset(lh, 0, sizeof lh);
    if (cur == 3 && S6[3] == 0) for (int i = 0; i < nir; i++) { if (IR[i].line < MAXL) lh[IR[i].line] = 1; }
    if (cur == 4 && S6[4] == 0) for (int i = 0; i < nop_; i++) { if (OP[i].line < MAXL) lh[OP[i].line] = 1; }
    if (cur == 5 && S6[5] == 0) for (int i = 0; i < nb; i++) { if (B[i].line < MAXL) lh[B[i].line] = 1; }
    for (int i = 0; i < nt; i++) {
        Tok *t = &T[i];
        if (cur == 0) tagTok(t, i % 2 ? tm2 : tm1);
        else if (cur == 1) {
            if (t->role == R_TYPE || t->role == R_CTRL) tagTok(t, tm1);
            else if (t->role == R_FN || t->role == R_CALL) tagTok(t, tm3);
            else if (t->role == R_DECL) tagTok(t, tm2);
        } else if (cur == 2) { if (t->st) tagTok(t, tm1); if (t->warn) tagTok(t, twrn); }
        else if (t->line < MAXL && lh[t->line]) tagTok(t, tm2);
        if (t->bad) tagTok(t, tbad);
        if (focS >= 0 && t->s >= focS && t->e <= focE) tagTok(t, tfoc);
    }
}
static void tree(Node *n, int d, int ty) {
    if (!n) return;
    char tail[40] = "";
    if (ty && n->k >= K_NUM) snprintf(tail, sizeof tail, " : %s", YN[n->ty]);
    if (ty && n->k == K_VAR) snprintf(tail, sizeof tail, " : %s", YN[n->ty]);
    int s, e; lineRange(lo(n), &s, &e);
    row(s, e, 0, "%*s%s%s%s", d * 2, "", d ? "\xE2\x94\x94 " : "", n->label, tail);
    for (int i = 0; i < n->nc; i++) tree(n->c[i], d + 1, ty);
}
static void showOut(void) {
    og = g_string_new(""); nrow = 0; char buf[300];
    if (S6[cur] == 2) row(-1, -1, 1, "Blocked: fix the error in an earlier phase first.");
    else if (cur == 0) {
        for (int i = 0; i < nt; i++) if (T[i].ty == T_ERR) { row(T[i].s, T[i].e, 1, "Unknown or unterminated token on line %d", T[i].line); }
        row(-1, -1, 3, "%-4s %-20s %-14s %s", "#", "Lexeme", "Token type", "Line");
        for (int i = 0; i < nt && i < 600; i++) row(T[i].s, T[i].e, 0, "%-4d %-20.20s %-14s %d", i + 1, T[i].v, TYN[T[i].ty], T[i].line);
    } else if (cur == 1) {
        if (synErr) row(perrTok >= 0 ? T[perrTok].s : -1, perrTok >= 0 ? T[perrTok].e : -1, 1, "Syntax error on line %d: %s", perrTok >= 0 ? T[perrTok].line : 0, perr);
        else tree(ast, 0, 0);
    } else if (cur == 2) {
        if (!ne) row(-1, -1, 0, "No semantic errors.");
        for (int i = 0; i < ne; i++) ROWL(E[i].line, E[i].warn ? 2 : 1, "%s, line %d: %s", E[i].warn ? "Warning" : "Error", E[i].line, E[i].m);
        row(-1, -1, 3, "\nSymbol table"); row(-1, -1, 3, "%-14s %-7s %-9s %-6s %s", "Name", "Type", "Kind", "Scope", "Line");
        for (int i = 0; i < ns; i++) ROWL(S[i].line, 0, "%-14.14s %-7s %-9s %-6d %d", S[i].name, YN[S[i].ty], S[i].kind == 2 ? "function" : S[i].kind ? "param" : "variable", S[i].scope, S[i].line);
        row(-1, -1, 3, "\nType-annotated tree"); tree(ast, 0, 1);
    } else if (cur == 3) {
        for (int i = 0; i < nir; i++) { fmtI(&IR[i], buf, sizeof buf); ROWL(IR[i].line, 0, "%02d  %s%s", i + 1, IR[i].k == I_LBL || IR[i].k == I_FUNC ? "" : "  ", buf); }
    } else if (cur == 4) {
        row(-1, -1, 3, "Before (%d instructions)", nir);
        for (int i = 0; i < nir; i++) { fmtI(&IR[i], buf, sizeof buf); ROWL(IR[i].line, 0, "%02d  %s%s", i + 1, IR[i].k == I_LBL || IR[i].k == I_FUNC ? "" : "  ", buf); }
        row(-1, -1, 3, "\nAfter (%d instructions)", nop_);
        for (int i = 0; i < nop_; i++) { fmtI(&OP[i], buf, sizeof buf); ROWL(OP[i].line, 0, "%02d  %s%s", i + 1, OP[i].k == I_LBL || OP[i].k == I_FUNC ? "" : "  ", buf); }
    } else {
        for (int i = 0; i < nas; i++) ROWL(AS[i].line, 0, "%s", AS[i].t);
    }
    gtk_text_buffer_set_text(ob, og->str, -1); g_string_free(og, TRUE); hovrow = -1;
    for (int i = 0; i < nrow; i++) {
        GtkTextIter a, b; gtk_text_buffer_get_iter_at_line(ob, &a, i); b = a; gtk_text_iter_forward_to_line_end(&b);
        if (rf[i]) gtk_text_buffer_apply_tag(ob, rf[i] == 1 ? te : rf[i] == 2 ? tw : th, &a, &b);
    }
}
static void showProc(void) {
    GString *g = g_string_new(PD[cur]); g_string_append(g, "\n\nProcess log:\n");
    char **ls = g_strsplit(lgs[cur]->str, "\n", -1); int k = 0;
    for (int i = 0; ls[i] && k < 80; i++) if (*ls[i]) g_string_append_printf(g, "%2d. %s\n", ++k, ls[i]);
    if (!k) g_string_append(g, "(nothing yet)\n");
    g_strfreev(ls); gtk_text_buffer_set_text(pb, g->str, -1); g_string_free(g, TRUE);
}
static void updateBtn(void) {
    char b[64];
    for (int i = 0; i < 6; i++) {
        snprintf(b, sizeof b, "%d %s %s", i + 1, SN[i], SM_[S6[i]]); gtk_button_set_label(GTK_BUTTON(btn[i]), b);
        GtkStyleContext *sc = gtk_widget_get_style_context(btn[i]);
        if (i == cur) gtk_style_context_add_class(sc, "on"); else gtk_style_context_remove_class(sc, "on");
    }
}
static void refresh(void) { paint(); showProc(); showOut(); updateBtn(); }
static void run(void) {
    GtkTextIter a, b; gtk_text_buffer_get_bounds(eb, &a, &b);
    g_free(src); src = gtk_text_buffer_get_text(eb, &a, &b, FALSE); slen = strlen(src);
    nlines = 1; LO[0] = 0; for (int i = 0; i < slen && nlines < MAXL; i++) if (src[i] == '\n') LO[nlines++] = i + 1;
    freeAst(); ast = NULL; for (int i = 0; i < 6; i++) { g_string_truncate(lgs[i], 0); S6[i] = 0; }
    ne = ns = nir = nop_ = nb = nas = synErr = 0; focS = focE = -1;
    lexer(src); np = 0; int cnt[11] = {0}, lerr = 0;
    for (int i = 0; i < nt; i++) {
        cnt[T[i].ty]++; if (T[i].ty == T_ERR) { T[i].bad = 1; lerr = 1; }
        if (T[i].ty != T_COM && T[i].ty != T_PRE && T[i].ty != T_ERR) P[np++] = i;
    }
    lg(0, "read %d characters, %d lines", slen, nlines);
    for (int i = 0; i < 11; i++) if (cnt[i]) lg(0, "%d x %s", cnt[i], TYN[i]);
    S6[0] = lerr;
    if (setjmp(jb) == 0) ast = program(); else { synErr = 1; S6[1] = 1; if (perrTok >= 0) T[perrTok].bad = 1; }
    if (ast && !synErr) { semantic(ast); for (int i = 0; i < ne; i++) if (!E[i].warn) S6[2] = 1; } else S6[2] = 2;
    if (S6[2] == 0) {
        generate(ast); lg(3, "%d instructions, %d temporaries, %d labels", nir, tc, lc);
        optimize(); lg(4, "result: %d instructions become %d", nir, nop_);
        asmgen(); lg(5, "%d assembly lines, %d data symbols, %d strings", nas, nd, nstr);
    } else S6[3] = S6[4] = S6[5] = 2;
    char sb[160]; int bad = 0; for (int i = 0; i < 6; i++) if (S6[i] == 1) bad++;
    snprintf(sb, sizeof sb, "%d tokens, %d lines, %s", nt, nlines, bad ? "errors found" : "no errors");
    gtk_label_set_text(GTK_LABEL(status), sb);
    refresh();
}
static void on_changed(GtkTextBuffer *x, gpointer d) { run(); }
static void on_phase(GtkWidget *w, gpointer d) { cur = GPOINTER_TO_INT(d); refresh(); }
static void on_sample(GtkComboBox *c, gpointer d) { int i = gtk_combo_box_get_active(c); if (i >= 0) gtk_text_buffer_set_text(eb, SAMPLES[i], -1); }
static void hover(int ln) {
    if (ln == hovrow) return;
    hovrow = ln; GtkTextIter a, b; gtk_text_buffer_get_bounds(ob, &a, &b); gtk_text_buffer_remove_tag(ob, thov, &a, &b);
    focS = focE = -1;
    if (ln >= 0 && ln < nrow) {
        gtk_text_buffer_get_iter_at_line(ob, &a, ln); b = a; gtk_text_iter_forward_to_line_end(&b);
        gtk_text_buffer_apply_tag(ob, thov, &a, &b); focS = rs_[ln]; focE = re_[ln];
    }
    paint();
}
static gboolean on_motion(GtkWidget *w, GdkEventMotion *ev, gpointer d) {
    gint bx, by; GtkTextIter it;
    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(w), GTK_TEXT_WINDOW_TEXT, (gint)ev->x, (gint)ev->y, &bx, &by);
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(w), &it, bx, by); hover(gtk_text_iter_get_line(&it)); return FALSE;
}
static gboolean on_leave(GtkWidget *w, GdkEventCrossing *ev, gpointer d) { hover(-1); return FALSE; }

static GtkWidget *titlebar(const char *t) {
    GtkWidget *b = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), *l = gtk_label_new(t);
    gtk_style_context_add_class(gtk_widget_get_style_context(b), "titlebar");
    gtk_style_context_add_class(gtk_widget_get_style_context(l), "tt");
    gtk_widget_set_halign(l, GTK_ALIGN_CENTER); gtk_box_pack_start(GTK_BOX(b), l, TRUE, FALSE, 0); return b;
}
static GtkWidget *scroller(GtkWidget *child, int minh) {
    GtkWidget *s = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(s), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    if (minh) gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(s), minh);
    gtk_container_add(GTK_CONTAINER(s), child); return s;
}
static GtkTextView *mkview(int editable) {
    GtkTextView *v = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_wrap_mode(v, GTK_WRAP_NONE); gtk_text_view_set_editable(v, editable);
    gtk_text_view_set_cursor_visible(v, editable); gtk_text_view_set_left_margin(v, 8); gtk_text_view_set_top_margin(v, 6);
    return v;
}
static void style(GtkWidget *w, const char *cls) { gtk_style_context_add_class(gtk_widget_get_style_context(w), cls); }

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    for (int i = 0; i < 6; i++) lgs[i] = g_string_new("");
    const char *css =
        "window{background:#cfcfcf;} *{font-family:'Monaco','Menlo','DejaVu Sans Mono','Liberation Mono',monospace;}"
        ".menubar{background:#fff;border-bottom:2px solid #000;padding:3px 12px;} .menubar label{color:#000;}"
        ".macwin{border:2px solid #000;background:#fff;}"
        ".titlebar{border-bottom:2px solid #000;padding:2px 6px;background-image:repeating-linear-gradient(to bottom,#000 0px,#000 1px,#fff 1px,#fff 4px);}"
        ".tt{background:#fff;padding:0 10px;color:#000;font-weight:bold;}"
        ".status{border-top:2px solid #000;background:#f2f2f2;padding:3px 10px;color:#000;}"
        ".pipe{background:#f2f2f2;border-bottom:2px solid #000;padding:6px;}"
        "button{background:#fff;background-image:none;color:#000;border:2px solid #000;border-radius:7px;box-shadow:none;text-shadow:none;padding:1px 6px 4px;}"
        "button:hover{background:#e6e6e6;} button.on{background:#000;color:#fff;}"
        "button.p0{box-shadow:inset 0 -5px #ff5a5a;} button.p1{box-shadow:inset 0 -5px #5a8cff;} button.p2{box-shadow:inset 0 -5px #3fbf6a;}"
        "button.p3{box-shadow:inset 0 -5px #ffa53f;} button.p4{box-shadow:inset 0 -5px #b06cff;} button.p5{box-shadow:inset 0 -5px #c79a5e;}"
        "textview{font-size:11pt;} textview text{background:#fff;color:#000;} .procbox textview text{background:#f7f7f7;}"
        "scrolledwindow{border:0;} .procbox{border-bottom:2px solid #000;}";
    GtkCssProvider *prov = gtk_css_provider_new(); gtk_css_provider_load_from_data(prov, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(prov), GTK_STYLE_PROVIDER_PRIORITY_USER);

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_title(GTK_WINDOW(win), "CLoupe - C compiler visualiser");
    gtk_window_set_default_size(GTK_WINDOW(win), 1180, 720); g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_container_add(GTK_CONTAINER(win), root);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16); style(bar, "menubar");
    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Loop & functions");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Semantic errors");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Syntax error");
    GtkWidget *name = gtk_label_new("\xE2\x8C\x98 CLoupe"), *sl = gtk_label_new("Samples:");
    gtk_box_pack_start(GTK_BOX(bar), name, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(bar), sl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), combo, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(root), bar, FALSE, FALSE, 0);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL); gtk_container_set_border_width(GTK_CONTAINER(paned), 14);
    gtk_box_pack_start(GTK_BOX(root), paned, TRUE, TRUE, 0);

    /* left window: editor */
    GtkWidget *lw = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); style(lw, "macwin");
    edv = mkview(TRUE); eb = gtk_text_view_get_buffer(edv);
    tm1 = gtk_text_buffer_create_tag(eb, "m1", NULL); tm2 = gtk_text_buffer_create_tag(eb, "m2", NULL); tm3 = gtk_text_buffer_create_tag(eb, "m3", "style", PANGO_STYLE_ITALIC, NULL);
    tbad = gtk_text_buffer_create_tag(eb, "bad", "underline", PANGO_UNDERLINE_ERROR, "background", "#ffc4c4", NULL);
    twrn = gtk_text_buffer_create_tag(eb, "wrn", "underline", PANGO_UNDERLINE_SINGLE, "background", "#ffe9a8", NULL);
    tfoc = gtk_text_buffer_create_tag(eb, "foc", "background", "#000000", "foreground", "#ffffff", NULL);
    status = gtk_label_new(""); gtk_widget_set_halign(status, GTK_ALIGN_START); style(status, "status");
    gtk_box_pack_start(GTK_BOX(lw), titlebar("untitled.c"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(lw), scroller(GTK_WIDGET(edv), 0), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(lw), status, FALSE, FALSE, 0);
    gtk_paned_pack1(GTK_PANED(paned), lw, TRUE, FALSE);

    /* right window: phases */
    GtkWidget *rw = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); style(rw, "macwin");
    GtkWidget *fb = gtk_flow_box_new(); style(fb, "pipe");
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(fb), GTK_SELECTION_NONE); gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(fb), 3);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(fb), TRUE); gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(fb), 6); gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(fb), 6);
    for (int i = 0; i < 6; i++) {
        char c[8]; snprintf(c, sizeof c, "p%d", i); btn[i] = gtk_button_new_with_label(SN[i]); style(btn[i], c);
        g_signal_connect(btn[i], "clicked", G_CALLBACK(on_phase), GINT_TO_POINTER(i)); gtk_flow_box_insert(GTK_FLOW_BOX(fb), btn[i], -1);
    }
    procv = mkview(FALSE); gtk_text_view_set_wrap_mode(procv, GTK_WRAP_WORD); pb = gtk_text_view_get_buffer(procv);
    GtkWidget *ps = scroller(GTK_WIDGET(procv), 170); style(ps, "procbox");
    outv = mkview(FALSE); ob = gtk_text_view_get_buffer(outv);
    te = gtk_text_buffer_create_tag(ob, "e", "foreground", "#cc0000", "weight", PANGO_WEIGHT_BOLD, NULL);
    tw = gtk_text_buffer_create_tag(ob, "w", "foreground", "#b36b00", "weight", PANGO_WEIGHT_BOLD, NULL);
    th = gtk_text_buffer_create_tag(ob, "h", "weight", PANGO_WEIGHT_BOLD, NULL);
    thov = gtk_text_buffer_create_tag(ob, "hov", "background", "#000000", "foreground", "#ffffff", NULL);
    gtk_widget_add_events(GTK_WIDGET(outv), GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(outv, "motion-notify-event", G_CALLBACK(on_motion), NULL);
    g_signal_connect(outv, "leave-notify-event", G_CALLBACK(on_leave), NULL);
    gtk_box_pack_start(GTK_BOX(rw), titlebar("Compiler phases"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rw), fb, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rw), ps, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rw), scroller(GTK_WIDGET(outv), 0), TRUE, TRUE, 0);
    gtk_paned_pack2(GTK_PANED(paned), rw, TRUE, FALSE); gtk_paned_set_position(GTK_PANED(paned), 470);

    g_signal_connect(eb, "changed", G_CALLBACK(on_changed), NULL);
    g_signal_connect(combo, "changed", G_CALLBACK(on_sample), NULL);
    gtk_widget_show_all(win);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    gtk_main();
    return 0;
}
