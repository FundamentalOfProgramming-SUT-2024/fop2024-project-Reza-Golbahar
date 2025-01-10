#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>
#include "game.h"
//#include "users.h"

#define DOOR '+'
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define MIN_ROOM_COUNT 6
#define MIN_WIDTH_OR_LENGTH 4
#define MAX_POINTS 100
#define MAX_ROOMS 10
#define MAP_WIDTH 80
#define MAP_HEIGHT 24

void game_menu(struct UserManager* manager) {
    while (1) {
        clear();
        mvprintw(0, 0, "Game Menu");
        mvprintw(2, 0, "1. New Game (n)");
        mvprintw(3, 0, "2. Load Game (l)");
        mvprintw(4, 0, "3. View Scoreboard (s)");
        mvprintw(5, 0, "4. Settings (t)");
        mvprintw(6, 0, "5. Profile (p)");
        mvprintw(7, 0, "6. Back to Main Menu (q)");
        
        if (manager->current_user == NULL) {
            mvprintw(9, 0, "Note: Save/Load features unavailable for guest users");
        }
        
        refresh();

        int choice = getch();
        
        switch (choice) {
            case 'n': {
                // Start new game
                struct Map game_map = generate_map();
                struct Point character_location = game_map.initial_position;
                play_game(manager, &game_map, &character_location, 0);
                break;
            }
            
            case 'l': {
                if (manager->current_user) {
                    struct SavedGame saved_game;
                    if (load_saved_game(manager, &saved_game)) {
                        play_game(manager, &saved_game.game_map, 
                                &saved_game.character_location, saved_game.score);
                    }
                } else {
                    mvprintw(11, 0, "Guest users cannot load games");
                    refresh();
                    getch();
                }
                break;
            }
            
            case 's':
                print_scoreboard(manager);
                break;
                
            case 't':
                settings(manager);
                break;
                
            case 'p':
                if (manager->current_user) {
                    print_profile(manager);
                } else {
                    mvprintw(11, 0, "Guest users cannot view profile");
                    refresh();
                    getch();
                }
                break;
                
            case 'q':
                return;
        }
    }
}

void play_game(struct UserManager* manager, struct Map* game_map, 
               struct Point* character_location, int initial_score) {
    // Print the map for debugging (initial state)
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            printw("%c", game_map->grid[y][x]);
        }
        printw("\n");
    }
    refresh();

    bool game_running = true;
    int score = initial_score;

    // Initialize visibility array (all tiles hidden initially)
    bool visible[MAP_HEIGHT][MAP_WIDTH] = {0};
    visible[character_location->y][character_location->x] = 1; // Starting position is visible

    while (game_running) {
        clear();

        // Update visibility: Update the visibility array based on the character's position
        update_visibility(game_map, character_location, visible);

        // Print the map: Use print_map to render the game map with visibility applied
        print_map(game_map, visible, *character_location);

        // Show game info
        mvprintw(MAP_HEIGHT + 1, 0, "Score: %d", score);
        if (manager->current_user) {
            mvprintw(MAP_HEIGHT + 2, 0, "Player: %s", manager->current_user->username);
        } else {
            mvprintw(MAP_HEIGHT + 2, 0, "Player: Guest");
        }
        mvprintw(MAP_HEIGHT + 3, 0, "Use arrow keys to move, 'q' to quit");
        refresh();

        // Handle input
        int key = getch();
        switch (key) {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_LEFT:
            case KEY_RIGHT: {
                // Move the character and update visibility
                struct Point new_location = *character_location;
                move_character(&new_location, key, game_map);

                // Check if the new position is valid
                if (game_map->grid[new_location.y][new_location.x] != WALL_HORIZONTAL &&
                    game_map->grid[new_location.y][new_location.x] != WALL_VERTICAL) {
                    *character_location = new_location;
                    visible[character_location->y][character_location->x] = 1; // Reveal tile
                }
                break;
            }
            case 'q':
                game_running = false;
                break;
        }

        // Check for game over conditions (e.g., player reaching the STAIRS)
        if (game_map->grid[character_location->y][character_location->x] == STAIRS) {
            clear();
            mvprintw(0, 0, "You found the stairs! Level completed.");
            refresh();
            getch();

            // Handle level completion (save score for the user)
            if (manager->current_user) {
                manager->current_user->score += score;
                save_users_to_json(manager);
            }
            game_running = false;
        }
    }
}


// Implementation of print_point
void print_point(struct Point p, const char* type) {
    if (p.y < 0 || p.x < 0 || p.y >= MAP_HEIGHT || p.x >= MAP_WIDTH) return;

    char symbol;
    if (strcmp(type, "wall") == 0) symbol = '#';
    else if (strcmp(type, "floor") == 0) symbol = '.';
    else if (strcmp(type, "trap") == 0) symbol = '!';
    else symbol = ' ';

    mvaddch(p.y, p.x, symbol);
}

void sight_range(struct Map* game_map, struct Point* character_location) {
    // Arrays to track visited and currently visible areas
    static char visited[MAP_HEIGHT][MAP_WIDTH] = {0};
    static char visible[MAP_HEIGHT][MAP_WIDTH] = {0};

    // Reset visibility for the new frame
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            visible[y][x] = 0;
        }
    }

    // Mark the current position as visited
    visited[character_location->y][character_location->x] = 1;

    // Find if the player is in a room
    struct Room* current_room = NULL;
    for (int i = 0; i < game_map->room_count; i++) {
        if (isPointInRoom(character_location, &game_map->rooms[i])) {
            current_room = &game_map->rooms[i];
            break;
        }
    }

    if (current_room != NULL) {
        // Player is in a room - reveal the entire current room
        showRoom(game_map, current_room, visible);

        // Reveal corridors and adjacent rooms that are connected to this room
        for (int y = current_room->top_wall; y <= current_room->bottom_wall; y++) {
            for (int x = current_room->left_wall; x <= current_room->right_wall; x++) {
                if (game_map->grid[y][x] == CORRIDOR) {
                    // Mark corridor as visible
                    visible[y][x] = 1;

                    // Check for adjacent rooms connected to this corridor
                    for (int i = 0; i < game_map->room_count; i++) {
                        struct Room* adjacent_room = &game_map->rooms[i];
                        if (adjacent_room != current_room && isPointInRoom(&(struct Point){x, y}, adjacent_room)) {
                            showRoom(game_map, adjacent_room, visible);
                        }
                    }
                }
            }
        }
    } else {
        // Player is in a corridor or open area - show within sight range
        for (int dy = -SIGHT_RANGE; dy <= SIGHT_RANGE; dy++) {
            for (int dx = -SIGHT_RANGE; dx <= SIGHT_RANGE; dx++) {
                int new_y = character_location->y + dy;
                int new_x = character_location->x + dx;

                // Check bounds
                if (new_y >= 0 && new_y < MAP_HEIGHT && new_x >= 0 && new_x < MAP_WIDTH) {
                    // Show if within range or previously visited
                    if (visited[new_y][new_x] ||
                        manhattanDistance(character_location->x, character_location->y, new_x, new_y) <= SIGHT_RANGE) {
                        visible[new_y][new_x] = 1;
                    }
                }
            }
        }
    }

    // Display the map with visibility
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (visible[y][x]) {
                // Show actual map content
                mvaddch(y, x, game_map->grid[y][x]);
            } else {
                // Show fog of war for unexplored areas
                mvaddch(y, x, ' ');
            }
        }
    }
}


// Add this after room generation in generate_map
void add_food(struct Map* game_map) {
    const int FOOD_COUNT = 10;  // Adjust as needed
    int food_placed = 0;
    
    while (food_placed < FOOD_COUNT) {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;
        
        // Place food only on floor tiles
        if (game_map->grid[y][x] == FLOOR) {
            game_map->grid[y][x] = FOOD;
            food_placed++;
        }
    }
}


// Helper function for corridor visibility
void showCorridorVisibility(struct Map* game_map, int x, int y) {
    // Use SIGHT_RANGE directly from game.h
    for (int dy = -SIGHT_RANGE; dy <= SIGHT_RANGE; dy++) {
        for (int dx = -SIGHT_RANGE; dx <= SIGHT_RANGE; dx++) {
            int new_y = y + dy;
            int new_x = x + dx;
            
            if (new_y >= 0 && new_y < MAP_HEIGHT && new_x >= 0 && new_x < MAP_WIDTH) {
                if (abs(dx) + abs(dy) <= SIGHT_RANGE) {
                    game_map->visibility[new_y][new_x] = true;
                }
            }
        }
    }
}

void print_map(struct Map* game_map, bool visible[MAP_HEIGHT][MAP_WIDTH], struct Point character_location) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char display_char;

            if (x == character_location.x && y == character_location.y) {
                display_char = '@'; // Player character
            } else if (visible[y][x]) {
                display_char = game_map->grid[y][x]; // Visible tiles
            } else if (game_map->discovered[y][x]) {
                display_char = game_map->grid[y][x]; // Previously discovered tiles
            } else {
                display_char = FOG; // Unexplored tiles
            }

            mvaddch(y, x, display_char);
        }
    }
    refresh();
}

void create_corridors(struct Map* game_map, Room* rooms, int room_count) {
    bool connected[MAX_ROOMS] = { false };
    connected[0] = true;  // The first room is connected

    for (int count = 1; count < room_count; count++) {
        int best_src = -1;
        int best_dst = -1;
        int min_distance = INT_MAX;

        // Find the closest connection between a connected and unconnected room
        for (int i = 0; i < room_count; i++) {
            if (!connected[i]) continue;

            for (int j = 0; j < room_count; j++) {
                if (connected[j]) continue;

                int distance = abs(rooms[i].x - rooms[j].x) + abs(rooms[i].y - rooms[j].y);
                if (distance < min_distance) {
                    min_distance = distance;
                    best_src = i;
                    best_dst = j;
                }
            }
        }

        if (best_src != -1 && best_dst != -1) {
            connected[best_dst] = true;

            // Draw corridor between rooms
            int start_x = rooms[best_src].x + rooms[best_src].width / 2;
            int start_y = rooms[best_src].y + rooms[best_src].height / 2;
            int end_x = rooms[best_dst].x + rooms[best_dst].width / 2;
            int end_y = rooms[best_dst].y + rooms[best_dst].height / 2;

            // Draw L-shaped corridor
            if (rand() % 2 == 0) {
                // Horizontal first, then vertical
                int current_x = start_x;
                while (current_x != end_x) {
                    if (game_map->grid[start_y][current_x] == ' ') {
                        game_map->grid[start_y][current_x] = '=';  // Corridor
                    }
                    current_x += (current_x < end_x) ? 1 : -1;
                }

                int current_y = start_y;
                while (current_y != end_y) {
                    if (game_map->grid[current_y][end_x] == ' ') {
                        game_map->grid[current_y][end_x] = '=';  // Corridor
                    }
                    current_y += (current_y < end_y) ? 1 : -1;
                }
            } else {
                // Vertical first, then horizontal
                int current_y = start_y;
                while (current_y != end_y) {
                    if (game_map->grid[current_y][start_x] == ' ') {
                        game_map->grid[current_y][start_x] = '=';  // Corridor
                    }
                    current_y += (current_y < end_y) ? 1 : -1;
                }

                int current_x = start_x;
                while (current_x != end_x) {
                    if (game_map->grid[end_y][current_x] == ' ') {
                        game_map->grid[end_y][current_x] = '=';  // Corridor
                    }
                    current_x += (current_x < end_x) ? 1 : -1;
                }
            }
        }
    }
}



void init_map(struct Map* map) {
    // Initialize empty map
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map->grid[y][x] = FOG;
            map->visibility[y][x] = false;
            map->discovered[y][x] = false;
        }
    }
}

bool rooms_overlap(const struct Room* r1, const struct Room* r2) {
    return !(r1->right_wall + MIN_ROOM_DISTANCE < r2->left_wall ||
             r2->right_wall + MIN_ROOM_DISTANCE < r1->left_wall ||
             r1->top_wall + MIN_ROOM_DISTANCE < r2->bottom_wall ||
             r2->top_wall + MIN_ROOM_DISTANCE < r1->bottom_wall);
}

struct Map generate_map(void) {
    struct Map map;
    init_map(&map);

    // Generate rooms
    map.room_count = 0;
    int num_rooms = MIN_ROOMS + rand() % (MAX_ROOMS - MIN_ROOMS + 1);

    // Generate rooms
    for (int i = 0; i < num_rooms; i++) {
        struct Room room;
        int attempts = 0;
        bool placed = false;

        while (!placed && attempts < 100) {
            room.width = MIN_ROOM_SIZE + rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE + 1);
            room.height = MIN_ROOM_SIZE + rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE + 1);
            room.left_wall = 1 + rand() % (MAP_WIDTH - room.width - 2);
            room.top_wall = 1 + rand() % (MAP_HEIGHT - room.height - 2);
            room.right_wall = room.left_wall + room.width;
            room.bottom_wall = room.top_wall + room.height;
            room.door_count = 0;
            room.has_stairs = false;
            room.visited = false;

            if (is_valid_room_placement(&map, &room)) {
                map.rooms[map.room_count++] = room;
                place_room(&map, &room);
                placed = true;
            }
            attempts++;
        }
    }

    // Ensure that the stairs are placed in a valid room (make sure it is accessible)
    place_stairs(&map); // Assuming `place_stairs` ensures the stairs are placed somewhere valid

    // Call the create_corridors function to connect all rooms with corridors
    create_corridors(&map, map.rooms, map.room_count);

    // Set initial player position in the first room
    if (map.room_count > 0) {
        map.initial_position.x = map.rooms[0].left_wall + map.rooms[0].width / 2;
        map.initial_position.y = map.rooms[0].top_wall + map.rooms[0].height / 2;
    }

    return map;
}



void message(const char* text) {
    mvprintw(MAP_HEIGHT, 0, "%s", text);
    refresh();
}

void move_character(struct Point* character_location, int key, struct Map* game_map) {
    struct Point new_location = *character_location;

    // Determine new position based on key input
    switch (key) {
        case KEY_UP:    new_location.y--; break;
        case KEY_DOWN:  new_location.y++; break;
        case KEY_LEFT:  new_location.x--; break;
        case KEY_RIGHT: new_location.x++; break;
        default: return; // Ignore invalid keys
    }

    // Validate new position
    if (new_location.x >= 0 && new_location.x < MAP_WIDTH &&
        new_location.y >= 0 && new_location.y < MAP_HEIGHT &&
        (game_map->grid[new_location.y][new_location.x] == FLOOR || 
         game_map->grid[new_location.y][new_location.x] == CORRIDOR || 
         game_map->grid[new_location.y][new_location.x] == DOOR)) {
        // Update position
        *character_location = new_location;
    }
}

bool isPointInRoom(struct Point* point, struct Room* room) {
    return (point->x > room->left_wall && point->x < room->right_wall &&
            point->y > room->bottom_wall && point->y < room->top_wall);
}

void showRoom(struct Map* game_map, struct Room* room, char visible[MAP_HEIGHT][MAP_WIDTH]) {
    for (int y = room->bottom_wall; y <= room->top_wall; y++) {
        for (int x = room->left_wall; x <= room->right_wall; x++) {
            if (y >= 0 && y < MAP_HEIGHT && x >= 0 && x < MAP_WIDTH) {
                visible[y][x] = 1;
            }
        }
    }
}

bool canSeeRoomThroughWindow(struct Map* game_map, struct Room* room1, struct Room* room2) {
    bool share_horizontal = (room1->top_wall == room2->bottom_wall - 1) ||
                          (room1->bottom_wall == room2->top_wall + 1);

    bool share_vertical = (room1->right_wall == room2->left_wall - 1) ||
                         (room1->left_wall == room2->right_wall + 1);
    
    // Check for windows on shared walls
    if (share_horizontal) {
        for (int x = MAX(room1->left_wall, room2->left_wall); 
             x <= MIN(room1->right_wall, room2->right_wall); x++) {
            if (game_map->grid[room1->top_wall][x] == WINDOW ||
                game_map->grid[room1->bottom_wall][x] == WINDOW) {
                return true;
            }
        }
    }
    
    if (share_vertical) {
        for (int y = MAX(room1->bottom_wall, room2->bottom_wall);
             y <= MIN(room1->top_wall, room2->top_wall); y++) {
            if (game_map->grid[y][room1->right_wall] == WINDOW ||
                game_map->grid[y][room1->left_wall] == WINDOW) {
                return true;
            }
        }
    }
    
    return false;
}

int manhattanDistance(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

void add_game_message(struct MessageQueue* queue, const char* text, int color_pair) {
    if (queue->count >= MAX_MESSAGES) {
        // Shift messages up
        for (int i = 0; i < MAX_MESSAGES - 1; i++) {
            queue->messages[i] = queue->messages[i + 1];
        }
        queue->count--;
    }
    
    struct GameMessage* msg = &queue->messages[queue->count];
    strncpy(msg->text, text, MESSAGE_LENGTH - 1);
    msg->text[MESSAGE_LENGTH - 1] = '\0';
    msg->time_to_live = 60;  // Show for ~2 seconds at 30 fps
    msg->color_pair = color_pair;
    queue->count++;
}

void update_messages(struct MessageQueue* queue) {
    for (int i = 0; i < queue->count; i++) {
        queue->messages[i].time_to_live--;
        if (queue->messages[i].time_to_live <= 0) {
            // Remove this message
            for (int j = i; j < queue->count - 1; j++) {
                queue->messages[j] = queue->messages[j + 1];
            }
            queue->count--;
            i--;
        }
    }
}

void draw_messages(struct MessageQueue* queue, int start_y, int start_x) {
    for (int i = 0; i < queue->count; i++) {
        attron(COLOR_PAIR(queue->messages[i].color_pair));
        mvprintw(start_y + i, start_x, "%s", queue->messages[i].text);
        attroff(COLOR_PAIR(queue->messages[i].color_pair));
    }
}


bool is_valid_room_placement(struct Map* map, struct Room* room) {
    // Check map boundaries
    if (room->left_wall < 1 || room->right_wall >= MAP_WIDTH-1 ||
        room->top_wall < 1 || room->bottom_wall >= MAP_HEIGHT-1) {
        return false;
    }
    
    // Check overlap with existing rooms
    for (int i = 0; i < map->room_count; i++) {
        struct Room* other = &map->rooms[i];
        if (room->left_wall - MIN_ROOM_DISTANCE <= other->right_wall &&
            room->right_wall + MIN_ROOM_DISTANCE >= other->left_wall &&
            room->top_wall - MIN_ROOM_DISTANCE <= other->bottom_wall &&
            room->bottom_wall + MIN_ROOM_DISTANCE >= other->top_wall) {
            return false;
        }
    }
    
    return true;
}

void place_room(struct Map* map, struct Room* room) {
    // Place floor
    for (int y = room->top_wall + 1; y < room->bottom_wall; y++) {
        for (int x = room->left_wall + 1; x < room->right_wall; x++) {
            map->grid[y][x] = FLOOR;
        }
    }
    
    // Place walls
    for (int x = room->left_wall; x <= room->right_wall; x++) {
        map->grid[room->top_wall][x] = WALL_HORIZONTAL;
        map->grid[room->bottom_wall][x] = WALL_HORIZONTAL;
    }
    for (int y = room->top_wall; y <= room->bottom_wall; y++) {
        map->grid[y][room->left_wall] = WALL_VERTICAL;
        map->grid[y][room->right_wall] = WALL_VERTICAL;
    }
    
    // Place pillars and windows
    place_pillars(map, room);
    place_windows(map, room);
}

void update_visibility(struct Map* game_map, struct Point* player_pos, bool visible[MAP_HEIGHT][MAP_WIDTH]) {
    int view_range = 5; // Player's sight range

    // Reset visibility
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            visible[y][x] = false;
        }
    }

    // Mark the current tile as visible and discovered
    visible[player_pos->y][player_pos->x] = true;
    game_map->discovered[player_pos->y][player_pos->x] = true;

    // Update visibility within the view range
    for (int dy = -view_range; dy <= view_range; dy++) {
        for (int dx = -view_range; dx <= view_range; dx++) {
            int tx = player_pos->x + dx;
            int ty = player_pos->y + dy;

            if (tx >= 0 && tx < MAP_WIDTH && ty >= 0 && ty < MAP_HEIGHT) {
                if (game_map->grid[ty][tx] != FOG) {
                    visible[ty][tx] = true;
                    game_map->discovered[ty][tx] = true; // Mark as discovered
                }
            }
        }
    }
}



void place_stairs(struct Map* map) {
    // Place stairs in a random room that doesn't already have stairs
    while (1) {
        int room_index = rand() % map->room_count;
        struct Room* room = &map->rooms[room_index];
        
        if (!room->has_stairs) {
            // Place stairs randomly within the room
            int x = room->left_wall + 1 + rand() % (room->width - 2);
            int y = room->top_wall + 1 + rand() % (room->height - 2);
            
            map->grid[y][x] = STAIRS;
            room->has_stairs = true;
            map->stairs_location.x = x;
            map->stairs_location.y = y;
            break;
        }
    }
}

void place_pillars(struct Map* map, struct Room* room) {
    // Place pillars in corners if room is large enough
    if (room->width >= 6 && room->height >= 6) {
        // Top-left pillar
        map->grid[room->top_wall + 2][room->left_wall + 2] = PILLAR;
        
        // Top-right pillar
        map->grid[room->top_wall + 2][room->right_wall - 2] = PILLAR;
        
        // Bottom-left pillar
        map->grid[room->bottom_wall - 2][room->left_wall + 2] = PILLAR;
        
        // Bottom-right pillar
        map->grid[room->bottom_wall - 2][room->right_wall - 2] = PILLAR;
    }
}

bool can_see_room(struct Map* map, struct Room* room1, struct Room* room2) {
    // Check if rooms are connected through a window
    if (canSeeRoomThroughWindow(map, room1, room2)) {
        return true;
    }

    // Check if rooms are close enough and have line of sight
    int room1_center_x = (room1->left_wall + room1->right_wall) / 2;
    int room1_center_y = (room1->top_wall + room1->bottom_wall) / 2;
    int room2_center_x = (room2->left_wall + room2->right_wall) / 2;
    int room2_center_y = (room2->top_wall + room2->bottom_wall) / 2;

    // Check if rooms are within sight range
    if (abs(room1_center_x - room2_center_x) + abs(room1_center_y - room2_center_y) > SIGHT_RANGE * 2) {
        return false;
    }

    // Simple line of sight check
    int dx = room2_center_x - room1_center_x;
    int dy = room2_center_y - room1_center_y;
    int steps = MAX(abs(dx), abs(dy));

    if (steps == 0) return true;

    float x_inc = (float)dx / steps;
    float y_inc = (float)dy / steps;

    float x = room1_center_x;
    float y = room1_center_y;

    for (int i = 0; i < steps; i++) {
        x += x_inc;
        y += y_inc;

        int check_x = (int)x;
        int check_y = (int)y;

        // Check if line of sight is blocked by any wall or obstruction
        if (map->grid[check_y][check_x] == WALL_VERTICAL || 
            map->grid[check_y][check_x] == WALL_HORIZONTAL) {
            return false;
        }
    }

    return true;
}


// Add validation for staircase placement
bool validate_stair_placement(struct Map* map) {
    int stair_count = 0;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map->grid[y][x] == STAIRS) stair_count++;
        }
    }
    return stair_count == 1;
}

/**
 * Creates a safe filename for save games.
 * Maximum combined length of username and savename must be less than
 * MAX_FILENAME_LEN - 12 characters (accounting for "saves/", "_", and ".sav")
 * 
 * @return false if the resulting filename would be too long
 */

bool create_safe_filename(char* dest, size_t dest_size, const char* username, const char* savename) {
    // Calculate required space
    size_t required = strlen("saves/") + strlen(username) + 1 + strlen(savename) + 
                     strlen(".sav") + 1;
    
    if (required > dest_size) {
        return false;
    }
    
    snprintf(dest, dest_size, "saves/%s_%s.sav", username, savename);
    return true;
}

void save_current_game(struct UserManager* manager, struct Map* game_map, 
                      struct Point* character_location, int score) {
    if (!manager->current_user) {
        mvprintw(0, 0, "Cannot save game as guest user.");
        refresh();
        getch();
        return;
    }

    char filename[256]; // Increased buffer size to prevent truncation
    char save_name[100];

    clear();
    echo();
    mvprintw(0, 0, "Enter name for this save: ");
    refresh();
    getnstr(save_name, sizeof(save_name) - 1); // Limit input to fit within the buffer
    noecho();

    // Truncate username and save_name if they exceed a safe length
    char safe_username[94]; // Leave space for prefix, underscore, extension
    char safe_save_name[94];
    strncpy(safe_username, manager->current_user->username, 93);
    safe_username[93] = '\0';
    strncpy(safe_save_name, save_name, 93);
    safe_save_name[93] = '\0';

    // Create filename using truncated strings
    int written = snprintf(filename, sizeof(filename), "saves/%s_%s.sav", safe_username, safe_save_name);
    if (written < 0 || written >= sizeof(filename)) {
        mvprintw(2, 0, "Error: Filename too long.");
        refresh();
        getch();
        return;
    }

    FILE* file = fopen(filename, "wb");
    if (!file) {
        mvprintw(2, 0, "Error: Could not create save file.");
        refresh();
        getch();
        return;
    }

    struct SavedGame save = {
        .game_map = *game_map,
        .character_location = *character_location,
        .score = score,
        .save_time = time(NULL)
    };
    strncpy(save.name, save_name, sizeof(save.name) - 1);

    fwrite(&save, sizeof(struct SavedGame), 1, file);
    fclose(file);

    mvprintw(2, 0, "Game saved successfully!");
    refresh();
    getch();
}

bool load_saved_game(struct UserManager* manager, struct SavedGame* saved_game) {
    if (!manager->current_user) {
        mvprintw(0, 0, "Cannot load game as guest user.");
        refresh();
        getch();
        return false;
    }

    clear();
    mvprintw(0, 0, "Enter save name to load: ");
    refresh();

    char save_name[100];
    echo();
    getnstr(save_name, sizeof(save_name) - 1); // Limit input to fit within buffer
    noecho();

    char safe_username[94];
    char safe_save_name[94];
    strncpy(safe_username, manager->current_user->username, 93);
    safe_username[93] = '\0';
    strncpy(safe_save_name, save_name, 93);
    safe_save_name[93] = '\0';

    char filename[256];
    int written = snprintf(filename, sizeof(filename), "saves/%s_%s.sav", safe_username, safe_save_name);
    if (written < 0 || written >= sizeof(filename)) {
        mvprintw(2, 0, "Error: Filename too long.");
        refresh();
        getch();
        return false;
    }

    FILE* file = fopen(filename, "rb");
    if (!file) {
        mvprintw(2, 0, "Error: Could not find save file.");
        refresh();
        getch();
        return false;
    }

    fread(saved_game, sizeof(struct SavedGame), 1, file);
    fclose(file);
    return true;
}


void list_saved_games(struct UserManager* manager) {
    if (!manager->current_user) return;

    char cmd[MAX_STRING_LEN * 3];
    snprintf(cmd, sizeof(cmd), "ls saves/%s_*.sav 2>/dev/null", 
             manager->current_user->username);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return;

    mvprintw(0, 0, "Available saved games:");
    int line = 2;
    char buffer[256];
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        // Extract save name from filename
        char* start = strrchr(buffer, '_') + 1;
        char* end = strrchr(buffer, '.');
        if (start && end) {
            *end = '\0';
            mvprintw(line++, 0, "%s", start);
        }
    }
    
    pclose(pipe);
    refresh();
}

void place_windows(struct Map* map, struct Room* room) {
    // Place windows on room walls with a certain chance
    for (int y = room->top_wall + 1; y < room->bottom_wall; y++) {
        if (rand() % 100 < WINDOW_CHANCE) {
            map->grid[y][room->left_wall] = WINDOW;
        }
        if (rand() % 100 < WINDOW_CHANCE) {
            map->grid[y][room->right_wall] = WINDOW;
        }
    }
    
    for (int x = room->left_wall + 1; x < room->right_wall; x++) {
        if (rand() % 100 < WINDOW_CHANCE) {
            map->grid[room->top_wall][x] = WINDOW;
        }
        if (rand() % 100 < WINDOW_CHANCE) {
            map->grid[room->bottom_wall][x] = WINDOW;
        }
    }
}

