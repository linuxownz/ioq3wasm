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

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "../client/client.h"

#include <emscripten.h>
#include <emscripten/html5.h>

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#define KMOD_SCROLL KMOD_RESERVED
#endif

static cvar_t *in_keyboardDebug     = NULL;

static SDL_GameController *gamepad = NULL;
static SDL_Joystick         *stick = NULL;

static qboolean mouseAvailable = qfalse;
static qboolean mouseActive    = qfalse;

static cvar_t *in_mouse             = NULL;

static cvar_t *in_joystick          = NULL;
static cvar_t *in_joystickThreshold = NULL;
static cvar_t *in_joystickNo        = NULL;
static cvar_t *in_joystickUseAnalog = NULL;

static int vidRestartTime = 0;

static int in_eventTime = 0;

static SDL_Window *SDL_window = NULL;

#define CTRL(a) ((a)-'a'+1)

/*
===============
IN_PrintKey
===============
*/
static void IN_PrintKey( const SDL_Keysym *keysym, keyNum_t key, qboolean down )
{
    if( down ) {
        Com_Printf( "+ " );
    } else {
        Com_Printf( "  " );
    }

    Com_Printf( "Scancode: 0x%02x(%s) Sym: 0x%02x(%s)",
            keysym->scancode, SDL_GetScancodeName( keysym->scancode ),
            keysym->sym, SDL_GetKeyName( keysym->sym ) );

    if( keysym->mod & KMOD_LSHIFT )   Com_Printf( " KMOD_LSHIFT" );
    if( keysym->mod & KMOD_RSHIFT )   Com_Printf( " KMOD_RSHIFT" );
    if( keysym->mod & KMOD_LCTRL )    Com_Printf( " KMOD_LCTRL" );
    if( keysym->mod & KMOD_RCTRL )    Com_Printf( " KMOD_RCTRL" );
    if( keysym->mod & KMOD_LALT )     Com_Printf( " KMOD_LALT" );
    if( keysym->mod & KMOD_RALT )     Com_Printf( " KMOD_RALT" );
    if( keysym->mod & KMOD_LGUI )     Com_Printf( " KMOD_LGUI" );
    if( keysym->mod & KMOD_RGUI )     Com_Printf( " KMOD_RGUI" );
    if( keysym->mod & KMOD_NUM )      Com_Printf( " KMOD_NUM" );
    if( keysym->mod & KMOD_CAPS )     Com_Printf( " KMOD_CAPS" );
    if( keysym->mod & KMOD_MODE )     Com_Printf( " KMOD_MODE" );
    if( keysym->mod & KMOD_SCROLL )   Com_Printf( " KMOD_SCROLL" );

    Com_Printf( " Q:0x%02x(%s)\n", key, Key_KeynumToString( key ) );
}

#define MAX_CONSOLE_KEYS 16

/*
===============
IN_IsConsoleKey
T O D O: If the SDL_Scancode situation improves,
use it instead of both of these methods
===============
*/
static qboolean IN_IsConsoleKey( keyNum_t key, int character )
{
    typedef struct consoleKey_s
    {
        enum
        {
            QUAKE_KEY,
            CHARACTER
        } type;

        union
        {
            keyNum_t key;
            int character;
        } u;
    } consoleKey_t;

    static consoleKey_t consoleKeys[ MAX_CONSOLE_KEYS ];
    static int numConsoleKeys = 0;
    int i;

    // Only parse the variable when it changes
    if( cl_consoleKeys->modified )
    {
        char *text_p, *token;

        cl_consoleKeys->modified = qfalse;
        text_p = cl_consoleKeys->string;
        numConsoleKeys = 0;

        while( numConsoleKeys < MAX_CONSOLE_KEYS )
        {
            consoleKey_t *c = &consoleKeys[ numConsoleKeys ];
            int charCode = 0;

            token = COM_Parse( &text_p );

            if( !token[ 0 ] ) {
                break;
            }

            charCode = Com_HexStrToInt( token );

            if( charCode > 0 )
            {
                c->type = CHARACTER;
                c->u.character = charCode;
            }
            else
            {
                c->type = QUAKE_KEY;
                c->u.key = Key_StringToKeynum( token );

                // 0 isn't a key
                if( c->u.key <= 0 ) {
                    continue;
                }
            }

            numConsoleKeys++;
        }
    }

    // If the character is the same as the key, prefer the character
    if( key == character ) {
        key = 0;
    }

    for( i = 0; i < numConsoleKeys; i++ )
    {
        consoleKey_t *c = &consoleKeys[ i ];

        switch( c->type )
        {
            case QUAKE_KEY:
                if( key && c->u.key == key ) {
                    return qtrue;
                }
                break;

            case CHARACTER:
                if( c->u.character == character ) {
                    return qtrue;
                }
                break;
        }
    }

    return qfalse;
}

/*
===============
IN_TranslateSDLToQ3Key
===============
*/
static keyNum_t IN_TranslateSDLToQ3Key( SDL_Keysym *keysym, qboolean down )
{
    keyNum_t key = 0;

    if( keysym->scancode >= SDL_SCANCODE_1 && keysym->scancode <= SDL_SCANCODE_0 )
    {
        // Always map the number keys as such even if they actually map
        // to other characters (eg, "1" is "&" on an AZERTY keyboard).
        // This is required for SDL before 2.0.6, except on Windows
        // which already had this behavior.
        if( keysym->scancode == SDL_SCANCODE_0 ) {
            key = '0';
        } else {
            key = '1' + keysym->scancode - SDL_SCANCODE_1;
        }
    }
    else if( keysym->sym >= SDLK_SPACE && keysym->sym < SDLK_DELETE )
    {
        // These happen to match the ASCII chars
        key = (int)keysym->sym;
    }
    else
    {
        switch( keysym->sym )
        {
            case SDLK_PAGEUP:       key = K_PGUP;          break;
            case SDLK_KP_9:         key = K_KP_PGUP;       break;
            case SDLK_PAGEDOWN:     key = K_PGDN;          break;
            case SDLK_KP_3:         key = K_KP_PGDN;       break;
            case SDLK_KP_7:         key = K_KP_HOME;       break;
            case SDLK_HOME:         key = K_HOME;          break;
            case SDLK_KP_1:         key = K_KP_END;        break;
            case SDLK_END:          key = K_END;           break;
            case SDLK_KP_4:         key = K_KP_LEFTARROW;  break;
            case SDLK_LEFT:         key = K_LEFTARROW;     break;
            case SDLK_KP_6:         key = K_KP_RIGHTARROW; break;
            case SDLK_RIGHT:        key = K_RIGHTARROW;    break;
            case SDLK_KP_2:         key = K_KP_DOWNARROW;  break;
            case SDLK_DOWN:         key = K_DOWNARROW;     break;
            case SDLK_KP_8:         key = K_KP_UPARROW;    break;
            case SDLK_UP:           key = K_UPARROW;       break;
            case SDLK_ESCAPE:       key = K_ESCAPE;        break;
            case SDLK_KP_ENTER:     key = K_KP_ENTER;      break;
            case SDLK_RETURN:       key = K_ENTER;         break;
            case SDLK_TAB:          key = K_TAB;           break;
            case SDLK_F1:           key = K_F1;            break;
            case SDLK_F2:           key = K_F2;            break;
            case SDLK_F3:           key = K_F3;            break;
            case SDLK_F4:           key = K_F4;            break;
            case SDLK_F5:           key = K_F5;            break;
            case SDLK_F6:           key = K_F6;            break;
            case SDLK_F7:           key = K_F7;            break;
            case SDLK_F8:           key = K_F8;            break;
            case SDLK_F9:           key = K_F9;            break;
            case SDLK_F10:          key = K_F10;           break;
            case SDLK_F11:          key = K_F11;           break;
            case SDLK_F12:          key = K_F12;           break;
            case SDLK_F13:          key = K_F13;           break;
            case SDLK_F14:          key = K_F14;           break;
            case SDLK_F15:          key = K_F15;           break;

            case SDLK_BACKSPACE:    key = K_BACKSPACE;     break;
            case SDLK_KP_PERIOD:    key = K_KP_DEL;        break;
            case SDLK_DELETE:       key = K_DEL;           break;
            case SDLK_PAUSE:        key = K_PAUSE;         break;

            case SDLK_LSHIFT:
            case SDLK_RSHIFT:       key = K_SHIFT;         break;

            case SDLK_LCTRL:
            case SDLK_RCTRL:        key = K_CTRL;          break;

#ifdef __APPLE__
            case SDLK_RGUI:
            case SDLK_LGUI:         key = K_COMMAND;       break;
#else
            case SDLK_RGUI:
            case SDLK_LGUI:         key = K_SUPER;         break;
#endif

            case SDLK_RALT:
            case SDLK_LALT:         key = K_ALT;           break;

            case SDLK_KP_5:         key = K_KP_5;          break;
            case SDLK_INSERT:       key = K_INS;           break;
            case SDLK_KP_0:         key = K_KP_INS;        break;
            case SDLK_KP_MULTIPLY:  key = K_KP_STAR;       break;
            case SDLK_KP_PLUS:      key = K_KP_PLUS;       break;
            case SDLK_KP_MINUS:     key = K_KP_MINUS;      break;
            case SDLK_KP_DIVIDE:    key = K_KP_SLASH;      break;

            case SDLK_MODE:         key = K_MODE;          break;
            case SDLK_HELP:         key = K_HELP;          break;
            case SDLK_PRINTSCREEN:  key = K_PRINT;         break;
            case SDLK_SYSREQ:       key = K_SYSREQ;        break;
            case SDLK_MENU:         key = K_MENU;          break;
            case SDLK_APPLICATION:  key = K_MENU;          break;
            case SDLK_POWER:        key = K_POWER;         break;
            case SDLK_UNDO:         key = K_UNDO;          break;
            case SDLK_SCROLLLOCK:   key = K_SCROLLOCK;     break;
            case SDLK_NUMLOCKCLEAR: key = K_KP_NUMLOCK;    break;
            case SDLK_CAPSLOCK:     key = K_CAPSLOCK;      break;

            default:
                if( !( keysym->sym & SDLK_SCANCODE_MASK ) && keysym->scancode <= 95 )
                {
                    // Map Unicode characters to 95 world keys using the key's scan code.
                    // FIXME: There aren't enough world keys to cover all the scancodes.
                    // Maybe create a map of scancode to quake key at start up and on
                    // key map change; allocate world key numbers as needed similar
                    // to SDL 1.2.
                    key = K_WORLD_0 + (int)keysym->scancode;
                }
                break;
        }
    }

    if( in_keyboardDebug->integer ) {
        IN_PrintKey( keysym, key, down );
    }

    if( IN_IsConsoleKey( key, 0 ) )
    {
        // Console keys can't be bound or generate characters
        key = K_CONSOLE;
    }

    return key;
}

/*
===============
IN_GobbleMotionEvents
===============
*/
static void IN_GobbleMotionEvents( void )
{
    SDL_Event dummy[ 1 ];
    int val = 0;

    // Gobble any mouse motion events
    SDL_PumpEvents();

    while( ( val = SDL_PeepEvents( dummy, 1, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION ) ) > 0 ) { }

    if ( val < 0 ) {
        Com_Printf( "IN_GobbleMotionEvents failed: %s\n", SDL_GetError( ) );
    }
}

/*
===============
IN_ActivateMouse
===============
*/
static void IN_ActivateMouse( qboolean isFullscreen )
{
    if ( ! SDL_window ) {
        Com_Error(ERR_FATAL,"SDL_Window is NULL");
    }

    if ( ! mouseAvailable || ! SDL_WasInit( SDL_INIT_VIDEO ) ) {
        return;
    }

    if( ! mouseActive )
    {
        SDL_SetRelativeMouseMode( SDL_TRUE );
        SDL_SetWindowGrab( SDL_window, SDL_TRUE );

        IN_GobbleMotionEvents();
    }

    if( ! isFullscreen )
    {
        if( ! mouseActive )
        {
            SDL_SetRelativeMouseMode( SDL_TRUE );
            SDL_SetWindowGrab( SDL_window, SDL_TRUE );
        }
    }

    mouseActive = qtrue;
}

/*
===============
IN_DeactivateMouse
===============
*/
static void IN_DeactivateMouse( qboolean isFullscreen )
{
    if ( ! SDL_window ) {
        Com_Error(ERR_FATAL,"SDL_Window is NULL");
    }

    if( ! SDL_WasInit( SDL_INIT_VIDEO ) ) {
        return;
    }

    // Always show the cursor when the mouse is disabled,
    // but not when fullscreen
    if( ! isFullscreen ) {
        SDL_ShowCursor( SDL_TRUE );
    }

    if( ! mouseAvailable ) {
        return;
    }

    if( mouseActive )
    {
        IN_GobbleMotionEvents( );

        SDL_SetWindowGrab( SDL_window, SDL_FALSE );
        SDL_SetRelativeMouseMode( SDL_FALSE );

        // Don't warp the mouse unless the cursor is within the window
        if( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_MOUSE_FOCUS ) {
            SDL_WarpMouseInWindow( SDL_window, cls.glconfig.vidWidth / 2, cls.glconfig.vidHeight / 2 );
        }

        mouseActive = qfalse;
    }
}

// We translate axes movement into keypresses
static int joy_keys[16] = {
    K_LEFTARROW, K_RIGHTARROW,
    K_UPARROW,   K_DOWNARROW,
    K_JOY17,     K_JOY18,
    K_JOY19,     K_JOY20,
    K_JOY21,     K_JOY22,
    K_JOY23,     K_JOY24,
    K_JOY25,     K_JOY26,
    K_JOY27,     K_JOY28
};

// translate hat events into keypresses
// the 4 highest buttons are used for the first hat ...
static int hat_keys[16] = {
    K_JOY29, K_JOY30,
    K_JOY31, K_JOY32,
    K_JOY25, K_JOY26,
    K_JOY27, K_JOY28,
    K_JOY21, K_JOY22,
    K_JOY23, K_JOY24,
    K_JOY17, K_JOY18,
    K_JOY19, K_JOY20
};

struct {
    qboolean buttons[SDL_CONTROLLER_BUTTON_MAX + 1]; // +1 because old max was 16, current SDL_CONTROLLER_BUTTON_MAX is 15
    unsigned int oldaxes;
    int oldaaxes[MAX_JOYSTICK_AXIS];
    unsigned int oldhats;
} stick_state;

/*
===============
IN_InitJoystick
===============
*/
static void IN_InitJoystick( void )
{
    int i = 0;
    int total = 0;
    char buf[16384] = "";

    if (gamepad) {
        SDL_GameControllerClose(gamepad);
    }

    if (stick != NULL)
        SDL_JoystickClose(stick);

    stick = NULL;
    gamepad = NULL;
    memset(&stick_state, '\0', sizeof (stick_state));

    // SDL 2.0.4 requires SDL_INIT_JOYSTICK to be initialized separately from
    // SDL_INIT_GAMECONTROLLER for SDL_JoystickOpen() to work correctly,
    // despite https://wiki.libsdl.org/SDL_Init (retrieved 2016-08-16)
    // indicating SDL_INIT_JOYSTICK should be initialized automatically.
    if (!SDL_WasInit(SDL_INIT_JOYSTICK))
    {
        Com_DPrintf("Calling SDL_Init(SDL_INIT_JOYSTICK)...\n");
        if (SDL_Init(SDL_INIT_JOYSTICK) != 0)
        {
            Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) failed: %s\n", SDL_GetError());
            return;
        }
        Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) passed.\n");
    }

    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER))
    {
        Com_DPrintf("Calling SDL_Init(SDL_INIT_GAMECONTROLLER)...\n");
        if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0)
        {
            Com_DPrintf("SDL_Init(SDL_INIT_GAMECONTROLLER) failed: %s\n", SDL_GetError());
            return;
        }
        Com_DPrintf("SDL_Init(SDL_INIT_GAMECONTROLLER) passed.\n");
    }

    total = SDL_NumJoysticks();
    Com_DPrintf("%d possible joysticks\n", total);

    // Print list and build cvar to allow ui to select joystick.
    for (i = 0; i < total; i++)
    {
        Q_strcat(buf, sizeof(buf), SDL_JoystickNameForIndex(i));
        Q_strcat(buf, sizeof(buf), "\n");
    }

    Cvar_Get( "in_availableJoysticks", "", CVAR_ROM );

    // Update cvar on in_restart or controller add/remove.
    Cvar_Set( "in_availableJoysticks", buf );

    if( !in_joystick->integer ) {
        Com_DPrintf( "Joystick is not active.\n" );
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        return;
    }

    in_joystickNo = Cvar_Get( "in_joystickNo", "0", CVAR_ARCHIVE );
    if( in_joystickNo->integer < 0 || in_joystickNo->integer >= total )
        Cvar_Set( "in_joystickNo", "0" );

    in_joystickUseAnalog = Cvar_Get( "in_joystickUseAnalog", "0", CVAR_ARCHIVE );

    stick = SDL_JoystickOpen( in_joystickNo->integer );

    if (stick == NULL) {
        Com_DPrintf( "No joystick opened: %s\n", SDL_GetError() );
        return;
    }

    if (SDL_IsGameController(in_joystickNo->integer))
        gamepad = SDL_GameControllerOpen(in_joystickNo->integer);

    Com_DPrintf( "Joystick %d opened\n", in_joystickNo->integer );
    Com_DPrintf( "Name:       %s\n", SDL_JoystickNameForIndex(in_joystickNo->integer) );
    Com_DPrintf( "Axes:       %d\n", SDL_JoystickNumAxes(stick) );
    Com_DPrintf( "Hats:       %d\n", SDL_JoystickNumHats(stick) );
    Com_DPrintf( "Buttons:    %d\n", SDL_JoystickNumButtons(stick) );
    Com_DPrintf( "Balls:      %d\n", SDL_JoystickNumBalls(stick) );
    Com_DPrintf( "Use Analog: %s\n", in_joystickUseAnalog->integer ? "Yes" : "No" );
    Com_DPrintf( "Is gamepad: %s\n", gamepad ? "Yes" : "No" );

    SDL_JoystickEventState(SDL_QUERY);
    SDL_GameControllerEventState(SDL_QUERY);
}

/*
===============
IN_ShutdownJoystick
===============
*/
static void IN_ShutdownJoystick( void )
{
    if ( !SDL_WasInit( SDL_INIT_GAMECONTROLLER ) ) {
        return;
    }

    if ( !SDL_WasInit( SDL_INIT_JOYSTICK ) ) {
        return;
    }

    if (gamepad)
    {
        SDL_GameControllerClose(gamepad);
        gamepad = NULL;
    }

    if (stick)
    {
        SDL_JoystickClose(stick);
        stick = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}


static qboolean KeyToAxisAndSign(int keynum, int *outAxis, int *outSign)
{
    char *bind;

    if (!keynum)
        return qfalse;

    bind = Key_GetBinding(keynum);

    if (!bind || *bind != '+')
        return qfalse;

    *outSign = 0;

    if (Q_stricmp(bind, "+forward") == 0)
    {
        *outAxis = j_forward_axis->integer;
        *outSign = j_forward->value > 0.0f ? 1 : -1;
    }
    else if (Q_stricmp(bind, "+back") == 0)
    {
        *outAxis = j_forward_axis->integer;
        *outSign = j_forward->value > 0.0f ? -1 : 1;
    }
    else if (Q_stricmp(bind, "+moveleft") == 0)
    {
        *outAxis = j_side_axis->integer;
        *outSign = j_side->value > 0.0f ? -1 : 1;
    }
    else if (Q_stricmp(bind, "+moveright") == 0)
    {
        *outAxis = j_side_axis->integer;
        *outSign = j_side->value > 0.0f ? 1 : -1;
    }
    else if (Q_stricmp(bind, "+lookup") == 0)
    {
        *outAxis = j_pitch_axis->integer;
        *outSign = j_pitch->value > 0.0f ? -1 : 1;
    }
    else if (Q_stricmp(bind, "+lookdown") == 0)
    {
        *outAxis = j_pitch_axis->integer;
        *outSign = j_pitch->value > 0.0f ? 1 : -1;
    }
    else if (Q_stricmp(bind, "+left") == 0)
    {
        *outAxis = j_yaw_axis->integer;
        *outSign = j_yaw->value > 0.0f ? 1 : -1;
    }
    else if (Q_stricmp(bind, "+right") == 0)
    {
        *outAxis = j_yaw_axis->integer;
        *outSign = j_yaw->value > 0.0f ? -1 : 1;
    }
    else if (Q_stricmp(bind, "+moveup") == 0)
    {
        *outAxis = j_up_axis->integer;
        *outSign = j_up->value > 0.0f ? 1 : -1;
    }
    else if (Q_stricmp(bind, "+movedown") == 0)
    {
        *outAxis = j_up_axis->integer;
        *outSign = j_up->value > 0.0f ? -1 : 1;
    }

    return *outSign != 0;
}

/*
===============
IN_GamepadMove
===============
*/
static void IN_GamepadMove( void )
{
    int i;
    int translatedAxes[MAX_JOYSTICK_AXIS];
    qboolean translatedAxesSet[MAX_JOYSTICK_AXIS];

    SDL_GameControllerUpdate();

    // check buttons
    for (i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
    {
        qboolean pressed = SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_A + i);
        if (pressed != stick_state.buttons[i])
        {
#if SDL_VERSION_ATLEAST( 2, 0, 14 )
            if ( i >= SDL_CONTROLLER_BUTTON_MISC1 ) {
                Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_MISC1 + i - SDL_CONTROLLER_BUTTON_MISC1, pressed, 0, NULL);
            } else
#endif
            {
                Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_A + i, pressed, 0, NULL);
            }
            stick_state.buttons[i] = pressed;
        }
    }

    // must defer translated axes until all real axes are processed
    // must be done this way to prevent a later mapped axis from zeroing out a previous one
    if (in_joystickUseAnalog->integer)
    {
        for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
        {
            translatedAxes[i] = 0;
            translatedAxesSet[i] = qfalse;
        }
    }

    // check axes
    for (i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++)
    {
        int axis = SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTX + i);
        int oldAxis = stick_state.oldaaxes[i];

        // Smoothly ramp from dead zone to maximum value
        float f = ((float)abs(axis) / 32767.0f - in_joystickThreshold->value) / (1.0f - in_joystickThreshold->value);

        if (f < 0.0f)
            f = 0.0f;

        axis = (int)(32767 * ((axis < 0) ? -f : f));

        if (axis != oldAxis)
        {
            const int negMap[SDL_CONTROLLER_AXIS_MAX] = { K_PAD0_LEFTSTICK_LEFT,  K_PAD0_LEFTSTICK_UP,   K_PAD0_RIGHTSTICK_LEFT,  K_PAD0_RIGHTSTICK_UP, 0, 0 };
            const int posMap[SDL_CONTROLLER_AXIS_MAX] = { K_PAD0_LEFTSTICK_RIGHT, K_PAD0_LEFTSTICK_DOWN, K_PAD0_RIGHTSTICK_RIGHT, K_PAD0_RIGHTSTICK_DOWN, K_PAD0_LEFTTRIGGER, K_PAD0_RIGHTTRIGGER };

            qboolean posAnalog = qfalse, negAnalog = qfalse;
            int negKey = negMap[i];
            int posKey = posMap[i];

            if (in_joystickUseAnalog->integer)
            {
                int posAxis = 0, posSign = 0, negAxis = 0, negSign = 0;

                // get axes and axes signs for keys if available
                posAnalog = KeyToAxisAndSign(posKey, &posAxis, &posSign);
                negAnalog = KeyToAxisAndSign(negKey, &negAxis, &negSign);

                // positive to negative/neutral -> keyup if axis hasn't yet been set
                if (posAnalog && !translatedAxesSet[posAxis] && oldAxis > 0 && axis <= 0)
                {
                    translatedAxes[posAxis] = 0;
                    translatedAxesSet[posAxis] = qtrue;
                }

                // negative to positive/neutral -> keyup if axis hasn't yet been set
                if (negAnalog && !translatedAxesSet[negAxis] && oldAxis < 0 && axis >= 0)
                {
                    translatedAxes[negAxis] = 0;
                    translatedAxesSet[negAxis] = qtrue;
                }

                // negative/neutral to positive -> keydown
                if (posAnalog && axis > 0)
                {
                    translatedAxes[posAxis] = axis * posSign;
                    translatedAxesSet[posAxis] = qtrue;
                }

                // positive/neutral to negative -> keydown
                if (negAnalog && axis < 0)
                {
                    translatedAxes[negAxis] = -axis * negSign;
                    translatedAxesSet[negAxis] = qtrue;
                }
            }

            // keyups first so they get overridden by keydowns later

            // positive to negative/neutral -> keyup
            if (!posAnalog && posKey && oldAxis > 0 && axis <= 0)
                Com_QueueEvent(in_eventTime, SE_KEY, posKey, qfalse, 0, NULL);

            // negative to positive/neutral -> keyup
            if (!negAnalog && negKey && oldAxis < 0 && axis >= 0)
                Com_QueueEvent(in_eventTime, SE_KEY, negKey, qfalse, 0, NULL);

            // negative/neutral to positive -> keydown
            if (!posAnalog && posKey && oldAxis <= 0 && axis > 0)
                Com_QueueEvent(in_eventTime, SE_KEY, posKey, qtrue, 0, NULL);

            // positive/neutral to negative -> keydown
            if (!negAnalog && negKey && oldAxis >= 0 && axis < 0)
                Com_QueueEvent(in_eventTime, SE_KEY, negKey, qtrue, 0, NULL);

            stick_state.oldaaxes[i] = axis;
        }
    }

    // set translated axes
    if (in_joystickUseAnalog->integer)
    {
        for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
        {
            if (translatedAxesSet[i])
                Com_QueueEvent(in_eventTime, SE_JOYSTICK_AXIS, i, translatedAxes[i], 0, NULL);
        }
    }
}


/*
===============
IN_JoyMove
===============
*/
static void IN_JoyMove( void )
{
    unsigned int axes = 0;
    unsigned int hats = 0;
    int total = 0;
    int i = 0;

    if (gamepad)
    {
        IN_GamepadMove();
        return;
    }

    if (!stick)
        return;

    SDL_JoystickUpdate();

    // update the ball state.
    total = SDL_JoystickNumBalls(stick);
    if (total > 0)
    {
        int balldx = 0;
        int balldy = 0;
        for (i = 0; i < total; i++)
        {
            int dx = 0;
            int dy = 0;
            SDL_JoystickGetBall(stick, i, &dx, &dy);
            balldx += dx;
            balldy += dy;
        }
        if (balldx || balldy)
        {
            // !!! FIXME: is this good for stick balls, or just mice?
            // Scale like the mouse input...
            if (abs(balldx) > 1)
                balldx *= 2;
            if (abs(balldy) > 1)
                balldy *= 2;
            Com_QueueEvent( in_eventTime, SE_MOUSE, balldx, balldy, 0, NULL );
        }
    }

    // now query the stick buttons...
    total = SDL_JoystickNumButtons(stick);
    if (total > 0)
    {
        if (total > ARRAY_LEN(stick_state.buttons))
            total = ARRAY_LEN(stick_state.buttons);
        for (i = 0; i < total; i++)
        {
            qboolean pressed = (SDL_JoystickGetButton(stick, i) != 0);
            if (pressed != stick_state.buttons[i])
            {
                Com_QueueEvent( in_eventTime, SE_KEY, K_JOY1 + i, pressed, 0, NULL );
                stick_state.buttons[i] = pressed;
            }
        }
    }

    // look at the hats...
    total = SDL_JoystickNumHats(stick);
    if (total > 0)
    {
        if (total > 4) total = 4;
        for (i = 0; i < total; i++)
        {
            ((Uint8 *)&hats)[i] = SDL_JoystickGetHat(stick, i);
        }
    }

    // update hat state
    if (hats != stick_state.oldhats)
    {
        for( i = 0; i < 4; i++ ) {
            if( ((Uint8 *)&hats)[i] != ((Uint8 *)&stick_state.oldhats)[i] ) {
                // release event
                switch( ((Uint8 *)&stick_state.oldhats)[i] ) {
                    case SDL_HAT_UP:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
                        break;
                    case SDL_HAT_RIGHT:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
                        break;
                    case SDL_HAT_DOWN:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
                        break;
                    case SDL_HAT_LEFT:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
                        break;
                    case SDL_HAT_RIGHTUP:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
                        break;
                    case SDL_HAT_RIGHTDOWN:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
                        break;
                    case SDL_HAT_LEFTUP:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
                        break;
                    case SDL_HAT_LEFTDOWN:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
                        break;
                    default:
                        break;
                }
                // press event
                switch( ((Uint8 *)&hats)[i] ) {
                    case SDL_HAT_UP:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
                        break;
                    case SDL_HAT_RIGHT:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
                        break;
                    case SDL_HAT_DOWN:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
                        break;
                    case SDL_HAT_LEFT:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
                        break;
                    case SDL_HAT_RIGHTUP:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
                        break;
                    case SDL_HAT_RIGHTDOWN:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
                        break;
                    case SDL_HAT_LEFTUP:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
                        break;
                    case SDL_HAT_LEFTDOWN:
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
                        Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
                        break;
                    default:
                        break;
                }
            }
        }
    }

    // save hat state
    stick_state.oldhats = hats;

    // finally, look at the axes...
    total = SDL_JoystickNumAxes(stick);
    if (total > 0)
    {
        if (in_joystickUseAnalog->integer)
        {
            if (total > MAX_JOYSTICK_AXIS) total = MAX_JOYSTICK_AXIS;
            for (i = 0; i < total; i++)
            {
                Sint16 axis = SDL_JoystickGetAxis(stick, i);
                float f = ( (float) abs(axis) ) / 32767.0f;

                if( f < in_joystickThreshold->value ) axis = 0;

                if ( axis != stick_state.oldaaxes[i] )
                {
                    Com_QueueEvent( in_eventTime, SE_JOYSTICK_AXIS, i, axis, 0, NULL );
                    stick_state.oldaaxes[i] = axis;
                }
            }
        }
        else
        {
            if (total > 16) total = 16;
            for (i = 0; i < total; i++)
            {
                Sint16 axis = SDL_JoystickGetAxis(stick, i);
                float f = ( (float) axis ) / 32767.0f;
                if( f < -in_joystickThreshold->value ) {
                    axes |= ( 1 << ( i * 2 ) );
                } else if( f > in_joystickThreshold->value ) {
                    axes |= ( 1 << ( ( i * 2 ) + 1 ) );
                }
            }
        }
    }

    /* Time to update axes state based on old vs. new. */
    if (axes != stick_state.oldaxes)
    {
        for( i = 0; i < 16; i++ ) {
            if( ( axes & ( 1 << i ) ) && !( stick_state.oldaxes & ( 1 << i ) ) ) {
                Com_QueueEvent( in_eventTime, SE_KEY, joy_keys[i], qtrue, 0, NULL );
            }

            if( !( axes & ( 1 << i ) ) && ( stick_state.oldaxes & ( 1 << i ) ) ) {
                Com_QueueEvent( in_eventTime, SE_KEY, joy_keys[i], qfalse, 0, NULL );
            }
        }
    }

    /* Save for future generations. */
    stick_state.oldaxes = axes;
}

#if 0
static const char *SDL_EventToString ( int event ) {
    switch ( event ) {
        case SDL_FIRSTEVENT:                return "SDL_FIRSTEVENT";
        case SDL_QUIT:                      return "SDL_QUIT";
        case SDL_APP_TERMINATING:           return "SDL_APP_TERMINATING";
        case SDL_APP_LOWMEMORY:             return "SDL_APP_LOWMEMORY";
        case SDL_APP_WILLENTERBACKGROUND:   return "SDL_APP_WILLENTERBACKGROUND";
        case SDL_APP_DIDENTERBACKGROUND:    return "SDL_APP_DIDENTERBACKGROUND";
        case SDL_APP_WILLENTERFOREGROUND:   return "SDL_APP_WILLENTERFOREGROUND";
        case SDL_APP_DIDENTERFOREGROUND:    return "SDL_APP_DIDENTERFOREGROUND";
        case SDL_LOCALECHANGED:             return "SDL_LOCALECHANGED";
        case SDL_DISPLAYEVENT:              return "SDL_DISPLAYEVENT";
        case SDL_WINDOWEVENT:               return "SDL_WINDOWEVENT";
        case SDL_SYSWMEVENT:                return "SDL_SYSWMEVENT";
        case SDL_KEYDOWN:                   return "SDL_KEYDOWN";
        case SDL_KEYUP:                     return "SDL_KEYUP";
        case SDL_TEXTEDITING:               return "SDL_TEXTEDITING";
        case SDL_TEXTINPUT:                 return "SDL_TEXTINPUT";
        case SDL_KEYMAPCHANGED:             return "SDL_KEYMAPCHANGED";
        case SDL_TEXTEDITING_EXT:           return "SDL_TEXTEDITING_EXT";
        case SDL_MOUSEMOTION:               return "SDL_MOUSEMOTION";
        case SDL_MOUSEBUTTONDOWN:           return "SDL_MOUSEBUTTONDOWN";
        case SDL_MOUSEBUTTONUP:             return "SDL_MOUSEBUTTONUP";
        case SDL_MOUSEWHEEL:                return "SDL_MOUSEWHEEL";
        case SDL_JOYAXISMOTION:             return "SDL_JOYAXISMOTION";
        case SDL_JOYBALLMOTION:             return "SDL_JOYBALLMOTION";
        case SDL_JOYHATMOTION:              return "SDL_JOYHATMOTION";
        case SDL_JOYBUTTONDOWN:             return "SDL_JOYBUTTONDOWN";
        case SDL_JOYBUTTONUP:               return "SDL_JOYBUTTONUP";
        case SDL_JOYDEVICEADDED:            return "SDL_JOYDEVICEADDED";
        case SDL_JOYDEVICEREMOVED:          return "SDL_JOYDEVICEREMOVED";
        case SDL_JOYBATTERYUPDATED:         return "SDL_JOYBATTERYUPDATED";
        case SDL_CONTROLLERAXISMOTION:      return "SDL_CONTROLLERAXISMOTION";
        case SDL_CONTROLLERBUTTONDOWN:      return "SDL_CONTROLLERBUTTONDOWN";
        case SDL_CONTROLLERBUTTONUP:        return "SDL_CONTROLLERBUTTONUP";
        case SDL_CONTROLLERDEVICEADDED:     return "SDL_CONTROLLERDEVICEADDED";
        case SDL_CONTROLLERDEVICEREMOVED:   return "SDL_CONTROLLERDEVICEREMOVED";
        case SDL_CONTROLLERDEVICEREMAPPED:  return "SDL_CONTROLLERDEVICEREMAPPED";
        case SDL_CONTROLLERTOUCHPADDOWN:    return "SDL_CONTROLLERTOUCHPADDOWN";
        case SDL_CONTROLLERTOUCHPADMOTION:  return "SDL_CONTROLLERTOUCHPADMOTION";
        case SDL_CONTROLLERTOUCHPADUP:      return "SDL_CONTROLLERTOUCHPADUP";
        case SDL_CONTROLLERSENSORUPDATE:    return "SDL_CONTROLLERSENSORUPDATE";
        case SDL_FINGERDOWN:                return "SDL_FINGERDOWN";
        case SDL_FINGERUP:                  return "SDL_FINGERUP";
        case SDL_FINGERMOTION:              return "SDL_FINGERMOTION";
        case SDL_DOLLARGESTURE:             return "SDL_DOLLARGESTURE";
        case SDL_DOLLARRECORD:              return "SDL_DOLLARRECORD";
        case SDL_MULTIGESTURE:              return "SDL_MULTIGESTURE";
        case SDL_CLIPBOARDUPDATE:           return "SDL_CLIPBOARDUPDATE";
        case SDL_DROPFILE:                  return "SDL_DROPFILE";
        case SDL_DROPTEXT:                  return "SDL_DROPTEXT";
        case SDL_DROPBEGIN:                 return "SDL_DROPBEGIN";
        case SDL_DROPCOMPLETE:              return "SDL_DROPCOMPLETE";
        case SDL_AUDIODEVICEADDED:          return "SDL_AUDIODEVICEADDED";
        case SDL_AUDIODEVICEREMOVED:        return "SDL_AUDIODEVICEREMOVED";
        case SDL_SENSORUPDATE:              return "SDL_SENSORUPDATE";
        case SDL_RENDER_TARGETS_RESET:      return "SDL_RENDER_TARGETS_RESET";
        case SDL_RENDER_DEVICE_RESET:       return "SDL_RENDER_DEVICE_RESET";
        case SDL_POLLSENTINEL:              return "SDL_POLLSENTINEL";
        case SDL_USEREVENT:                 return "SDL_USEREVENT";
        case SDL_LASTEVENT:                 return "SDL_LASTEVENT";
    }

    return "??";
}

static void IN_SDL_WindowFlagsAppStateToString(int appState) {
    const char *fls[12] = { 0 };
    unsigned int pos = 0;
    static int oldAppState = 0;

    appState & SDL_WINDOW_FULLSCREEN    ? fls[pos++] = "SDL_WINDOW_FULLSCREEN"    : qfalse;
    appState & SDL_WINDOW_OPENGL        ? fls[pos++] = "SDL_WINDOW_OPENGL"        : qfalse;
    appState & SDL_WINDOW_SHOWN         ? fls[pos++] = "SDL_WINDOW_SHOWN"         : qfalse;
    appState & SDL_WINDOW_HIDDEN        ? fls[pos++] = "SDL_WINDOW_HIDDEN"        : qfalse;
    appState & SDL_WINDOW_BORDERLESS    ? fls[pos++] = "SDL_WINDOW_BORDERLESS"    : qfalse;
    appState & SDL_WINDOW_RESIZABLE     ? fls[pos++] = "SDL_WINDOW_RESIZABLE"     : qfalse;
    appState & SDL_WINDOW_MINIMIZED     ? fls[pos++] = "SDL_WINDOW_MINIMIZED"     : qfalse;
    appState & SDL_WINDOW_MAXIMIZED     ? fls[pos++] = "SDL_WINDOW_MAXIMIZED"     : qfalse;
    appState & SDL_WINDOW_INPUT_GRABBED ? fls[pos++] = "SDL_WINDOW_INPUT_GRABBED" : qfalse;
    appState & SDL_WINDOW_INPUT_FOCUS   ? fls[pos++] = "SDL_WINDOW_INPUT_FOCUS"   : qfalse;
    appState & SDL_WINDOW_MOUSE_FOCUS   ? fls[pos++] = "SDL_WINDOW_MOUSE_FOCUS"   : qfalse;
    appState & SDL_WINDOW_FOREIGN       ? fls[pos++] = "SDL_WINDOW_FOREIGN"       : qfalse;

    if ( oldAppState == 0 || appState != oldAppState ) {
        oldAppState = appState;
        if ( pos > 0 ) {
            Com_Printf("SDL_Window app state:\n-------------\n");
            for ( int i = 0 ; i < pos ; i++ ) {
                Com_Printf("  %s\n", fls[i]);
            }
        }
    }
}
#endif

/* D. J. Bernstein hash function */
static size_t djb_hash(const char* cp)
{
    size_t hash = 5381;
    while (*cp)
        hash = 33 * hash ^ (unsigned char) *cp++;
    return hash;
}

/* Fowler/Noll/Vo (FNV) hash function, variant 1a */
#if 0
static size_t fnv1a_hash(const char* cp)
{
    size_t hash = 0x811c9dc5;
    while (*cp) {
        hash ^= (unsigned char) *cp++;
        hash *= 0x01000193;
    }
    return hash;
}
#endif

#define MAX_ERRORS 16
void IN_SDL_ShowSDLNewError(void) {
    static size_t errors[MAX_ERRORS];
    static int times[MAX_ERRORS];

    const char *p = SDL_GetError();

    if ( p && p[0] ) {
        size_t hash = djb_hash(p);
        qboolean found = qfalse;
        for ( int i = 0 ; i < MAX_ERRORS ; i++ ) {
            if ( hash == errors[i] ) {
                found = qtrue;
                int t = times[i];
                if ( t == 0 || times[i]++ % 10 == 0 ) { // TODO
                    Com_Printf("SDL_Error : %s\n", p);
                    break;
                }
            }
        }

        if ( ! found ) {
            for ( int i = 0 ; i < MAX_ERRORS; i++ ) {
                if ( errors[i] == 0 ) {
                    errors[i] = hash;
                    break;
                }
            }
        }
    }

    SDL_ClearError();
}

static void IN_ProcessEvent( SDL_Event *e );

#ifdef USE_EMSCRIPTEN_EVENTS
static EM_BOOL IN_ProcessEmEvent (int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData) {
    SDL_Event e = { 0 };

    switch ( eventType ) {
        //case EMSCRIPTEN_EVENT_KEYPRESS: break;
        case EMSCRIPTEN_EVENT_KEYDOWN:
        case EMSCRIPTEN_EVENT_KEYUP:
            printf("EM keyup|down event\n");

#if 0
    SDL_GetKeyboardState
    SDL_GetKeyFromScancode
    SDL_GetKeyName
    SDL_GetScancodeFromKey
    SDL_GetScancodeName
#endif


            e.type                = eventType == EMSCRIPTEN_EVENT_KEYUP ? SDL_KEYUP    : SDL_KEYDOWN;
            e.key.state           = eventType == EMSCRIPTEN_EVENT_KEYUP ? SDL_RELEASED : SDL_PRESSED;
            e.key.keysym.scancode = keyEvent->keyCode;
            e.key.keysym.sym      = keyEvent->keyCode;
            e.key.timestamp       = keyEvent->timestamp;
            e.key.repeat          = keyEvent->repeat;
            e.key.type            = eventType == EMSCRIPTEN_EVENT_KEYUP ? SDL_KEYUP    : SDL_KEYDOWN;

            printf("timestamp:%f\n", keyEvent->timestamp);
            break;

        case EMSCRIPTEN_EVENT_CLICK: break;
        case EMSCRIPTEN_EVENT_MOUSEDOWN: break;
        case EMSCRIPTEN_EVENT_MOUSEUP: break;
        case EMSCRIPTEN_EVENT_DBLCLICK: break;
        case EMSCRIPTEN_EVENT_MOUSEMOVE: break;
        case EMSCRIPTEN_EVENT_WHEEL: break;
        case EMSCRIPTEN_EVENT_RESIZE: break;
        case EMSCRIPTEN_EVENT_SCROLL: break;
        case EMSCRIPTEN_EVENT_BLUR: break;
        case EMSCRIPTEN_EVENT_FOCUS: break;
        case EMSCRIPTEN_EVENT_FOCUSIN: break;
        case EMSCRIPTEN_EVENT_FOCUSOUT: break;
        case EMSCRIPTEN_EVENT_DEVICEORIENTATION: break;
        case EMSCRIPTEN_EVENT_DEVICEMOTION: break;
        case EMSCRIPTEN_EVENT_ORIENTATIONCHANGE: break;
        case EMSCRIPTEN_EVENT_FULLSCREENCHANGE: break;
        case EMSCRIPTEN_EVENT_POINTERLOCKCHANGE: break;
        case EMSCRIPTEN_EVENT_VISIBILITYCHANGE: break;
        case EMSCRIPTEN_EVENT_TOUCHSTART: break;
        case EMSCRIPTEN_EVENT_TOUCHEND: break;
        case EMSCRIPTEN_EVENT_TOUCHMOVE: break;
        case EMSCRIPTEN_EVENT_TOUCHCANCEL: break;
        case EMSCRIPTEN_EVENT_GAMEPADCONNECTED: break;
        case EMSCRIPTEN_EVENT_GAMEPADDISCONNECTED: break;
        case EMSCRIPTEN_EVENT_BEFOREUNLOAD: break;
        case EMSCRIPTEN_EVENT_BATTERYCHARGINGCHANGE: break;
        case EMSCRIPTEN_EVENT_BATTERYLEVELCHANGE: break;
        case EMSCRIPTEN_EVENT_WEBGLCONTEXTLOST: break;
        case EMSCRIPTEN_EVENT_WEBGLCONTEXTRESTORED: break;
        case EMSCRIPTEN_EVENT_MOUSEENTER: break;
        case EMSCRIPTEN_EVENT_MOUSELEAVE: break;
        case EMSCRIPTEN_EVENT_MOUSEOVER: break;
        case EMSCRIPTEN_EVENT_MOUSEOUT: break;
        case EMSCRIPTEN_EVENT_CANVASRESIZED: break;
        case EMSCRIPTEN_EVENT_POINTERLOCKERROR: break;
        default:
            Com_Printf("Unknown event\n");
    }

    IN_ProcessEvent ( &e );

    return EM_TRUE; // if the process consumed the event
}
#endif

/*
===============
IN_ProcessEvent
EM_BOOL event_callback (int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData) {
===============
*/
int Key_GetCatcher(void);
static void IN_ProcessEvent( SDL_Event *e )
{
       keyNum_t key         = 0;
static keyNum_t lastKeyDown = 0;

    switch( e->type )
    {
        case SDL_KEYDOWN:
            if ( e->key.repeat && Key_GetCatcher() == 0 ) {
                break;
            }

            if( ( key = IN_TranslateSDLToQ3Key( &e->key.keysym, qtrue ) ) ) {
                Com_QueueEvent( in_eventTime, SE_KEY, key, qtrue, 0, NULL );
            }

            if( key == K_BACKSPACE ) {
                Com_QueueEvent( in_eventTime, SE_CHAR, CTRL('h'), 0, 0, NULL );
            }
            else if( keys[K_CTRL].down && key >= 'a' && key <= 'z' ) {
                Com_QueueEvent( in_eventTime, SE_CHAR, CTRL(key), 0, 0, NULL );
            }

            lastKeyDown = key;
            break;

        case SDL_KEYUP:
            if( ( key = IN_TranslateSDLToQ3Key( &e->key.keysym, qfalse ) ) ) {
                Com_QueueEvent( in_eventTime, SE_KEY, key, qfalse, 0, NULL );
            }

            lastKeyDown = 0;
            break;

        case SDL_TEXTINPUT:
            if( lastKeyDown != K_CONSOLE )
            {
                char *c = e->text.text;

                // Quick and dirty UTF-8 to UTF-32 conversion
                while( *c )
                {
                    int utf32 = 0;

                    if( ( *c & 0x80 ) == 0 )
                        utf32 = *c++;
                    else if( ( *c & 0xE0 ) == 0xC0 ) // 110x xxxx
                    {
                        utf32 |= ( *c++ & 0x1F ) << 6;
                        utf32 |= ( *c++ & 0x3F );
                    }
                    else if( ( *c & 0xF0 ) == 0xE0 ) // 1110 xxxx
                    {
                        utf32 |= ( *c++ & 0x0F ) << 12;
                        utf32 |= ( *c++ & 0x3F ) << 6;
                        utf32 |= ( *c++ & 0x3F );
                    }
                    else if( ( *c & 0xF8 ) == 0xF0 ) // 1111 0xxx
                    {
                        utf32 |= ( *c++ & 0x07 ) << 18;
                        utf32 |= ( *c++ & 0x3F ) << 12;
                        utf32 |= ( *c++ & 0x3F ) << 6;
                        utf32 |= ( *c++ & 0x3F );
                    }
                    else
                    {
                        Com_DPrintf( "Unrecognised UTF-8 lead byte: 0x%x\n", (unsigned int)*c );
                        c++;
                    }

                    if( utf32 != 0 )
                    {
                        if( IN_IsConsoleKey( 0, utf32 ) )
                        {
                            Com_QueueEvent( in_eventTime, SE_KEY, K_CONSOLE, qtrue, 0, NULL );
                            Com_QueueEvent( in_eventTime, SE_KEY, K_CONSOLE, qfalse, 0, NULL );
                        }
                        else
                            Com_QueueEvent( in_eventTime, SE_CHAR, utf32, 0, 0, NULL );
                    }
                }
            }
            break;

        case SDL_MOUSEMOTION:
            if( mouseActive )
            {
                //printf("SDL_MOUSEMOTION timestamp: %d\n", e->motion.timestamp);

                if( !e->motion.xrel && !e->motion.yrel )
                    break;
                Com_QueueEvent( in_eventTime, SE_MOUSE, e->motion.xrel, e->motion.yrel, 0, NULL );
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            {
                // Com_Printf("SDL_MOUSEBUTTONDOWN|SDL_MOUSEBUTTONUP\n");
                int b;
                switch( e->button.button )
                {
                    case SDL_BUTTON_LEFT:   b = K_MOUSE1;     break;
                    case SDL_BUTTON_MIDDLE: b = K_MOUSE3;     break;
                    case SDL_BUTTON_RIGHT:  b = K_MOUSE2;     break;
                    case SDL_BUTTON_X1:     b = K_MOUSE4;     break;
                    case SDL_BUTTON_X2:     b = K_MOUSE5;     break;
                    default:                b = K_AUX1 + ( e->button.button - SDL_BUTTON_X2 + 1 ) % 16; break;
                }

                Com_QueueEvent( in_eventTime, SE_KEY, b, ( e->type == SDL_MOUSEBUTTONDOWN ? qtrue : qfalse ), 0, NULL );
            }
            break;

        case SDL_MOUSEWHEEL:
            // Com_Printf("SDL_MOUSEWHEEL\n");
            if( e->wheel.y > 0 )
            {
                Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
                Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
            }
            else if( e->wheel.y < 0 )
            {
                Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
                Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
            }
            break;

        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
            if (in_joystick->integer)
                IN_InitJoystick();
            break;

        case SDL_QUIT:
            println;
            Cbuf_ExecuteText(EXEC_NOW, "quit Closed window\n");
            break;

        case SDL_WINDOWEVENT:
            switch( e->window.event )
            {
                case SDL_WINDOWEVENT_RESIZED:
                    {
                        println;
                        int width, height;

                        width = e->window.data1;
                        height = e->window.data2;

                        // ignore this event on fullscreen
                        if( cls.glconfig.isFullscreen )
                        {
                            break;
                        }

                        // check if size actually changed
                        if( cls.glconfig.vidWidth == width && cls.glconfig.vidHeight == height )
                        {
                            break;
                        }

                        Cvar_SetValue( "r_customwidth", width );
                        Cvar_SetValue( "r_customheight", height );
                        Cvar_Set( "r_mode", "-1" );

                        // Wait until user stops dragging for 1 second, so
                        // we aren't constantly recreating the GL context while
                        // he tries to drag...
                        vidRestartTime = Sys_Milliseconds( ) + 1000;
                    }
                    break;

                case SDL_WINDOWEVENT_MINIMIZED:    Cvar_SetValue( "com_minimized", 1 ); break;
                case SDL_WINDOWEVENT_RESTORED:
                case SDL_WINDOWEVENT_MAXIMIZED:    Cvar_SetValue( "com_minimized", 0 ); break;
                case SDL_WINDOWEVENT_FOCUS_LOST:   Cvar_SetValue( "com_unfocused", 1 ); break;
                case SDL_WINDOWEVENT_FOCUS_GAINED: Cvar_SetValue( "com_unfocused", 0 ); break;

                case SDL_WINDOWEVENT_SHOWN:        // Com_Printf("SDL_WindowEvent SDL_WINDOWEVENT_SHOWN\n");        break;
                case SDL_WINDOWEVENT_HIDDEN:       // Com_Printf("SDL_WindowEvent SDL_WINDOWEVENT_HIDDEN\n");       break;
                case SDL_WINDOWEVENT_EXPOSED:      // Com_Printf("SDL_WindowEvent SDL_WINDOWEVENT_EXPOSED\n");      break;
                case SDL_WINDOWEVENT_MOVED:        // Com_Printf("SDL_WindowEvent SDL_WINDOWEVENT_MOVED\n");        break;
                case SDL_WINDOWEVENT_ENTER:        // Com_Printf("SDL_WindowEvent SDL_WINDOWEVENT_ENTER\n");        break;
                case SDL_WINDOWEVENT_LEAVE:        // Com_Printf("SDL_WindowEvent SDL_WINDOWEVENT_LEAVE\n");        break;
                case SDL_WINDOWEVENT_CLOSE:        // Com_Printf("SDL_WindowEvent SDL_WINDOWEVENT_CLOSE\n");        break;
                case SDL_WINDOWEVENT_SIZE_CHANGED: // Com_Printf("SDL_WindowEvent SDL_WINDOWEVENT_SIZE_CHANGED\n"); break;
                    break;

                default:
                    Com_Error(ERR_FATAL,"Unknown window event %d\n", e->window.event);
            }
            break;

        default:
            Com_Printf("Unknown event type %d\n", e->type);
            Com_Error(ERR_FATAL,"Unknown event type %d\n", e->type);
            break;
    }
}

/*
===============
IN_ProcessEvents
===============
*/
static void IN_ProcessEvents( void )
{
    SDL_Event e;

    if( ! SDL_WasInit( SDL_INIT_VIDEO ) ) {
        return;
    }

    // proxy-to-worker setting prevents keyboard events.// capturing with emscripten_set_keyup_callback ( EMSCRIPTEN_EVENT_TARGET_DOCUMENT working with testing code Projects/wasm/sdl
    // going to try emscripten_event_callbacks as well as SDL_PollEvent
    // GOING TO REMOVE --proxy-to-worker to get SDL events back
    while( SDL_PollEvent( &e ) )
    {
        IN_ProcessEvent( &e );
    }
}

/*
===============
IN_Frame
===============
*/
void IN_Frame( void )
{
    if ( ! SDL_window ) {
        Com_Error(ERR_FATAL,"SDL_Window is NULL");
    }

    qboolean loading;

    IN_JoyMove( );

    // If not DISCONNECTED (main menu) or ACTIVE (in game), we're loading
    loading = ( clc.state != CA_DISCONNECTED && clc.state != CA_ACTIVE );

    // update isFullscreen since it might of changed since the last vid_restart
    cls.glconfig.isFullscreen = Cvar_VariableIntegerValue( "r_fullscreen" ) != 0;

    if( !cls.glconfig.isFullscreen )
    {
        // Console is down in windowed mode
        IN_DeactivateMouse( cls.glconfig.isFullscreen );
    }
    else if( !cls.glconfig.isFullscreen && loading )
    {
        // Loading in windowed mode
        IN_DeactivateMouse( cls.glconfig.isFullscreen );
    }
    else if( !( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_INPUT_FOCUS ) )
    {
        // Window not got focus
        IN_DeactivateMouse( cls.glconfig.isFullscreen );
    }
    else {
        IN_ActivateMouse( cls.glconfig.isFullscreen );
    }

    IN_ProcessEvents( );

    // Set event time for next frame to earliest possible time an event could happen
    in_eventTime = Sys_Milliseconds( );

    // In case we had to delay actual restart of video system
    if( ( vidRestartTime != 0 ) && ( vidRestartTime < Sys_Milliseconds( ) ) )
    {
        vidRestartTime = 0;
        Cbuf_AddText( "vid_restart\n" );
    }
}

/*
===============
IN_Init
===============
*/
void IN_Init( void *windowData )
{
    if( ! SDL_WasInit( SDL_INIT_VIDEO ) )
    {
        Com_Error( ERR_FATAL, "IN_Init called before SDL_Init( SDL_INIT_VIDEO )" );
        return;
    }

    SDL_ClearError();

    SDL_window = (SDL_Window *)windowData;

    Com_DPrintf( "\n------- Input Initialization -------\n" );

    in_keyboardDebug = Cvar_Get( "in_keyboardDebug", "1", CVAR_ARCHIVE ); // TODO  set to 1 for debugging keyboard events

    // mouse variables
    in_mouse  = Cvar_Get( "in_mouse",  "1", CVAR_ARCHIVE );

    in_joystick          = Cvar_Get( "in_joystick",      "0", CVAR_ARCHIVE|CVAR_LATCH );
    in_joystickThreshold = Cvar_Get( "joy_threshold", "0.15", CVAR_ARCHIVE );

    SDL_StartTextInput( );

    //SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "1");

    mouseAvailable = ( in_mouse->value != 0 );
    IN_DeactivateMouse( Cvar_VariableIntegerValue( "r_fullscreen" ) != 0 );

    int appState = SDL_GetWindowFlags( SDL_window );
    Cvar_SetValue( "com_unfocused", !( appState & SDL_WINDOW_INPUT_FOCUS ) );
    Cvar_SetValue( "com_minimized",    appState & SDL_WINDOW_MINIMIZED );

    //IN_SDL_WindowFlagsAppStateToString(appState);

#ifdef USE_EMSCRIPTEN_EVENTS
    EMSCRIPTEN_RESULT res = emscripten_set_keyup_callback ( EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_TRUE, IN_ProcessEmEvent );
#endif

    IN_InitJoystick( );

    Com_DPrintf( "------------------------------------\n" );
}

/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown( void )
{
    SDL_StopTextInput( );

    IN_DeactivateMouse( Cvar_VariableIntegerValue( "r_fullscreen" ) != 0 );
    mouseAvailable = qfalse;

    IN_ShutdownJoystick( );

    SDL_window = NULL;
}

/*
===============
IN_Restart
===============
*/
void IN_Restart( void )
{
    IN_ShutdownJoystick( );
    IN_Init( SDL_window );
}
