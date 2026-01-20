#include <ncurses.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_ENTRIES 1000
#define MAX_PATH 4096
#define MAX_SEARCH_RESULTS 100

typedef struct {
    char name[256];
    char path[MAX_PATH];
    int is_dir;
    off_t size;
} Entry;

typedef struct {
    char display[512];
    char path[MAX_PATH];
    int type; // 0=folder, 1=filename, 2=content match
    int is_dir;
} SearchResult;

Entry entries[MAX_ENTRIES];
int entry_count = 0;
int selected = 0;
int scroll_offset = 0;
char current_dir[MAX_PATH];

SearchResult search_results[MAX_SEARCH_RESULTS];
int search_result_count = 0;
int search_selected = 0;
int search_scroll = 0;

void format_size(off_t size, char *buf) {
    if (size < 1024) sprintf(buf, "%ldB", size);
    else if (size < 1024*1024) sprintf(buf, "%.1fK", size/1024.0);
    else if (size < 1024*1024*1024) sprintf(buf, "%.1fM", size/(1024.0*1024));
    else sprintf(buf, "%.2fG", size/(1024.0*1024*1024));
}

int compare_entries(const void *a, const void *b) {
    Entry *ea = (Entry*)a, *eb = (Entry*)b;
    if (ea->is_dir && !eb->is_dir) return -1;
    if (!ea->is_dir && eb->is_dir) return 1;
    return strcmp(ea->name, eb->name);
}

void load_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;

    entry_count = 0;
    struct dirent *ent;

    // Add parent directory
    if (strcmp(path, "/") != 0) {
        Entry *e = &entries[entry_count];
        strcpy(e->name, "..");
        snprintf(e->path, MAX_PATH, "%s/..", path);
        e->is_dir = 1;
        entry_count++;
    }

    // Add "New File" option
    Entry *new_e = &entries[entry_count];
    strcpy(new_e->name, "[+ New File]");
    strcpy(new_e->path, "");
    new_e->is_dir = 0;
    new_e->size = 0;
    entry_count++;

    while ((ent = readdir(dir)) && entry_count < MAX_ENTRIES) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (ent->d_name[0] == '.') continue;

        Entry *e = &entries[entry_count];
        strncpy(e->name, ent->d_name, 255);
        snprintf(e->path, MAX_PATH, "%s/%s", path, ent->d_name);

        struct stat st;
        if (stat(e->path, &st) == 0) {
            e->is_dir = S_ISDIR(st.st_mode);
            e->size = st.st_size;
            entry_count++;
        }
    }
    closedir(dir);

    if (entry_count > 2) {
        int start = (strcmp(entries[0].name, "..") == 0) ? 2 : 1;
        qsort(entries + start, entry_count - start, sizeof(Entry), compare_entries);
    }

    selected = 0;
    scroll_offset = 0;
}

void draw_ui() {
    int height, width;
    getmaxyx(stdscr, height, width);
    clear();

    // Header
    attron(COLOR_PAIR(1) | A_BOLD);
    mvhline(0, 0, ' ', width);
    char *dir_name = strrchr(current_dir, '/');
    dir_name = dir_name ? dir_name + 1 : current_dir;
    if (strlen(dir_name) == 0) dir_name = "/";
    mvprintw(0, 2, "[\\] %s", dir_name);
    attroff(COLOR_PAIR(1) | A_BOLD);

    // File list
    int list_height = height - 3;
    for (int i = scroll_offset; i < scroll_offset + list_height && i < entry_count; i++) {
        int y = i - scroll_offset + 1;
        Entry *e = &entries[i];

        if (i == selected) {
            attron(COLOR_PAIR(2) | A_REVERSE);
            mvhline(y, 0, ' ', width);
            attroff(COLOR_PAIR(2) | A_REVERSE);
        }

        // Draw tree line
        if (strcmp(e->name, "..") == 0) {
            mvprintw(y, 2, "+- [\\] %s", e->name);
        } else if (strcmp(e->name, "[+ New File]") == 0) {
            attron(COLOR_PAIR(3) | A_BOLD);
            mvprintw(y, 2, "+- %s", e->name);
            attroff(COLOR_PAIR(3) | A_BOLD);
        } else {
            int is_last = (i == entry_count - 1);
            if (is_last) {
                mvprintw(y, 2, "`- %s %s", e->is_dir ? "[\\]" : "[~]", e->name);
            } else {
                mvprintw(y, 2, "|- %s %s", e->is_dir ? "[\\]" : "[~]", e->name);
            }
        }

        // Size/type indicator
        if (!e->is_dir && strcmp(e->name, "[+ New File]") != 0) {
            char size_str[20];
            format_size(e->size, size_str);
            attron(COLOR_PAIR(3));
            mvprintw(y, width - 12, "%10s", size_str);
            attroff(COLOR_PAIR(3));
        } else if (e->is_dir) {
            attron(COLOR_PAIR(4));
            mvprintw(y, width - 12, "    <DIR>");
            attroff(COLOR_PAIR(4));
        }
    }

    // Footer
    attron(COLOR_PAIR(1));
    mvhline(height-1, 0, ' ', width);
    mvprintw(height-1, 2, "Enter:Open | Ctrl+D:Del | /:Search | Backspace:Back | q:Quit");
    attroff(COLOR_PAIR(1));

    refresh();
}

void navigate_to(const char *path) {
    char resolved[MAX_PATH];
    if (realpath(path, resolved)) {
        strcpy(current_dir, resolved);
        load_directory(current_dir);
    }
}

void open_file(const char *path) {
    endwin();
    char cmd[MAX_PATH + 20];

    if (system("which micro > /dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "micro '%s'", path);
    } else if (system("which nano > /dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "nano '%s'", path);
    } else {
        snprintf(cmd, sizeof(cmd), "vi '%s'", path);
    }

    system(cmd);

    // Reinitialize ncurses after editor closes
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    refresh();
}

void create_new_file() {
    int height, width;
    getmaxyx(stdscr, height, width);

    WINDOW *win = newwin(7, 60, (height - 7) / 2, (width - 60) / 2);

    box(win, 0, 0);
    wattron(win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(win, 1, 2, "CREATE NEW FILE");
    wattroff(win, COLOR_PAIR(1) | A_BOLD);

    mvwprintw(win, 3, 2, "Enter filename: ");
    wrefresh(win);

    echo();
    curs_set(1);

    char filename[256];
    mvwgetnstr(win, 3, 18, filename, 255);

    noecho();
    curs_set(0);
    delwin(win);

    if (strlen(filename) > 0) {
        char filepath[MAX_PATH];
        snprintf(filepath, MAX_PATH, "%s/%s", current_dir, filename);

        FILE *f = fopen(filepath, "w");
        if (f) {
            fclose(f);
            load_directory(current_dir);

            for (int i = 0; i < entry_count; i++) {
                if (strcmp(entries[i].name, filename) == 0) {
                    selected = i;
                    break;
                }
            }
        }
    }

    clear();
}

void delete_entry(Entry *e) {
    int height, width;
    getmaxyx(stdscr, height, width);

    WINDOW *win = newwin(7, 60, (height - 7) / 2, (width - 60) / 2);

    box(win, 0, 0);
    wattron(win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(win, 1, 2, "DELETE CONFIRMATION");
    wattroff(win, COLOR_PAIR(1) | A_BOLD);

    mvwprintw(win, 3, 2, "Delete: %s", e->name);
    if (e->is_dir) {
        wattron(win, A_BOLD);
        mvwprintw(win, 4, 2, "WARNING: Entire directory will be deleted!");
        wattroff(win, A_BOLD);
    }
    mvwprintw(win, 5, 2, "Press 'y' to confirm, any other key to cancel");

    wrefresh(win);

    int ch = wgetch(win);
    delwin(win);

    if (ch == 'y' || ch == 'Y') {
        char cmd[MAX_PATH + 30];
        if (e->is_dir) {
            snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null", e->path);
        } else {
            snprintf(cmd, sizeof(cmd), "rm '%s' 2>/dev/null", e->path);
        }
        system(cmd);

        load_directory(current_dir);
        if (selected >= entry_count) selected = entry_count - 1;
        if (selected < 0) selected = 0;
    }

    clear();
}

int case_insensitive_strstr(const char *haystack, const char *needle) {
    if (!*needle) return 1;

    int needle_len = strlen(needle);
    int haystack_len = strlen(haystack);

    for (int i = 0; i <= haystack_len - needle_len; i++) {
        int match = 1;
        for (int j = 0; j < needle_len; j++) {
            if (tolower(haystack[i + j]) != tolower(needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

void search_in_file(const char *filepath, const char *query, const char *display_path) {
    if (search_result_count >= MAX_SEARCH_RESULTS) return;

    FILE *f = fopen(filepath, "r");
    if (!f) return;

    char line[1024];
    int line_num = 1;
    int found = 0;

    while (fgets(line, sizeof(line), f) && !found) {
        if (case_insensitive_strstr(line, query)) {
            SearchResult *r = &search_results[search_result_count++];
            r->type = 2; // content match
            r->is_dir = 0;

            // Trim line
            int len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

            // Truncate if too long
            if (strlen(line) > 60) {
                line[57] = '.';
                line[58] = '.';
                line[59] = '.';
                line[60] = '\0';
            }

            snprintf(r->display, sizeof(r->display), "[~] %s:%d: %s", display_path, line_num, line);
            strncpy(r->path, filepath, MAX_PATH);
            found = 1;
        }
        line_num++;
    }

    fclose(f);
}

void recursive_search(const char *base_path, const char *query, int max_depth, int current_depth) {
    if (current_depth > max_depth || search_result_count >= MAX_SEARCH_RESULTS) return;

    DIR *dir = opendir(base_path);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) && search_result_count < MAX_SEARCH_RESULTS) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (ent->d_name[0] == '.') continue;

        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s/%s", base_path, ent->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        int is_dir = S_ISDIR(st.st_mode);

        // Check if name matches
        if (case_insensitive_strstr(ent->d_name, query)) {
            SearchResult *r = &search_results[search_result_count++];
            r->type = is_dir ? 0 : 1;
            r->is_dir = is_dir;

            // Make path relative to current_dir
            char *rel_path = full_path + strlen(current_dir);
            if (*rel_path == '/') rel_path++;

            if (is_dir) {
                snprintf(r->display, sizeof(r->display), "[\\] %s", rel_path);
            } else {
                snprintf(r->display, sizeof(r->display), "[~] %s", rel_path);
            }
            strncpy(r->path, full_path, MAX_PATH);
        }

        // Recurse into directories
        if (is_dir) {
            recursive_search(full_path, query, max_depth, current_depth + 1);
        }
        // Search file contents for non-directories
        else if (!is_dir && st.st_size < 1024 * 1024) { // Only search files < 1MB
            char *rel_path = full_path + strlen(current_dir);
            if (*rel_path == '/') rel_path++;
            search_in_file(full_path, query, rel_path);
        }
    }

    closedir(dir);
}

int compare_search_results(const void *a, const void *b) {
    SearchResult *ra = (SearchResult*)a;
    SearchResult *rb = (SearchResult*)b;

    if (ra->type != rb->type) return ra->type - rb->type;
    return strcmp(ra->display, rb->display);
}

void perform_search(const char *query) {
    search_result_count = 0;
    search_selected = 0;
    search_scroll = 0;

    if (strlen(query) == 0) return;

    recursive_search(current_dir, query, 3, 0); // max depth 3

    // Sort: folders first, then files, then content
    qsort(search_results, search_result_count, sizeof(SearchResult), compare_search_results);
}

void show_search_ui() {
    int height, width;
    getmaxyx(stdscr, height, width);

    int win_height = height - 6;
    int win_width = width - 10;
    if (win_width > 100) win_width = 100;

    WINDOW *win = newwin(win_height, win_width, 3, (width - win_width) / 2);

    char query[256] = "";
    int query_len = 0;

    int running = 1;

    while (running) {
        // Perform search
        perform_search(query);

        // Draw window
        werase(win);
        box(win, 0, 0);

        wattron(win, COLOR_PAIR(1) | A_BOLD);
        mvwprintw(win, 0, 2, " SEARCH ");
        wattroff(win, COLOR_PAIR(1) | A_BOLD);

        // Search input
        wattron(win, COLOR_PAIR(3) | A_BOLD);
        mvwprintw(win, 1, 2, "> %s", query);
        wattroff(win, COLOR_PAIR(3) | A_BOLD);

        // Separator
        mvwhline(win, 2, 1, ACS_HLINE, win_width - 2);

        // Results
        int result_height = win_height - 5;

        if (search_result_count == 0 && strlen(query) > 0) {
            wattron(win, COLOR_PAIR(3));
            mvwprintw(win, 4, 2, "No results found");
            wattroff(win, COLOR_PAIR(3));
        } else if (strlen(query) == 0) {
            wattron(win, COLOR_PAIR(3));
            mvwprintw(win, 4, 2, "Type to search...");
            wattroff(win, COLOR_PAIR(3));
        } else {
            for (int i = search_scroll; i < search_scroll + result_height && i < search_result_count; i++) {
                int y = i - search_scroll + 3;
                SearchResult *r = &search_results[i];

                if (i == search_selected) {
                    wattron(win, A_REVERSE);
                }

                // Color based on type
                if (r->type == 0) { // folder
                    wattron(win, COLOR_PAIR(4));
                } else if (r->type == 1) { // filename
                    wattron(win, COLOR_PAIR(3));
                }

                mvwprintw(win, y, 2, "%-*.*s", win_width - 4, win_width - 4, r->display);

                if (r->type == 0) {
                    wattroff(win, COLOR_PAIR(4));
                } else if (r->type == 1) {
                    wattroff(win, COLOR_PAIR(3));
                }

                if (i == search_selected) {
                    wattroff(win, A_REVERSE);
                }
            }
        }

        // Footer
        wattron(win, COLOR_PAIR(1));
        mvwhline(win, win_height - 2, 1, ' ', win_width - 2);
        mvwprintw(win, win_height - 2, 2, "Enter:Open | ESC:Close | Results:%d", search_result_count);
        wattroff(win, COLOR_PAIR(1));

        wrefresh(win);

        int ch = wgetch(win);

        switch(ch) {
            case 27: // ESC
                running = 0;
                break;

            case KEY_BACKSPACE:
            case 127:
            case 8:
                if (query_len > 0) {
                    query[--query_len] = '\0';
                    search_selected = 0;
                    search_scroll = 0;
                }
                break;

            case KEY_UP:
                if (search_selected > 0) {
                    search_selected--;
                    if (search_selected < search_scroll) {
                        search_scroll = search_selected;
                    }
                }
                break;

            case KEY_DOWN:
                if (search_selected < search_result_count - 1) {
                    search_selected++;
                    if (search_selected >= search_scroll + result_height) {
                        search_scroll = search_selected - result_height + 1;
                    }
                }
                break;

            case 10:
            case 13: // Enter
                if (search_result_count > 0) {
                    SearchResult *r = &search_results[search_selected];
                    running = 0;
                    delwin(win);
                    clear();
                    refresh();

                    if (r->is_dir) {
                        navigate_to(r->path);
                    } else {
                        open_file(r->path);
                        load_directory(current_dir); // Reload in case file was modified
                    }
                    return;
                }
                break;

            default:
                if (ch >= 32 && ch < 127 && query_len < 255) {
                    query[query_len++] = ch;
                    query[query_len] = '\0';
                    search_selected = 0;
                    search_scroll = 0;
                }
                break;
        }
    }

    delwin(win);
    clear();
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        strcpy(current_dir, argv[1]);
    } else {
        getcwd(current_dir, MAX_PATH);
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_CYAN, COLOR_BLACK);
    init_pair(4, COLOR_GREEN, COLOR_BLACK);

    load_directory(current_dir);

    int running = 1;
    while (running) {
        draw_ui();
        int ch = getch();
        int height = getmaxy(stdscr) - 3;

        switch(ch) {
            case 'q':
            case 'Q':
                running = 0;
                break;

            case '/':
                show_search_ui();
                break;

            case KEY_UP:
            case 'k':
                if (selected > 0) {
                    selected--;
                    if (selected < scroll_offset) scroll_offset = selected;
                }
                break;

            case KEY_DOWN:
            case 'j':
                if (selected < entry_count - 1) {
                    selected++;
                    if (selected >= scroll_offset + height) scroll_offset = selected - height + 1;
                }
                break;

            case 10:
            case 13:
                if (entry_count > 0) {
                    if (strcmp(entries[selected].name, "[+ New File]") == 0) {
                        create_new_file();
                    } else if (entries[selected].is_dir) {
                        navigate_to(entries[selected].path);
                    } else {
                        open_file(entries[selected].path);
                    }
                }
                break;

            case KEY_BACKSPACE:
            case 127:
            case 8:
                if (strcmp(current_dir, "/") != 0) {
                    navigate_to("..");
                }
                break;

            case 4:
                if (entry_count > 0 && strcmp(entries[selected].name, "..") != 0
                    && strcmp(entries[selected].name, "[+ New File]") != 0) {
                    delete_entry(&entries[selected]);
                }
                break;
        }
    }

    endwin();
    return 0;
}
