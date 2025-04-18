// from game/bg_misc.c

#include <stdio.h>

typedef enum {
    HI_NONE,

    HI_TELEPORTER,
    HI_MEDKIT,
    HI_KAMIKAZE,
    HI_PORTAL,
    HI_INVULNERABILITY,

    HI_NUM_HOLDABLE
} holdable_t;

typedef enum {
    WP_NONE,

    WP_GAUNTLET,
    WP_MACHINEGUN,
    WP_SHOTGUN,
    WP_GRENADE_LAUNCHER,
    WP_ROCKET_LAUNCHER,
    WP_LIGHTNING,
    WP_RAILGUN,
    WP_PLASMAGUN,
    WP_BFG,
    WP_GRAPPLING_HOOK,
#ifdef MISSIONPACK
    WP_NAILGUN,
    WP_PROX_LAUNCHER,
    WP_CHAINGUN,
#endif

    WP_NUM_WEAPONS
} weapon_t;


typedef enum {
    PW_NONE,

    PW_QUAD,
    PW_BATTLESUIT,
    PW_HASTE,
    PW_INVIS,
    PW_REGEN,
    PW_FLIGHT,

    PW_REDFLAG,
    PW_BLUEFLAG,
    PW_NEUTRALFLAG,

    PW_SCOUT,
    PW_GUARD,
    PW_DOUBLER,
    PW_AMMOREGEN,
    PW_INVULNERABILITY,

    PW_NUM_POWERUPS

} powerup_t;


// gitem_t->type
typedef enum {
    IT_BAD,
    IT_WEAPON,              // EFX: rotate + upscale + minlight
    IT_AMMO,                // EFX: rotate
    IT_ARMOR,               // EFX: rotate + minlight
    IT_HEALTH,              // EFX: static external sphere + rotating internal
    IT_POWERUP,             // instant on, timer based
                            // EFX: rotate + external ring that rotates
    IT_HOLDABLE,            // single use, holdable item
                            // EFX: rotate + bob
    IT_PERSISTANT_POWERUP,
    IT_TEAM
} itemType_t;

#define MAX_ITEM_MODELS 4

typedef struct gitem_s {
    char        *classname; // spawning name
    char        *pickup_sound;
    char        *world_model[MAX_ITEM_MODELS];

    char        *icon;
    char        *pickup_name;   // for printing on pickup

    int         quantity;       // for ammo how much, or duration of powerup
    itemType_t  giType;         // IT_* flags

    int         giTag;

    char        *precaches;     // string of all models and images this item will use
    char        *sounds;        // string of all sounds this item will use
} gitem_t;

gitem_t bg_itemlist[] =
{
    {
        NULL,
        NULL,
        { NULL,
            NULL,
            NULL, NULL} ,
        /* icon */      NULL,
        /* pickup */    NULL,
        0,
        0,
        0,
        /* precache */ "",
        /* sounds */ ""
    },  // leave index 0 alone

    //
    // ARMOR
    //

    /*QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_armor_shard",
        "sound/misc/ar1_pkup.ogg",
        { "models/powerups/armor/shard.md3",
            "models/powerups/armor/shard_sphere.md3",
            NULL, NULL} ,
        /* icon */      "icons/iconr_shard",
        /* pickup */    "Armor Shard",
        5,
        IT_ARMOR,
        0,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_armor_combat",
        "sound/misc/ar2_pkup.ogg",
        { "models/powerups/armor/armor_yel.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconr_yellow",
        /* pickup */    "Armor",
        50,
        IT_ARMOR,
        0,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_armor_body",
        "sound/misc/ar2_pkup.ogg",
        { "models/powerups/armor/armor_red.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconr_red",
        /* pickup */    "Heavy Armor",
        100,
        IT_ARMOR,
        0,
        /* precache */ "",
        /* sounds */ ""
    },

    //
    // health
    //
    /*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_health_small",
        "sound/items/s_health.ogg",
        { "models/powerups/health/small_cross.md3",
            "models/powerups/health/small_sphere.md3",
            NULL, NULL },
        /* icon */      "icons/iconh_green",
        /* pickup */    "5 Health",
        5,
        IT_HEALTH,
        0,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_health",
        "sound/items/n_health.ogg",
        { "models/powerups/health/medium_cross.md3",
            "models/powerups/health/medium_sphere.md3",
            NULL, NULL },
        /* icon */      "icons/iconh_yellow",
        /* pickup */    "25 Health",
        25,
        IT_HEALTH,
        0,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_health_large",
        "sound/items/l_health.ogg",
        { "models/powerups/health/large_cross.md3",
            "models/powerups/health/large_sphere.md3",
            NULL, NULL },
        /* icon */      "icons/iconh_red",
        /* pickup */    "50 Health",
        50,
        IT_HEALTH,
        0,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_health_mega",
        "sound/items/m_health.ogg",
        { "models/powerups/health/mega_cross.md3",
            "models/powerups/health/mega_sphere.md3",
            NULL, NULL },
        /* icon */      "icons/iconh_mega",
        /* pickup */    "Mega Health",
        100,
        IT_HEALTH,
        0,
        /* precache */ "",
        /* sounds */ ""
    },


    //
    // WEAPONS
    //

    /*QUAKED weapon_gauntlet (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_gauntlet",
        "sound/misc/w_pkup.ogg",
        { "models/weapons2/gauntlet/gauntlet.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_gauntlet",
        /* pickup */    "Gauntlet",
        0,
        IT_WEAPON,
        WP_GAUNTLET,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_shotgun",
        "sound/misc/w_pkup.ogg",
        { "models/weapons2/shotgun/shotgun.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_shotgun",
        /* pickup */    "Shotgun",
        10,
        IT_WEAPON,
        WP_SHOTGUN,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED weapon_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_machinegun",
        "sound/misc/w_pkup.ogg",
        { "models/weapons2/machinegun/machinegun.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_machinegun",
        /* pickup */    "Machinegun",
        40,
        IT_WEAPON,
        WP_MACHINEGUN,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_grenadelauncher",
        "sound/misc/w_pkup.ogg",
        { "models/weapons2/grenadel/grenadel.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_grenade",
        /* pickup */    "Grenade Launcher",
        10,
        IT_WEAPON,
        WP_GRENADE_LAUNCHER,
        /* precache */ "",
        /* sounds */ "sound/weapons/grenade/hgrenb1a.ogg sound/weapons/grenade/hgrenb2a.ogg"
    },

    /*QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_rocketlauncher",
        "sound/misc/w_pkup.ogg",
        { "models/weapons2/rocketl/rocketl.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_rocket",
        /* pickup */    "Rocket Launcher",
        10,
        IT_WEAPON,
        WP_ROCKET_LAUNCHER,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED weapon_lightning (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_lightning",
        "sound/misc/w_pkup.ogg",
        { "models/weapons2/lightning/lightning.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_lightning",
        /* pickup */    "Lightning Gun",
        100,
        IT_WEAPON,
        WP_LIGHTNING,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED weapon_railgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_railgun",
        "sound/misc/w_pkup.ogg",
        { "models/weapons2/railgun/railgun.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_railgun",
        /* pickup */    "Railgun",
        10,
        IT_WEAPON,
        WP_RAILGUN,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED weapon_plasmagun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_plasmagun",
        "sound/misc/w_pkup.ogg",
        { "models/weapons2/plasma/plasma.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_plasma",
        /* pickup */    "Plasma Gun",
        50,
        IT_WEAPON,
        WP_PLASMAGUN,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED weapon_bfg (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_bfg",
        "sound/misc/w_pkup.ogg",
        { "models/weapons2/bfg/bfg.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_bfg",
        /* pickup */    "BFG10K",
        20,
        IT_WEAPON,
        WP_BFG,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED weapon_grapplinghook (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_grapplinghook",
        "sound/misc/w_pkup.ogg",
        { "models/weapons2/grapple/grapple.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_grapple",
        /* pickup */    "Grappling Hook",
        0,
        IT_WEAPON,
        WP_GRAPPLING_HOOK,
        /* precache */ "",
        /* sounds */ ""
    },

    //
    // AMMO ITEMS
    //

    /*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "ammo_shells",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/ammo/shotgunam.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/icona_shotgun",
        /* pickup */    "Shells",
        10,
        IT_AMMO,
        WP_SHOTGUN,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED ammo_bullets (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "ammo_bullets",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/ammo/machinegunam.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/icona_machinegun",
        /* pickup */    "Bullets",
        50,
        IT_AMMO,
        WP_MACHINEGUN,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED ammo_grenades (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "ammo_grenades",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/ammo/grenadeam.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/icona_grenade",
        /* pickup */    "Grenades",
        5,
        IT_AMMO,
        WP_GRENADE_LAUNCHER,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "ammo_cells",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/ammo/plasmaam.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/icona_plasma",
        /* pickup */    "Cells",
        30,
        IT_AMMO,
        WP_PLASMAGUN,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED ammo_lightning (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "ammo_lightning",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/ammo/lightningam.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/icona_lightning",
        /* pickup */    "Lightning",
        60,
        IT_AMMO,
        WP_LIGHTNING,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "ammo_rockets",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/ammo/rocketam.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/icona_rocket",
        /* pickup */    "Rockets",
        5,
        IT_AMMO,
        WP_ROCKET_LAUNCHER,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED ammo_slugs (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "ammo_slugs",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/ammo/railgunam.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/icona_railgun",
        /* pickup */    "Slugs",
        10,
        IT_AMMO,
        WP_RAILGUN,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED ammo_bfg (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "ammo_bfg",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/ammo/bfgam.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/icona_bfg",
        /* pickup */    "Bfg Ammo",
        15,
        IT_AMMO,
        WP_BFG,
        /* precache */ "",
        /* sounds */ ""
    },

    //
    // HOLDABLE ITEMS
    //
    /*QUAKED holdable_teleporter (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "holdable_teleporter",
        "sound/items/holdable.ogg",
        { "models/powerups/holdable/teleporter.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/teleporter",
        /* pickup */    "Personal Teleporter",
        60,
        IT_HOLDABLE,
        HI_TELEPORTER,
        /* precache */ "",
        /* sounds */ ""
    },
    /*QUAKED holdable_medkit (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "holdable_medkit",
        "sound/items/holdable.ogg",
        {
            "models/powerups/holdable/medkit.md3",
            "models/powerups/holdable/medkit_sphere.md3",
            NULL, NULL},
        /* icon */      "icons/medkit",
        /* pickup */    "Medkit",
        60,
        IT_HOLDABLE,
        HI_MEDKIT,
        /* precache */ "",
        /* sounds */ "sound/items/use_medkit.ogg"
    },

    //
    // POWERUP ITEMS
    //
    /*QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_quad",
        "sound/items/quaddamage.ogg",
        { "models/powerups/instant/quad.md3",
            "models/powerups/instant/quad_ring.md3",
            NULL, NULL },
        /* icon */      "icons/quad",
        /* pickup */    "Quad Damage",
        30,
        IT_POWERUP,
        PW_QUAD,
        /* precache */ "",
        /* sounds */ "sound/items/damage2.ogg sound/items/damage3.ogg"
    },

    /*QUAKED item_enviro (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_enviro",
        "sound/items/protect.ogg",
        { "models/powerups/instant/enviro.md3",
            "models/powerups/instant/enviro_ring.md3",
            NULL, NULL },
        /* icon */      "icons/envirosuit",
        /* pickup */    "Battle Suit",
        30,
        IT_POWERUP,
        PW_BATTLESUIT,
        /* precache */ "",
        /* sounds */ "sound/items/airout.ogg sound/items/protect3.ogg"
    },

    /*QUAKED item_haste (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_haste",
        "sound/items/haste.ogg",
        { "models/powerups/instant/haste.md3",
            "models/powerups/instant/haste_ring.md3",
            NULL, NULL },
        /* icon */      "icons/haste",
        /* pickup */    "Speed",
        30,
        IT_POWERUP,
        PW_HASTE,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED item_invis (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_invis",
        "sound/items/invisibility.ogg",
        { "models/powerups/instant/invis.md3",
            "models/powerups/instant/invis_ring.md3",
            NULL, NULL },
        /* icon */      "icons/invis",
        /* pickup */    "Invisibility",
        30,
        IT_POWERUP,
        PW_INVIS,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED item_regen (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_regen",
        "sound/items/regeneration.ogg",
        { "models/powerups/instant/regen.md3",
            "models/powerups/instant/regen_ring.md3",
            NULL, NULL },
        /* icon */      "icons/regen",
        /* pickup */    "Regeneration",
        30,
        IT_POWERUP,
        PW_REGEN,
        /* precache */ "",
        /* sounds */ "sound/items/regen.ogg"
    },

    /*QUAKED item_flight (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "item_flight",
        "sound/items/flight.ogg",
        { "models/powerups/instant/flight.md3",
            "models/powerups/instant/flight_ring.md3",
            NULL, NULL },
        /* icon */      "icons/flight",
        /* pickup */    "Flight",
        60,
        IT_POWERUP,
        PW_FLIGHT,
        /* precache */ "",
        /* sounds */ "sound/items/flight.ogg"
    },

    /*QUAKED team_CTF_redflag (1 0 0) (-16 -16 -16) (16 16 16)
      Only in CTF games
      */
    {
        "team_CTF_redflag",
        NULL,
        { "models/flags/r_flag.md3",
            NULL, NULL, NULL },
        /* icon */      "icons/iconf_red1",
        /* pickup */    "Red Flag",
        0,
        IT_TEAM,
        PW_REDFLAG,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED team_CTF_blueflag (0 0 1) (-16 -16 -16) (16 16 16)
      Only in CTF games
      */
    {
        "team_CTF_blueflag",
        NULL,
        { "models/flags/b_flag.md3",
            NULL, NULL, NULL },
        /* icon */      "icons/iconf_blu1",
        /* pickup */    "Blue Flag",
        0,
        IT_TEAM,
        PW_BLUEFLAG,
        /* precache */ "",
        /* sounds */ ""
    },

#ifdef MISSIONPACK
    /*QUAKED holdable_kamikaze (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "holdable_kamikaze",
        "sound/items/holdable.ogg",
        { "models/powerups/kamikazi.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/kamikaze",
        /* pickup */    "Kamikaze",
        60,
        IT_HOLDABLE,
        HI_KAMIKAZE,
        /* precache */ "",
        /* sounds */ "sound/items/kamikazerespawn.ogg"
    },

    /*QUAKED holdable_portal (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "holdable_portal",
        "sound/items/holdable.ogg",
        { "models/powerups/holdable/porter.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/portal",
        /* pickup */    "Portal",
        60,
        IT_HOLDABLE,
        HI_PORTAL,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED holdable_invulnerability (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "holdable_invulnerability",
        "sound/items/holdable.ogg",
        { "models/powerups/holdable/invulnerability.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/invulnerability",
        /* pickup */    "Invulnerability",
        60,
        IT_HOLDABLE,
        HI_INVULNERABILITY,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED ammo_nails (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "ammo_nails",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/ammo/nailgunam.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/icona_nailgun",
        /* pickup */    "Nails",
        20,
        IT_AMMO,
        WP_NAILGUN,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED ammo_mines (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "ammo_mines",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/ammo/proxmineam.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/icona_proxlauncher",
        /* pickup */    "Proximity Mines",
        10,
        IT_AMMO,
        WP_PROX_LAUNCHER,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED ammo_belt (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "ammo_belt",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/ammo/chaingunam.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/icona_chaingun",
        /* pickup */    "Chaingun Belt",
        100,
        IT_AMMO,
        WP_CHAINGUN,
        /* precache */ "",
        /* sounds */ ""
    },

    //
    // PERSISTANT POWERUP ITEMS
    //
    /*QUAKED item_scout (.3 .3 1) (-16 -16 -16) (16 16 16) suspended redTeam blueTeam
    */
    {
        "item_scout",
        "sound/items/scout.ogg",
        { "models/powerups/scout.md3",
            NULL, NULL, NULL },
        /* icon */      "icons/scout",
        /* pickup */    "Scout",
        30,
        IT_PERSISTANT_POWERUP,
        PW_SCOUT,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED item_guard (.3 .3 1) (-16 -16 -16) (16 16 16) suspended redTeam blueTeam
    */
    {
        "item_guard",
        "sound/items/guard.ogg",
        { "models/powerups/guard.md3",
            NULL, NULL, NULL },
        /* icon */      "icons/guard",
        /* pickup */    "Guard",
        30,
        IT_PERSISTANT_POWERUP,
        PW_GUARD,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED item_doubler (.3 .3 1) (-16 -16 -16) (16 16 16) suspended redTeam blueTeam
    */
    {
        "item_doubler",
        "sound/items/doubler.ogg",
        { "models/powerups/doubler.md3",
            NULL, NULL, NULL },
        /* icon */      "icons/doubler",
        /* pickup */    "Doubler",
        30,
        IT_PERSISTANT_POWERUP,
        PW_DOUBLER,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED item_doubler (.3 .3 1) (-16 -16 -16) (16 16 16) suspended redTeam blueTeam
    */
    {
        "item_ammoregen",
        "sound/items/ammoregen.ogg",
        { "models/powerups/ammo.md3",
            NULL, NULL, NULL },
        /* icon */      "icons/ammo_regen",
        /* pickup */    "Ammo Regen",
        30,
        IT_PERSISTANT_POWERUP,
        PW_AMMOREGEN,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED team_CTF_neutralflag (0 0 1) (-16 -16 -16) (16 16 16)
      Only in One Flag CTF games
      */
    {
        "team_CTF_neutralflag",
        NULL,
        { "models/flags/n_flag.md3",
            NULL, NULL, NULL },
        /* icon */      "icons/iconf_neutral1",
        /* pickup */    "Neutral Flag",
        0,
        IT_TEAM,
        PW_NEUTRALFLAG,
        /* precache */ "",
        /* sounds */ ""
    },

    {
        "item_redcube",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/orb/r_orb.md3",
            NULL, NULL, NULL },
        /* icon */      "icons/iconh_rorb",
        /* pickup */    "Red Cube",
        0,
        IT_TEAM,
        0,
        /* precache */ "",
        /* sounds */ ""
    },

    {
        "item_bluecube",
        "sound/misc/am_pkup.ogg",
        { "models/powerups/orb/b_orb.md3",
            NULL, NULL, NULL },
        /* icon */      "icons/iconh_borb",
        /* pickup */    "Blue Cube",
        0,
        IT_TEAM,
        0,
        /* precache */ "",
        /* sounds */ ""
    },
    /*QUAKED weapon_nailgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_nailgun",
        "sound/misc/w_pkup.ogg",
        { "models/weapons/nailgun/nailgun.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_nailgun",
        /* pickup */    "Nailgun",
        10,
        IT_WEAPON,
        WP_NAILGUN,
        /* precache */ "",
        /* sounds */ ""
    },

    /*QUAKED weapon_prox_launcher (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_prox_launcher",
        "sound/misc/w_pkup.ogg",
        { "models/weapons/proxmine/proxmine.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_proxlauncher",
        /* pickup */    "Prox Launcher",
        5,
        IT_WEAPON,
        WP_PROX_LAUNCHER,
        /* precache */ "",
        /* sounds */ "sound/weapons/proxmine/wstbtick.ogg "
            "sound/weapons/proxmine/wstbactv.ogg "
            "sound/weapons/proxmine/wstbimpl.ogg "
            "sound/weapons/proxmine/wstbimpm.ogg "
            "sound/weapons/proxmine/wstbimpd.ogg "
            "sound/weapons/proxmine/wstbactv.ogg"
    },

    /*QUAKED weapon_chaingun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
    */
    {
        "weapon_chaingun",
        "sound/misc/w_pkup.ogg",
        { "models/weapons/vulcan/vulcan.md3",
            NULL, NULL, NULL},
        /* icon */      "icons/iconw_chaingun",
        /* pickup */    "Chaingun",
        80,
        IT_WEAPON,
        WP_CHAINGUN,
        /* precache */ "",
        /* sounds */ "sound/weapons/vulcan/wvulwind.ogg"
    },
#endif

    // end of list marker
    {NULL}
};

int num_items = (sizeof(bg_itemlist)/sizeof(bg_itemlist[0]));

int main () {
    for ( int i = 1 ; i < num_items ; i++ ) {
        gitem_t item = bg_itemlist[i];

        if ( item.classname ) {
            printf("%s ", item.classname);
        }

        if ( item.pickup_sound ) {
            printf("%s ", item.pickup_sound);
        }

        if ( item.icon ) {
            printf("%s ", item.icon);
        }

        for ( int j = 0 ; j < MAX_ITEM_MODELS ; j++ ) {
            if ( item.world_model[j] ) {
                printf("%s ", item.world_model[j] );
            }
        }

        if ( item.precaches ) {
            printf("%s ", item.precaches);
        }

        if ( item.sounds) {
            printf("%s ", item.sounds);
        }

        printf("\n");
    }
}

