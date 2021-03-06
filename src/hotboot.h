/****************************************************************************
 *                   ^     +----- |  / ^     ^ |     | +-\                  *
 *                  / \    |      | /  |\   /| |     | |  \                 *
 *                 /   \   +---   |<   | \ / | |     | |  |                 *
 *                /-----\  |      | \  |  v  | |     | |  /                 *
 *               /       \ |      |  \ |     | +-----+ +-/                  *
 ****************************************************************************
 * AFKMud Copyright 1997-2003 by Roger Libiez (Samson),                     *
 * Levi Beckerson (Whir), Michael Ward (Tarl), Erik Wolfe (Dwip),           *
 * Cameron Carroll (Cam), Cyberfox, Karangi, Rathian, Raine, and Adjani.    *
 * All Rights Reserved.                                                     *
 *                                                                          *
 * Original SMAUG 1.8b written by Thoric (Derek Snider) with Altrag,        *
 * Blodkai, Haus, Narn, Scryn, Swordbearer, Tricops, Gorog, Rennard,        *
 * Grishnakh, Fireblade, Edmond, Conran, and Nivek.                         *
 *                                                                          *
 * Original MERC 2.1 code by Hatchet, Furey, and Kahn.                      *
 *                                                                          *
 * Original DikuMUD code by: Hans Staerfeldt, Katja Nyboe, Tom Madsen,      *
 * Michael Seifert, and Sebastian Hammer.                                   *
 *****************************************************************************
 *			         Hotboot support headers                          *
 ****************************************************************************/

#ifndef CH
#define CH(d)			((d)->original ? (d)->original : (d)->character)
#endif /* 
        */

#define HOTBOOT_DIR     "../hotboot/"   /* For storing objects across hotboots */
#define HOTBOOT_FILE    HOTBOOT_DIR "copyover.dat"  /* for hotboots */
#define MOB_FILE        HOTBOOT_DIR "mobs.dat"  /* For storing mobs across hotboots */
#define EXE_FILE        "../src/smaug"

/* warmboot code */
void hotboot_recover( void );

void load_world( void );

DECLARE_DO_FUN( do_hotboot ); /* Hotboot command - Samson 3-31-01 */
