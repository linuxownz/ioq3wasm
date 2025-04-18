// https://gist.githubusercontent.com/Jelvan1/837983b2abaf8f2dd81b2be41078a3cd/raw/a9d3e3bba575e79b75af8691c62fdd7807737ccc/CGaz_StrafeHud.c

#include "cg_local.h"
#include "../qcommon/q_shared_client.h"
#include "../renderercommon/tr_public.h"
#include "../qcommon/cm_public.h"

// #define M_PI 3.14159274101f

typedef struct
{
    vec3_t prev_vel;         // 0x0
    float  prev_width;       // 0xc
    int    prev_commandTime; // 0x10
} accelBarState_t;
accelBarState_t accelBarState; // 0x2b0bb0

extern vmCvar_t    cg_drawCGAZHud;

void CG_DrawCGAZHud(int draw, float opacity, int ypos)
{
    if (!cg.snap || cg.hyperspace) {
        return;
    }

    if ( ! cg_drawCGAZHud.integer ) {
        return;
    }

    // speedometer
    if (qfalse && draw & 2)
    {
        int const   charWidthBig   = 32;
        int const   charWidthSmall = (3 * charWidthBig) / 4;
        float const integer        = floor(cg.xyspeed);

        // integer-part
        char const* str = va("^%d%4.0f", 7, integer);
        int const   len = CG_DrawStrlen(str);
        CG_DrawStringExt(
            640 - len * charWidthBig - 5 * charWidthSmall /*x*/,
            480 - (int)(1.5f * (2.5f * charWidthBig)) /*y*/,
            str,
            colorWhite,
            qfalse /*forceColor*/,
            qtrue /*shadow*/,
            charWidthBig,
            (int)(1.5f * charWidthBig) /*charHeight*/,
            0 /*maxChars*/);

        // fractional-part
        str = va("%0.01f^3u/s", cg.xyspeed - integer);
        //assert(CG_DrawStrlen(str) == 5);
        CG_DrawStringExt(
            640 - 5 * charWidthSmall /*x*/,
            480 - (int)(1.5f * (2.375f * charWidthBig)) /*y*/,
            str + 1, // start at decimal point, skip 0
            colorWhite,
            qfalse /*forceColor*/,
            qtrue /*shadow*/,
            charWidthSmall,
            (int)(1.5f * charWidthSmall) /*charHeight*/,
            0 /*maxChars*/);
    }

    if (!(draw & 1))
    {
      return;
    }

    vec4_t white  = { 1, 1, 1, 1 };
    vec4_t blue   = { .5, .5, 1, 1 };
    vec4_t orange = { 1, .5, 0, 1 };

    float opacity2 = 2.f * opacity;
    if (opacity2 > 1.f)
    {
        opacity2 = 1.f;
    }
    white[3]  = opacity2;
    blue[3]   = opacity2;
    orange[3] = opacity2;

    // accel bar
    if (!(draw & 4))
    {
        vec4_t red = { 1, 0, 0, .8f };
        red[3] *= opacity;

        playerState_t const* ps;
        if (cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_FOLLOW) || cg_nopredict.integer ||
            cg_synchronousClients.integer)
        {
          ps = cg.nextSnap ? &cg.nextSnap->ps : &cg.snap->ps;
        }
        else
        {
          ps = &cg.predictedPlayerState;
        }

        float width;
        if (ps->commandTime <= accelBarState.prev_commandTime)
        {
            width = accelBarState.prev_width;
        }
        else
        {
            vec3_t vel_xy;
            memcpy(vel_xy, ps->velocity, sizeof(vec3_t));
            vel_xy[2] = 0;
            float const speed      = VectorLength(vel_xy);
            float const prev_speed = VectorLength(accelBarState.prev_vel);
            width = (speed - prev_speed) / (ps->commandTime - accelBarState.prev_commandTime);
            width *= 256.f;
            if (ps->groundEntityNum != ENTITYNUM_NONE)
            {
                width *= .1f;
            }

            // update prev state
            memcpy(accelBarState.prev_vel, vel_xy, sizeof(vec3_t));
            accelBarState.prev_commandTime = ps->commandTime;
            accelBarState.prev_width       = width;
        }

        if (width > 0)
        {
            CG_FillRect(cg_crosshairSize.integer + 320.f, 232.f, width, 16.f, red);
        }
        else if (width < 0)
        {
            CG_FillRect(320.f - cg_crosshairSize.integer, 232.f, width, 16.f, red);
        }

        RE_SetColor(colorWhite);
        CG_FillRect(cg_crosshairSize.integer + 320 + 81.92f, 236.f, 1.f, 8.f, white);
        CG_FillRect(320 - cg_crosshairSize.integer - 81.92f, 236.f, -1.f, 8.f, white);
    }

    float const tan_fov_2 = tan(DEG2RAD(cg.refdef.fov_x / 2.f));

    // compass (quadrants & ticks)
    if (!(draw & 8))
    {
        vec4_t colors[4] = {
            { 1, 1, .5, .5 },
            { 0, 1, .5, .5 },
            { 0, 0, .5, .5 },
            { 1, 0, .5, .5 },
        };
        colors[0][3] *= opacity;
        colors[1][3] *= opacity;
        colors[2][3] *= opacity;
        colors[3][3] *= opacity;

        for (float angle = 0.f; angle < 360.f; angle += 45.f)
        {
            float angle_a = AngleSubtract( cg.predictedPlayerState.viewangles[YAW], angle );
            float angle_b = AngleSubtract( cg.predictedPlayerState.viewangles[YAW], angle + 90.f );

            // clip
            if (fabs(angle_a) > cg.refdef.fov_x / 2.f)
            {
                angle_a = copysign(cg.refdef.fov_x / 2.f, angle_a);
            }
            if (fabs(angle_b) > cg.refdef.fov_x / 2.f)
            {
                angle_b = copysign(cg.refdef.fov_x / 2.f, angle_b);
            }

            // project
            float const x_a = (320.f * tan(DEG2RAD(angle_a))) / tan_fov_2;
            float const x_b = (320.f * tan(DEG2RAD(angle_b))) / tan_fov_2;
            if (x_a >= x_b)
            {
                qboolean const mainAxis = fmodf(angle, 90.f) == 0;
                if (mainAxis)
                {
                    int const quadrant = (int)(angle / 90.f);
                    CG_FillRect(x_b + 320.f, ypos + 16.f, x_a - x_b, 16.f, colors[quadrant]);
                }

                // tick
                RE_SetColor(white);
                float const h = mainAxis ? 8 : 4;;
                CG_FillRect(x_a + 320.f - 1.f, ypos + 16.f + 16.f - h, 2.f, h, white);
            }
        }
    }

    if (fabs(cg.predictedPlayerState.velocity[0]) + fabs(cg.predictedPlayerState.velocity[1]) == 0)
    {
      return;
    }

    float const vel_dir = RAD2DEG(atan2(cg.predictedPlayerState.velocity[1], cg.predictedPlayerState.velocity[0]));

    // compass (arrow)
    if (!(draw & 8))
    {
        vec4_t const arrows[2] = {
            { .439453125f, .5, .498046875f, .55859375f }, // char 135 - arrow pointing up
            { .373046875f, .5, .431640625f, .55859375f }, // char 134 - arrow pointing down
        };

        enum { up = 0, down = 1 } dir = up;
        float angle = AngleSubtract(cg.predictedPlayerState.viewangles[YAW], vel_dir);
        if (angle > 90.f)
        {
            // flip arrow
            dir = down;
            angle -= 180.f;
        }

        // clip
        if (fabs(angle) > cg.refdef.fov_x / 2.f)
        {
            angle = copysign(cg.refdef.fov_x / 2.f, angle);
        }

        // project
        float x = (320.f * tan(DEG2RAD(angle))) / tan_fov_2 + 320.f - 8.f;
        float y = ypos + 32.f;
        float w = 16.f;
        float h = 16.f;

        CG_AdjustFrom640(&x, &y, &w, &h);
        RE_SetColor(cg.predictedPlayerState.velocity[0] * cg.predictedPlayerState.velocity[1] != 0
            ? white
            : orange);
        RE_StretchPic(
            x, y, w, h, arrows[dir][0], arrows[dir][1], arrows[dir][2], arrows[dir][3], cgs.media.charsetShader);
    }

    // cgaz
    if (!(draw & 16))
    {
        vec3_t vel_xy;
        vel_xy[0] = cg.predictedPlayerState.velocity[0];
        vel_xy[1] = cg.predictedPlayerState.velocity[1];
        vel_xy[2] = 0;
        float speed = VectorLength(vel_xy);
        float accel = (float)cg.snap->ps.speed * (pmove_msec.value / 1000.f);
        if (cg.predictedPlayerState.groundEntityNum != ENTITYNUM_NONE)
        {
            accel *= 10.f; // hard coded for vq3
            speed *= 1.f - 6.f * (pmove_msec.value / 1000.f); // friction
        }

        if (speed <= cg.snap->ps.speed - accel)
        {
          return;
        }

        vec4_t colors[8] = {
            {   1,   1,   0, 1 }, // yellow
            {   0, .25, .25, 1 }, // cyan
            {   0,   1,   0, 1 }, // green
            { .25, .25, .25, 1 }, // grey
            { .25, .25, .25, 1 }, // grey
            {   0,   1,   0, 1 }, // green
            {   0, .25, .25, 1 }, // cyan
            {   1,   1,   0, 1 }, // yellow
        };
        colors[0][3] *= opacity;
        colors[1][3] *= opacity;
        colors[2][3] *= opacity;
        colors[3][3] *= opacity;
        colors[4][3] *= opacity;
        colors[5][3] *= opacity;
        colors[6][3] *= opacity;
        colors[7][3] *= opacity;

        float yaw = cg.predictedPlayerState.viewangles[YAW];

        // 0x1b = 0x1 | 0x2 | 0x8 | 0x10 = KEY_FORWARD | KEY_BACK | KEY_LEFT | KEY_RIGHT
        // defrag_server = 1 when CS_SERVERINFO contains defrag_vers or defrag_version
        if (cg.predictedPlayerState.stats[13] & 0x1b ) // || !defrag_server)
        {
            yaw += 45.f * cg.predictedPlayerState.movementDir;
        }

        float const max_angle = RAD2DEG(acos((-.5f * accel) / speed));
        float const opt_angle = RAD2DEG(acos((cg.snap->ps.speed - accel) / speed));
        float const min_angle = speed > cg.snap->ps.speed ? RAD2DEG(acos(cg.snap->ps.speed / speed)) : 0;

        float angles[9] = {
            vel_dir - max_angle,
            vel_dir - 90.f,
            vel_dir - opt_angle,
            vel_dir - min_angle,
            vel_dir,
            vel_dir + min_angle,
            vel_dir + opt_angle,
            vel_dir + 90.f,
            vel_dir + max_angle,
        };

        for (int i = 0; i < 8; ++i)
        {
            float angle_a = AngleSubtract( yaw, angles[i] );
            float angle_b = AngleSubtract( yaw, angles[i + 1] );

            // clip
            if (fabs(angle_a) > cg.refdef.fov_x / 2.f)
            {
                angle_a = copysign(cg.refdef.fov_x / 2.f, angle_a);
            }
            if (fabs(angle_b) > cg.refdef.fov_x / 2.f)
            {
                angle_b = copysign(cg.refdef.fov_x / 2.f, angle_b);
            }

            // project
            float const x_a = (320.f * tan(DEG2RAD(angle_a))) / tan_fov_2;
            float const x_b = (320.f * tan(DEG2RAD(angle_b))) / tan_fov_2;

            if (x_a >= x_b)
            {
                CG_FillRect(x_b + 320.f, (float)ypos, x_a - x_b, 16.f, colors[i]);
                if (i == 4)
                {
                    CG_FillRect(x_a + 320.f - 1.f, ypos + 2.f, 2.f, 12.f, blue); // little blue bar in the middle
                }
            }
        }
    }
}
