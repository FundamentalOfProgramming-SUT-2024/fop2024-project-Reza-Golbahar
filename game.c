#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "game.h"


#define MIN_ROOM_COUNT 6
#define MIN_WIDTH_OR_LENGTH 4
#define MAX_POINTS 100
#define MAX_ROOMS 9
#define MAP_WIDTH 80
#define MAP_HEIGHT 24

#define DOOR_CHANCE 10    // Percentage chance for a wall segment to become a door
#define FOOD_CHANCE 20    // Percentage chance for a floor tile to have food


// Implementation of print_point
void print_point(struct Point p, const char* type) {
    if (p.y < 0 || p.x < 0 || p.y >= MAP_HEIGHT || p.x >= MAP_WIDTH) return;

    char symbol;
    if (strcmp(type, "wall") == 0) symbol = '#';
    else if (strcmp(type, "floor") == 0) symbol = '.';
    else if (strcmp(type, "food") == 0) symbol = '@';
    else if (strcmp(type, "trap") == 0) symbol = '!';
    else symbol = ' ';

    mvaddch(p.y, p.x, symbol);
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
    
    // Initialize map with empty spaces
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            game_map.grid[y][x] = ' ';
        }
    }
    
    game_map.room_count = (rand() % 3) + MIN_ROOM_COUNT;
    int rooms_placed = 0;
    
    while (rooms_placed < game_map.room_count) {
        struct Room new_room;
        bool valid_placement = false;
        int attempts = 0;
        
        while (!valid_placement && attempts < MAX_ATTEMPTS) {
            new_room.width = (rand() % 6) + MIN_WIDTH_OR_LENGTH;
            new_room.height = (rand() % 6) + MIN_WIDTH_OR_LENGTH;
            
            new_room.left_wall = rand() % (MAP_WIDTH - new_room.width - 4) + 2;
            new_room.down_wall = rand() % (MAP_HEIGHT - new_room.height - 4) + 2;
            new_room.right_wall = new_room.left_wall + new_room.width;
            new_room.up_wall = new_room.down_wall + new_room.height;
            
            valid_placement = true;
            for (int i = 0; i < rooms_placed; i++) {
                if (rooms_overlap(&new_room, &game_map.rooms[i])) {
                    valid_placement = false;
                    break;
                }
            }
            attempts++;
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
        
        // Generate floor and food
        for (int y = new_room.down_wall + 1; y < new_room.up_wall; y++) {
            for (int x = new_room.left_wall + 1; x < new_room.right_wall; x++) {
                if (rand() % 100 < FOOD_CHANCE) {
                    game_map.grid[y][x] = FOOD;
                } else {
                    game_map.grid[y][x] = FLOOR;
                }
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
    
    if (rooms_placed > 0) {
        game_map.initial_position = game_map.rooms[0].spawn_point;
    }
    
    return game_map;
}

// Helper function to check if two rooms are directly connected
bool are_rooms_connected(const struct Map* game_map, const struct Room* r1, const struct Room* r2) {
    // Check if there's a path of corridors between the rooms
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (game_map->grid[y][x] == '+') {
                // Check if this door belongs to either room
                bool door_of_r1 = (y == r1->up_wall || y == r1->down_wall) && 
                                (x >= r1->left_wall && x <= r1->right_wall) ||
                                (x == r1->left_wall || x == r1->right_wall) && 
                                (y >= r1->down_wall && y <= r1->up_wall);
                
                bool door_of_r2 = (y == r2->up_wall || y == r2->down_wall) && 
                                (x >= r2->left_wall && x <= r2->right_wall) ||
                                (x == r2->left_wall || x == r2->right_wall) && 
                                (y >= r2->down_wall && y <= r2->up_wall);
                
                if (door_of_r1 && door_of_r2) return true;
            }
        }
    }
    return false;
}

void connect_rooms(struct Map* game_map) {
    bool* connected = calloc(game_map->room_count, sizeof(bool));
    connected[0] = true;
    int connected_count = 1;

    while (connected_count < game_map->room_count) {
        int best_r1 = -1, best_r2 = -1;
        int min_distance = MAP_WIDTH + MAP_HEIGHT;
        struct Point best_door1, best_door2;

        // Find closest unconnected rooms
        for (int i = 0; i < game_map->room_count; i++) {
            if (!connected[i]) continue;
            
            struct Room* room1 = &game_map->rooms[i];
            
            for (int j = 0; j < game_map->room_count; j++) {
                if (connected[j] || i == j) continue;
                
                struct Room* room2 = &game_map->rooms[j];
                
                // Try all possible door combinations
                struct Point door1, door2;
                int min_local_dist = MAP_WIDTH + MAP_HEIGHT;

                // Try doors on horizontal walls
                for (int x1 = room1->left_wall + 1; x1 < room1->right_wall; x1++) {
                    for (int x2 = room2->left_wall + 1; x2 < room2->right_wall; x2++) {
                        // Try top and bottom walls
                        struct Point d1_top = {x1, room1->up_wall};
                        struct Point d1_bottom = {x1, room1->down_wall};
                        struct Point d2_top = {x2, room2->up_wall};
                        struct Point d2_bottom = {x2, room2->down_wall};

                        // Check all combinations
                        struct Point pairs[4][2] = {
                            {d1_top, d2_bottom},
                            {d1_top, d2_top},
                            {d1_bottom, d2_bottom},
                            {d1_bottom, d2_top}
                        };

                        for (int p = 0; p < 4; p++) {
                            int dist = abs(pairs[p][0].x - pairs[p][1].x) + 
                                     abs(pairs[p][0].y - pairs[p][1].y);
                            if (dist < min_local_dist) {
                                min_local_dist = dist;
                                door1 = pairs[p][0];
                                door2 = pairs[p][1];
                            }
                        }
                    }
                }

                // Try doors on vertical walls
                for (int y1 = room1->down_wall + 1; y1 < room1->up_wall; y1++) {
                    for (int y2 = room2->down_wall + 1; y2 < room2->up_wall; y2++) {
                        struct Point d1_left = {room1->left_wall, y1};
                        struct Point d1_right = {room1->right_wall, y1};
                        struct Point d2_left = {room2->left_wall, y2};
                        struct Point d2_right = {room2->right_wall, y2};

                        struct Point pairs[4][2] = {
                            {d1_left, d2_right},
                            {d1_left, d2_left},
                            {d1_right, d2_right},
                            {d1_right, d2_left}
                        };

                        for (int p = 0; p < 4; p++) {
                            int dist = abs(pairs[p][0].x - pairs[p][1].x) + 
                                     abs(pairs[p][0].y - pairs[p][1].y);
                            if (dist < min_local_dist) {
                                min_local_dist = dist;
                                door1 = pairs[p][0];
                                door2 = pairs[p][1];
                            }
                        }
                    }
                }

                if (min_local_dist < min_distance) {
                    min_distance = min_local_dist;
                    best_r1 = i;
                    best_r2 = j;
                    best_door1 = door1;
                    best_door2 = door2;
                }
            }
        }

        if (best_r1 != -1 && best_r2 != -1) {
            // Place doors
            game_map->grid[best_door1.y][best_door1.x] = '+';
            game_map->grid[best_door2.y][best_door2.x] = '+';

            // Create L-shaped corridor between doors
            int x1 = best_door1.x, y1 = best_door1.y;
            int x2 = best_door2.x, y2 = best_door2.y;
            int corner_x, corner_y;

            // Decide corner position
            if (rand() % 2 == 0) {
                corner_x = x1;
                corner_y = y2;
            } else {
                corner_x = x2;
                corner_y = y1;
            }

            // Draw first segment
            int min_x = x1 < corner_x ? x1 : corner_x;
            int max_x = x1 < corner_x ? corner_x : x1;
            int min_y = y1 < corner_y ? y1 : corner_y;
            int max_y = y1 < corner_y ? corner_y : y1;

            for (int x = min_x; x <= max_x; x++) {
                if (game_map->grid[y1][x] == ' ') {
                    game_map->grid[y1][x] = '#';
                }
            }

            // Draw second segment
            min_x = corner_x < x2 ? corner_x : x2;
            max_x = corner_x < x2 ? x2 : corner_x;
            min_y = corner_y < y2 ? corner_y : y2;
            max_y = corner_y < y2 ? y2 : corner_y;

            for (int y = min_y; y <= max_y; y++) {
                if (game_map->grid[y][corner_x] == ' ') {
                    game_map->grid[y][corner_x] = '#';
                }
            }

            connected[best_r2] = true;
            connected_count++;
        }
    }

    free(connected);
}

// Print the entire map
void print_map(struct Map* game_map) {
    clear();
    for (int y = 0; y < game_map->height; y++) {
        for (int x = 0; x < game_map->width; x++) {
            mvaddch(y, x, game_map->grid[y][x]);
        }
    }
    refresh();
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