#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_CAPACITY 1024
#define GAP_SIZE         512

typedef struct {
    char *buffer;
    int   gap_start;
    int   gap_end;
    int   capacity;
    int   length;
} GapBuffer;

GapBuffer *create_buffer(int cap)
{
    if (cap <= 0) cap = INITIAL_CAPACITY;
    GapBuffer *gb = malloc(sizeof(GapBuffer));
    if (!gb) { perror("Fatal: malloc GapBuffer"); exit(1); }
    gb->buffer = malloc((size_t)cap);
    if (!gb->buffer) { perror("Fatal: malloc buffer"); exit(1); }
    gb->capacity  = cap;
    gb->gap_start = 0;
    gb->gap_end   = cap;
    gb->length    = 0;
    return gb;
}

static void grow_buffer(GapBuffer *gb)
{
    int new_cap     = gb->capacity * 2 + GAP_SIZE;
    int post_len    = gb->capacity - gb->gap_end;
    int new_gap_end = new_cap - post_len;
    char *new_buf   = malloc((size_t)new_cap);
    if (!new_buf) { perror("Fatal: out of memory"); exit(1); }
    memcpy(new_buf,               gb->buffer,              (size_t)gb->gap_start);
    memcpy(new_buf + new_gap_end, gb->buffer + gb->gap_end, (size_t)post_len);
    free(gb->buffer);
    gb->buffer   = new_buf;
    gb->gap_end  = new_gap_end;
    gb->capacity = new_cap;
}

static void move_gap(GapBuffer *gb, int target)
{
    if (target < 0)          target = 0;
    if (target > gb->length) target = gb->length;
    if (target < gb->gap_start) {
        int delta = gb->gap_start - target;
        memmove(gb->buffer + gb->gap_end - delta, gb->buffer + target, (size_t)delta);
        gb->gap_start -= delta;
        gb->gap_end   -= delta;
    } else if (target > gb->gap_start) {
        int delta = target - gb->gap_start;
        memmove(gb->buffer + gb->gap_start, gb->buffer + gb->gap_end, (size_t)delta);
        gb->gap_start += delta;
        gb->gap_end   += delta;
    }
}

static void insert_char(GapBuffer *gb, char c)
{
    if (gb->gap_start == gb->gap_end) grow_buffer(gb);
    gb->buffer[gb->gap_start++] = c;
    gb->length++;
}

/* Gap-adjusted logical character read */
static char buf_char(const GapBuffer *gb, int pos)
{
    if (pos < 0 || pos >= gb->length) return 0;
    if (pos < gb->gap_start) return gb->buffer[pos];
    return gb->buffer[gb->gap_end + (pos - gb->gap_start)];
}

static void auto_indent(GapBuffer *gb)
{
    if (gb->gap_start == 0) return;
    int scan = gb->gap_start - 1;
    if (scan >= 0 && buf_char(gb, scan) == '\n') scan--;

    while (scan >= 0) {
        int line_start = scan;
        while (line_start > 0 && buf_char(gb, line_start - 1) != '\n') line_start--;

        int has_content = 0;
        for (int i = line_start; i <= scan; i++) {
            char c = buf_char(gb, i);
            if (c != ' ' && c != '\t') { has_content = 1; break; }
        }
        if (has_content) {
            for (int i = line_start; i <= scan; i++) {
                char c = buf_char(gb, i);
                if (c != ' ' && c != '\t') break;
                insert_char(gb, c);
            }
            return;
        }
        scan = line_start - 1;
    }
}

static int find_line_offset(const GapBuffer *gb, int target_line)
{
    if (target_line <= 1) return 0;
    int line = 1;
    for (int i = 0; i < gb->gap_start; i++) {
        if (gb->buffer[i] == '\n') { line++; if (line == target_line) return i + 1; }
    }
    for (int i = gb->gap_end; i < gb->capacity; i++) {
        if (gb->buffer[i] == '\n') {
            line++;
            if (line == target_line) return gb->gap_start + (i + 1 - gb->gap_end);
        }
    }
    return gb->length;
}

static void load_file(const char *filename, GapBuffer *gb)
{
    FILE *f = fopen(filename, "r");
    if (!f) return;
    int ch;
    while ((ch = fgetc(f)) != EOF) insert_char(gb, (char)ch);
    fclose(f);
}

static void save_file(const char *filename, const GapBuffer *gb)
{
    FILE *out = fopen(filename, "w");
    if (!out) { perror("Error saving file"); return; }
    fwrite(gb->buffer,               1, (size_t)gb->gap_start,             out);
    fwrite(gb->buffer + gb->gap_end, 1, (size_t)(gb->capacity - gb->gap_end), out);
    fclose(out);
}

static void refresh_screen(const char *filename, const GapBuffer *gb)
{
    printf("\033[2J\033[H");
    double usage = (gb->capacity > 0) ? ((double)gb->length / gb->capacity) * 100.0 : 0.0;
    printf("--- STATUS [%s] | Used: %d/%d bytes (%.1f%%) ---\n", filename, gb->length, gb->capacity, usage);
    printf("--- :m [L] | :d [L] | :d *[L] | :d *x y* | :t | :n | :w | ESVA ---\n\n");

    int line = 1;
    printf("%2d: ", line);
    for (int i = 0; i < gb->gap_start; i++) {
        putchar(gb->buffer[i]);
        if (gb->buffer[i] == '\n') printf("%2d: ", ++line);
    }
    printf("\033[7m|\033[0m");
    for (int i = gb->gap_end; i < gb->capacity; i++) {
        putchar(gb->buffer[i]);
        if (gb->buffer[i] == '\n') printf("%2d: ", ++line);
    }
    putchar('\n');
    fflush(stdout);
}

static int parse_int(const char *s, int *out)
{
    if (!s || !*s) return 0;
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || v < 0) return 0;
    *out = (int)v;
    return 1;
}

int main(void)
{
    char filename[256];
    char line[1024];
    GapBuffer *gb = create_buffer(INITIAL_CAPACITY);

    printf("Enter filename: ");
    fflush(stdout);
    if (scanf("%255s", filename) != 1) { free(gb->buffer); free(gb); return 1; }
    getchar();

    load_file(filename, gb);

    while (1) {
        refresh_screen(filename, gb);
        if (!fgets(line, sizeof(line), stdin)) break;

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';

        if (strcmp(line, "ESVA") == 0) break;

        if (strcmp(line, ":w") == 0) { save_file(filename, gb); continue; }

        if (strncmp(line, ":m ", 3) == 0) {
            int target = 1;
            if (parse_int(line + 3, &target)) move_gap(gb, find_line_offset(gb, target));
            continue;
        }

        if (strncmp(line, ":d ", 3) == 0) {
            const char *cmd = line + 3;
            if (*cmd == '*') {
                int x = -1, y = -1;
                if (sscanf(cmd + 1, "%d %d", &x, &y) == 2 && x >= 1 && y >= x) {
                    int start = find_line_offset(gb, x);
                    int end   = find_line_offset(gb, y + 1);
                    if (end > start) { move_gap(gb, start); gb->gap_end += (end - start); gb->length -= (end - start); }
                } else if (x >= 1) {
                    int start   = find_line_offset(gb, x);
                    int deleted = gb->length - start;
                    move_gap(gb, start);
                    gb->gap_end = gb->capacity;
                    gb->length -= deleted;
                }
            } else {
                int target = -1;
                if (parse_int(cmd, &target) && target >= 1) {
                    move_gap(gb, find_line_offset(gb, target));
                    while (gb->gap_end < gb->capacity) {
                        char c = gb->buffer[gb->gap_end++];
                        gb->length--;
                        if (c == '\n') break;
                    }
                }
            }
            continue;
        }

        if (strcmp(line, ":t") == 0) { insert_char(gb, '\t'); continue; }

        if (strcmp(line, ":n") == 0) { insert_char(gb, '\n'); auto_indent(gb); continue; }

        for (size_t i = 0; i < len; i++) insert_char(gb, line[i]);
        insert_char(gb, '\n');
        if (len > 0) auto_indent(gb);
    }

    save_file(filename, gb);
    free(gb->buffer);
    free(gb);
    return 0;
}
