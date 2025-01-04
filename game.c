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
    
    if (rooms_placed > 0) {
        game_map.initial_position = game_map.rooms[0].spawn_point;
    }
    
    return game_map;
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