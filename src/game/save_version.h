#ifndef GAME_SAVE_VERSION_H
#define GAME_SAVE_VERSION_H

typedef enum {
    SAVE_GAME_CURRENT_VERSION = 0x98,

    SAVE_GAME_LAST_ORIGINAL_LIMITS_VERSION = 0x66,
    SAVE_GAME_LAST_SMALLER_IMAGE_ID_VERSION = 0x76,
    SAVE_GAME_LAST_NO_DELIVERIES_VERSION = 0x77,
    SAVE_GAME_LAST_STATIC_VERSION = 0x78,
    SAVE_GAME_LAST_JOINED_IMPORT_EXPORT_VERSION = 0x79,
    SAVE_GAME_LAST_STATIC_BUILDING_COUNT_VERSION = 0x80,
    SAVE_GAME_LAST_STATIC_MONUMENT_DELIVERIES_VERSION = 0x81,
    SAVE_GAME_LAST_STORED_IMAGE_IDS = 0x83,
    // SAVE_GAME_INCREASE_GRANARY_CAPACITY shall be updated if we decide to change granary capacity again.
    SAVE_GAME_INCREASE_GRANARY_CAPACITY = 0x85,
    SAVE_GAME_ROADBLOCK_DATA_MOVED_FROM_SUBTYPE = 0x86,
    SAVE_GAME_LAST_ORIGINAL_TERRAIN_DATA_SIZE_VERSION = 0x86,
    SAVE_GAME_LAST_CARAVANSERAI_WRONG_OFFSET = 0x87,
    SAVE_GAME_LAST_ZIP_COMPRESSION = 0x88,
    SAVE_GAME_LAST_ENEMY_ARMIES_BUFFER_BUG = 0x89,
    SAVE_GAME_LAST_BARRACKS_TOWER_SENTRY_REQUEST = 0x8a,
    // SAVE_GAME_LAST_WITHOUT_HIGHWAYS = 0x8b, no actual changes to how games are saved. Crudelios just wants this here
    SAVE_GAME_LAST_UNVERSIONED_SCENARIOS = 0x8c,
    SAVE_GAME_LAST_EMPIRE_RESOURCES_ALWAYS_WRITE = 0x8d,
    // the difference between this version and UNVERSIONED_SCENARIOS above is this one actually saves the scenario version
    // in the data, whereas the previous one did a lookup based on the save version
    SAVE_GAME_LAST_NO_SCENARIO_VERSION = 0x8e,
    SAVE_GAME_LAST_UNKNOWN_UNUSED_CITY_DATA = 0x8f,
    SAVE_GAME_LAST_STATIC_RESOURCES = 0x90,
    SAVE_GAME_LAST_GLOBAL_BUILDING_INFO = 0x91,
    SAVE_GAME_LAST_NO_GOLD_AND_MINTING = 0x92,
    SAVE_GAME_LAST_STATIC_SCENARIO_OBJECTS = 0x93,
    SAVE_GAME_LAST_NO_EXTENDED_REQUESTS = 0x94,
    SAVE_GAME_LAST_NO_EVENTS = 0x95,
    SAVE_GAME_LAST_NO_CUSTOM_MESSAGES = 0x96,
    SAVE_GAME_LAST_NO_CART_DEPOT = 0x97,
    SAVE_GAME_LAST_NO_NEW_MONUMENT_RESOURCES = 0x98
} savegame_version_t;

typedef enum {
    SCENARIO_CURRENT_VERSION = 12,

    SCENARIO_VERSION_NONE = 0,
    SCENARIO_LAST_UNVERSIONED = 1,
    SCENARIO_LAST_NO_WAGE_LIMITS = 2,
    SCENARIO_LAST_EMPIRE_OBJECT_BUFFERS = 3,
    SCENARIO_LAST_EMPIRE_RESOURCES_U8 = 4,
    SCENARIO_LAST_EMPIRE_RESOURCES_ALWAYS_WRITE = 5,
    SCENARIO_LAST_NO_SAVE_VERSION_WRITE = 6,
    SCENARIO_LAST_NO_STATIC_RESOURCES = 7,
    SCENARIO_LAST_NO_DYNAMIC_OBJECTS = 8,
    SCENARIO_LAST_NO_EXTENDED_REQUESTS = 9,
    SCENARIO_LAST_NO_EVENTS = 10,
    SCENARIO_LAST_NO_CUSTOM_MESSAGES = 11
} scenario_version_t;

typedef enum {
    SAVEGAME_STATUS_NEWER_VERSION = -1,
    SAVEGAME_STATUS_INVALID = 0,
    SAVEGAME_STATUS_OK = 1
} savegame_load_status;

#endif // GAME_SAVE_VERSION_H
