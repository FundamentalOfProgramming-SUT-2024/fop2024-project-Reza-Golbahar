#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include "users.h"  // Add this to make UserManager visible

#define MIN_ROOM_COUNT 6
#define MIN_WIDTH_OR_LENGTH 4
#define MAX_POINTS 100
#define MAX_ROOMS 9
#define MAP_WIDTH 80
#define MAP_HEIGHT 24
#define MIN_ROOM_DISTANCE 2
#define WINDOW_CHANCE 20
#define MAX_DOORS 4
#define DOOR '+'
#define WALL '|'

// Map symbols
#define FLOOR '.'
#define WALL_HORIZONTAL '_'
#define WALL_VERTICAL '|'
#define WINDOW '*'
#define PLAYER '@'
#define FOOD '%'
#define DOOR '+'
#define WALL '|'

struct Point {
    int x;
    int y;
};

struct Room {
    int width;
    int height;
    int left_wall;
    int right_wall;
    int up_wall;
    int down_wall;
    struct Point spawn_point;
    struct Point doors[MAX_DOORS];
    int door_count;
    int floor_count;
    int wall_count;
    int window_count;
};

struct Map {
    char grid[MAP_HEIGHT][MAP_WIDTH];
    int width;
    int height;
    int room_count;
    struct Room rooms[MAX_ROOMS];
    struct Point initial_position;
};

struct SavedGame {
    char name[MAX_STRING_LEN];
    struct Map game_map;
    struct Point character_location;
    int score;
    time_t save_time;
};

// Function declarations
void print_point(struct Point p, const char* type);
void connect_doors(struct Map* game_map, struct Point door1, struct Point door2);
void create_corridor(struct Map* game_map, struct Room room1, struct Room room2);
void init_map(struct Map* game_map);
bool rooms_overlap(const struct Room* r1, const struct Room* r2);
struct Map generate_map(void);
void print_map(struct Map* game_map, char visible[MAP_HEIGHT][MAP_WIDTH], struct Point character_location);
void message(const char* text);
void move_character(struct Point* character_location, char key, struct Map* game_map);
void sight_range(struct Map* game_map, struct Point* character_location);
void game_menu(struct UserManager* manager);
void connect_rooms(struct Map* game_map);
// Add these function declarations at the top of game.c, after the includes
bool isPointInRoom(struct Point* point, struct Room* room);
void showRoom(struct Map* game_map, struct Room* room, char visible[MAP_HEIGHT][MAP_WIDTH]);
bool canSeeRoomThroughWindow(struct Map* game_map, struct Room* room1, struct Room* room2);
void showCorridorVisibility(struct Map* game_map, struct Point* pos, char visible[MAP_HEIGHT][MAP_WIDTH], char visited[MAP_HEIGHT][MAP_WIDTH]);
int manhattanDistance(int x1, int y1, int x2, int y2);
bool areRoomsConnectable(struct Room* room1, struct Room* room2);
void create_door(struct Map* game_map, struct Room* room1, struct Room* room2);
struct Room* findNearestRoom(struct Map* game_map, struct Room* room);
bool hasConnectingDoor(struct Map* game_map, struct Room* room1, struct Room* room2);
void play_game(struct UserManager* manager, struct Map* game_map, 
               struct Point* character_location, int initial_score);
extern void settings(struct UserManager* manager);
bool load_saved_game(struct UserManager* manager, struct SavedGame* saved_game);
void save_current_game(struct UserManager* manager, struct Map* game_map, 
                      struct Point* character_location, int score);


#endif