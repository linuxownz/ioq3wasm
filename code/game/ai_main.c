/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//

/*****************************************************************************
 * name:        ai_main.c
 *
 * desc:        Quake3 bot AI
 *
 * $Archive: /MissionPack/code/game/ai_main.c $
 *
 *****************************************************************************/


#include "g_local.h"
#include "../qcommon/q_shared_server.h"
#include "../botlib/botlib.h"       //bot lib interface
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_weap.h"
//
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_chat.h"
#include "ai_cmd.h"
#include "ai_dmnet.h"
#include "ai_vcmd.h"

//
#include "chars.h"
#include "inv.h"
#include "syn.h"

// TODO
void SV_Trace( trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, int capsule );
int  SV_BotGetSnapshotEntity( int client, int ent );
void SV_GetConfigstring( int index, char *buffer, int bufferSize );
int  SV_BotGetConsoleMessage( int client, char *buf, int size );
int  SV_BotUserCommand ( int clientNum, usercmd_t *ucmd);
int  SV_BotLibSetup( void );
int  SV_BotLibShutdown( void );

// extern botlib_export_t * botlib_export;

//bot states
static bot_state_t botstates[MAX_CLIENTS];
//number of bots
int numbots;
//floating point time
float floattime;
//time to do a regular update
static float regularupdate_time = 0.0f;
//
int bot_interbreed;
int bot_interbreedmatchcount;
//
vmCvar_t bot_thinktime;
vmCvar_t bot_memorydump;
vmCvar_t bot_saveroutingcache;
vmCvar_t bot_pause;
vmCvar_t bot_report;
vmCvar_t bot_testsolid;
vmCvar_t bot_testclusters;
vmCvar_t bot_developer;
vmCvar_t bot_interbreedchar;
vmCvar_t bot_interbreedbots;
vmCvar_t bot_interbreedcycle;
vmCvar_t bot_interbreedwrite;


void ExitLevel( void );


/*
==================
BotAI_Print
==================
*/
void BotAI_Print(int type, char *fmt, ...) {
    char str[2048];
    va_list ap;

    va_start(ap, fmt);
    Q_vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);

    switch(type) {
        case PRT_MESSAGE: {
            Com_Printf("%s", str);
            break;
        }
        case PRT_WARNING: {
            Com_Printf( S_COLOR_YELLOW "Warning: %s", str );
            break;
        }
        case PRT_ERROR: {
            Com_Printf( S_COLOR_RED "Error: %s", str );
            break;
        }
        case PRT_FATAL: {
            Com_Printf( S_COLOR_RED "Fatal: %s", str );
            break;
        }
        case PRT_EXIT: {
            Com_Error( ERR_DROP, S_COLOR_RED "Exit: %s", str );
            break;
        }
        default: {
            Com_Printf( "unknown print type\n" );
            break;
        }
    }
}


/*
==================
BotAI_Trace
==================
*/
void BotAI_Trace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask) {
    trace_t trace;

    SV_Trace(&trace, start, mins, maxs, end, passent, contentmask, qfalse);
    //copy the trace information
    bsptrace->allsolid = trace.allsolid;
    bsptrace->startsolid = trace.startsolid;
    bsptrace->fraction = trace.fraction;
    VectorCopy(trace.endpos, bsptrace->endpos);
    bsptrace->plane.dist = trace.plane.dist;
    VectorCopy(trace.plane.normal, bsptrace->plane.normal);
    bsptrace->plane.signbits = trace.plane.signbits;
    bsptrace->plane.type = trace.plane.type;
    bsptrace->surface.value = 0;
    bsptrace->surface.flags = trace.surfaceFlags;
    bsptrace->ent = trace.entityNum;
    bsptrace->exp_dist = 0;
    bsptrace->sidenum = 0;
    bsptrace->contents = 0;
}

/*
==================
BotAI_GetClientState
==================
*/
int BotAI_GetClientState( int clientNum, playerState_t *state ) {
    gentity_t   *ent;

    ent = &g_entities[clientNum];
    if ( !ent->inuse ) {
        return qfalse;
    }
    if ( !ent->client ) {
        return qfalse;
    }

    memcpy( state, &ent->client->ps, sizeof(playerState_t) );
    return qtrue;
}

/*
==================
BotAI_GetEntityState
==================
*/
int BotAI_GetEntityState( int entityNum, entityState_t *state ) {
    gentity_t   *ent;

    ent = &g_entities[entityNum];
    memset( state, 0, sizeof(entityState_t) );
    if (!ent->inuse) return qfalse;
    if (!ent->r.linked) return qfalse;
    if (ent->r.svFlags & SVF_NOCLIENT) return qfalse;
    memcpy( state, &ent->s, sizeof(entityState_t) );
    return qtrue;
}

/*
==================
BotAI_GetSnapshotEntity
==================
*/
int BotAI_GetSnapshotEntity( int clientNum, int sequence, entityState_t *state ) {
    int     entNum;

    entNum = SV_BotGetSnapshotEntity( clientNum, sequence );
    if ( entNum == -1 ) {
        memset(state, 0, sizeof(entityState_t));
        return -1;
    }

    BotAI_GetEntityState( entNum, state );

    return sequence + 1;
}

/*
==================
BotAI_BotInitialChat
==================
*/
void BotAI_BotInitialChat( bot_state_t *bs, char *type, ... ) {
    int     i, mcontext;
    va_list ap;
    char    *p;
    char    *vars[MAX_MATCHVARIABLES];

    memset(vars, 0, sizeof(vars));
    va_start(ap, type);
    p = va_arg(ap, char *);
    for (i = 0; i < MAX_MATCHVARIABLES; i++) {
        if( !p ) {
            break;
        }
        vars[i] = p;
        p = va_arg(ap, char *);
    }
    va_end(ap);

    mcontext = BotSynonymContext(bs);

    BotInitialChat( bs->cs, type, mcontext, vars[0], vars[1], vars[2], vars[3], vars[4], vars[5], vars[6], vars[7] );
}


/*
==================
BotTestAAS
==================
*/
void BotTestAAS(vec3_t origin) {
    int areanum;
    aas_areainfo_t info;

    Cvar_Update(&bot_testsolid);
    Cvar_Update(&bot_testclusters);
    if (bot_testsolid.integer) {
        if (!AAS_Initialized()) return;
        areanum = BotPointAreaNum(origin);
        if (areanum) BotAI_Print(PRT_MESSAGE, "\rempty area");
        else BotAI_Print(PRT_MESSAGE, "\r^1SOLID area");
    }
    else if (bot_testclusters.integer) {
        if (!AAS_Initialized()) return;
        areanum = BotPointAreaNum(origin);
        if (!areanum)
            BotAI_Print(PRT_MESSAGE, "\r^1Solid!                              ");
        else {
            AAS_AreaInfo(areanum, &info);
            BotAI_Print(PRT_MESSAGE, "\rarea %d, cluster %d       ", areanum, info.cluster);
        }
    }
}

/*
==================
BotReportStatus
==================
*/
#if 0
// unused
static void BotReportStatus(bot_state_t *bs) {
    char goalname[MAX_MESSAGE_SIZE];
    char netname[MAX_MESSAGE_SIZE];
    char *leader, flagstatus[32];
    //
    ClientName(bs->client, netname, sizeof(netname));
    if (Q_stricmp(netname, bs->teamleader) == 0) leader = "L";
    else leader = " ";

    strcpy(flagstatus, "  ");
    if (gametype == GT_CTF) {
        if (BotCTFCarryingFlag(bs)) {
            if (BotTeam(bs) == TEAM_RED) strcpy(flagstatus, S_COLOR_RED"F ");
            else strcpy(flagstatus, S_COLOR_BLUE"F ");
        }
    }
#ifdef MISSIONPACK
    else if (gametype == GT_1FCTF) {
        if (Bot1FCTFCarryingFlag(bs)) {
            if (BotTeam(bs) == TEAM_RED) strcpy(flagstatus, S_COLOR_RED"F ");
            else strcpy(flagstatus, S_COLOR_BLUE"F ");
        }
    }
    else if (gametype == GT_HARVESTER) {
        if (BotHarvesterCarryingCubes(bs)) {
            if (BotTeam(bs) == TEAM_RED) Com_sprintf(flagstatus, sizeof(flagstatus), S_COLOR_RED"%2d", bs->inventory[INVENTORY_REDCUBE]);
            else Com_sprintf(flagstatus, sizeof(flagstatus), S_COLOR_BLUE"%2d", bs->inventory[INVENTORY_BLUECUBE]);
        }
    }
#endif

    switch(bs->ltgtype) {
        case LTG_TEAMHELP:
        {
            EasyClientName(bs->teammate, goalname, sizeof(goalname));
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: helping %s\n", netname, leader, flagstatus, goalname);
            break;
        }
        case LTG_TEAMACCOMPANY:
        {
            EasyClientName(bs->teammate, goalname, sizeof(goalname));
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: accompanying %s\n", netname, leader, flagstatus, goalname);
            break;
        }
        case LTG_DEFENDKEYAREA:
        {
            BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: defending %s\n", netname, leader, flagstatus, goalname);
            break;
        }
        case LTG_GETITEM:
        {
            BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: getting item %s\n", netname, leader, flagstatus, goalname);
            break;
        }
        case LTG_KILL:
        {
            ClientName(bs->teamgoal.entitynum, goalname, sizeof(goalname));
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: killing %s\n", netname, leader, flagstatus, goalname);
            break;
        }
        case LTG_CAMP:
        case LTG_CAMPORDER:
        {
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: camping\n", netname, leader, flagstatus);
            break;
        }
        case LTG_PATROL:
        {
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: patrolling\n", netname, leader, flagstatus);
            break;
        }
        case LTG_GETFLAG:
        {
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: capturing flag\n", netname, leader, flagstatus);
            break;
        }
        case LTG_RUSHBASE:
        {
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: rushing base\n", netname, leader, flagstatus);
            break;
        }
        case LTG_RETURNFLAG:
        {
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: returning flag\n", netname, leader, flagstatus);
            break;
        }
        case LTG_ATTACKENEMYBASE:
        {
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: attacking the enemy base\n", netname, leader, flagstatus);
            break;
        }
        case LTG_HARVEST:
        {
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: harvesting\n", netname, leader, flagstatus);
            break;
        }
        default:
        {
            BotAI_Print(PRT_MESSAGE, "%-20s%s%s: roaming\n", netname, leader, flagstatus);
            break;
        }
    }
}
#endif

/*
==================
BotTeamplayReport
==================
*/
#if 0
// unused
static void BotTeamplayReport(void) {
    int i;
    char buf[MAX_INFO_STRING];

    BotAI_Print(PRT_MESSAGE, S_COLOR_RED"RED\n");
    for (i = 0; i < level.maxclients; i++) {
        //
        if ( !botstates[i] || !botstates[i]->inuse ) continue;
        //
        SV_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
        //if no config string or no name
        if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n"))) continue;
        //skip spectators
        if (atoi(Info_ValueForKey(buf, "t")) == TEAM_RED) {
            BotReportStatus(botstates[i]);
        }
    }
    BotAI_Print(PRT_MESSAGE, S_COLOR_BLUE"BLUE\n");
    for (i = 0; i < level.maxclients; i++) {
        //
        if ( !botstates[i] || !botstates[i]->inuse ) continue;
        //
        SV_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
        //if no config string or no name
        if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n"))) continue;
        //skip spectators
        if (atoi(Info_ValueForKey(buf, "t")) == TEAM_BLUE) {
            BotReportStatus(botstates[i]);
        }
    }
}
#endif

/*
==================
BotSetInfoConfigString
==================
*/
static void BotSetInfoConfigString(bot_state_t *bs) {
    char goalname[MAX_MESSAGE_SIZE];
    char netname[MAX_MESSAGE_SIZE];
    char action[MAX_MESSAGE_SIZE];
    char *leader, carrying[32], *cs;
    bot_goal_t goal;
    //
    ClientName(bs->client, netname, sizeof(netname));
    if (Q_stricmp(netname, bs->teamleader) == 0) leader = "L";
    else leader = " ";

    strcpy(carrying, "  ");
    if (gametype == GT_CTF) {
        if (BotCTFCarryingFlag(bs)) {
            strcpy(carrying, "F ");
        }
    }
#ifdef MISSIONPACK
    else if (gametype == GT_1FCTF) {
        if (Bot1FCTFCarryingFlag(bs)) {
            strcpy(carrying, "F ");
        }
    }
    else if (gametype == GT_HARVESTER) {
        if (BotHarvesterCarryingCubes(bs)) {
            if (BotTeam(bs) == TEAM_RED) Com_sprintf(carrying, sizeof(carrying), "%2d", bs->inventory[INVENTORY_REDCUBE]);
            else Com_sprintf(carrying, sizeof(carrying), "%2d", bs->inventory[INVENTORY_BLUECUBE]);
        }
    }
#endif

    switch(bs->ltgtype) {
        case LTG_TEAMHELP:
        {
            EasyClientName(bs->teammate, goalname, sizeof(goalname));
            Com_sprintf(action, sizeof(action), "helping %s", goalname);
            break;
        }
        case LTG_TEAMACCOMPANY:
        {
            EasyClientName(bs->teammate, goalname, sizeof(goalname));
            Com_sprintf(action, sizeof(action), "accompanying %s", goalname);
            break;
        }
        case LTG_DEFENDKEYAREA:
        {
            BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
            Com_sprintf(action, sizeof(action), "defending %s", goalname);
            break;
        }
        case LTG_GETITEM:
        {
            BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
            Com_sprintf(action, sizeof(action), "getting item %s", goalname);
            break;
        }
        case LTG_KILL:
        {
            ClientName(bs->teamgoal.entitynum, goalname, sizeof(goalname));
            Com_sprintf(action, sizeof(action), "killing %s", goalname);
            break;
        }
        case LTG_CAMP:
        case LTG_CAMPORDER:
        {
            Com_sprintf(action, sizeof(action), "camping");
            break;
        }
        case LTG_PATROL:
        {
            Com_sprintf(action, sizeof(action), "patrolling");
            break;
        }
        case LTG_GETFLAG:
        {
            Com_sprintf(action, sizeof(action), "capturing flag");
            break;
        }
        case LTG_RUSHBASE:
        {
            Com_sprintf(action, sizeof(action), "rushing base");
            break;
        }
        case LTG_RETURNFLAG:
        {
            Com_sprintf(action, sizeof(action), "returning flag");
            break;
        }
        case LTG_ATTACKENEMYBASE:
        {
            Com_sprintf(action, sizeof(action), "attacking the enemy base");
            break;
        }
        case LTG_HARVEST:
        {
            Com_sprintf(action, sizeof(action), "harvesting");
            break;
        }
        default:
        {
            BotGetTopGoal(bs->gs, &goal);
            BotGoalName(goal.number, goalname, sizeof(goalname));
            Com_sprintf(action, sizeof(action), "roaming %s", goalname);
            break;
        }
    }
    cs = va("l\\%s\\c\\%s\\a\\%s", leader, carrying, action);
    SV_SetConfigstring (CS_BOTINFO + bs->client, cs);
}

/*
==============
BotUpdateInfoConfigStrings
==============
*/
static void BotUpdateInfoConfigStrings(void) {
    int i;
    char buf[MAX_INFO_STRING];

    for (i = 0; i < level.maxclients; i++) {
        //
        if ( /*!botstates[i] || */!botstates[i].inuse )
            continue;
        //
        SV_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
        //if no config string or no name
        if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n")))
            continue;
        BotSetInfoConfigString(&botstates[i]);
    }
}

/*
==============
BotInterbreedBots
==============
*/
static void BotInterbreedBots(void) {
    float ranks[MAX_CLIENTS];
    int parent1, parent2, child;
    int i;

    // get rankings for all the bots
    for (i = 0; i < MAX_CLIENTS; i++) {
        if ( /*botstates[i] && */botstates[i].inuse ) {
            ranks[i] = botstates[i].num_kills * 2 - botstates[i].num_deaths;
        }
        else {
            ranks[i] = -1;
        }
    }

    if (GeneticParentsAndChildSelection(MAX_CLIENTS, ranks, &parent1, &parent2, &child)) {
        BotInterbreedGoalFuzzyLogic(botstates[parent1].gs, botstates[parent2].gs, botstates[child].gs);
        BotMutateGoalFuzzyLogic(botstates[child].gs, 1);
    }
    // reset the kills and deaths
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (/*botstates[i] && */botstates[i].inuse) {
            botstates[i].num_kills  = 0;
            botstates[i].num_deaths = 0;
        }
    }
}

/*
==============
BotWriteInterbreeded
==============
*/
static void BotWriteInterbreeded(char *filename) {
    float rank, bestrank;
    int i, bestbot;

    bestrank = 0;
    bestbot = -1;
    // get the best bot
    for (i = 0; i < MAX_CLIENTS; i++) {
        if ( /*botstates[i] &&*/ botstates[i].inuse ) {
            rank = botstates[i].num_kills * 2 - botstates[i].num_deaths;
        }
        else {
            rank = -1;
        }
        if (rank > bestrank) {
            bestrank = rank;
            bestbot = i;
        }
    }
    if (bestbot >= 0) {
        //write out the new goal fuzzy logic
        BotSaveGoalFuzzyLogic(botstates[bestbot].gs, filename);
    }
}

/*
==============
BotInterbreedEndMatch

add link back into ExitLevel?
==============
*/
void BotInterbreedEndMatch(void) {

    if (!bot_interbreed) return;
    bot_interbreedmatchcount++;
    if (bot_interbreedmatchcount >= bot_interbreedcycle.integer) {
        bot_interbreedmatchcount = 0;
        //
        Cvar_Update(&bot_interbreedwrite);
        if (strlen(bot_interbreedwrite.string)) {
            BotWriteInterbreeded(bot_interbreedwrite.string);
            Cvar_Set("bot_interbreedwrite", "");
        }
        BotInterbreedBots();
    }
}

/*
==============
BotInterbreeding
==============
*/
static void BotInterbreeding(void) {
    int i;

    Cvar_Update(&bot_interbreedchar);
    if (!strlen(bot_interbreedchar.string)) return;
    //make sure we are in tournament mode
    if (gametype != GT_TOURNAMENT) {
        Cvar_Set("g_gametype", va("%d", GT_TOURNAMENT));
        ExitLevel();
        return;
    }
    //shutdown all the bots
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (/*botstates[i] && */botstates[i].inuse) {
            BotAIShutdownClient(botstates[i].client, qfalse);
        }
    }
    //make sure all item weight configs are reloaded and Not shared
    /*botlib_*/Export_BotLibVarSet("bot_reloadcharacters", "1");
    //add a number of bots using the desired bot character
    for (i = 0; i < bot_interbreedbots.integer; i++) {
        Cbuf_ExecuteText( EXEC_INSERT, va("addbot %s 4 free %i %s%d\n", bot_interbreedchar.string, i * 50, bot_interbreedchar.string, i) );
    }
    //
    Cvar_Set("bot_interbreedchar", "");
    bot_interbreed = qtrue;
}

/*
==============
BotEntityInfo
==============
*/
void BotEntityInfo(int entnum, aas_entityinfo_t *info) {
    AAS_EntityInfo(entnum, info);
}

/*
==============
NumBots
==============
*/
int NumBots(void) {
    return numbots;
}

/*
==============
BotTeamLeader
==============
*/
int BotTeamLeader(bot_state_t *bs) {
    int leader;

    leader = ClientFromName(bs->teamleader);
    if (leader < 0) return qfalse;
    if (/*!botstates[leader] || */!botstates[leader].inuse) return qfalse;
    return qtrue;
}

/*
==============
AngleDifference
==============
*/
static float AngleDifference(float ang1, float ang2) {
    float diff;

    diff = ang1 - ang2;
    if (ang1 > ang2) {
        if (diff > 180.0) diff -= 360.0;
    }
    else {
        if (diff < -180.0) diff += 360.0;
    }
    return diff;
}

/*
==============
BotChangeViewAngle
==============
*/
static float BotChangeViewAngle(float angle, float ideal_angle, float speed) {
    float move;

    angle = AngleMod(angle);
    ideal_angle = AngleMod(ideal_angle);
    if (angle == ideal_angle) return angle;
    move = ideal_angle - angle;
    if (ideal_angle > angle) {
        if (move > 180.0) move -= 360.0;
    }
    else {
        if (move < -180.0) move += 360.0;
    }
    if (move > 0) {
        if (move > speed) move = speed;
    }
    else {
        if (move < -speed) move = -speed;
    }
    return AngleMod(angle + move);
}

/*
==============
BotChangeViewAngles
==============
*/
static void BotChangeViewAngles(bot_state_t *bs, float thinktime) {
    float diff, factor, maxchange, anglespeed, disired_speed;
    int i;

    if (bs->ideal_viewangles[PITCH] > 180) bs->ideal_viewangles[PITCH] -= 360;
    //
    if (bs->enemy >= 0) {
        factor = Characteristic_BFloat(bs->character, CHARACTERISTIC_VIEW_FACTOR, 0.01f, 1);
        maxchange = Characteristic_BFloat(bs->character, CHARACTERISTIC_VIEW_MAXCHANGE, 1, 1800);
    }
    else {
        factor = 0.05f;
        maxchange = 360;
    }
    if (maxchange < 240) maxchange = 240;
    maxchange *= thinktime;
    for (i = 0; i < 2; i++) {
        //
        if (bot_challenge.integer) {
            //smooth slowdown view model
            diff = fabs(AngleDifference(bs->viewangles[i], bs->ideal_viewangles[i]));
            anglespeed = diff * factor;
            if (anglespeed > maxchange) anglespeed = maxchange;
            bs->viewangles[i] = BotChangeViewAngle(bs->viewangles[i],
                                            bs->ideal_viewangles[i], anglespeed);
        }
        else {
            //over reaction view model
            bs->viewangles[i] = AngleMod(bs->viewangles[i]);
            bs->ideal_viewangles[i] = AngleMod(bs->ideal_viewangles[i]);
            diff = AngleDifference(bs->viewangles[i], bs->ideal_viewangles[i]);
            disired_speed = diff * factor;
            bs->viewanglespeed[i] += (bs->viewanglespeed[i] - disired_speed);
            if (bs->viewanglespeed[i] > 180) bs->viewanglespeed[i] = maxchange;
            if (bs->viewanglespeed[i] < -180) bs->viewanglespeed[i] = -maxchange;
            anglespeed = bs->viewanglespeed[i];
            if (anglespeed > maxchange) anglespeed = maxchange;
            if (anglespeed < -maxchange) anglespeed = -maxchange;
            bs->viewangles[i] += anglespeed;
            bs->viewangles[i] = AngleMod(bs->viewangles[i]);
            //demping
            bs->viewanglespeed[i] *= 0.45 * (1 - factor);
        }
        //BotAI_Print(PRT_MESSAGE, "ideal_angles %f %f\n", bs->ideal_viewangles[0], bs->ideal_viewangles[1], bs->ideal_viewangles[2]);`
        //bs->viewangles[i] = bs->ideal_viewangles[i];
    }
    //bs->viewangles[PITCH] = 0;
    if (bs->viewangles[PITCH] > 180) bs->viewangles[PITCH] -= 360;
    //elementary action: view
    EA_View(bs->client, bs->viewangles);
}

/*
==============
BotInputToUserCommand
==============
*/
static void BotInputToUserCommand(bot_input_t *bi, usercmd_t *ucmd, int delta_angles[3], int time) {
    vec3_t angles, forward, right;
    short temp;
    int j;
    float f, r, u, m;

    //clear the whole structure
    memset(ucmd, 0, sizeof(usercmd_t));
    //the duration for the user command in milli seconds
    ucmd->serverTime = time;
    //
    if (bi->actionflags & ACTION_DELAYEDJUMP) {
        bi->actionflags |= ACTION_JUMP;
        bi->actionflags &= ~ACTION_DELAYEDJUMP;
    }
    //set the buttons
    if (bi->actionflags & ACTION_RESPAWN) ucmd->buttons = BUTTON_ATTACK;
    if (bi->actionflags & ACTION_ATTACK) ucmd->buttons |= BUTTON_ATTACK;
    if (bi->actionflags & ACTION_TALK) ucmd->buttons |= BUTTON_TALK;
    if (bi->actionflags & ACTION_GESTURE) ucmd->buttons |= BUTTON_GESTURE;
    if (bi->actionflags & ACTION_USE) ucmd->buttons |= BUTTON_USE_HOLDABLE;
    if (bi->actionflags & ACTION_WALK) ucmd->buttons |= BUTTON_WALKING;
    if (bi->actionflags & ACTION_AFFIRMATIVE) ucmd->buttons |= BUTTON_AFFIRMATIVE;
    if (bi->actionflags & ACTION_NEGATIVE) ucmd->buttons |= BUTTON_NEGATIVE;
    if (bi->actionflags & ACTION_GETFLAG) ucmd->buttons |= BUTTON_GETFLAG;
    if (bi->actionflags & ACTION_GUARDBASE) ucmd->buttons |= BUTTON_GUARDBASE;
    if (bi->actionflags & ACTION_PATROL) ucmd->buttons |= BUTTON_PATROL;
    if (bi->actionflags & ACTION_FOLLOWME) ucmd->buttons |= BUTTON_FOLLOWME;
    //
    ucmd->weapon = bi->weapon;
    //set the view angles
    //NOTE: the ucmd->angles are the angles WITHOUT the delta angles
    ucmd->angles[PITCH] = ANGLE2SHORT(bi->viewangles[PITCH]);
    ucmd->angles[YAW] = ANGLE2SHORT(bi->viewangles[YAW]);
    ucmd->angles[ROLL] = ANGLE2SHORT(bi->viewangles[ROLL]);
    //subtract the delta angles
    for (j = 0; j < 3; j++) {
        temp = ucmd->angles[j] - delta_angles[j];
        /*NOTE: disabled because temp should be mod first
        if ( j == PITCH ) {
            // don't let the player look up or down more than 90 degrees
            if ( temp > 16000 ) temp = 16000;
            else if ( temp < -16000 ) temp = -16000;
        }
        */
        ucmd->angles[j] = temp;
    }
    //NOTE: movement is relative to the REAL view angles
    //get the horizontal forward and right vector
    //get the pitch in the range [-180, 180]
    if (bi->dir[2]) angles[PITCH] = bi->viewangles[PITCH];
    else angles[PITCH] = 0;
    angles[YAW] = bi->viewangles[YAW];
    angles[ROLL] = 0;
    AngleVectors(angles, forward, right, NULL);
    //bot input speed is in the range [0, 400]
    bi->speed = bi->speed * 127 / 400;
    //set the view independent movement
    f = DotProduct(forward, bi->dir);
    r = DotProduct(right, bi->dir);
    u = fabs(forward[2]) * bi->dir[2];
    m = fabs(f);

    if (fabs(r) > m) {
        m = fabs(r);
    }

    if (fabs(u) > m) {
        m = fabs(u);
    }

    if (m > 0) {
        f *= bi->speed / m;
        r *= bi->speed / m;
        u *= bi->speed / m;
    }

    ucmd->forwardmove = f;
    ucmd->rightmove = r;
    ucmd->upmove = u;

    if (bi->actionflags & ACTION_MOVEFORWARD) ucmd->forwardmove = 127;
    if (bi->actionflags & ACTION_MOVEBACK) ucmd->forwardmove = -127;
    if (bi->actionflags & ACTION_MOVELEFT) ucmd->rightmove = -127;
    if (bi->actionflags & ACTION_MOVERIGHT) ucmd->rightmove = 127;
    //jump/moveup
    if (bi->actionflags & ACTION_JUMP) ucmd->upmove = 127;
    //crouch/movedown
    if (bi->actionflags & ACTION_CROUCH) ucmd->upmove = -127;
}

/*
==============
BotUpdateInput
==============
*/
static void BotUpdateInput(bot_state_t *bs, int time, int elapsed_time) {
    bot_input_t bi;
    int j;

    //add the delta angles to the bot's current view angles
    for (j = 0; j < 3; j++) {
        bs->viewangles[j] = AngleMod(bs->viewangles[j] + SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
    }
    //change the bot view angles
    BotChangeViewAngles(bs, (float) elapsed_time / 1000);
    //retrieve the bot input
    EA_GetInput(bs->client, (float) time / 1000, &bi);
    //respawn hack
    if (bi.actionflags & ACTION_RESPAWN) {
        if (bs->lastucmd.buttons & BUTTON_ATTACK) bi.actionflags &= ~(ACTION_RESPAWN|ACTION_ATTACK);
    }
    //convert the bot input to a usercmd
    BotInputToUserCommand(&bi, &bs->lastucmd, bs->cur_ps.delta_angles, time);
    //subtract the delta angles
    for (j = 0; j < 3; j++) {
        bs->viewangles[j] = AngleMod(bs->viewangles[j] - SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
    }
}

/*
==============
BotAIRegularUpdate
==============
*/
static void BotAIRegularUpdate(void) {
    if (regularupdate_time < FloatTime()) {
        BotUpdateEntityItems();
        regularupdate_time = FloatTime() + 0.3;
    } else {
        regularupdate_time = FloatTime();
    }
}

/*
==============
RemoveColorEscapeSequences
==============
*/
static void RemoveColorEscapeSequences( char *text ) {
    int i, l;

    l = 0;
    for ( i = 0; text[i]; i++ ) {
        if (Q_IsColorString(&text[i])) {
            i++;
            continue;
        }
        if (text[i] > 0x7E)
            continue;
        text[l++] = text[i];
    }
    text[l] = '\0';
}

/*
==============
BotAI
==============
*/
static int BotAI(int client, float thinktime) {
    bot_state_t *bs;
    char buf[1024], *args;
    int j;

    EA_ResetInput(client);
    //
    bs = &botstates[client];
    if (!bs || !bs->inuse) {
        BotAI_Print(PRT_FATAL, "BotAI: client %d is not setup\n", client);
        return qfalse;
    }

    //retrieve the current client state
    if (!BotAI_GetClientState(client, &bs->cur_ps)) {
        BotAI_Print(PRT_FATAL, "BotAI: failed to get player state for player %d\n", client);
        return qfalse;
    }
    //retrieve any waiting server commands
    //while( trap_BotGetServerCommand(client, buf, sizeof(buf)) ) { // TODO
    while( SV_BotGetConsoleMessage(client, buf, sizeof(buf)) ) {
        //have buf point to the command and args to the command arguments
        args = strchr( buf, ' ');
        if (!args) continue;
        *args++ = '\0';

        //remove color espace sequences from the arguments
        RemoveColorEscapeSequences( args );

        if (!Q_stricmp(buf, "cp "))
            { /*CenterPrintf*/ }
        else if (!Q_stricmp(buf, "cs"))
            { /*ConfigStringModified*/ }
        else if (!Q_stricmp(buf, "print")) {
            //remove first and last quote from the chat message
            memmove(args, args+1, strlen(args));
            args[strlen(args)-1] = '\0';
            BotQueueConsoleMessage(bs->cs, CMS_NORMAL, args);
        }
        else if (!Q_stricmp(buf, "chat")) {
            //remove first and last quote from the chat message
            memmove(args, args+1, strlen(args));
            args[strlen(args)-1] = '\0';
            BotQueueConsoleMessage(bs->cs, CMS_CHAT, args);
        }
        else if (!Q_stricmp(buf, "tchat")) {
            //remove first and last quote from the chat message
            memmove(args, args+1, strlen(args));
            args[strlen(args)-1] = '\0';
            BotQueueConsoleMessage(bs->cs, CMS_CHAT, args);
        }
#ifdef MISSIONPACK
        else if (!Q_stricmp(buf, "vchat")) {
            BotVoiceChatCommand(bs, SAY_ALL, args);
        }
        else if (!Q_stricmp(buf, "vtchat")) {
            BotVoiceChatCommand(bs, SAY_TEAM, args);
        }
        else if (!Q_stricmp(buf, "vtell")) {
            BotVoiceChatCommand(bs, SAY_TELL, args);
        }
#endif
        else if (!Q_stricmp(buf, "scores"))
            { /*FIXME: parse scores?*/ }
        else if (!Q_stricmp(buf, "clientLevelShot"))
            { /*ignore*/ }
    }
    //add the delta angles to the bot's current view angles
    for (j = 0; j < 3; j++) {
        bs->viewangles[j] = AngleMod(bs->viewangles[j] + SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
    }
    //increase the local time of the bot
    bs->ltime += thinktime;
    //
    bs->thinktime = thinktime;
    //origin of the bot
    VectorCopy(bs->cur_ps.origin, bs->origin);
    //eye coordinates of the bot
    VectorCopy(bs->cur_ps.origin, bs->eye);
    bs->eye[2] += bs->cur_ps.viewheight;
    //get the area the bot is in
    bs->areanum = BotPointAreaNum(bs->origin);
    //the real AI
    BotDeathmatchAI(bs, thinktime);
    //set the weapon selection every AI frame
    EA_SelectWeapon(bs->client, bs->weaponnum);
    //subtract the delta angles
    for (j = 0; j < 3; j++) {
        bs->viewangles[j] = AngleMod(bs->viewangles[j] - SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
    }
    //everything was ok
    return qtrue;
}

/*
==================
BotScheduleBotThink
==================
*/
static void BotScheduleBotThink(void) {
    int i, botnum;

    botnum = 0;

    for( i = 0; i < MAX_CLIENTS; i++ ) {
        if( /*!botstates[i] || */!botstates[i].inuse ) {
            continue;
        }
        //initialize the bot think residual time
        botstates[i].botthink_residual = bot_thinktime.integer * botnum / numbots;
        botnum++;
    }
}

/*
==============
BotWriteSessionData
==============
*/
static void BotWriteSessionData(bot_state_t *bs) {
    const char  *s;
    const char  *var;

    s = va(
            "%i %i %i %i %i %i %i %i"
            " %f %f %f"
            " %f %f %f"
            " %f %f %f"
            " %f",
        bs->lastgoal_decisionmaker,
        bs->lastgoal_ltgtype,
        bs->lastgoal_teammate,
        bs->lastgoal_teamgoal.areanum,
        bs->lastgoal_teamgoal.entitynum,
        bs->lastgoal_teamgoal.flags,
        bs->lastgoal_teamgoal.iteminfo,
        bs->lastgoal_teamgoal.number,
        bs->lastgoal_teamgoal.origin[0],
        bs->lastgoal_teamgoal.origin[1],
        bs->lastgoal_teamgoal.origin[2],
        bs->lastgoal_teamgoal.mins[0],
        bs->lastgoal_teamgoal.mins[1],
        bs->lastgoal_teamgoal.mins[2],
        bs->lastgoal_teamgoal.maxs[0],
        bs->lastgoal_teamgoal.maxs[1],
        bs->lastgoal_teamgoal.maxs[2],
        bs->formation_dist
        );

    var = va( "botsession%i", bs->client );

    Cvar_Set( var, s );
}

/*
==============
BotReadSessionData
==============
*/
static void BotReadSessionData(bot_state_t *bs) {
    char    s[MAX_STRING_CHARS];
    const char  *var;

    var = va( "botsession%i", bs->client );
    Cvar_VariableStringBuffer( var, s, sizeof(s) );

    sscanf(s,
            "%i %i %i %i %i %i %i %i"
            " %f %f %f"
            " %f %f %f"
            " %f %f %f"
            " %f",
        &bs->lastgoal_decisionmaker,
        &bs->lastgoal_ltgtype,
        &bs->lastgoal_teammate,
        &bs->lastgoal_teamgoal.areanum,
        &bs->lastgoal_teamgoal.entitynum,
        &bs->lastgoal_teamgoal.flags,
        &bs->lastgoal_teamgoal.iteminfo,
        &bs->lastgoal_teamgoal.number,
        &bs->lastgoal_teamgoal.origin[0],
        &bs->lastgoal_teamgoal.origin[1],
        &bs->lastgoal_teamgoal.origin[2],
        &bs->lastgoal_teamgoal.mins[0],
        &bs->lastgoal_teamgoal.mins[1],
        &bs->lastgoal_teamgoal.mins[2],
        &bs->lastgoal_teamgoal.maxs[0],
        &bs->lastgoal_teamgoal.maxs[1],
        &bs->lastgoal_teamgoal.maxs[2],
        &bs->formation_dist
        );
}

/*
==============
BotAISetupClient
==============
*/
int BotAISetupClient(int client, struct bot_settings_s *settings, qboolean restart) {
    char filename[144], name[144], gender[144];
    bot_state_t *bs;
    int errnum;

    if (!botstates[client].inuse) {
        //botstates[client] = G_Alloc(sizeof(bot_state_t));
        memset(&botstates[client], 0, sizeof(botstates[0]));
    }

    bs = &botstates[client];

    if (!bs) {
        return qfalse;
    }

    if (bs && bs->inuse) {
        BotAI_Print(PRT_FATAL, "BotAISetupClient: client %d already setup\n", client);
        return qfalse;
    }

    if (!AAS_Initialized()) {
        BotAI_Print(PRT_FATAL, "AAS not initialized\n");
        return qfalse;
    }

    //load the bot character
    bs->character = BotLoadCharacter(settings->characterfile, settings->skill);
    if (!bs->character) {
        BotAI_Print(PRT_FATAL, "couldn't load skill %f from %s\n", settings->skill, settings->characterfile);
        return qfalse;
    }
    //copy the settings
    memcpy(&bs->settings, settings, sizeof(bot_settings_t));
    //allocate a goal state
    bs->gs = BotAllocGoalState(client);
    //load the item weights
    Characteristic_String(bs->character, CHARACTERISTIC_ITEMWEIGHTS, filename, sizeof(filename));
    errnum = BotLoadItemWeights(bs->gs, filename);
    if (errnum != BLERR_NOERROR) {
        BotFreeGoalState(bs->gs);
        return qfalse;
    }
    //allocate a weapon state
    bs->ws = BotAllocWeaponState();
    //load the weapon weights
    Characteristic_String(bs->character, CHARACTERISTIC_WEAPONWEIGHTS, filename, sizeof(filename));
    errnum = BotLoadWeaponWeights(bs->ws, filename);
    if (errnum != BLERR_NOERROR) {
        BotFreeGoalState(bs->gs);
        BotFreeWeaponState(bs->ws);
        return qfalse;
    }
    //allocate a chat state
    bs->cs = BotAllocChatState();
    //load the chat file
    Characteristic_String(bs->character, CHARACTERISTIC_CHAT_FILE, filename, sizeof(filename));
    Characteristic_String(bs->character, CHARACTERISTIC_CHAT_NAME, name, sizeof(name));
    errnum = BotLoadChatFile(bs->cs, filename, name);
    if (errnum != BLERR_NOERROR) {
        BotFreeChatState(bs->cs);
        BotFreeGoalState(bs->gs);
        BotFreeWeaponState(bs->ws);
        return qfalse;
    }
    //get the gender characteristic
    Characteristic_String(bs->character, CHARACTERISTIC_GENDER, gender, sizeof(gender));
    //set the chat gender
    if (*gender == 'f' || *gender == 'F') BotSetChatGender(bs->cs, CHAT_GENDERFEMALE);
    else if (*gender == 'm' || *gender == 'M') BotSetChatGender(bs->cs, CHAT_GENDERMALE);
    else BotSetChatGender(bs->cs, CHAT_GENDERLESS);

    bs->inuse = qtrue;
    bs->client = client;
    bs->entitynum = client;
    bs->setupcount = 4;
    bs->entergame_time = FloatTime();
    bs->ms = BotAllocMoveState();
    bs->walker = Characteristic_BFloat(bs->character, CHARACTERISTIC_WALKER, 0, 1);
    numbots++;

    if (Cvar_VariableIntegerValue("bot_testichat")) {
        Export_BotLibVarSet("bot_testichat", "1");
        BotChatTest(bs);
    }
    //NOTE: reschedule the bot thinking
    BotScheduleBotThink();
    //if interbreeding start with a mutation
    if (bot_interbreed) {
        BotMutateGoalFuzzyLogic(bs->gs, 1);
    }
    // if we kept the bot client
    if (restart) {
        BotReadSessionData(bs);
    }
    //bot has been setup successfully
    return qtrue;
}

/*
==============
BotAIShutdownClient
==============
*/
int BotAIShutdownClient(int client, qboolean restart) {
    bot_state_t *bs;

    bs = &botstates[client];
    if (!bs || !bs->inuse) {
        // BotAISetupClient failed
        BotAI_Print(PRT_ERROR, "BotAIShutdownClient: client %d already shutdown\n", client);
        return qfalse;
    }

    if (restart) {
        BotWriteSessionData(bs);
    }

    if (BotChat_ExitGame(bs)) {
        BotEnterChat(bs->cs, bs->client, CHAT_ALL);
    }

    BotFreeMoveState(bs->ms);
    //free the goal state
    BotFreeGoalState(bs->gs);
    //free the chat file
    BotFreeChatState(bs->cs);
    //free the weapon weights
    BotFreeWeaponState(bs->ws);
    //free the bot character
    BotFreeCharacter(bs->character);
    //
    BotFreeWaypoints(bs->checkpoints);
    BotFreeWaypoints(bs->patrolpoints);
    //clear activate goal stack
    BotClearActivateGoalStack(bs);
    //clear the bot state
    memset(bs, 0, sizeof(bot_state_t));
    //set the inuse flag to qfalse
    bs->inuse = qfalse;
    //there's one bot less
    numbots--;
    //everything went ok
    return qtrue;
}

/*
==============
BotResetState

called when a bot enters the intermission or observer mode and
when the level is changed
==============
*/
void BotResetState(bot_state_t *bs) {
    int client, entitynum, inuse;
    int movestate, goalstate, chatstate, weaponstate;
    bot_settings_t settings;
    int character;
    playerState_t ps;                           //current player state
    float entergame_time;

    //save some things that should not be reset here
    memcpy(&settings, &bs->settings, sizeof(bot_settings_t));
    memcpy(&ps, &bs->cur_ps, sizeof(playerState_t));
    inuse = bs->inuse;
    client = bs->client;
    entitynum = bs->entitynum;
    character = bs->character;
    movestate = bs->ms;
    goalstate = bs->gs;
    chatstate = bs->cs;
    weaponstate = bs->ws;
    entergame_time = bs->entergame_time;
    //free checkpoints and patrol points
    BotFreeWaypoints(bs->checkpoints);
    BotFreeWaypoints(bs->patrolpoints);
    //reset the whole state
    memset(bs, 0, sizeof(bot_state_t));
    //copy back some state stuff that should not be reset
    bs->ms = movestate;
    bs->gs = goalstate;
    bs->cs = chatstate;
    bs->ws = weaponstate;
    memcpy(&bs->cur_ps, &ps, sizeof(playerState_t));
    memcpy(&bs->settings, &settings, sizeof(bot_settings_t));
    bs->inuse = inuse;
    bs->client = client;
    bs->entitynum = entitynum;
    bs->character = character;
    bs->entergame_time = entergame_time;
    //reset several states
    if (bs->ms) BotResetMoveState(bs->ms);
    if (bs->gs) BotResetGoalState(bs->gs);
    if (bs->ws) BotResetWeaponState(bs->ws);
    if (bs->gs) BotResetAvoidGoals(bs->gs);
    if (bs->ms) BotResetAvoidReach(bs->ms);
}

/*
==============
BotAILoadMap
==============
*/
int BotAILoadMap( int restart ) {
    int         i;
    vmCvar_t    mapname;

    if (!restart) {
        Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
        /*botlib_*/Export_BotLibLoadMap( mapname.string );
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (/*botstates[i] && */botstates[i].inuse) {
            BotResetState( &botstates[i] );
            botstates[i].setupcount = 4;
        }
    }

    BotSetupDeathmatchAI();

    return qtrue;
}

#ifdef MISSIONPACK
void ProximityMine_Trigger( gentity_t *trigger, gentity_t *other, trace_t *trace );
#endif

/*
==================
BotAIStartFrame
==================
*/
int BotAIStartFrame(int time) {
    int i;
    gentity_t   *ent;
    bot_entitystate_t state;
    int elapsed_time = 0, thinktime = 0;
    static int local_time;
    static int botlib_residual;
    static int lastbotthink_time;

    if ( time == 0 ) {
        local_time = lastbotthink_time = botlib_residual = 0;
    }

    G_CheckBotSpawn();

    Cvar_Update(&bot_rocketjump);
    Cvar_Update(&bot_grapple);
    Cvar_Update(&bot_fastchat);
    Cvar_Update(&bot_nochat);
    Cvar_Update(&bot_testrchat);
    Cvar_Update(&bot_thinktime);
    Cvar_Update(&bot_memorydump);
    Cvar_Update(&bot_saveroutingcache);
    Cvar_Update(&bot_pause);
    Cvar_Update(&bot_report);

    if (bot_report.integer) {
//      BotTeamplayReport();
//      Cvar_Set("bot_report", "0");
        BotUpdateInfoConfigStrings();
    }

    if (bot_pause.integer) {
        // execute bot user commands every frame
        for( i = 0; i < MAX_CLIENTS; i++ ) {
            if( /*!botstates[i] || */!botstates[i].inuse ) {
                continue;
            }
            if( g_entities[i].client->pers.connected != CON_CONNECTED ) {
                continue;
            }
            botstates[i].lastucmd.forwardmove = 0;
            botstates[i].lastucmd.rightmove = 0;
            botstates[i].lastucmd.upmove = 0;
            botstates[i].lastucmd.buttons = 0;
            botstates[i].lastucmd.serverTime = time;
            SV_BotUserCommand(botstates[i].client, &botstates[i].lastucmd); // TODO check for correctness
        }
        return qtrue;
    }

    if (bot_memorydump.integer) {
        Export_BotLibVarSet("memorydump", "1");
        Cvar_Set("bot_memorydump", "0");
    }
    if (bot_saveroutingcache.integer) {
        Export_BotLibVarSet("saveroutingcache", "1");
        Cvar_Set("bot_saveroutingcache", "0");
    }
    //check if bot interbreeding is activated
    BotInterbreeding();
    //cap the bot think time
    if (bot_thinktime.integer > 200) {
        Cvar_Set("bot_thinktime", "200");
    }
    //if the bot think time changed we should reschedule the bots
    if (bot_thinktime.integer != lastbotthink_time) {
        lastbotthink_time = bot_thinktime.integer;
        BotScheduleBotThink();
    }

    elapsed_time = time - local_time;
    local_time = time;

    botlib_residual += elapsed_time;

    if (elapsed_time > bot_thinktime.integer) thinktime = elapsed_time;
    else thinktime = bot_thinktime.integer;

    // update the bot library
    if ( botlib_residual >= thinktime ) {
        botlib_residual -= thinktime;

        /*botlib_*/Export_BotLibStartFrame((float) time / 1000);

        if (!AAS_Initialized()) return qfalse;

        //update entities in the botlib
        for (i = 0; i < MAX_GENTITIES; i++) {
            ent = &g_entities[i];
            if (!ent->inuse) {
                /*botlib_*/Export_BotLibUpdateEntity(i, NULL);
                continue;
            }
            if (!ent->r.linked) {
                /*botlib_*/Export_BotLibUpdateEntity(i, NULL);
                continue;
            }
            if (ent->r.svFlags & SVF_NOCLIENT) {
                /*botlib_*/Export_BotLibUpdateEntity(i, NULL);
                continue;
            }
            // do not update missiles
            if (ent->s.eType == ET_MISSILE && ent->s.weapon != WP_GRAPPLING_HOOK) {
                /*botlib_*/Export_BotLibUpdateEntity(i, NULL);
                continue;
            }
            // do not update event only entities
            if (ent->s.eType > ET_EVENTS) {
                /*botlib_*/Export_BotLibUpdateEntity(i, NULL);
                continue;
            }
#ifdef MISSIONPACK
            // never link prox mine triggers
            if (ent->r.contents == CONTENTS_TRIGGER) {
                if (ent->touch == ProximityMine_Trigger) {
                    /*botlib_*/Export_BotLibUpdateEntity(i, NULL);
                    continue;
                }
            }
#endif
            //
            memset(&state, 0, sizeof(bot_entitystate_t));
            //
            VectorCopy(ent->r.currentOrigin, state.origin);
            if (i < MAX_CLIENTS) {
                VectorCopy(ent->s.apos.trBase, state.angles);
            } else {
                VectorCopy(ent->r.currentAngles, state.angles);
            }
            VectorCopy(ent->s.origin2, state.old_origin);
            VectorCopy(ent->r.mins, state.mins);
            VectorCopy(ent->r.maxs, state.maxs);
            state.type = ent->s.eType;
            state.flags = ent->s.eFlags;
            if (ent->r.bmodel) state.solid = SOLID_BSP;
            else state.solid = SOLID_BBOX;
            state.groundent = ent->s.groundEntityNum;
            state.modelindex = ent->s.modelindex;
            state.modelindex2 = ent->s.modelindex2;
            state.frame = ent->s.frame;
            state.event = ent->s.event;
            state.eventParm = ent->s.eventParm;
            state.powerups = ent->s.powerups;
            state.legsAnim = ent->s.legsAnim;
            state.torsoAnim = ent->s.torsoAnim;
            state.weapon = ent->s.weapon;
            //
            /*botlib_*/Export_BotLibUpdateEntity(i, &state);
        }

        BotAIRegularUpdate();
    }

    floattime = AAS_Time();

    // execute scheduled bot AI
    for( i = 0; i < MAX_CLIENTS; i++ ) {
        if( /*!botstates[i] || */!botstates[i].inuse ) {
            continue;
        }
        //
        botstates[i].botthink_residual += elapsed_time;
        //
        if ( botstates[i].botthink_residual >= thinktime ) {
            botstates[i].botthink_residual -= thinktime;

            if (!AAS_Initialized()) return qfalse;

            if (g_entities[i].client->pers.connected == CON_CONNECTED) {
                BotAI(i, (float) thinktime / 1000);
            }
        }
    }


    // execute bot user commands every frame
    for( i = 0; i < MAX_CLIENTS; i++ ) {
        if( /*!botstates[i] || */!botstates[i].inuse ) {
            continue;
        }
        if( g_entities[i].client->pers.connected != CON_CONNECTED ) {
            continue;
        }

        BotUpdateInput(&botstates[i], time, elapsed_time);
        SV_BotUserCommand(botstates[i].client, &botstates[i].lastucmd); // TODO
    }

    return qtrue;
}

/*
==============
BotInitLibrary
==============
*/
static int BotInitLibrary(void) {
    char buf[144];

    regularupdate_time = 0.0f;

    //set the maxclients and maxentities library variables before calling BotSetupLibrary
    Com_sprintf(buf, sizeof(buf), "%d", level.maxclients);
    Export_BotLibVarSet("maxclients", buf);
    Com_sprintf(buf, sizeof(buf), "%d", MAX_GENTITIES);
    Export_BotLibVarSet("maxentities", buf);
    //bsp checksum
    Cvar_VariableStringBuffer("sv_mapChecksum", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("sv_mapChecksum", buf);
    //maximum number of aas links
    Cvar_VariableStringBuffer("max_aaslinks", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("max_aaslinks", buf);
    //maximum number of items in a level
    Cvar_VariableStringBuffer("max_levelitems", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("max_levelitems", buf);
    //game type
    Cvar_VariableStringBuffer("g_gametype", buf, sizeof(buf));
    if (!strlen(buf)) strcpy(buf, "0");
    Export_BotLibVarSet("g_gametype", buf);
    //bot developer mode and log file
    Export_BotLibVarSet("bot_developer", bot_developer.string);
    Cvar_VariableStringBuffer("logfile", buf, sizeof(buf));
    Export_BotLibVarSet("log", buf);
    //no chatting
    Cvar_VariableStringBuffer("bot_nochat", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("nochat", buf);
    //visualize jump pads
    Cvar_VariableStringBuffer("bot_visualizejumppads", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("bot_visualizejumppads", buf);
    //forced clustering calculations
    Cvar_VariableStringBuffer("bot_forceclustering", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("forceclustering", buf);
    //forced reachability calculations
    Cvar_VariableStringBuffer("bot_forcereachability", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("forcereachability", buf);
    //force writing of AAS to file
    Cvar_VariableStringBuffer("bot_forcewrite", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("forcewrite", buf);
    //no AAS optimization
    Cvar_VariableStringBuffer("bot_aasoptimize", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("aasoptimize", buf);
    //
    Cvar_VariableStringBuffer("bot_saveroutingcache", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("saveroutingcache", buf);
    //reload instead of cache bot character files
    Cvar_VariableStringBuffer("bot_reloadcharacters", buf, sizeof(buf));
    if (!strlen(buf)) strcpy(buf, "0");
    Export_BotLibVarSet("bot_reloadcharacters", buf);
    //base directory
    Cvar_VariableStringBuffer("fs_basepath", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("basedir", buf);
    //game directory
    Cvar_VariableStringBuffer("fs_game", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("gamedir", buf);
    //home directory
    Cvar_VariableStringBuffer("fs_homepath", buf, sizeof(buf));
    if (strlen(buf)) Export_BotLibVarSet("homedir", buf);
    //
#ifdef MISSIONPACK
    trap_BotLibDefine("MISSIONPACK");
#endif
    //setup the bot library
    return SV_BotLibSetup();
}

/*
==============
BotAISetup
#0  BotAISetup (restart=0) at code/game/ai_main.c:1683
#1  0x00000000004cf4ce in G_InitGame (levelTime=0, randomSeed=436, restart=0) at code/game/g_main.c:445
#2  0x0000000000457759 in SV_InitGameVM (restart=qfalse) at code/server/sv_game.c:904
#3  0x00000000004577c5 in SV_InitGameProgs () at code/server/sv_game.c:942
#4  0x0000000000458515 in SV_SpawnServer (server=0x7fffffffce50 "pro-q3dm13", killBots=qfalse) at code/server/sv_init.c:479
#5  0x000000000044fddd in SV_Map_f () at code/server/sv_ccmds.c:211
#6  0x000000000045e531 in Cmd_ExecuteString (text=0x7fffffffcf30 "map pro-q3dm13 ") at code/qcommon/cmd_server.c:795
#7  0x000000000045db45 in Cbuf_Execute () at code/qcommon/cmd_server.c:260
#8  0x00000000004748b2 in Com_Frame () at code/qcommon/common_server.c:2745
#9  0x000000000040240f in main (argc=8, argv=0x7fffffffd8f8) at code/sys/sys_main_server.c:469
==============
*/
int BotAISetup( int restart ) {
    int         errnum;

    Cvar_Register(&bot_thinktime, "bot_thinktime", "100", CVAR_CHEAT);
    Cvar_Register(&bot_memorydump, "bot_memorydump", "0", CVAR_CHEAT);
    Cvar_Register(&bot_saveroutingcache, "bot_saveroutingcache", "0", CVAR_CHEAT);
    Cvar_Register(&bot_pause, "bot_pause", "0", CVAR_CHEAT);
    Cvar_Register(&bot_report, "bot_report", "0", CVAR_CHEAT);
    Cvar_Register(&bot_testsolid, "bot_testsolid", "0", CVAR_CHEAT);
    Cvar_Register(&bot_testclusters, "bot_testclusters", "0", CVAR_CHEAT);
    Cvar_Register(&bot_developer, "bot_developer", "0", CVAR_CHEAT);
    Cvar_Register(&bot_interbreedchar, "bot_interbreedchar", "", 0);
    Cvar_Register(&bot_interbreedbots, "bot_interbreedbots", "10", 0);
    Cvar_Register(&bot_interbreedcycle, "bot_interbreedcycle", "20", 0);
    Cvar_Register(&bot_interbreedwrite, "bot_interbreedwrite", "", 0);

    //if the game is restarted for a tournament
    if (restart) {
        Com_Printf(">>>>>>>>>>>>>> Not zeroing botstates <<<<<<<<<<<<\n");
        return qtrue;
    }

    //initialize the bot states
    //initialize the bot states
    //initialize the bot states
    //initialize the bot states
    Com_Printf(">>>>>>>>>>>>>> ZEROING BOTSTATES <<<<<<<<<<<<\n");
    memset( botstates, 0, sizeof(botstates) );
    //initialize the bot states
    //initialize the bot states
    //initialize the bot states
    //initialize the bot states

    errnum = BotInitLibrary();
    if (errnum != BLERR_NOERROR) return qfalse;
    return qtrue;
}

/*
==============
BotAIShutdown
==============
*/
int BotAIShutdown( int restart ) {

    int i;

    //if the game is restarted for a tournament
    if ( restart ) {
        //shutdown all the bots in the botlib
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (/*botstates[i] && */botstates[i].inuse) {
                BotAIShutdownClient(botstates[i].client, restart);
            }
        }
        //don't shutdown the bot library
    }
    else {
        SV_BotLibShutdown();
    }
    return qtrue;
}

