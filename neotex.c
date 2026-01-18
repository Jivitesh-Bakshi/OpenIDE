#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//neotex is an extremely simple, lightweight CLi first Text editor.

#define INITIAL_CAPACITY 1024
#define GAP_SIZE 512

typedef struct {
    char *buffer;
    int gap_start;
    int gap_end;
    int capacity;
    int length;
} GapBuffer;

GapBuffer* create_buffer(int cap) {
    if (cap <= 0) cap = INITIAL_CAPACITY;

    GapBuffer *gb = malloc(sizeof(GapBuffer));
    if (!gb) { perror("Fatal: malloc GapBuffer"); exit(1); }

    gb->buffer = malloc(cap);
    if (!gb->buffer) { perror("Fatal: malloc buffer"); exit(1); }

    gb->capacity = cap;
    gb->gap_start = 0;
    gb->gap_end = cap;
    gb->length = 0;

    return gb;
}

void grow_buffer(GapBuffer *gb) {
    int new_cap = gb->capacity + GAP_SIZE;
    char *new_buf = malloc(new_cap);
    if (!new_buf) {
        perror("Fatal: Out of memory");
        exit(1);
    }

    int post_len = gb->capacity - gb->gap_end;
    int new_gap_end = new_cap - post_len;

    memcpy(new_buf, gb->buffer, gb->gap_start);
    memcpy(new_buf + new_gap_end,
           gb->buffer + gb->gap_end,
           post_len);

    free(gb->buffer);
    gb->buffer = new_buf;
    gb->gap_end = new_gap_end;
    gb->capacity = new_cap;
}

void move_gap(GapBuffer *gb, int target) {
    if (target < 0) target = 0;
    if (target > gb->length) target = gb->length;

    if (target < gb->gap_start) {
        int delta = gb->gap_start - target;
        memmove(gb->buffer + gb->gap_end - delta,
                gb->buffer + target,
                delta);
        gb->gap_start = target;
        gb->gap_end -= delta;
    }
    else if (target > gb->gap_start) {
        int delta = target - gb->gap_start;
        memmove(gb->buffer + gb->gap_start,
                gb->buffer + gb->gap_end,
                delta);
        gb->gap_start += delta;
        gb->gap_end += delta;
    }
}

void insert_char(GapBuffer *gb, char c) {
    if (gb->gap_start == gb->gap_end) grow_buffer(gb);
    gb->buffer[gb->gap_start++] = c;
    gb->length++;
}

void insert_string(GapBuffer *gb, const char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++)
        insert_char(gb, str[i]);
}

// Auto-indent: copy leading spaces/tabs from previous line
// Auto-indent: copy leading spaces/tabs from last non-empty line above
void auto_indent(GapBuffer *gb) {
    if (gb->gap_start == 0) return;

    int pos = gb->gap_start - 1;
    int last_nonempty_line_start = 0;

    // Step 1: Scan backward to find the last non-empty line
    while (pos >= 0) {
        // Find start of current line
        int line_start = pos;
        while (line_start >= 0 && gb->buffer[line_start] != '\n') line_start--;
        line_start++; // move to first char of the line

        // Check if line has non-whitespace characters
        int has_content = 0;
        for (int i = line_start; i <= pos; i++) {
            char c;
            if (i < gb->gap_start) c = gb->buffer[i];                  // before gap
            else if (i >= gb->gap_end) c = gb->buffer[i - (gb->gap_end - gb->gap_start)]; // after gap
            else continue; // inside gap, skip

            if (c != ' ' && c != '\t') {
                has_content = 1;
                break;
            }
        }

        if (has_content) {
            last_nonempty_line_start = line_start;
            break;
        }

        pos = line_start - 2; // move to the previous line
    }

    // Step 2: Copy leading spaces/tabs from that line
    int copy_pos = last_nonempty_line_start;
    while (1) {
        char c;
        if (copy_pos < gb->gap_start) c = gb->buffer[copy_pos];
        else if (copy_pos >= gb->gap_end) c = gb->buffer[copy_pos - (gb->gap_end - gb->gap_start)];
        else break; // inside gap

        if (c != ' ' && c != '\t') break; // stop at first non-whitespace
        insert_char(gb, c);
        copy_pos++;
    }
}

int find_line_offset(GapBuffer *gb, int target_line) {
    if (target_line <= 1) return 0;

    int line = 1;
    int pos = 0;

    while (pos < gb->gap_start) {
        if (gb->buffer[pos++] == '\n' && ++line == target_line)
            return pos;
    }

    int post = gb->gap_end;
    while (post < gb->capacity) {
        if (gb->buffer[post++] == '\n' && ++line == target_line)
            return gb->gap_start + (post - gb->gap_end);
    }

    return gb->length;
}

void save_file(const char *filename, GapBuffer *gb) {
    FILE *out = fopen(filename, "w");
    if (!out) {
        perror("Error saving file");
        return;
    }

    fwrite(gb->buffer, 1, gb->gap_start, out);
    fwrite(gb->buffer + gb->gap_end,
           1, gb->capacity - gb->gap_end, out);
    fclose(out);
}

void refresh_screen(const char* filename, GapBuffer *gb) {
    printf("\033[2J\033[H");

    double usage = (gb->capacity > 0)
        ? ((double)gb->length / gb->capacity) * 100
        : 0;

    printf("--- STATUS [%s] | Used: %d/%d bytes (%.1f%%) ---\n",
           filename, gb->length, gb->capacity, usage);
    printf("--- :m [L] | :d [L] | :d *[L] | :d *x y* | :t | :n | :w | ESVA ---\n\n");

    int line = 1;
    printf("%2d: ", line++);

    for (int i = 0; i < gb->gap_start; i++) {
        putchar(gb->buffer[i]);
        if (gb->buffer[i] == '\n')
            printf("%2d: ", line++);
    }

    printf("\033[7m|\033[0m");

    for (int i = gb->gap_end; i < gb->capacity; i++) {
        putchar(gb->buffer[i]);
        if (gb->buffer[i] == '\n')
            printf("%2d: ", line++);
    }

    printf("\n");
}

int main() {
    char filename[100], line[1024];
    GapBuffer *gb = create_buffer(INITIAL_CAPACITY);

    printf("Enter filename: ");
    if (scanf("%99s", filename) != 1) return 1;
    getchar();

    FILE *f = fopen(filename, "r");
    if (f) {
        int ch;
        while ((ch = fgetc(f)) != EOF) {
            insert_char(gb, (char)ch);
        }
        fclose(f);
    }

    while (1) {
        refresh_screen(filename, gb);
        if (!fgets(line, sizeof(line), stdin)) break;

        // Remove trailing newline for command detection
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = 0;

        // Exit
        if (strcmp(line, "ESVA") == 0)
            break;

        // Save
        if (strcmp(line, ":w") == 0) {
            save_file(filename, gb);
            continue;
        }

        // Move line
        if (strncmp(line, ":m ", 3) == 0) {
            move_gap(gb, find_line_offset(gb, atoi(line + 3)));
            continue;
        }

        // Delete line(s)
        if (strncmp(line, ":d ", 3) == 0) {
            char *cmd = line + 3;

            if (*cmd == '*') {
                int x, y;
                if (sscanf(cmd + 1, "%d %d", &x, &y) == 2 && x <= y) {
                    int start = find_line_offset(gb, x);
                    int end   = find_line_offset(gb, y + 1);
                    move_gap(gb, start);
                    gb->length -= (end - start);
                    gb->gap_end += (end - start);
                }
                else {
                    move_gap(gb, find_line_offset(gb, atoi(cmd + 1)));
                    gb->length = gb->gap_start;
                    gb->gap_end = gb->capacity;
                }
            }
            else {
                move_gap(gb, find_line_offset(gb, atoi(cmd)));
                if (gb->gap_end < gb->capacity) {
                    while (gb->gap_end < gb->capacity) {
                        char c = gb->buffer[gb->gap_end++];
                        gb->length--;
                        if (c == '\n') break;
                    }
                }
            }
            continue;
        }

        // Insert indentation
        if (strcmp(line, ":t") == 0) {
            insert_char(gb, '\t');
            continue;
        }

        // Move to next line with auto-indent
        if (strcmp(line, ":n") == 0) {
            insert_char(gb, '\n');
            auto_indent(gb);
            continue;
        }

        // Insert line with handling Enter and Tab
        for (size_t i = 0; i < strlen(line); i++) {
            if (line[i] == '\t') insert_char(gb, '\t');
            else insert_char(gb, line[i]);
        }
        insert_char(gb, '\n'); // simulate Enter after every fgets line
        auto_indent(gb);
    }

    save_file(filename, gb);
    free(gb->buffer);
    free(gb);
    return 0;
}
