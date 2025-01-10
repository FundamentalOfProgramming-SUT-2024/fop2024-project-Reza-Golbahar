#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

// Assuming a simple Room structure
typedef struct {
    int x, y; // Position of the room
    int width, height; // Dimensions of the room
    bool has_door; // If the room already has a door
} Room;

// Map grid size
#define MAP_WIDTH 50
#define MAP_HEIGHT 50

// Map to represent the dungeon layout (0 = empty, 1 = wall, 2 = corridor)
int map[MAP_HEIGHT][MAP_WIDTH];

// Define directions
typedef enum {
    UP, DOWN, LEFT, RIGHT
} Direction;

// Function to check if a position is within bounds
bool is_within_bounds(int x, int y) {
    return x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT;
}

// Function to place a wall or corridor in the map
void place_tile(int x, int y, int value) {
    if (is_within_bounds(x, y)) {
        map[y][x] = value;
    }
}

// Function to randomly select two rooms and create a corridor between them
void create_corridor(Room *room1, Room *room2) {
    // Randomly select the start and end points of the corridor on the room's edges
    int start_x = room1->x + rand() % room1->width;
    int start_y = room1->y + rand() % room1->height;
    int end_x = room2->x + rand() % room2->width;
    int end_y = room2->y + rand() % room2->height;

    // Create a curved path (randomly change direction at each step)
    int current_x = start_x;
    int current_y = start_y;

    while (current_x != end_x || current_y != end_y) {
        // Place a corridor tile at the current position
        place_tile(current_x, current_y, 2);

        // Choose a direction to move towards the destination
        if (rand() % 2 == 0) { // Randomly choose either horizontal or vertical movement
            if (current_x < end_x) current_x++;
            else if (current_x > end_x) current_x--;
        } else {
            if (current_y < end_y) current_y++;
            else if (current_y > end_y) current_y--;
        }

        // Add some randomness to make the path slightly curved
        if (rand() % 5 == 0) { // 20% chance to change direction
            if (rand() % 2 == 0) {
                if (current_x < MAP_WIDTH - 1) current_x++;
                else if (current_x > 0) current_x--;
            } else {
                if (current_y < MAP_HEIGHT - 1) current_y++;
                else if (current_y > 0) current_y--;
            }
        }
    }

    // Place the door where the corridor meets the room (at the room edge)
    place_tile(current_x, current_y, 1); // Assuming 1 represents a door (a wall with a door)
    room1->has_door = true;
    room2->has_door = true;
}

// Function to connect all rooms by corridors
void connect_all_rooms(Room *rooms, int room_count) {
    for (int i = 0; i < room_count - 1; i++) {
        create_corridor(&rooms[i], &rooms[i + 1]);
    }
}

// Function to initialize rooms (for testing purposes)
void initialize_rooms(Room *rooms, int room_count) {
    for (int i = 0; i < room_count; i++) {
        rooms[i].x = rand() % (MAP_WIDTH - 10);
        rooms[i].y = rand() % (MAP_HEIGHT - 10);
        rooms[i].width = rand() % 5 + 3; // Room width between 3 and 7
        rooms[i].height = rand() % 5 + 3; // Room height between 3 and 7
        rooms[i].has_door = false;
    }
}

// Function to print the map (for testing purposes)
void print_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map[y][x] == 0) {
                printf(".");
            } else if (map[y][x] == 1) {
                printf("#");
            } else if (map[y][x] == 2) {
                printf(" ");
            }
        }
        printf("\n");
    }
}

int main() {
    srand(time(NULL));

    // Initialize rooms and create corridors
    Room rooms[5];
    initialize_rooms(rooms, 5);
    connect_all_rooms(rooms, 5);

    // Print the map after corridor creation
    print_map();

    return 0;
}
