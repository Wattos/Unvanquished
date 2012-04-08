/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

This file is part of Daemon.

Daemon is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Daemon is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "g_local.h"
#include "g_bot.h"
#define AS_OVER_RT3         ((ALIENSENSE_RANGE*0.5f)/M_ROOT3)

float CalcAimPitch(gentity_t *self, botTarget_t target, vec_t launchSpeed ) {
	vec3_t startPos;
	vec3_t targetPos;
	float initialHeight;
	vec3_t forward,right,up;
	vec3_t muzzle;

	BotGetTargetPos(target, targetPos);
	AngleVectors(self->s.origin,forward,right,up);
	CalcMuzzlePoint(self,forward,right,up,muzzle);
	VectorCopy(muzzle, startPos);

	//project everything onto a 2D plane with initial position at (0,0)
	initialHeight = startPos[2];
	targetPos[2] -= initialHeight;
	startPos[2] -= initialHeight;
	float distance2D = sqrt(Square(startPos[0] - targetPos[0]) + Square(startPos[1] - targetPos[1]));
	targetPos[0] = distance2D;

	//for readability's sake
	const float x = targetPos[0];
	const float y = targetPos[2];
	float v = launchSpeed;
	const float g = self->client->ps.gravity;

	//make sure we won't get NaN
	float check = Square(Square(v)) - g * (g * Square(x) + 2*y*Square(v));

	//as long as we will get NaN, increase velocity to compensate
	//This is better than returning some failure value because it gives us the best launch angle possible, even if it wont hit in the end.
	while(check < 0) {
		v+=5;
		check = Square(Square(v)) - g * (g * Square(x) + 2*y*Square(v));
	}
	//calculate required angle of launch
	float angle1 = atan((Square(v) + sqrt(check))/(g * x));
	float angle2 = atan((Square(v) - sqrt(check))/(g * x));

	//take the smaller angle
	float angle = (angle1 < angle2) ? angle1 : angle2;

	//convert to degrees (ps.viewangles units)
	angle = RAD2DEG(angle);
	return angle;
}
float CalcPounceAimPitch(gentity_t *self, botTarget_t target) {
	vec_t speed = (self->client->ps.stats[STAT_CLASS] == PCL_ALIEN_LEVEL3) ? LEVEL3_POUNCE_JUMP_MAG : LEVEL3_POUNCE_JUMP_MAG_UPG;
	return CalcAimPitch(self, target, speed);

	//in usrcmd angles, a positive angle is down, so multiply angle by -1
	// botCmdBuffer->angles[PITCH] = ANGLE2SHORT(-angle);
}
float CalcBarbAimPitch(gentity_t *self, botTarget_t target) {
	vec_t speed = LEVEL3_BOUNCEBALL_SPEED;
	return CalcAimPitch(self, target, speed);

	//in usrcmd angles, a positive angle is down, so multiply angle by -1
	//botCmdBuffer->angles[PITCH] = ANGLE2SHORT(-angle);
}
bool G_RoomForClassChange( gentity_t *ent, class_t classt,
	vec3_t newOrigin )
{
	vec3_t    fromMins, fromMaxs;
	vec3_t    toMins, toMaxs;
	vec3_t    temp;
	trace_t   tr;
	float     nudgeHeight;
	float     maxHorizGrowth;
	class_t   oldClass = (class_t)ent->client->ps.stats[ STAT_CLASS ];

	BG_ClassBoundingBox( oldClass, fromMins, fromMaxs, NULL, NULL, NULL );
	BG_ClassBoundingBox( classt, toMins, toMaxs, NULL, NULL, NULL );

	VectorCopy( ent->s.origin, newOrigin );

	// find max x/y diff
	maxHorizGrowth = toMaxs[ 0 ] - fromMaxs[ 0 ];
	if( toMaxs[ 1 ] - fromMaxs[ 1 ] > maxHorizGrowth )
		maxHorizGrowth = toMaxs[ 1 ] - fromMaxs[ 1 ];
	if( toMins[ 0 ] - fromMins[ 0 ] > -maxHorizGrowth )
		maxHorizGrowth = -( toMins[ 0 ] - fromMins[ 0 ] );
	if( toMins[ 1 ] - fromMins[ 1 ] > -maxHorizGrowth )
		maxHorizGrowth = -( toMins[ 1 ] - fromMins[ 1 ] );

	if( maxHorizGrowth > 0.0f )
	{
		// test by moving the player up the max required on a 60 degree slope
		nudgeHeight = maxHorizGrowth * 2.0f;
	}
	else
	{
		// player is shrinking, so there's no need to nudge them upwards
		nudgeHeight = 0.0f;
	}

	// find what the new origin would be on a level surface
	newOrigin[ 2 ] -= toMins[ 2 ] - fromMins[ 2 ];

	//compute a place up in the air to start the real trace
	VectorCopy( newOrigin, temp );
	temp[ 2 ] += nudgeHeight;
	trap_Trace( &tr, newOrigin, toMins, toMaxs, temp, ent->s.number, MASK_PLAYERSOLID );

	//trace down to the ground so that we can evolve on slopes
	VectorCopy( newOrigin, temp );
	temp[ 2 ] += ( nudgeHeight * tr.fraction );
	trap_Trace( &tr, temp, toMins, toMaxs, newOrigin, ent->s.number, MASK_PLAYERSOLID );
	VectorCopy( tr.endpos, newOrigin );

	//make REALLY sure
	trap_Trace( &tr, newOrigin, toMins, toMaxs, newOrigin,
		ent->s.number, MASK_PLAYERSOLID );

	//check there is room to evolve
	return (!tr.startsolid && tr.fraction == 1.0f );
}
bool BotEvolveToClass( gentity_t *ent, class_t newClass)
{
	int clientNum;
	int i;
	vec3_t infestOrigin;
	class_t currentClass = ent->client->pers.classSelection;
	int numLevels;
	int entityList[ MAX_GENTITIES ];
	vec3_t range = { AS_OVER_RT3, AS_OVER_RT3, AS_OVER_RT3 };
	vec3_t mins, maxs;
	int num;
	gentity_t *other;


	if( ent->client->ps.stats[ STAT_HEALTH ] <= 0 )
		return false;

	clientNum = ent->client - level.clients;

	//if we are not currently spectating, we are attempting evolution
	if( ent->client->pers.classSelection != PCL_NONE )
	{
		if( ( ent->client->ps.stats[ STAT_STATE ] & SS_WALLCLIMBING ) )
			ent->client->pers.cmd.upmove = 0;

		//check there are no humans nearby
		VectorAdd( ent->client->ps.origin, range, maxs );
		VectorSubtract( ent->client->ps.origin, range, mins );

		num = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );
		for( i = 0; i < num; i++ )
		{
			other = &g_entities[ entityList[ i ] ];

			if( ( other->client && other->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS ) ||
				( other->s.eType == ET_BUILDABLE && other->buildableTeam == TEAM_HUMANS ) )
				return false;
		}

		if( !G_Overmind() )
			return false;

		numLevels = BG_ClassCanEvolveFromTo( currentClass, newClass,(short)ent->client->ps.persistant[ PERS_CREDIT ], g_alienStage.integer, 0);

		if( G_RoomForClassChange( ent, newClass, infestOrigin ) )
		{
			//...check we can evolve to that class
			if( numLevels >= 0 &&
				BG_ClassAllowedInStage( newClass, (stage_t)g_alienStage.integer ) &&
				BG_ClassIsAllowed( newClass ) )
			{

				ent->client->pers.evolveHealthFraction = (float)ent->client->ps.stats[ STAT_HEALTH ] /
					(float)BG_Class( currentClass )->health;

				if( ent->client->pers.evolveHealthFraction < 0.0f )
					ent->client->pers.evolveHealthFraction = 0.0f;
				else if( ent->client->pers.evolveHealthFraction > 1.0f )
					ent->client->pers.evolveHealthFraction = 1.0f;

				//remove credit
				G_AddCreditToClient( ent->client, -(short)numLevels, qtrue );
				ent->client->pers.classSelection = newClass;
				ent->botMind->navQuery = navQuerys[newClass];
				ent->botMind->navFilter = &navFilters[newClass];
				ClientUserinfoChanged( clientNum, qfalse );
				VectorCopy( infestOrigin, ent->s.pos.trBase );
				ClientSpawn( ent, ent, ent->s.pos.trBase, ent->s.apos.trBase );

				//trap_SendServerCommand( -1, va( "print \"evolved to %s\n\"", classname) );

				return true;
			}
			else
				//trap_SendServerCommand( -1, va( "print \"Not enough evos to evolve to %s\n\"", classname) );
				return false;
		}
		else
			return false;
	}
	return false;
}
bool BotCanEvolveToClass(gentity_t *self, class_t newClass) {
	return (BG_ClassCanEvolveFromTo((class_t)self->client->ps.stats[STAT_CLASS],newClass,self->client->ps.persistant[PERS_CREDIT],g_alienStage.integer,0) >= 0);
}
/*
==============================
BotTaskEvolve
==============================
*/
botTaskStatus_t BotTaskEvolve ( gentity_t *self, usercmd_t *botCmdBuffer )
{
	if(BotGetTeam(self) != TEAM_ALIENS)
		return TASK_STOPPED;

	if(!g_bot_evolve.integer)
		return TASK_STOPPED;

	if(BotCanEvolveToClass(self, PCL_ALIEN_LEVEL4) && g_bot_level4.integer) {
		BotEvolveToClass(self, PCL_ALIEN_LEVEL4);
	} else if(BotCanEvolveToClass(self, PCL_ALIEN_LEVEL3_UPG) && g_bot_level3upg.integer) {
		BotEvolveToClass(self, PCL_ALIEN_LEVEL3_UPG);
	} else if(BotCanEvolveToClass(self, PCL_ALIEN_LEVEL3) && (!BG_ClassAllowedInStage(PCL_ALIEN_LEVEL3_UPG, (stage_t) g_alienStage.integer) || !g_bot_level2upg.integer || !g_bot_level3upg.integer) && g_bot_level3.integer){
		BotEvolveToClass(self, PCL_ALIEN_LEVEL3);
	} else if(BotCanEvolveToClass(self, PCL_ALIEN_LEVEL2_UPG) && g_bot_level2upg.integer) {
		BotEvolveToClass(self, PCL_ALIEN_LEVEL2_UPG);
	} else if(BotCanEvolveToClass(self, PCL_ALIEN_LEVEL2) && g_humanStage.integer == 0 && g_bot_level2.integer) {
		BotEvolveToClass(self, PCL_ALIEN_LEVEL2);
	} else if(BotCanEvolveToClass(self, PCL_ALIEN_LEVEL1_UPG) && g_humanStage.integer == 0 && g_bot_level1upg.integer) {
		BotEvolveToClass(self, PCL_ALIEN_LEVEL1_UPG);
	} else if(BotCanEvolveToClass(self, PCL_ALIEN_LEVEL1) && g_humanStage.integer == 0 && g_bot_level1.integer) {
		BotEvolveToClass(self, PCL_ALIEN_LEVEL1);
	} else if(BotCanEvolveToClass(self, PCL_ALIEN_LEVEL0)) {
		BotEvolveToClass(self, PCL_ALIEN_LEVEL0);
	}
	return TASK_STOPPED;
}
botTaskStatus_t BotTaskBuildA(gentity_t *self, usercmd_t *botCmdBuffer) {
	return TASK_STOPPED; //TODO: Implement
}

botTaskStatus_t BotTaskHealA(gentity_t *self, usercmd_t *botCmdBuffer) {
	const int maxHealth = BG_Class((class_t)self->client->ps.stats[STAT_CLASS])->health;
	gentity_t *healTarget = NULL;
	float distToHealTarget = 0;

	//find best heal target
	if(!(healTarget = self->botMind->closestBuildings.booster.ent)) {
		if(!(healTarget = self->botMind->closestBuildings.egg.ent)) {
				healTarget = self->botMind->closestBuildings.overmind.ent;
				distToHealTarget = Com_Clamp(0,MAX_HEAL_DIST,self->botMind->closestBuildings.overmind.distance);
		} else {
			distToHealTarget = Com_Clamp(0,MAX_HEAL_DIST,self->botMind->closestBuildings.egg.distance);
		}
	} else {
		distToHealTarget = Com_Clamp(0,MAX_HEAL_DIST,self->botMind->closestBuildings.booster.distance);
	}

	if(!healTarget)
		return TASK_STOPPED;

	if(BotGetTeam(self) != TEAM_ALIENS)
		return TASK_STOPPED;

		//we are fully healed 
	if(maxHealth == self->health)
		return TASK_STOPPED;

	if(self->botMind->bestEnemy.ent) {
		if(self->botMind->bestEnemy.distance <= BOT_ENGAGE_DIST)
			return TASK_STOPPED;
	}
	//check conditions upon entering task first time
	if(self->botMind->task != BOT_TASK_HEAL) {
		float percentHealth = ((float) self->health / maxHealth);
		if(distToHealTarget * percentHealth > BOT_FUZZY_HEAL_VALUE)
			return TASK_STOPPED;
	}

	if(BotRoutePermission(self, BOT_TASK_HEAL)) {
		if(!BotChangeTarget(self, healTarget,NULL)) {
			return TASK_STOPPED;
		}
	}

	if(!BotTargetIsEntity(self->botMind->goal)) {
		return TASK_STOPPED;
	}

	//target has died, signal goal is unusable
	if(self->botMind->goal.ent->health <= 0) {
		return TASK_STOPPED;
	}

	if(DistanceToGoalSquared(self) > Square(self->r.maxs[0]+100))
		BotMoveToGoal(self, botCmdBuffer);
	return TASK_RUNNING;
}
