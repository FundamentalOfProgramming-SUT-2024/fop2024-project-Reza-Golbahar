// weapons.h
#ifndef WEAPONS_H
#define WEAPONS_H

#include <wchar.h>


// Enumerate different weapon types
typedef enum {
    WEAPON_NONE = 0,
    WEAPON_MACE,
    WEAPON_DAGGER,
    WEAPON_MAGIC_WAND,
    WEAPON_NORMAL_ARROW,
    WEAPON_SWORD,
    WEAPON_COUNT // Total number of weapon types
} WeaponType;

// Structure to hold weapon information
typedef struct {
    WeaponType type;
    wchar_t symbol; // Unicode character for the weapon
} Weapon;

// Function declarations
void init_weapons();
Weapon get_weapon_by_type(WeaponType type);

#endif // WEAPONS_H
