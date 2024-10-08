/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cl.input.c  -- builds an intended movement command to send to the server

// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include "quakedef.h"

cvar_t	cl_smartjump = {"cl_smartjump", "0"};
cvar_t	cl_lag = {"cl_lag", "0"};

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/

kbutton_t	in_mlook, in_klook, in_freeflymlook;
kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_jump, in_attack;
kbutton_t	in_up, in_down;

int		in_impulse;

void KeyDown (kbutton_t *b)
{
	int	k;
	char	*c;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key

	if (!b->down[0])
	{
		b->down[0] = k;
	}
	else if (!b->down[1])
	{
		b->down[1] = k;
	}
	else
	{
		Con_Printf ("Three keys down for a button!\n");
		return;
	}

	if (b->state & 1)
		return;		// still down
	b->state |= 1 + 2;	// down + impulse down
}

void KeyUp (kbutton_t *b)
{
	int	k;
	char	*c;
	
	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
		return;		// some other key is still holding it down

	if (!(b->state & 1))
		return;		// still up (this should not happen)
	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

void IN_KLookDown (void) {KeyDown(&in_klook);}
void IN_KLookUp (void) {KeyUp(&in_klook);}
void IN_MLookDown (void) {KeyDown(&in_mlook);}
void IN_MLookUp (void)
{
	KeyUp (&in_mlook);
	if (!mlook_active && lookspring.value)
		V_StartPitchDrift ();
}
void IN_FreeFlyMLookDown (void) {KeyDown(&in_freeflymlook);}
void IN_FreeFlyMLookUp (void) {KeyUp(&in_freeflymlook);}
void IN_UpDown(void) {KeyDown(&in_up);}
void IN_UpUp(void) {KeyUp(&in_up);}
void IN_DownDown(void) {KeyDown(&in_down);}
void IN_DownUp(void) {KeyUp(&in_down);}
void IN_LeftDown(void) {KeyDown(&in_left);}
void IN_LeftUp(void) {KeyUp(&in_left);}
void IN_RightDown(void) {KeyDown(&in_right);}
void IN_RightUp(void) {KeyUp(&in_right);}
void IN_ForwardDown(void) {KeyDown(&in_forward);}
void IN_ForwardUp(void) {KeyUp(&in_forward);}
void IN_BackDown(void) {KeyDown(&in_back);}
void IN_BackUp(void) {KeyUp(&in_back);}
void IN_LookupDown(void) {KeyDown(&in_lookup);}
void IN_LookupUp(void) {KeyUp(&in_lookup);}
void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {KeyDown(&in_moveright);}
void IN_MoverightUp(void) {KeyUp(&in_moveright);}

void IN_SpeedDown(void) {KeyDown(&in_speed);}
void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

void IN_AttackDown(void) {KeyDown(&in_attack);}
void IN_AttackUp(void) {KeyUp(&in_attack);}

void IN_UseDown (void) {KeyDown(&in_use);}
void IN_UseUp (void) {KeyUp(&in_use);}

void IN_JumpDown (void)
{
	if (cls.state == ca_connected && cl_smartjump.value && cl.stats[STAT_HEALTH] > 0 && 
	    !cls.demoplayback && cl.inwater && !(in_forward.state & 1))
		KeyDown (&in_up);
	else
		KeyDown (&in_jump);
}

void IN_JumpUp (void)
{
	if (cl_smartjump.value)
		KeyUp (&in_up);
	KeyUp (&in_jump);
}

void IN_Impulse (void) {in_impulse = Q_atoi(Cmd_Argv(1));}

int	weaponstat[7] = {STAT_SHELLS, STAT_SHELLS, STAT_NAILS, STAT_NAILS, STAT_ROCKETS, STAT_ROCKETS, STAT_CELLS};

/*
===============
IN_BestWeapon
===============
*/
void IN_BestWeapon (void)
{
	int	i, impulse;

	for (i = 1 ; i < Cmd_Argc() ; i++)
	{
		impulse = Q_atoi(Cmd_Argv(i));
		if (impulse > 0 && impulse < 9 && (impulse == 1 || ((cl.items & (IT_SHOTGUN << (impulse - 2))) && cl.stats[weaponstat[impulse-2]])))
		{
			in_impulse = impulse;
			break;
		}
	}
}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CL_KeyState (kbutton_t *key)
{
	float		val;
	qboolean	impulsedown, impulseup, down;
	
	impulsedown = key->state & 2;
	impulseup = key->state & 4;
	down = key->state & 1;
	val = 0;
	
	if (impulsedown && !impulseup)
		val = down ? 0.5 : 0;
	if (impulseup && !impulsedown)
		val = 0;
	if (!impulsedown && !impulseup)
		val = down ? 1.0 : 0;
	if (impulsedown && impulseup)
		val = down ? 0.75 : 0.25;

	key->state &= 1;		// clear impulses
	
	return val;
}

//==========================================================================

cvar_t	cl_upspeed = {"cl_upspeed", "200"};
cvar_t	cl_forwardspeed = {"cl_forwardspeed", "200", CVAR_ARCHIVE};
cvar_t	cl_backspeed = {"cl_backspeed", "200", CVAR_ARCHIVE};
cvar_t	cl_sidespeed = {"cl_sidespeed", "350"};

cvar_t	cl_movespeedkey = {"cl_movespeedkey", "2.0"};
cvar_t	cl_anglespeedkey = {"cl_anglespeedkey", "1.5"};

cvar_t	cl_yawspeed = {"cl_yawspeed", "140"};
cvar_t	cl_pitchspeed = {"cl_pitchspeed", "150"};

cvar_t	lookspring = {"lookspring", "0", CVAR_ARCHIVE};
cvar_t	lookstrafe = {"lookstrafe", "0", CVAR_ARCHIVE};
cvar_t	sensitivity = {"sensitivity", "3", CVAR_ARCHIVE};
cvar_t	cursor_sensitivity = { "scr_cursor_sensitivity", "1", CVAR_ARCHIVE }; 
qboolean OnChange_freelook (cvar_t *var, char *string);
cvar_t	freelook = {"freelook", "1", CVAR_ARCHIVE, OnChange_freelook};

cvar_t	m_pitch = {"m_pitch", "0.022", CVAR_ARCHIVE};
cvar_t	m_yaw = {"m_yaw", "0.022", CVAR_ARCHIVE};
cvar_t	m_forward = {"m_forward", "1", CVAR_ARCHIVE};
cvar_t	m_side = {"m_side", "0.8", CVAR_ARCHIVE};
cvar_t	m_accel = { "m_accel", "0", CVAR_ARCHIVE };

cvar_t	cl_maxpitch = { "cl_maxpitch", "80", CVAR_ARCHIVE }; //johnfitz -- variable pitch clamping
cvar_t	cl_minpitch = { "cl_minpitch", "-70", CVAR_ARCHIVE }; //johnfitz -- variable pitch clamping

qboolean OnChange_freelook (cvar_t *var, char *string)
{   
	if (!string[0])
		return true;

	if (!mlook_active && lookspring.value)
		V_StartPitchDrift ();
   
	return false;
}

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles (void)
{
	float	speed, up, down;

	speed = (in_speed.state & 1) ? host_frametime * cl_anglespeedkey.value : host_frametime;

	if (!(in_strafe.state & 1))
	{
		cl.viewangles[YAW] -= speed * cl_yawspeed.value * CL_KeyState (&in_right);
		cl.viewangles[YAW] += speed * cl_yawspeed.value * CL_KeyState (&in_left);
		cl.viewangles[YAW] = anglemod(cl.viewangles[YAW]);
	}
	if (in_klook.state & 1)
	{
		V_StopPitchDrift ();
		cl.viewangles[PITCH] -= speed * cl_pitchspeed.value * CL_KeyState (&in_forward);
		cl.viewangles[PITCH] += speed * cl_pitchspeed.value * CL_KeyState (&in_back);
	}

	up = CL_KeyState (&in_lookup);
	down = CL_KeyState (&in_lookdown);

	cl.viewangles[PITCH] -= speed * cl_pitchspeed.value * up;
	cl.viewangles[PITCH] += speed * cl_pitchspeed.value * down;

	if (up || down)
		V_StopPitchDrift ();

	cl.viewangles[PITCH] = bound (cl_minpitch.value, cl.viewangles[PITCH], cl_maxpitch.value);
	cl.viewangles[ROLL] = bound (-50, cl.viewangles[ROLL], 50);		
}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void CL_BaseMove (usercmd_t *cmd)
{	
	if (cls.signon != SIGNONS)
		return;

	CL_AdjustAngles ();

	memset (cmd, 0, sizeof(*cmd));

	if (in_strafe.state & 1)
	{
		cmd->sidemove += cl_sidespeed.value * CL_KeyState (&in_right);
		cmd->sidemove -= cl_sidespeed.value * CL_KeyState (&in_left);
	}

	cmd->sidemove += cl_sidespeed.value * CL_KeyState (&in_moveright);
	cmd->sidemove -= cl_sidespeed.value * CL_KeyState (&in_moveleft);

	cmd->upmove += cl_upspeed.value * CL_KeyState (&in_up);
	cmd->upmove -= cl_upspeed.value * CL_KeyState (&in_down);

	if (!(in_klook.state & 1))
	{	
		cmd->forwardmove += cl_forwardspeed.value * CL_KeyState (&in_forward);
		cmd->forwardmove -= cl_backspeed.value * CL_KeyState (&in_back);
	}	

// adjust for speed key
	if (in_speed.state & 1)
	{
		cmd->forwardmove *= cl_movespeedkey.value;
		cmd->sidemove *= cl_movespeedkey.value;
		cmd->upmove *= cl_movespeedkey.value;
	}
}

// joe: support for synthetic lag, from ProQuake
sizebuf_t	lag_buff[32];
byte		lag_data[32][128];
unsigned int	lag_head, lag_tail;
double		lag_sendtime[32];

/*
==============
CL_SendLagMove
==============
*/
void CL_SendLagMove (void)
{
	if (cls.state != ca_connected || cls.signon != SIGNONS)
		return;

	while ((lag_tail < lag_head) && (lag_sendtime[lag_tail&31] <= realtime))
	{
		lag_tail++;
		if (++cl.movemessages <= 2)
		{
			lag_head = lag_tail = 0; // JPG - hack: if cl.movemessages has been reset, we should reset these too
			continue;	// return -> continue
		}

		if (NET_SendUnreliableMessage(cls.netcon, &lag_buff[(lag_tail-1)&31]) == -1)
		{
			Con_Printf ("CL_SendMove: lost server connection\n");
			CL_Disconnect ();
		}
	}
}

/*
==============
CL_SendMove
==============
*/
void CL_SendMove (usercmd_t *cmd)
{
	int		i, bits;
	float		lag;
	sizebuf_t	*buf;

	lag = bound(0, cl_lag.value, 1000);

	buf = &lag_buff[lag_head&31];
	buf->maxsize = 128;
	buf->cursize = 0;
	buf->data = lag_data[lag_head&31];
	lag_sendtime[lag_head++&31] = realtime + (lag / 1000.0);

	cl.cmd = *cmd;

// send the movement message
	MSG_WriteByte (buf, clc_move);
	MSG_WriteFloat (buf, cl.mtime[0]);	// so server can get ping times

	if (!cls.demoplayback && (cls.netcon->mod == MOD_JOEQUAKE))
	{
		for (i = 0 ; i < 3 ; i++)
			MSG_WriteAngle16 (buf, cl.viewangles[i], cl.protocolflags);
	}
	else
	{
		for (i = 0 ; i < 3 ; i++)
			MSG_WriteAngle (buf, cl.viewangles[i], cl.protocolflags);
	}

	MSG_WriteShort (buf, cmd->forwardmove);
	MSG_WriteShort (buf, cmd->sidemove);
	MSG_WriteShort (buf, cmd->upmove);

// send button bits
	bits = 0;

	if (in_attack.state & 3)
		bits |= 1;
	in_attack.state &= ~2;

	if (in_jump.state & 3)
		bits |= 2;
	in_jump.state &= ~2;

	MSG_WriteByte (buf, bits);

	MSG_WriteByte (buf, in_impulse);
	in_impulse = 0;

// deliver the message
	if (cls.demoplayback)
		return;

	CL_SendLagMove ();
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	Cmd_AddCommand ("+moveup", IN_UpDown);
	Cmd_AddCommand ("-moveup", IN_UpUp);
	Cmd_AddCommand ("+movedown", IN_DownDown);
	Cmd_AddCommand ("-movedown", IN_DownUp);
	Cmd_AddCommand ("+left", IN_LeftDown);
	Cmd_AddCommand ("-left", IN_LeftUp);
	Cmd_AddCommand ("+right", IN_RightDown);
	Cmd_AddCommand ("-right", IN_RightUp);
	Cmd_AddCommand ("+forward", IN_ForwardDown);
	Cmd_AddCommand ("-forward", IN_ForwardUp);
	Cmd_AddCommand ("+back", IN_BackDown);
	Cmd_AddCommand ("-back", IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+strafe", IN_StrafeDown);
	Cmd_AddCommand ("-strafe", IN_StrafeUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	Cmd_AddCommand ("+attack", IN_AttackDown);
	Cmd_AddCommand ("-attack", IN_AttackUp);
	Cmd_AddCommand ("+use", IN_UseDown);
	Cmd_AddCommand ("-use", IN_UseUp);
	Cmd_AddCommand ("+jump", IN_JumpDown);
	Cmd_AddCommand ("-jump", IN_JumpUp);
	Cmd_AddCommand ("impulse", IN_Impulse);
	Cmd_AddCommand ("+klook", IN_KLookDown);
	Cmd_AddCommand ("-klook", IN_KLookUp);
	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);
	Cmd_AddCommand ("+freeflymlook", IN_FreeFlyMLookDown);
	Cmd_AddCommand ("-freeflymlook", IN_FreeFlyMLookUp);

	Cmd_AddCommand ("bestweapon", IN_BestWeapon);

	Cvar_Register (&cl_upspeed);
	Cvar_Register (&cl_forwardspeed);
	Cvar_Register (&cl_backspeed);
	Cvar_Register (&cl_sidespeed);

	Cvar_Register (&cl_anglespeedkey);
	Cvar_Register (&cl_movespeedkey);

	Cvar_Register (&cl_yawspeed);
	Cvar_Register (&cl_pitchspeed);

	Cvar_Register (&lookspring);
	Cvar_Register (&lookstrafe);
	Cvar_Register (&sensitivity);
	Cvar_Register(&cursor_sensitivity);	
	Cvar_Register (&freelook);

	Cvar_Register (&m_pitch);
	Cvar_Register (&m_yaw);
	Cvar_Register (&m_forward);
	Cvar_Register (&m_side);
	Cvar_Register (&m_accel);

	Cvar_Register(&cl_minpitch);
	Cvar_Register(&cl_maxpitch);

	Cvar_Register (&cl_smartjump);
	Cvar_Register (&cl_lag);
}
