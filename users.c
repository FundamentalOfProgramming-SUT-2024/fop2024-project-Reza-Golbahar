#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include "users.h"

// File handling functions
void handle_file_error(const char* operation) {
    clear();
    printw("Error: Could not %s file.\n", operation);
    printw("Press any key to continue...");
    refresh();
    getch();
}

// Password validation functions
int hasupper(const char password[]) {
    size_t len = strlen(password);
    for (size_t i = 0; i < len; i++) {
        if (password[i] >= 65 && password[i] <= 90)
            return 1;
    }
    return 0;
}

int haslower(const char password[]) {
    size_t len = strlen(password);
    for (size_t i = 0; i < len; i++) {
        if (password[i] >= 97 && password[i] <= 122)
            return 1;
    }
    return 0;
}

int hasdigit(const char password[]) {
    size_t len = strlen(password);
    for (size_t i = 0; i < len; i++) {
        if (password[i] >= 48 && password[i] <= 57)
            return 1;
    }
    return 0;
}

bool validate_password(char *password) {
    if (strlen(password) < 7) {
        printw("Password must be at least 7 characters long\n");
        return false;
    }
    if (!hasupper(password)) {
        printw("Password must contain at least one uppercase letter\n");
        return false;
    }
    if (!haslower(password)) {
        printw("Password must contain at least one lowercase letter\n");
        return false;
    }
    if (!hasdigit(password)) {
        printw("Password must contain at least one number\n");
        return false;
    }
    return true;
}

bool validate_email(const char *email) {
    const char *at = strchr(email, '@');
    if (!at) return false;
    
    const char *dot = strchr(at, '.');
    if (!dot || dot == at + 1 || *(dot + 1) == '\0') {
        printw("Invalid email format. Must be xxxx@yyy.zzz\n");
        return false;
    }
    return true;
}

// User management functions
struct UserManager* create_user_manager(void) {
    struct UserManager* manager = malloc(sizeof(struct UserManager));
    if (manager != NULL) {
        manager->user_count = 0;
        manager->current_user = NULL;
        memset(manager->users, 0, sizeof(struct User) * MAX_USERS);
        load_users_from_json(manager);
    }
    return manager;
}

void free_user_manager(struct UserManager* manager) {
    if (manager != NULL) {
        free(manager);
    }
}

void load_users_from_json(struct UserManager* manager) {
    FILE* file = fopen("users.json", "r");
    if (file == NULL) {
        manager->user_count = 0;
        return;
    }

    char line[256];
    manager->user_count = 0;

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "\"username\":") != NULL) {
            struct User* user = &manager->users[manager->user_count];
            
            sscanf(line, " \"username\": \"%[^\"]\",", user->username);
            fgets(line, sizeof(line), file);
            sscanf(line, " \"password\": \"%[^\"]\",", user->password);
            fgets(line, sizeof(line), file);
            sscanf(line, " \"email\": \"%[^\"]\",", user->email);
            fgets(line, sizeof(line), file);
            char score_str[10];
            sscanf(line, " \"score\": \"%[^\"]\"", score_str);
            user->score = atoi(score_str);

            strncpy(manager->usernames[manager->user_count], user->username, MAX_STRING_LEN - 1);
            manager->usernames[manager->user_count][MAX_STRING_LEN - 1] = '\0';
            
            manager->user_count++;
        }
    }
    fclose(file);
}

void save_users_to_json(struct UserManager* manager) {
    FILE* file = fopen("users.json", "w");
    if (file == NULL) {
        handle_file_error("write");
        return;
    }

    fprintf(file, "[\n");
    for (int i = 0; i < manager->user_count; i++) {
        fprintf(file, "  {\n");
        fprintf(file, "    \"username\": \"%s\",\n", manager->users[i].username);
        fprintf(file, "    \"password\": \"%s\",\n", manager->users[i].password);
        fprintf(file, "    \"email\": \"%s\",\n", manager->users[i].email);
        fprintf(file, "    \"score\": \"%d\"%s\n", manager->users[i].score,
                i < manager->user_count - 1 ? "," : "");
        fprintf(file, "  }%s\n", i < manager->user_count - 1 ? "," : "");
    }
    fprintf(file, "]\n");
    fclose(file);
}


bool authenticate_user(struct UserManager* manager, int index, const char* password) {
    if (index < 0 || index >= manager->user_count) {
        return false;
    }
    
    if (strcmp(manager->users[index].password, password) == 0) {
        manager->current_user = &manager->users[index];
        return true;
    }
    return false;
}

void print_users(struct UserManager* manager) {
    if (manager->user_count == 0) {
        printw("No users found.\n");
        return;
    }

    mvprintw(2, 0, "Usernames");
    mvprintw(2, 30, "Emails");
    mvprintw(2, 60, "Score");

    for (int i = 0; i < manager->user_count; i++) {
        mvprintw(i + 3, 0, "%d-%s", i + 1, manager->users[i].username);
        mvprintw(i + 3, 30, "%s", manager->users[i].email);
        mvprintw(i + 3, 60, "%d", manager->users[i].score);
    }
}

void print_scoreboard(struct UserManager* manager) {
    if (manager->user_count == 0) {
        printw("No users found.\n");
        return;
    }

    // Create temporary array for sorting
    struct User sorted_users[MAX_USERS];
    memcpy(sorted_users, manager->users, sizeof(struct User) * manager->user_count);

    // Sort users by score in descending order
    for (int i = 0; i < manager->user_count - 1; i++) {
        for (int j = i + 1; j < manager->user_count; j++) {
            if (sorted_users[j].score > sorted_users[i].score) {
                struct User temp = sorted_users[i];
                sorted_users[i] = sorted_users[j];
                sorted_users[j] = temp;
            }
        }
    }

    // Print scoreboard
    clear();
    mvprintw(0, 0, "SCOREBOARD");
    mvprintw(2, 0, "Rank");
    mvprintw(2, 10, "Username");
    mvprintw(2, 35, "Score");
    
    // Print horizontal line
    mvprintw(3, 0, "----------------------------------------");

    for (int i = 0; i < manager->user_count; i++) {
        mvprintw(i + 4, 0, "%d", i + 1);
        mvprintw(i + 4, 10, "%s", sorted_users[i].username);
        mvprintw(i + 4, 35, "%d", sorted_users[i].score);
    }

    // Print bottom line
    mvprintw(manager->user_count + 4, 0, "----------------------------------------");
    mvprintw(manager->user_count + 6, 0, "Press any key to continue...");
    refresh();
    getch();
}

void print_profile(struct UserManager* manager) {
    if (manager->current_user == NULL) {
        printw("No user is currently logged in.\n");
        refresh();
        getch();
        return;
    }

    clear();
    int line_num = 0;  // Add this line
    // Print user profile
    mvprintw(0, 0, "User Profile");
    mvprintw(2, 0, "----------------------------------------");
    mvprintw(3, 0, "Username: %s", manager->current_user->username);
    mvprintw(4, 0, "Email: %s", manager->current_user->email);
    mvprintw(5, 0, "Current Score: %d", manager->current_user->score);
    mvprintw(6, 0, "----------------------------------------");
    
    line_num = 7;  // Start settings from line 7
    // Load game settings if they exist
    FILE *settings_file = fopen("load.json", "r");
    if (settings_file != NULL) {
        mvprintw(line_num++, 0, "Current Settings:");
        char settings_line[256];
        while (fgets(settings_line, sizeof(settings_line), settings_file)) {
            mvprintw(line_num++, 0, "%s", settings_line);
        }
        fclose(settings_file);
    } else {
        handle_file_error("open settings");
    }

    mvprintw(line_num + 1, 0, "Press any key to return...");
    refresh();
    getch();
}