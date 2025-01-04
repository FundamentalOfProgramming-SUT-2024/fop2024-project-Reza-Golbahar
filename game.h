#ifndef GAME_H
#define GAME_H

#include <ncurses.h>
#include <stdbool.h>

#define MIN_ROOM_COUNT 6
#define MIN_WIDTH_OR_LENGTH 4
#define MAX_POINTS 100
#define MAX_ROOMS 9
#define MAP_WIDTH 80
#define MAP_HEIGHT 24
#define WINDOW_CHANCE 15

// Symbols
#define WALL_VERTICAL '|'
#define WALL_HORIZONTAL '_'
#define CORRIDOR '#'
#define DOOR '+'
#define WINDOW '='
#define FLOOR '.'
#define FOOD '@'
#define PLAYER 'P'

struct Point {
    int x;
    int y;
};

struct Room {
    int left_wall;
    int right_wall;
    int down_wall;
    int up_wall;
    int width;
    int height;
    struct Point floors[MAX_POINTS];
    struct Point walls[MAX_POINTS];
    struct Point doors[4];
    struct Point windows[8];
    struct Point spawn_point;
    int floor_count;
    int wall_count;
    int door_count;
    int window_count;
};

struct Map {
    int width;
    int height;
    struct Room rooms[MAX_ROOMS];
    int room_count;
    char grid[MAP_HEIGHT][MAP_WIDTH];
    struct Point initial_position;
};

bool init_ncurses(void);
void handle_file_error(const char* operation);
void print_point(struct Point p, const char* type);
void init_map(struct Map* game_map);
bool rooms_overlap(const struct Room* r1, const struct Room* r2);
struct Map generate_map(void);
void print_map(struct Map* game_map);
void message(const char* text);
void move_character(struct Point* character_location, char key, struct Map* game_map);

#endif