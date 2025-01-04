 #include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "users.h"  // Add this before game.h
#include "game.h"
#include <limits.h>  // For INT_MAX

#define DOOR '+'
#define WINDOW '*'
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define MIN_ROOM_COUNT 6
#define MIN_WIDTH_OR_LENGTH 4
#define MAX_POINTS 100
#define MAX_ROOMS 9
#define MAP_WIDTH 80
#define MAP_HEIGHT 24



#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "users.h"  // Add this before game.h
#include "game.h"

// ... rest of your existing functions ...

void game_menu(struct UserManager* manager) {
    // Initialize color pairs for game elements
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);   // Normal text/walls
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);  // Food
    init_pair(3, COLOR_GREEN, COLOR_BLACK);   // Player
    init_pair(4, COLOR_BLUE, COLOR_BLACK);    // Doors/Windows
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK); // Score/UI text

    // Initialize game state
    struct Map game_map = generate_map();
    struct Point character_location = game_map.initial_position;
    int score = 0;
    bool game_running = true;
    time_t start_time = time(NULL);

    // Static array to track visited areas
    static char visited[MAP_HEIGHT][MAP_WIDTH] = {0};
    
    // Mark starting position as visited
    visited[character_location.y][character_location.x] = 1;

    // Game loop
    while (game_running) {
        // Clear screen
        clear();

        // Update visibility and draw map
        sight_range(&game_map, &character_location);

        // Draw UI elements
        attron(COLOR_PAIR(5));
        // Display game stats on the right side of the map
        mvprintw(0, MAP_WIDTH + 2, "Player: %s", 
                manager->current_user ? manager->current_user->username : "Guest");
        mvprintw(1, MAP_WIDTH + 2, "Score: %d", score);

        // Display elapsed time
        time_t current_time = time(NULL);
        int elapsed_time = (int)(current_time - start_time);
        int minutes = elapsed_time / 60;
        int seconds = elapsed_time % 60;
        mvprintw(2, MAP_WIDTH + 2, "Time: %02d:%02d", minutes, seconds);

        // Display controls
        mvprintw(4, MAP_WIDTH + 2, "Controls:");
        mvprintw(5, MAP_WIDTH + 2, "Movement:");
        mvprintw(6, MAP_WIDTH + 2, "  h - Left");
        mvprintw(7, MAP_WIDTH + 2, "  j - Down");
        mvprintw(8, MAP_WIDTH + 2, "  k - Up");
        mvprintw(9, MAP_WIDTH + 2, "  l - Right");
        mvprintw(10, MAP_WIDTH + 2, "Diagonal:");
        mvprintw(11, MAP_WIDTH + 2, "  y - Up-Left");
        mvprintw(12, MAP_WIDTH + 2, "  u - Up-Right");
        mvprintw(13, MAP_WIDTH + 2, "  b - Down-Left");
        mvprintw(14, MAP_WIDTH + 2, "  n - Down-Right");
        mvprintw(16, MAP_WIDTH + 2, "q - Quit Game");
        attroff(COLOR_PAIR(5));

        // Draw the player character
        attron(COLOR_PAIR(3));
        mvaddch(character_location.y, character_location.x, '@');
        attroff(COLOR_PAIR(3));

        refresh();

        // Get player input
        int key = getch();
        struct Point old_location = character_location;

        // Handle input
        switch(key) {
            case 'q':
                // Confirm quit
                attron(COLOR_PAIR(5));
                mvprintw(18, MAP_WIDTH + 2, "Really quit? (y/n)");
                refresh();
                attroff(COLOR_PAIR(5));
                
                if (getch() == 'y') {
                    // Save high score if logged in
                    if (manager->current_user && score > manager->current_user->score) {
                        manager->current_user->score = score;
                        save_users_to_json(manager);
                    }
                    game_running = false;
                }
                continue;

            case 'h': case 'j': case 'k': case 'l':
            case 'y': case 'u': case 'b': case 'n':
                move_character(&character_location, key, &game_map);
                
                // Mark new position as visited
                visited[character_location.y][character_location.x] = 1;
                
                // Check for food collection
                if (game_map.grid[character_location.y][character_location.x] == FOOD) {
                    score += 10;
                    game_map.grid[character_location.y][character_location.x] = FLOOR;
                    
                    // Display score message
                    attron(COLOR_PAIR(2));
                    mvprintw(18, MAP_WIDTH + 2, "+10 points!");
                    attroff(COLOR_PAIR(2));
                    refresh();
                    napms(500); // Show message briefly
                }
                break;

            default:
                continue;
        }

        // Check for game over conditions
        bool all_food_collected = true;
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (game_map.grid[y][x] == FOOD) {
                    all_food_collected = false;
                    break;
                }
            }
            if (!all_food_collected) break;
        }

        if (all_food_collected) {
            clear();
            attron(COLOR_PAIR(5));
            mvprintw(MAP_HEIGHT/2 - 1, MAP_WIDTH/2 - 10, "Congratulations!");
            mvprintw(MAP_HEIGHT/2, MAP_WIDTH/2 - 10, "You've collected all the food!");
            mvprintw(MAP_HEIGHT/2 + 1, MAP_WIDTH/2 - 10, "Final Score: %d", score);
            mvprintw(MAP_HEIGHT/2 + 2, MAP_WIDTH/2 - 10, "Time: %02d:%02d", minutes, seconds);
            attroff(COLOR_PAIR(5));
            refresh();
            napms(3000);
            game_running = false;
        }
    }

    // Game over cleanup and display
    clear();
    attron(COLOR_PAIR(5));
    mvprintw(MAP_HEIGHT/2 - 1, MAP_WIDTH/2 - 10, "Game Over!");
    mvprintw(MAP_HEIGHT/2, MAP_WIDTH/2 - 10, "Final Score: %d", score);
    
    // Update high score if logged in
    if (manager->current_user) {
        if (score > manager->current_user->score) {
            mvprintw(MAP_HEIGHT/2 + 1, MAP_WIDTH/2 - 10, "New High Score!");
            manager->current_user->score = score;
            save_users_to_json(manager);
        }
    }
    
    attroff(COLOR_PAIR(5));
    refresh();
    napms(2000);
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
                    if (visited[other_room->down_wall][other_room->left_wall]) {
                        showRoom(game_map, other_room, visible);
                    }
                }
                // Check if rooms are connected by window
                else if (canSeeRoomThroughWindow(game_map, current_room, other_room)) {
                    if (visited[other_room->down_wall][other_room->left_wall]) {
                        showRoom(game_map, other_room, visible);
                    }
                }
            }
        }
    } else {
        // Player is in corridor - show limited range
        const int SIGHT_RANGE = 5; // As per Persian requirements: "بیشتر از ۵ واحد جلوتر را در راهرو نمͳ توانید ببینید"
        
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
        int min_y = MAX(room1->down_wall, room2->down_wall);
        int max_y = MIN(room1->up_wall, room2->up_wall);
        for (int y = min_y; y <= max_y; y++) {
            if (game_map->grid[y][room1->right_wall] == DOOR ||
                game_map->grid[y][room1->left_wall] == DOOR) {
                return true;
            }
        }
    }
    
    if (room1->up_wall == room2->down_wall - 1 || room1->down_wall == room2->up_wall + 1) {
        // Horizontal wall - check for doors
        int min_x = MAX(room1->left_wall, room2->left_wall);
        int max_x = MIN(room1->right_wall, room2->right_wall);
        for (int x = min_x; x <= max_x; x++) {
            if (game_map->grid[room1->up_wall][x] == DOOR ||
                game_map->grid[room1->down_wall][x] == DOOR) {
                return true;
            }
        }
    }
    
    return false;
}

// Helper function for corridor visibility
void showCorridorVisibility(struct Map* game_map, struct Point* pos, char visible[MAP_HEIGHT][MAP_WIDTH], char visited[MAP_HEIGHT][MAP_WIDTH]) {
    const int SIGHT_RANGE = 5;
    
    for (int y = pos->y - SIGHT_RANGE; y <= pos->y + SIGHT_RANGE; y++) {
        for (int x = pos->x - SIGHT_RANGE; x <= pos->x + SIGHT_RANGE; x++) {
            if (y >= 0 && y < MAP_HEIGHT && x >= 0 && x < MAP_WIDTH) {
                if (visited[y][x] || manhattanDistance(pos->x, pos->y, x, y) <= SIGHT_RANGE) {
                    visible[y][x] = 1;
                }
            }
        }
    }
}

void print_map(struct Map* game_map, char visible[MAP_HEIGHT][MAP_WIDTH], struct Point character_location) {
    clear();
    
    // Print the visible portion of the map
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (x == character_location.x && y == character_location.y) {
                mvaddch(y, x, '@');  // Player character
            } else {
                mvaddch(y, x, visible[y][x]);
            }
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

void create_corridor(struct Map* game_map, struct Room room1, struct Room room2) {
    // Choose the best walls to connect based on room positions
    struct Point door1, door2;
    
    // Determine the relative position of rooms
    bool room1_is_left = room1.right_wall < room2.left_wall;
    bool room1_is_right = room1.left_wall > room2.right_wall;
    bool room1_is_above = room1.down_wall > room2.up_wall;
    bool room1_is_below = room1.up_wall < room2.down_wall;
    
    // Create doors on appropriate walls based on relative positions
    if (room1_is_left) {
        // Place door on right wall of room1
        door1.x = room1.right_wall;
        door1.y = room1.down_wall + room1.height / 2;
        
        // Place door on left wall of room2
        door2.x = room2.left_wall;
        door2.y = room2.down_wall + room2.height / 2;
    }
    else if (room1_is_right) {
        // Place door on left wall of room1
        door1.x = room1.left_wall;
        door1.y = room1.down_wall + room1.height / 2;
        
        // Place door on right wall of room2
        door2.x = room2.right_wall;
        door2.y = room2.down_wall + room2.height / 2;
    }
    else if (room1_is_above) {
        // Place door on bottom wall of room1
        door1.x = room1.left_wall + room1.width / 2;
        door1.y = room1.down_wall;
        
        // Place door on top wall of room2
        door2.x = room2.left_wall + room2.width / 2;
        door2.y = room2.up_wall;
    }
    else if (room1_is_below) {
        // Place door on top wall of room1
        door1.x = room1.left_wall + room1.width / 2;
        door1.y = room1.up_wall;
        
        // Place door on bottom wall of room2
        door2.x = room2.left_wall + room2.width / 2;
        door2.y = room2.down_wall;
    }
    
    // Add doors to the rooms
    if (room1.door_count < MAX_DOORS && room2.door_count < MAX_DOORS) {
        room1.doors[room1.door_count++] = door1;
        room2.doors[room2.door_count++] = door2;
        
        // Create the corridor between doors
        connect_doors(game_map, door1, door2);
        
        // Mark door positions on the map
        game_map->grid[door1.y][door1.x] = '+';  // Use '+' for doors
        game_map->grid[door2.y][door2.x] = '+';
    }
}

// Initialize map with empty spaces
void init_map(struct Map* game_map) {
    game_map->width = MAP_WIDTH;
    game_map->height = MAP_HEIGHT;
    
    for (int y = 0; y < game_map->height; y++) {
        for (int x = 0; x < game_map->width; x++) {
            game_map->grid[y][x] = ' ';
        }
    }
}

bool rooms_overlap(const struct Room* r1, const struct Room* r2) {
    return !(r1->right_wall + MIN_ROOM_DISTANCE < r2->left_wall ||
             r2->right_wall + MIN_ROOM_DISTANCE < r1->left_wall ||
             r1->up_wall + MIN_ROOM_DISTANCE < r2->down_wall ||
             r2->up_wall + MIN_ROOM_DISTANCE < r1->down_wall);
}

struct Map generate_map(void) {
    struct Map game_map;
    init_map(&game_map);
    
    game_map.room_count = (rand() % 3) + MIN_ROOM_COUNT;
    int rooms_placed = 0;
    
    while (rooms_placed < game_map.room_count) {
        struct Room new_room;
        bool valid_placement = false;
        
        while (!valid_placement) {
            new_room.width = (rand() % 6) + MIN_WIDTH_OR_LENGTH;
            new_room.height = (rand() % 6) + MIN_WIDTH_OR_LENGTH;
            
            new_room.left_wall = rand() % (MAP_WIDTH - new_room.width) + 2;
            new_room.down_wall = rand() % (MAP_HEIGHT - new_room.height) + 2;
            new_room.right_wall = new_room.left_wall + new_room.width;
            new_room.up_wall = new_room.down_wall + new_room.height;
            
            valid_placement = true;
            for (int i = 0; i < rooms_placed; i++) {
                if (rooms_overlap(&new_room, &game_map.rooms[i])) {
                    valid_placement = false;
                    break;
                }
            }
        }
        
        if (!valid_placement) {
            game_map.room_count--;
            continue;
        }
        
        // Initialize room
        new_room.floor_count = 0;
        new_room.wall_count = 0;
        new_room.door_count = 0;
        new_room.window_count = 0;
        
        // Generate floor
        for (int y = new_room.down_wall + 1; y < new_room.up_wall; y++) {
            for (int x = new_room.left_wall + 1; x < new_room.right_wall; x++) {
                game_map.grid[y][x] = FLOOR;
            }
        }
        
        // Set spawn point
        new_room.spawn_point.x = new_room.left_wall + new_room.width / 2;
        new_room.spawn_point.y = new_room.down_wall + new_room.height / 2;
        
        // Generate walls and windows
        for (int x = new_room.left_wall; x <= new_room.right_wall; x++) {
            if (rand() % 100 < WINDOW_CHANCE && x > new_room.left_wall && x < new_room.right_wall) {
                game_map.grid[new_room.down_wall][x] = WINDOW;
                game_map.grid[new_room.up_wall][x] = WINDOW;
            } else {
                game_map.grid[new_room.down_wall][x] = WALL_HORIZONTAL;
                game_map.grid[new_room.up_wall][x] = WALL_HORIZONTAL;
            }
        }
        
        for (int y = new_room.down_wall; y <= new_room.up_wall; y++) {
            if (rand() % 100 < WINDOW_CHANCE && y > new_room.down_wall && y < new_room.up_wall) {
                game_map.grid[y][new_room.left_wall] = WINDOW;
                game_map.grid[y][new_room.right_wall] = WINDOW;
            } else {
                game_map.grid[y][new_room.left_wall] = WALL_VERTICAL;
                game_map.grid[y][new_room.right_wall] = WALL_VERTICAL;
            }
        }
        
        game_map.rooms[rooms_placed] = new_room;
        rooms_placed++;
    }
    add_food(&game_map);
    
    if (rooms_placed > 0) {
        game_map.initial_position = game_map.rooms[0].spawn_point;
    }
    
    return game_map;
}


void message(const char* text) {
    mvprintw(MAP_HEIGHT, 0, "%s", text);
    refresh();
}

void move_character(struct Point* character_location, char key, struct Map* game_map) {
    struct Point new_location = *character_location;

    switch(key) {
        case 'u': new_location.x++; new_location.y++; break;
        case 'y': new_location.x--; new_location.y++; break;
        case 'b': new_location.x--; new_location.y--; break;
        case 'n': new_location.x++; new_location.y--; break;
        case 'h': new_location.x--; break;
        case 'j': new_location.y++; break;
        case 'k': new_location.y--; break;
        case 'l': new_location.x++; break;
        default: return;
    }

    // Check bounds and wall collision
    if (new_location.x >= 0 && new_location.x < MAP_WIDTH &&
        new_location.y >= 0 && new_location.y < MAP_HEIGHT &&
        game_map->grid[new_location.y][new_location.x] != '|' &&
        game_map->grid[new_location.y][new_location.x] != '_') {
        *character_location = new_location;
    }
}

void connect_rooms(struct Map* game_map) {
    for (int i = 0; i < game_map->room_count; i++) {
        struct Room* room1 = &game_map->rooms[i];
        
        // Ensure at least one door per room
        bool has_door = false;
        
        for (int j = i + 1; j < game_map->room_count; j++) {
            struct Room* room2 = &game_map->rooms[j];
            
            // Check if rooms are close enough to connect
            if (areRoomsConnectable(room1, room2)) {
                create_door(game_map, room1, room2);
                has_door = true;
            }
        }
        
        // If room has no doors, force connect to nearest room
        if (!has_door) {
            struct Room* nearest = findNearestRoom(game_map, room1);
            if (nearest) {
                create_door(game_map, room1, nearest);
            }
        }
    }
}

bool isPointInRoom(struct Point* point, struct Room* room) {
    return (point->x > room->left_wall && point->x < room->right_wall &&
            point->y > room->down_wall && point->y < room->up_wall);
}

void showRoom(struct Map* game_map, struct Room* room, char visible[MAP_HEIGHT][MAP_WIDTH]) {
    for (int y = room->down_wall; y <= room->up_wall; y++) {
        for (int x = room->left_wall; x <= room->right_wall; x++) {
            if (y >= 0 && y < MAP_HEIGHT && x >= 0 && x < MAP_WIDTH) {
                visible[y][x] = 1;
            }
        }
    }
}

bool canSeeRoomThroughWindow(struct Map* game_map, struct Room* room1, struct Room* room2) {
    // Check if rooms share a wall and have a window
    bool share_horizontal = (room1->up_wall == room2->down_wall - 1) ||
                          (room1->down_wall == room2->up_wall + 1);
    bool share_vertical = (room1->right_wall == room2->left_wall - 1) ||
                         (room1->left_wall == room2->right_wall + 1);
    
    // Check for windows on shared walls
    if (share_horizontal) {
        for (int x = MAX(room1->left_wall, room2->left_wall); 
             x <= MIN(room1->right_wall, room2->right_wall); x++) {
            if (game_map->grid[room1->up_wall][x] == WINDOW ||
                game_map->grid[room1->down_wall][x] == WINDOW) {
                return true;
            }
        }
    }
    
    if (share_vertical) {
        for (int y = MAX(room1->down_wall, room2->down_wall);
             y <= MIN(room1->up_wall, room2->up_wall); y++) {
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
        manhattanDistance(room1->right_wall, room1->up_wall, room2->left_wall, room2->down_wall),
        manhattanDistance(room1->right_wall, room1->down_wall, room2->left_wall, room2->up_wall),
        manhattanDistance(room1->left_wall, room1->up_wall, room2->right_wall, room2->down_wall),
        manhattanDistance(room1->left_wall, room1->down_wall, room2->right_wall, room2->up_wall)
    };
    
    for (int i = 0; i < 4; i++) {
        if (distances[i] < min_distance) {
            min_distance = distances[i];
        }
    }
    
    return min_distance <= MAX_CORRIDOR_LENGTH;
}

void create_door(struct Map* game_map, struct Room* room1, struct Room* room2) {
    // Find best wall for door placement
    struct Point door1, door2;
    
    if (room1->right_wall < room2->left_wall) {
        // Room1 is to the left of Room2
        door1.x = room1->right_wall;
        door1.y = room1->down_wall + room1->height / 2;
        door2.x = room2->left_wall;
        door2.y = room2->down_wall + room2->height / 2;
    } else if (room1->left_wall > room2->right_wall) {
        // Room1 is to the right of Room2
        door1.x = room1->left_wall;
        door1.y = room1->down_wall + room1->height / 2;
        door2.x = room2->right_wall;
        door2.y = room2->down_wall + room2->height / 2;
    } else if (room1->down_wall > room2->up_wall) {
        // Room1 is below Room2
        door1.x = room1->left_wall + room1->width / 2;
        door1.y = room1->down_wall;
        door2.x = room2->left_wall + room2->width / 2;
        door2.y = room2->up_wall;
    } else {
        // Room1 is above Room2
        door1.x = room1->left_wall + room1->width / 2;
        door1.y = room1->up_wall;
        door2.x = room2->left_wall + room2->width / 2;
        door2.y = room2->down_wall;
    }
    
    // Place doors and connect with corridor
    game_map->grid[door1.y][door1.x] = DOOR;
    game_map->grid[door2.y][door2.x] = DOOR;
    connect_doors(game_map, door1, door2);
}

struct Room* findNearestRoom(struct Map* game_map, struct Room* room) {
    struct Room* nearest = NULL;
    int min_distance = INT_MAX;
    
    for (int i = 0; i < game_map->room_count; i++) {
        if (&game_map->rooms[i] == room) continue;
        
        int dist = manhattanDistance(
            room->left_wall + room->width/2,
            room->down_wall + room->height/2,
            game_map->rooms[i].left_wall + game_map->rooms[i].width/2,
            game_map->rooms[i].down_wall + game_map->rooms[i].height/2
        );
        
        if (dist < min_distance) {
            min_distance = dist;
            nearest = &game_map->rooms[i];
        }
    }
    
    return nearest;
}