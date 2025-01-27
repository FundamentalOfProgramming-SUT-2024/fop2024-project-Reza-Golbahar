// inventory.h
#ifndef INVENTORY_H
#define INVENTORY_H

#include "weapons.h"

// Structure to hold the player's inventory
typedef struct {
    int food_inventory[5];
    int food_count;
    int gold_count;
    int ancient_key_count;
    int broken_key_count;
    int weapon_counts[WEAPON_COUNT]; // Index corresponds to WeaponType
} Inventory;

#endif // INVENTORY_H
