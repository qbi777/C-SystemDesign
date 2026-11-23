/*
 * Lexical Analyzer Project (C)
 * ----------------------------------------------------------------------
 * Input  : a .c source file (command line argument)
 * Output : every token of the file with its classification:
 *            - Preprocessor directive   (line starts with '#')
 *            - Reserved keyword         (int, return, while, ...)
 *            - Non-reserved keyword     (NULL, EOF, size_t, ...)
 *            - Identifier               (variable / function names)
 *            - Numeric constant         (integer, float, hex)
 *            - Character / String constant
 *            - Operator                 (arithmetic, relational, ...)
 *            - Delimiter / special char (; , . and brackets)
 *          plus error messages: missing ';', unbalanced brackets,
 *          unterminated string/comment, invalid characters.
 *
 * Logic follows the flowchart: read a line, scan one token at a time until
 * end of the string, and test it in order:
 *   keyword -> float/int/char/string constant -> operator/bracket -> identifier
 * No strtok() is used (it destroys the line); the line is scanned by index.
 *
 * Build : gcc -Wall -o lexer lexical_analyzer.c
 * Run   : ./lexer test_ok.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE   4096
#define MAX_STACK  1024
#define MAX_LEX    4200

/* ---- tables (edit these to extend the analyzer) ---------------------- */
static const char *reserved_keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "inline", "int", "long", "register", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned",
    "void", "volatile", "while", NULL
};

static const char *non_reserved_keywords[] = {
    "NULL", "EOF", "size_t", "bool", "true", "false",
    "stdin", "stdout", "stderr", "FILE", NULL
};

/* multi-character operators, longest first */
static const char *multi_ops[] = {
    "<<=", ">>=", "++", "--", "==", "!=", "<=", ">=", "&&", "||", "<<",
    ">>", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "->", NULL
};
static const char *single_ops = "+-*/%=<>!&|^~?:";

/* ---- token categories ------------------------------------------------ */
enum { C_PREPROC, C_KEYWORD, C_NONRES, C_IDENT, C_INT, C_FLOAT, C_CHAR,
       C_STRING, C_OP, C_DELIM, C_BRACKET, C_ERROR, C_COUNT };

static const char *cat_names[C_COUNT] = {
    "Preprocessor directive", "Reserved keyword", "Non-reserved keyword",
    "Identifier", "Integer constant", "Float constant", "Character constant",
    "String constant", "Operator", "Delimiter / special char", "Bracket",
    "ERROR"
};

static int counts[C_COUNT];
static int error_count = 0;

/* previous token, used for the missing-semicolon check */
static struct { int valid, cat, line; char lex[64]; } prev;

/* bracket stack */
static char stack_ch[MAX_STACK];
static int  stack_line[MAX_STACK];
static int  sp = 0;
static int  paren_depth = 0;

/* ---- helpers --------------------------------------------------------- */
static int in_list(const char *word, const char **list)
{
    for (int i = 0; list[i]; i++)
        if (strcmp(word, list[i]) == 0) return 1;
    return 0;
}

static const char *op_kind(const char *op)
{
    if (strchr("+-*/%", op[0]) && (op[1] == '\0' || op[1] == op[0]) &&
        strcmp(op, "->") != 0 && !(op[1] == '=' ))
        return "Arithmetic operator";
    if (!strcmp(op, "==") || !strcmp(op, "!=") || !strcmp(op, "<") ||
        !strcmp(op, ">") || !strcmp(op, "<=") || !strcmp(op, ">="))
        return "Relational operator";
    if (!strcmp(op, "&&") || !strcmp(op, "||") || !strcmp(op, "!"))
        return "Logical operator";
    if (!strcmp(op, "=") || (strlen(op) >= 2 && op[strlen(op) - 1] == '=' &&
        strcmp(op, "==") && strcmp(op, "!=") && strcmp(op, "<=") &&
        strcmp(op, ">=")))
        return "Assignment operator";
    if (!strcmp(op, "&") || !strcmp(op, "|") || !strcmp(op, "^") ||
        !strcmp(op, "~") || !strcmp(op, "<<") || !strcmp(op, ">>"))
        return "Bitwise operator";
    return "Operator";
}

static void report_error(int line, const char *fmt, const char *a, int n)
{
    char msg[256];
    snprintf(msg, sizeof msg, fmt, a, n);
    printf("%-6d %-26s %s\n", line, "ERROR", msg);
    counts[C_ERROR]++;
    error_count++;
}

/*
 * Missing ';' detection. Called before each new token (and at EOF with
 * cat = -1). If the previous token ended a line and looks like the end of
 * a statement, but the next token does not continue it, report an error.
 */
static void check_semicolon(int next_line, int cat, const char *lex)
{
    if (!prev.valid || prev.line >= next_line || paren_depth > 0) return;

    int ends_stmt =
        prev.cat == C_IDENT || prev.cat == C_INT || prev.cat == C_FLOAT ||
        prev.cat == C_CHAR || prev.cat == C_STRING || prev.cat == C_NONRES ||
        (prev.cat == C_KEYWORD && (!strcmp(prev.lex, "return") ||
                                   !strcmp(prev.lex, "break") ||
                                   !strcmp(prev.lex, "continue")));
    if (!ends_stmt) return;

    if (cat >= 0) {                       /* a real next token exists */
        if (cat == C_DELIM) return;       /* ; , .  */
        if (cat == C_BRACKET && strchr("{)]", lex[0])) return;
        if (cat == C_OP && !strchr("*&!~", lex[0]) &&
            strcmp(lex, "++") && strcmp(lex, "--")) return; /* continuation */
    }
    printf("%-6d %-26s missing ';' after '%s'\n", prev.line, "ERROR", prev.lex);
    counts[C_ERROR]++;
    error_count++;
}

static void emit(int cat, const char *label, const char *lex, int line)
{
    check_semicolon(line, cat, lex);
    printf("%-6d %-26s %s\n", line, label ? label : cat_names[cat], lex);
    counts[cat]++;
    prev.valid = 1;
    prev.cat = cat;
    prev.line = line;
    snprintf(prev.lex, sizeof prev.lex, "%s", lex);
}

static void handle_bracket(char c, int line)
{
    char lex[2] = { c, '\0' };
    const char *label;
    switch (c) {
        case '(': label = "Open parenthesis";  break;
        case ')': label = "Close parenthesis"; break;
        case '{': label = "Open brace";        break;
        case '}': label = "Close brace";       break;
        case '[': label = "Open square bracket";  break;
        default:  label = "Close square bracket"; break;
    }
    emit(C_BRACKET, label, lex, line);

    if (c == '(' || c == '{' || c == '[') {
        if (sp < MAX_STACK) { stack_ch[sp] = c; stack_line[sp] = line; sp++; }
        if (c == '(') paren_depth++;
    } else {
        char open = (c == ')') ? '(' : (c == '}') ? '{' : '[';
        if (c == ')' && paren_depth > 0) paren_depth--;
        if (sp == 0) {
            char m[2] = { c, '\0' };
            report_error(line, "unexpected '%s' - no matching opening bracket", m, 0);
        } else {
            if (stack_ch[sp - 1] != open) {
                char m[2] = { c, '\0' };
                char buf[64];
                snprintf(buf, sizeof buf, "mismatched '%s' (open '%c' from line %%d)",
                         m, stack_ch[sp - 1]);
                char msg[128];
                snprintf(msg, sizeof msg, buf, stack_line[sp - 1]);
                printf("%-6d %-26s %s\n", line, "ERROR", msg);
                counts[C_ERROR]++;
                error_count++;
                if (stack_ch[sp - 1] == '(' && paren_depth > 0) paren_depth--;
            }
            sp--;
        }
    }
}

/* ---- main scanner ---------------------------------------------------- */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.c>\n", argv[0]);
        return 1;
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("Cannot open input file");
        return 1;
    }

    char line[MAX_LINE];
    int lineno = 0, in_comment = 0, comment_line = 0;

    printf("Lexical analysis of: %s\n\n", argv[1]);
    printf("%-6s %-26s %s\n", "LINE", "TOKEN TYPE", "LEXEME");
    printf("------------------------------------------------------------\n");

    while (fgets(line, sizeof line, fp)) {
        lineno++;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        int i = 0;

        /* preprocessor directive: line whose first character is '#' */
        if (!in_comment) {
            while (i < (int)len && isspace((unsigned char)line[i])) i++;
            if (i < (int)len && line[i] == '#') {
                emit(C_PREPROC, NULL, line + i, lineno);
                prev.valid = 0;           /* directive is not a statement */
                continue;
            }
            i = 0;
        }

        while (i < (int)len) {                    /* until end of string */
            char c = line[i];
            char lex[MAX_LEX];

            if (in_comment) {                     /* inside  / * ... * /  */
                char *end = strstr(line + i, "*/");
                if (!end) break;
                i = (int)(end - line) + 2;
                in_comment = 0;
                continue;
            }
            if (isspace((unsigned char)c)) { i++; continue; }

            /* comments */
            if (c == '/' && line[i + 1] == '/') break;
            if (c == '/' && line[i + 1] == '*') {
                in_comment = 1; comment_line = lineno; i += 2; continue;
            }

            /* keyword / identifier */
            if (isalpha((unsigned char)c) || c == '_') {
                int j = i;
                while (isalnum((unsigned char)line[j]) || line[j] == '_') j++;
                snprintf(lex, sizeof lex, "%.*s", j - i, line + i);
                if (in_list(lex, reserved_keywords))
                    emit(C_KEYWORD, NULL, lex, lineno);
                else if (in_list(lex, non_reserved_keywords))
                    emit(C_NONRES, NULL, lex, lineno);
                else
                    emit(C_IDENT, NULL, lex, lineno);
                i = j;
                continue;
            }

            /* numeric constant: int, float, hex */
            if (isdigit((unsigned char)c) ||
                (c == '.' && isdigit((unsigned char)line[i + 1]))) {
                int j = i, is_float = 0;
                if (c == '0' && (line[j + 1] == 'x' || line[j + 1] == 'X')) {
                    j += 2;
                    while (isxdigit((unsigned char)line[j])) j++;
                } else {
                    while (isdigit((unsigned char)line[j])) j++;
                    if (line[j] == '.') {
                        is_float = 1; j++;
                        while (isdigit((unsigned char)line[j])) j++;
                    }
                    if ((line[j] == 'e' || line[j] == 'E') &&
                        (isdigit((unsigned char)line[j + 1]) ||
                         ((line[j + 1] == '+' || line[j + 1] == '-') &&
                          isdigit((unsigned char)line[j + 2])))) {
                        is_float = 1; j += 2;
                        while (isdigit((unsigned char)line[j])) j++;
                    }
                }
                while (strchr("fFlLuU", line[j]) && line[j] != '\0') j++; /* suffix */
                if (isalpha((unsigned char)line[j]) || line[j] == '_') {
                    while (isalnum((unsigned char)line[j]) || line[j] == '_') j++;
                    snprintf(lex, sizeof lex, "%.*s", j - i, line + i);
                    report_error(lineno, "invalid numeric constant / identifier '%s'", lex, 0);
                } else {
                    snprintf(lex, sizeof lex, "%.*s", j - i, line + i);
                    emit(is_float ? C_FLOAT : C_INT, NULL, lex, lineno);
                }
                i = j;
                continue;
            }

            /* string constant (starts with double quote) */
            if (c == '"') {
                int j = i + 1;
                while (j < (int)len && line[j] != '"') {
                    if (line[j] == '\\' && j + 1 < (int)len) j++;
                    j++;
                }
                if (j >= (int)len) {
                    snprintf(lex, sizeof lex, "%s", line + i);
                    report_error(lineno, "unterminated string constant %s", lex, 0);
                    break;
                }
                snprintf(lex, sizeof lex, "%.*s", j - i + 1, line + i);
                emit(C_STRING, NULL, lex, lineno);
                i = j + 1;
                continue;
            }

            /* character constant */
            if (c == '\'') {
                int j = i + 1;
                while (j < (int)len && line[j] != '\'') {
                    if (line[j] == '\\' && j + 1 < (int)len) j++;
                    j++;
                }
                if (j >= (int)len) {
                    snprintf(lex, sizeof lex, "%s", line + i);
                    report_error(lineno, "unterminated character constant %s", lex, 0);
                    break;
                }
                snprintf(lex, sizeof lex, "%.*s", j - i + 1, line + i);
                emit(C_CHAR, NULL, lex, lineno);
                i = j + 1;
                continue;
            }

            /* brackets */
            if (strchr("(){}[]", c)) { handle_bracket(c, lineno); i++; continue; }

            /* delimiters */
            if (c == ';') { emit(C_DELIM, "Delimiter (end of statement)", ";", lineno); i++; continue; }
            if (c == ',') { emit(C_DELIM, "Delimiter (comma)", ",", lineno); i++; continue; }
            if (c == '.') { emit(C_DELIM, "Special character (dot)", ".", lineno); i++; continue; }

            /* operators: try multi-character first, then single */
            {
                int matched = 0;
                for (int k = 0; multi_ops[k]; k++) {
                    size_t n = strlen(multi_ops[k]);
                    if (strncmp(line + i, multi_ops[k], n) == 0) {
                        emit(C_OP, op_kind(multi_ops[k]), multi_ops[k], lineno);
                        i += (int)n;
                        matched = 1;
                        break;
                    }
                }
                if (matched) continue;
                if (strchr(single_ops, c)) {
                    char op[2] = { c, '\0' };
                    emit(C_OP, op_kind(op), op, lineno);
                    i++;
                    continue;
                }
            }

            /* anything else is a lexical error */
            {
                char bad[2] = { c, '\0' };
                report_error(lineno, "invalid character '%s'", bad, 0);
                i++;
            }
        }
    }
    fclose(fp);

    /* end-of-file checks */
    if (in_comment)
        report_error(comment_line, "unterminated comment%s", "", 0);
    check_semicolon(lineno + 1, -1, "");
    while (sp > 0) {
        sp--;
        char m[2] = { stack_ch[sp], '\0' };
        report_error(stack_line[sp], "'%s' opened here is never closed", m, 0);
    }

    printf("\n==================== SUMMARY ====================\n");
    int total = 0;
    for (int k = 0; k < C_COUNT; k++) {
        if (k == C_ERROR) continue;
        printf("%-26s %d\n", cat_names[k], counts[k]);
        total += counts[k];
    }
    printf("%-26s %d\n", "TOTAL TOKENS", total);
    printf("%-26s %d\n", "ERRORS", error_count);
    printf(error_count ? "\nResult: lexical/syntax problems found.\n"
                       : "\nResult: no errors found.\n");
    return error_count ? 2 : 0;
}
