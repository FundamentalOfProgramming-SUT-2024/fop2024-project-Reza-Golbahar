#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>
#include "game.h"
#include "users.h"

#define DOOR '+'
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define MIN_ROOM_COUNT 6
#define MIN_WIDTH_OR_LENGTH 4
#define MAX_POINTS 100
#define MAX_ROOMS 10
#define MAP_WIDTH 80
#define MAP_HEIGHT 24



#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "users.h"  // Add this before game.h
#include "game.h"

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
    bool game_running = true;
    int score = initial_score;

    printw("Game running");

    // Initialize visibility array (all tiles hidden initially)
    bool visible[MAP_HEIGHT][MAP_WIDTH] = {0}; 
    visible[character_location->y][character_location->x] = 1; // Start position is visible

    // Set visibility of rooms: Optionally, we can reveal entire rooms if we want.
    update_visibility(game_map, character_location); // Initially update visibility.

    while (game_running) {
        clear();

        // Debugging line to ensure the game is running
        printf("Game running...\n");

        // Update visibility: Update the visibility array based on the character's position
        update_visibility(game_map, character_location);

        // Print the map with visibility status
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
            case KEY_RIGHT:
                // Move the character
                move_character(character_location, key, game_map);

                // After moving, update visibility for the new position
                visible[character_location->y][character_location->x] = 1;

                break;
            case 'q':
                game_running = false;
                break;
        }

        // Check for game over conditions (e.g., player reaching the STAIRS)
        if (game_map->grid[character_location->y][character_location->x] == STAIRS) {
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
    
    // Reset visibility for new frame
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            visible[y][x] = 0;
        }
    }

    // Mark current position as visited
    visited[character_location->y][character_location->x] = 1;

    // Find if player is in a room
    struct Room* current_room = NULL;
    for (int i = 0; i < game_map->room_count; i++) {
        if (isPointInRoom(character_location, &game_map->rooms[i])) {
            current_room = &game_map->rooms[i];
            break;
        }
    }

    if (current_room != NULL) {
        // Player is in a room - show entire current room
        showRoom(game_map, current_room, visible);

        // Show connected rooms that have been visited
        for (int i = 0; i < game_map->room_count; i++) {
            struct Room* other_room = &game_map->rooms[i];
            if (other_room != current_room) {
                // Check if rooms are connected by door
                if (hasConnectingDoor(game_map, current_room, other_room)) {
                    if (visited[other_room->bottom_wall][other_room->left_wall]) {
                        showRoom(game_map, other_room, visible);
                    }
                }
                // Check if rooms are connected by window
                else if (canSeeRoomThroughWindow(game_map, current_room, other_room)) {
                    if (visited[other_room->bottom_wall][other_room->left_wall]) {
                        showRoom(game_map, other_room, visible);
                    }
                }
            }
        }
    } else {
        // Show corridor within sight range
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

    // Helper function to check if a point is visible
    bool isVisible(int x, int y) {
        return (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT && visible[y][x]);
    }

    // Display the map with visibility
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (isVisible(x, y)) {
                // Show actual map content
                mvaddch(y, x, game_map->grid[y][x]);
            } else {
                // Show fog of war for unexplored areas
                printw(" ");
            }
        }
        printw("\n");
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

// Helper function to check if two rooms have a connecting door
bool hasConnectingDoor(struct Map* game_map, struct Room* room1, struct Room* room2) {
    // Check shared walls for doors
    if (room1->right_wall == room2->left_wall - 1 || room1->left_wall == room2->right_wall + 1) {
        // Vertical wall - check for doors
        int min_y = MAX(room1->bottom_wall, room2->bottom_wall);
        int max_y = MIN(room1->top_wall, room2->top_wall);
        for (int y = min_y; y <= max_y; y++) {
            if (game_map->grid[y][room1->right_wall] == DOOR ||
                game_map->grid[y][room1->left_wall] == DOOR) {
                return true;
            }
        }
    }
    
    if (room1->top_wall == room2->bottom_wall - 1 || room1->bottom_wall == room2->top_wall + 1) {
        // Horizontal wall - check for doors
        int min_x = MAX(room1->left_wall, room2->left_wall);
        int max_x = MIN(room1->right_wall, room2->right_wall);
        for (int x = min_x; x <= max_x; x++) {
            if (game_map->grid[room1->top_wall][x] == DOOR ||
                game_map->grid[room1->bottom_wall][x] == DOOR) {
                return true;
            }
        }
    }
    
    return false;
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



// Connect two points with a corridor
void connect_doors(struct Map* game_map, struct Point door1, struct Point door2) {
    int x1 = door1.x;
    int y1 = door1.y;
    int x2 = door2.x;
    int y2 = door2.y;
    
    // Create an L-shaped corridor
    // First move horizontally
    int x_dir = (x2 > x1) ? 1 : -1;
    for (int x = x1; x != x2; x += x_dir) {
        if (game_map->grid[y1][x] == ' ') {
            game_map->grid[y1][x] = '#';
        }
    }
    
    // Then move vertically
    int y_dir = (y2 > y1) ? 1 : -1;
    for (int y = y1; y != y2 + y_dir; y += y_dir) {
        if (game_map->grid[y][x2] == ' ') {
            game_map->grid[y][x2] = '#';
        }
    }
}

void create_corridor(struct Map* game_map, struct Room* room1, struct Room* room2) {
    // Get the centers of both rooms for better corridor placement
    int room1_center_x = room1->left_wall + (room1->right_wall - room1->left_wall) / 2;
    int room1_center_y = room1->top_wall + (room1->bottom_wall - room1->top_wall) / 2;
    int room2_center_x = room2->left_wall + (room2->right_wall - room2->left_wall) / 2;
    int room2_center_y = room2->top_wall + (room2->bottom_wall - room2->top_wall) / 2;

    struct Point door1 = { room1_center_x, room1_center_y };
    struct Point door2 = { room2_center_x, room2_center_y };

    // Check door limits (no more than MAX_DOORS in each room)
    if (room1->door_count >= MAX_DOORS || room2->door_count >= MAX_DOORS) {
        return;
    }

    // Add doors to the rooms
    room1->doors[room1->door_count++] = door1;
    room2->doors[room2->door_count++] = door2;

    // Place doors on the map
    game_map->grid[door1.y][door1.x] = DOOR;
    game_map->grid[door2.y][door2.x] = DOOR;

    // Create the corridor: Either a straight line or L-shaped corridor
    int x = door1.x;
    int y = door1.y;

    // Horizontal first or vertical first based on proximity
    bool horizontal_corridor = abs(door1.x - door2.x) > abs(door1.y - door2.y);

    // Ensure that corridor stays within map bounds
    int start_x = (x < 0) ? 0 : (x >= MAP_WIDTH ? MAP_WIDTH - 1 : x);
    int start_y = (y < 0) ? 0 : (y >= MAP_HEIGHT ? MAP_HEIGHT - 1 : y);
    int end_x = (door2.x < 0) ? 0 : (door2.x >= MAP_WIDTH ? MAP_WIDTH - 1 : door2.x);
    int end_y = (door2.y < 0) ? 0 : (door2.y >= MAP_HEIGHT ? MAP_HEIGHT - 1 : door2.y);

    // Create straight or L-shaped corridor
    if (horizontal_corridor) {
        // Move horizontally first
        while (x != door2.x) {
            if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
                game_map->grid[y][x] = CORRIDOR;
            }
            x += (x < door2.x) ? 1 : -1;
        }
        // Then vertically
        while (y != door2.y) {
            if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
                game_map->grid[y][x] = CORRIDOR;
            }
            y += (y < door2.y) ? 1 : -1;
        }
    } else {
        // Move vertically first
        while (y != door2.y) {
            if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
                game_map->grid[y][x] = CORRIDOR;
            }
            y += (y < door2.y) ? 1 : -1;
        }
        // Then horizontally
        while (x != door2.x) {
            if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
                game_map->grid[y][x] = CORRIDOR;
            }
            x += (x < door2.x) ? 1 : -1;
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

    // Connect rooms with corridors
    connect_rooms(&map); // This function should ensure rooms are connected

    // Ensure rooms are accessible (a path exists between all rooms)
    while (!validate_room_accessibility(&map)) {
        connect_rooms(&map); // If not accessible, reconnect rooms
    }

    // Ensure that the stairs are placed in a valid room (make sure it is accessible)
    place_stairs(&map); // Assuming `place_stairs` ensures the stairs are placed somewhere valid

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

bool areRoomsConnectable(struct Room* room1, struct Room* room2) {
    // Check if rooms are close enough to connect (within reasonable corridor length)
    const int MAX_CORRIDOR_LENGTH = 10;
    
    int min_distance = INT_MAX;
    
    // Check distances between room corners
    int distances[] = {
        manhattanDistance(room1->right_wall, room1->top_wall, room2->left_wall, room2->bottom_wall),
        manhattanDistance(room1->right_wall, room1->bottom_wall, room2->left_wall, room2->top_wall),
        manhattanDistance(room1->left_wall, room1->top_wall, room2->right_wall, room2->bottom_wall),
        manhattanDistance(room1->left_wall, room1->bottom_wall, room2->right_wall, room2->top_wall)
    };
    
    for (int i = 0; i < 4; i++) {
        if (distances[i] < min_distance) {
            min_distance = distances[i];
        }
    }
    
    return min_distance <= MAX_CORRIDOR_LENGTH;
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

void update_visibility(struct Map* map, struct Point* player_pos) {
    // Reset visibility
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map->visibility[y][x] = false;
        }
    }
    
    // Find current room
    struct Room* current_room = NULL;
    for (int i = 0; i < map->room_count; i++) {
        struct Room* room = &map->rooms[i];
        if (player_pos->x > room->left_wall && player_pos->x < room->right_wall &&
            player_pos->y > room->top_wall && player_pos->y < room->bottom_wall) {
            current_room = room;
            room->visited = true;
            break;
        }
    }
    
    if (current_room) {
        // Show current room
        for (int y = current_room->top_wall; y <= current_room->bottom_wall; y++) {
            for (int x = current_room->left_wall; x <= current_room->right_wall; x++) {
                map->visibility[y][x] = true;
                map->discovered[y][x] = true;
            }
        }
        
        // Show connected rooms if visited
        for (int i = 0; i < map->room_count; i++) {
            struct Room* other = &map->rooms[i];
            if (other->visited && can_see_room(map, current_room, other)) {
                for (int y = other->top_wall; y <= other->bottom_wall; y++) {
                    for (int x = other->left_wall; x <= other->right_wall; x++) {
                        map->visibility[y][x] = true;
                    }
                }
            }
        }
    } else {
        // In corridor, show limited visibility
        for (int y = player_pos->y - CORRIDOR_VIEW_DISTANCE; 
             y <= player_pos->y + CORRIDOR_VIEW_DISTANCE; y++) {
            for (int x = player_pos->x - CORRIDOR_VIEW_DISTANCE; 
                 x <= player_pos->x + CORRIDOR_VIEW_DISTANCE; x++) {
                if (y >= 0 && y < MAP_HEIGHT && x >= 0 && x < MAP_WIDTH) {
                    if (map->discovered[y][x] || 
                        (abs(x - player_pos->x) + abs(y - player_pos->y)) <= CORRIDOR_VIEW_DISTANCE) {
                        map->visibility[y][x] = true;
                        map->discovered[y][x] = true;
                    }
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
    // Check if rooms are directly connected by a door
    if (hasConnectingDoor(map, room1, room2)) {
        return true;
    }
    
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

// DFS to check if all rooms are connected

void dfs_connect(struct Map* map, int room_idx, bool* visited) {
    visited[room_idx] = true;
    
    // Visit all neighboring rooms
    for (int i = 0; i < map->room_count; i++) {
        if (!visited[i]) {
            struct Room* room1 = &map->rooms[room_idx];
            struct Room* room2 = &map->rooms[i];

            // Check if room1 and room2 are close enough to be connected
            if (can_connect(room1, room2)) { // You need a function that checks if two rooms can be connected.
                create_corridor(map, room1, room2); // Create corridor between room1 and room2
                dfs_connect(map, i, visited); // Recursively connect the next room
            }
        }
    }
}

// Function to check if two rooms can be connected (basic check for proximity)

bool can_connect(struct Room* room1, struct Room* room2) {
    int dx = (room1->left_wall + room1->right_wall) / 2 - (room2->left_wall + room2->right_wall) / 2;
    int dy = (room1->top_wall + room1->bottom_wall) / 2 - (room2->top_wall + room2->bottom_wall) / 2;
    int distance = abs(dx) + abs(dy);

    return distance < MAX_ROOM_CONNECTION_DISTANCE; // Arbitrary threshold for how close rooms must be
}

// Modified connect_rooms function
void connect_rooms(struct Map* map) {
    bool* visited = (bool*)calloc(map->room_count, sizeof(bool)); // Array to track visited rooms
    
    // Start DFS from the first room
    dfs_connect(map, 0, visited);
    
    // Ensure all rooms are connected, check if there's any unvisited room
    for (int i = 0; i < map->room_count; i++) {
        if (!visited[i]) {
            // If any room is not visited, connect it to the first visited room
            create_corridor(map, &map->rooms[0], &map->rooms[i]);
        }
    }

    free(visited); // Free the visited array
}

bool validate_room_accessibility(struct Map* map) {
    bool visited[MAX_ROOMS] = {false};
    int stack[MAX_ROOMS];
    int top = 0;

    // Start from the first room
    visited[0] = true;
    stack[top++] = 0;

    // Perform a depth-first search (DFS)
    while (top > 0) {
        int current = stack[--top];

        for (int i = 0; i < map->room_count; i++) {
            if (i != current && !visited[i] && areRoomsConnectable(&map->rooms[current], &map->rooms[i])) {
                visited[i] = true;
                stack[top++] = i;
            }
        }
    }

    // Check if all rooms were visited
    for (int i = 0; i < map->room_count; i++) {
        if (!visited[i]) {
            return false;
        }
    }
    return true;
}

