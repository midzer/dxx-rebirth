/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Functions for refueling centers.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "fuelcen.h"
#include "gameseg.h"
#include "game.h"		// For FrameTime
#include "dxxerror.h"
#include "gauges.h"
#include "vclip.h"
#include "fireball.h"
#include "robot.h"
#include "powerup.h"

#include "wall.h"
#include "sounds.h"
#include "morph.h"
#include "3d.h"
#include "bm.h"
#include "polyobj.h"
#include "ai.h"
#include "gamemine.h"
#include "gamesave.h"
#include "player.h"
#include "collide.h"
#include "laser.h"
#include "multi.h"
#include "multibot.h"
#include "escort.h"
#include "byteswap.h"

// The max number of fuel stations per mine.

static const fix Fuelcen_refill_speed = i2f(1);
static const fix Fuelcen_give_amount = i2f(25);
static const fix Fuelcen_max_amount = i2f(100);

// Every time a robot is created in the morphing code, it decreases capacity of the morpher
// by this amount... when capacity gets to 0, no more morphers...
const fix EnergyToCreateOneRobot = i2f(1);

#define MATCEN_HP_DEFAULT			F1_0*500; // Hitpoints
#define MATCEN_INTERVAL_DEFAULT	F1_0*5;	//  5 seconds

matcen_info RobotCenters[MAX_ROBOT_CENTERS];
int Num_robot_centers;

FuelCenter Station[MAX_NUM_FUELCENS];
int Num_fuelcenters = 0;

segment * PlayerSegment= NULL;

#ifdef EDITOR
const char	Special_names[MAX_CENTER_TYPES][11] = {
	"NOTHING   ",
	"FUELCEN   ",
	"REPAIRCEN ",
	"CONTROLCEN",
	"ROBOTMAKER",
#if defined(DXX_BUILD_DESCENT_II)
	"GOAL_RED",
	"GOAL_BLUE",
#endif
};
#endif

//------------------------------------------------------------
// Resets all fuel center info
void fuelcen_reset()
{
	Num_fuelcenters = 0;
	for(unsigned i=0; i<sizeof(Segments)/sizeof(Segments[0]); i++ )
		Segments[i].special = SEGMENT_IS_NOTHING;

	Num_robot_centers = 0;

}

#ifndef NDEBUG		//this is sometimes called by people from the debugger
static void reset_all_robot_centers() __attribute_used;
static void reset_all_robot_centers()
{
	int i;

	// Remove all materialization centers
	for (i=0; i<Num_segments; i++)
		if (Segments[i].special == SEGMENT_IS_ROBOTMAKER) {
			Segments[i].special = SEGMENT_IS_NOTHING;
			Segments[i].matcen_num = -1;
		}
}
#endif

//------------------------------------------------------------
// Turns a segment into a fully charged up fuel center...
void fuelcen_create( segment *segp)
{
	int	station_type;

	station_type = segp->special;

	switch( station_type )	{
	case SEGMENT_IS_NOTHING:
#if defined(DXX_BUILD_DESCENT_II)
	case SEGMENT_IS_GOAL_BLUE:
	case SEGMENT_IS_GOAL_RED:
#endif
		return;
	case SEGMENT_IS_FUELCEN:
	case SEGMENT_IS_REPAIRCEN:
	case SEGMENT_IS_CONTROLCEN:
	case SEGMENT_IS_ROBOTMAKER:
		break;
	default:
		Error( "Invalid station type %d in fuelcen.c\n", station_type );
	}

	Assert( Num_fuelcenters < MAX_NUM_FUELCENS );
	Assert( Num_fuelcenters > -1 );

	segp->value = Num_fuelcenters;
	Station[Num_fuelcenters].Type = station_type;
	Station[Num_fuelcenters].MaxCapacity = Fuelcen_max_amount;
	Station[Num_fuelcenters].Capacity = Station[Num_fuelcenters].MaxCapacity;
	Station[Num_fuelcenters].segnum = segp-Segments;
	Station[Num_fuelcenters].Timer = -1;
	Station[Num_fuelcenters].Flag = 0;
//	Station[Num_fuelcenters].NextRobotType = -1;
//	Station[Num_fuelcenters].last_created_obj=NULL;
//	Station[Num_fuelcenters].last_created_sig = -1;
	compute_segment_center(&Station[Num_fuelcenters].Center, segp);

//	if (station_type == SEGMENT_IS_ROBOTMAKER)
//		Station[Num_fuelcenters].Capacity = i2f(Difficulty_level + 3);

	Num_fuelcenters++;
}

//------------------------------------------------------------
// Adds a matcen that already is a special type into the Station array.
// This function is separate from other fuelcens because we don't want values reset.
static void matcen_create( segment *segp)
{
	int	station_type = segp->special;

	Assert(station_type == SEGMENT_IS_ROBOTMAKER);

	Assert( Num_fuelcenters < MAX_NUM_FUELCENS );
	Assert( Num_fuelcenters > -1 );

	segp->value = Num_fuelcenters;
	Station[Num_fuelcenters].Type = station_type;
	Station[Num_fuelcenters].Capacity = i2f(Difficulty_level + 3);
	Station[Num_fuelcenters].MaxCapacity = Station[Num_fuelcenters].Capacity;

	Station[Num_fuelcenters].segnum = segp-Segments;
	Station[Num_fuelcenters].Timer = -1;
	Station[Num_fuelcenters].Flag = 0;
//	Station[Num_fuelcenters].NextRobotType = -1;
//	Station[Num_fuelcenters].last_created_obj=NULL;
//	Station[Num_fuelcenters].last_created_sig = -1;
	compute_segment_center(&Station[Num_fuelcenters].Center, segp );

	segp->matcen_num = Num_robot_centers;
	Num_robot_centers++;

	RobotCenters[segp->matcen_num].hit_points = MATCEN_HP_DEFAULT;
	RobotCenters[segp->matcen_num].interval = MATCEN_INTERVAL_DEFAULT;
	RobotCenters[segp->matcen_num].segnum = segp-Segments;
	RobotCenters[segp->matcen_num].fuelcen_num = Num_fuelcenters;

	Num_fuelcenters++;
}

//------------------------------------------------------------
// Adds a segment that already is a special type into the Station array.
void fuelcen_activate( segment * segp, int station_type )
{
	segp->special = station_type;

	if (segp->special == SEGMENT_IS_ROBOTMAKER)
		matcen_create( segp);
	else
		fuelcen_create( segp);
	
}

//	The lower this number is, the more quickly the center can be re-triggered.
//	If it's too low, it can mean all the robots won't be put out, but for about 5
//	robots, that's not real likely.
#define	MATCEN_LIFE (i2f(30-2*Difficulty_level))

//------------------------------------------------------------
//	Trigger (enable) the materialization center in segment segnum
void trigger_matcen(int segnum)
{
	segment		*segp = &Segments[segnum];
	vms_vector	pos, delta;
	FuelCenter	*robotcen;
	int			objnum;

	Assert(segp->special == SEGMENT_IS_ROBOTMAKER);
	Assert(segp->matcen_num < Num_fuelcenters);
	Assert((segp->matcen_num >= 0) && (segp->matcen_num <= Highest_segment_index));

	robotcen = &Station[RobotCenters[segp->matcen_num].fuelcen_num];

	if (robotcen->Enabled == 1)
		return;

	if (!robotcen->Lives)
		return;

#if defined(DXX_BUILD_DESCENT_II)
	//	MK: 11/18/95, At insane, matcens work forever!
	if (Difficulty_level+1 < NDL)
#endif
		robotcen->Lives--;

	robotcen->Timer = F1_0*1000;	//	Make sure the first robot gets emitted right away.
	robotcen->Enabled = 1;			//	Say this center is enabled, it can create robots.
	robotcen->Capacity = i2f(Difficulty_level + 3);
	robotcen->Disable_time = MATCEN_LIFE;

	//	Create a bright object in the segment.
	pos = robotcen->Center;
	vm_vec_sub(&delta, &Vertices[Segments[segnum].verts[0]], &robotcen->Center);
	vm_vec_scale_add2(&pos, &delta, F1_0/2);
	objnum = obj_create( OBJ_LIGHT, 0, segnum, &pos, NULL, 0, CT_LIGHT, MT_NONE, RT_NONE );
	if (objnum != -1) {
		Objects[objnum].lifeleft = MATCEN_LIFE;
		Objects[objnum].ctype.light_info.intensity = i2f(8);	//	Light cast by a fuelcen.
	} else {
		Int3();
	}
}

#ifdef EDITOR
//------------------------------------------------------------
// Takes away a segment's fuel center properties.
//	Deletes the segment point entry in the FuelCenter list.
void fuelcen_delete( segment * segp )
{
	int i, j;

Restart: ;
	segp->special = 0;

	for (i=0; i<Num_fuelcenters; i++ )	{
		if ( Station[i].segnum == segp-Segments )	{

			// If Robot maker is deleted, fix Segments and RobotCenters.
			if (Station[i].Type == SEGMENT_IS_ROBOTMAKER) {
				Assert(Num_robot_centers > 0);
				Num_robot_centers--;

				for (j=segp->matcen_num; j<Num_robot_centers; j++)
					RobotCenters[j] = RobotCenters[j+1];

				for (j=0; j<Num_fuelcenters; j++) {
					if ( Station[j].Type == SEGMENT_IS_ROBOTMAKER )
						if ( Segments[Station[j].segnum].matcen_num > segp->matcen_num )
							Segments[Station[j].segnum].matcen_num--;
				}
			}

#if defined(DXX_BUILD_DESCENT_II)
			//fix RobotCenters so they point to correct fuelcenter
			for (j=0; j<Num_robot_centers; j++ )
				if (RobotCenters[j].fuelcen_num > i)		//this robotcenter's fuelcen is changing
					RobotCenters[j].fuelcen_num--;
#endif
			Assert(Num_fuelcenters > 0);
			Num_fuelcenters--;
			for (j=i; j<Num_fuelcenters; j++ )	{
				Station[j] = Station[j+1];
				Segments[Station[j].segnum].value = j;
			}
			goto Restart;
		}
	}

}
#endif

#define	ROBOT_GEN_TIME (i2f(5))

object * create_morph_robot( segment *segp, vms_vector *object_pos, int object_id)
{
	short		objnum;
	object	*obj;
	int		default_behavior;

	Players[Player_num].num_robots_level++;
	Players[Player_num].num_robots_total++;

	objnum = obj_create(OBJ_ROBOT, object_id, segp-Segments, object_pos,
				&vmd_identity_matrix, Polygon_models[Robot_info[object_id].model_num].rad,
				CT_AI, MT_PHYSICS, RT_POLYOBJ);

	if ( objnum < 0 ) {
		Int3();
		return NULL;
	}

	obj = &Objects[objnum];

	//Set polygon-object-specific data

	obj->rtype.pobj_info.model_num = Robot_info[get_robot_id(obj)].model_num;
	obj->rtype.pobj_info.subobj_flags = 0;

	//set Physics info

	obj->mtype.phys_info.mass = Robot_info[get_robot_id(obj)].mass;
	obj->mtype.phys_info.drag = Robot_info[get_robot_id(obj)].drag;

	obj->mtype.phys_info.flags |= (PF_LEVELLING);

	obj->shields = Robot_info[get_robot_id(obj)].strength;
	
#if defined(DXX_BUILD_DESCENT_I)
	default_behavior = AIB_NORMAL;
	if (object_id == 10)						//	This is a toaster guy!
		default_behavior = AIB_RUN_FROM;
#elif defined(DXX_BUILD_DESCENT_II)
	default_behavior = Robot_info[get_robot_id(obj)].behavior;
#endif

	init_ai_object(obj, default_behavior, -1 );		//	Note, -1 = segment this robot goes to to hide, should probably be something useful

	create_n_segment_path(obj, 6, -1);		//	Create a 6 segment path from creation point.

#if defined(DXX_BUILD_DESCENT_I)
	if (default_behavior == AIB_RUN_FROM)
		Ai_local_info[objnum].mode = AIM_RUN_FROM_OBJECT;
#elif defined(DXX_BUILD_DESCENT_II)
	Ai_local_info[objnum].mode = ai_behavior_to_mode(default_behavior);
#endif

	return obj;
}

int Num_extry_robots = 15;

//	----------------------------------------------------------------------------------------------------------
static void robotmaker_proc( FuelCenter * robotcen )
{
	fix		dist_to_player;
	vms_vector	cur_object_loc; //, direction;
	int		matcen_num, segnum, objnum;
	object	*obj;
	fix		top_time;
	vms_vector	direction;

	if (robotcen->Enabled == 0)
		return;

	if (robotcen->Disable_time > 0) {
		robotcen->Disable_time -= FrameTime;
		if (robotcen->Disable_time <= 0) {
			robotcen->Enabled = 0;
		}
	}

	//	No robot making in multiplayer mode.
	if ((Game_mode & GM_MULTI) && (!(Game_mode & GM_MULTI_ROBOTS) || !multi_i_am_master()))
		return;

	// Wait until transmorgafier has capacity to make a robot...
	if ( robotcen->Capacity <= 0 ) {
		return;
	}

	matcen_num = Segments[robotcen->segnum].matcen_num;

	if ( matcen_num == -1 ) {
		return;
	}

	matcen_info *mi = &RobotCenters[matcen_num];
	for (unsigned i = 0;; ++i)
	{
		if (i >= (sizeof(mi->robot_flags) / sizeof(mi->robot_flags[0])))
			return;
		if (mi->robot_flags[i])
			break;
	}

	// Wait until we have a free slot for this puppy...
   //	  <<<<<<<<<<<<<<<< Num robots in mine >>>>>>>>>>>>>>>>>>>>>>>>>>    <<<<<<<<<<<< Max robots in mine >>>>>>>>>>>>>>>
	if ( (Players[Player_num].num_robots_level - Players[Player_num].num_kills_level) >= (Gamesave_num_org_robots + Num_extry_robots ) ) {
		return;
	}

	robotcen->Timer += FrameTime;

	switch( robotcen->Flag )	{
	case 0:		// Wait until next robot can generate
		if (Game_mode & GM_MULTI)
		{
			top_time = ROBOT_GEN_TIME;	
		}
		else
		{
			dist_to_player = vm_vec_dist_quick( &ConsoleObject->pos, &robotcen->Center );
			top_time = dist_to_player/64 + d_rand() * 2 + F1_0*2;
			if ( top_time > ROBOT_GEN_TIME )
				top_time = ROBOT_GEN_TIME + d_rand();
			if ( top_time < F1_0*2 )
				top_time = F1_0*3/2 + d_rand()*2;
		}

		if (robotcen->Timer > top_time )	{
			int	count=0;
			int	i, my_station_num = robotcen-Station;
			object *obj;

			//	Make sure this robotmaker hasn't put out its max without having any of them killed.
			for (i=0; i<=Highest_object_index; i++)
				if (Objects[i].type == OBJ_ROBOT)
					if ((Objects[i].matcen_creator^0x80) == my_station_num)
						count++;
			if (count > Difficulty_level + 3) {
				robotcen->Timer /= 2;
				return;
			}

			//	Whack on any robot or player in the matcen segment.
			count=0;
			segnum = robotcen->segnum;
			for (objnum=Segments[segnum].objects;objnum!=-1;objnum=Objects[objnum].next)	{
				count++;
				if ( count > MAX_OBJECTS )	{
					Int3();
					return;
				}
				if (Objects[objnum].type==OBJ_ROBOT) {
					collide_robot_and_materialization_center(&Objects[objnum]);
					robotcen->Timer = top_time/2;
					return;
				} else if (Objects[objnum].type==OBJ_PLAYER ) {
					collide_player_and_materialization_center(&Objects[objnum]);
					robotcen->Timer = top_time/2;
					return;
				}
			}

			compute_segment_center(&cur_object_loc, &Segments[robotcen->segnum]);
			// HACK!!! The 10 under here should be something equal to the 1/2 the size of the segment.
			obj = object_create_explosion(robotcen->segnum, &cur_object_loc, i2f(10), VCLIP_MORPHING_ROBOT );

			if (obj)
				extract_orient_from_segment(&obj->orient,&Segments[robotcen->segnum]);

			if ( Vclip[VCLIP_MORPHING_ROBOT].sound_num > -1 )		{
				digi_link_sound_to_pos( Vclip[VCLIP_MORPHING_ROBOT].sound_num, robotcen->segnum, 0, &cur_object_loc, 0, F1_0 );
			}
			robotcen->Flag	= 1;
			robotcen->Timer = 0;

		}
		break;
	case 1:			// Wait until 1/2 second after VCLIP started.
		if (robotcen->Timer > (Vclip[VCLIP_MORPHING_ROBOT].play_time/2) )	{

			robotcen->Capacity -= EnergyToCreateOneRobot;
			robotcen->Flag = 0;

			robotcen->Timer = 0;
			compute_segment_center(&cur_object_loc, &Segments[robotcen->segnum]);

			// If this is the first materialization, set to valid robot.
			{
				int	type;
				ubyte   legal_types[sizeof(mi->robot_flags) * 8];   // the width of robot_flags[].
				int	num_types;

				num_types = 0;
				for (unsigned i = 0;; ++i)
				{
					if (i >= (sizeof(mi->robot_flags) / sizeof(mi->robot_flags[0])))
						break;
					uint32_t flags = mi->robot_flags[i];
					for (unsigned j = 0; flags && j < 8 * sizeof(flags); ++j)
					{
						if (flags & 1)
							legal_types[num_types++] = (i * 32) + j;
						flags >>= 1;
					}
				}

				if (num_types == 1)
					type = legal_types[0];
				else
					type = legal_types[(d_rand() * num_types) / 32768];

				obj = create_morph_robot(&Segments[robotcen->segnum], &cur_object_loc, type );
				if (obj != NULL) {
					if (Game_mode & GM_MULTI)
						multi_send_create_robot(robotcen-Station, obj-Objects, type);
					obj->matcen_creator = (robotcen-Station) | 0x80;

					// Make object faces player...
					vm_vec_sub( &direction, &ConsoleObject->pos,&obj->pos );
					vm_vector_2_matrix( &obj->orient, &direction, &obj->orient.uvec, NULL);
	
					morph_start( obj );
					//robotcen->last_created_obj = obj;
					//robotcen->last_created_sig = robotcen->last_created_obj->signature;
				}
			}

		}
		break;
	default:
		robotcen->Flag = 0;
		robotcen->Timer = 0;
	}
}

//-------------------------------------------------------------
// Called once per frame, replenishes fuel supply.
void fuelcen_update_all()
{
	int i;
	fix AmountToreplenish;
	
	AmountToreplenish = fixmul(FrameTime,Fuelcen_refill_speed);

	for (i=0; i<Num_fuelcenters; i++ )	{
		if ( Station[i].Type == SEGMENT_IS_ROBOTMAKER )	{
			if (! (Game_suspended & SUSP_ROBOTS))
				robotmaker_proc( &Station[i] );
		} else if ( (Station[i].MaxCapacity > 0) && (PlayerSegment!=&Segments[Station[i].segnum]) )	{
			if ( Station[i].Capacity < Station[i].MaxCapacity )	{
 				Station[i].Capacity += AmountToreplenish;
				if ( Station[i].Capacity >= Station[i].MaxCapacity )		{
					Station[i].Capacity = Station[i].MaxCapacity;
					//gauge_message( "Fuel center is fully recharged!    " );
				}
			}
		}
	}
}

#if defined(DXX_BUILD_DESCENT_I)
#define FUELCEN_SOUND_DELAY (F1_0/3)
#elif defined(DXX_BUILD_DESCENT_II)
#define FUELCEN_SOUND_DELAY (f1_0/4)		//play every half second
#endif

//-------------------------------------------------------------
fix fuelcen_give_fuel(segment *segp, fix MaxAmountCanTake )
{
	static fix64 last_play_time = 0;

	Assert( segp != NULL );

	PlayerSegment = segp;

	if ( (segp) && (segp->special==SEGMENT_IS_FUELCEN) )	{
		fix amount;

#if defined(DXX_BUILD_DESCENT_II)
		detect_escort_goal_accomplished(-4);	//	UGLY! Hack! -4 means went through fuelcen.
#endif

//		if (Station[segp->value].MaxCapacity<=0)	{
//			HUD_init_message(HM_DEFAULT, "Fuelcenter %d is destroyed.", segp->value );
//			return 0;
//		}

//		if (Station[segp->value].Capacity<=0)	{
//			HUD_init_message(HM_DEFAULT, "Fuelcenter %d is empty.", segp->value );
//			return 0;
//		}

		if (MaxAmountCanTake <= 0 )	{
//			//gauge_message( "Fueled up!");
			return 0;
		}

		amount = fixmul(FrameTime,Fuelcen_give_amount);

		if (amount > MaxAmountCanTake )
			amount = MaxAmountCanTake;

//		if (!(Game_mode & GM_MULTI))
//			if ( Station[segp->value].Capacity < amount  )	{
//				amount = Station[segp->value].Capacity;
//				Station[segp->value].Capacity = 0;
//			} else {
//				Station[segp->value].Capacity -= amount;
//			}

		if (last_play_time + FUELCEN_SOUND_DELAY < GameTime64 || last_play_time > GameTime64)
		{
			last_play_time = GameTime64;
			digi_play_sample( SOUND_REFUEL_STATION_GIVING_FUEL, F1_0/2 );
			if (Game_mode & GM_MULTI)
				multi_send_play_sound(SOUND_REFUEL_STATION_GIVING_FUEL, F1_0/2);
		}

		//HUD_init_message(HM_DEFAULT, "Fuelcen %d has %d/%d fuel", segp->value,f2i(Station[segp->value].Capacity),f2i(Station[segp->value].MaxCapacity) );
		return amount;

	} else {
		return 0;
	}
}

#if defined(DXX_BUILD_DESCENT_II)
//-------------------------------------------------------------
// DM/050904
// Repair centers
// use same values as fuel centers
fix repaircen_give_shields(segment *segp, fix MaxAmountCanTake )
{
	static fix last_play_time=0;

	Assert( segp != NULL );
	PlayerSegment = segp;
	if ( (segp) && (segp->special==SEGMENT_IS_REPAIRCEN) ) {
		fix amount;
//             detect_escort_goal_accomplished(-4);    //      UGLY! Hack! -4 means went through fuelcen.
//             if (Station[segp->value].MaxCapacity<=0)        {
//                     HUD_init_message(HM_DEFAULT, "Repaircenter %d is destroyed.", segp->value );
//                     return 0;
//             }
//             if (Station[segp->value].Capacity<=0)   {
//                     HUD_init_message(HM_DEFAULT, "Repaircenter %d is empty.", segp->value );
//                     return 0;
//             }
		if (MaxAmountCanTake <= 0 ) {
			//gauge_message( "Shields restored!");
			return 0;
		}
		amount = fixmul(FrameTime,Fuelcen_give_amount);
		if (amount > MaxAmountCanTake )
			amount = MaxAmountCanTake;
//        if (!(Game_mode & GM_MULTI))
//                     if ( Station[segp->value].Capacity < amount  )  {
//                             amount = Station[segp->value].Capacity;
//                             Station[segp->value].Capacity = 0;
//                     } else {
//                             Station[segp->value].Capacity -= amount;
//                     }
		if (last_play_time > GameTime64)
			last_play_time = 0;
		if (GameTime64 > last_play_time+FUELCEN_SOUND_DELAY) {
			digi_play_sample( SOUND_REFUEL_STATION_GIVING_FUEL, F1_0/2 );
			if (Game_mode & GM_MULTI)
				multi_send_play_sound(SOUND_REFUEL_STATION_GIVING_FUEL, F1_0/2);
			last_play_time = GameTime64;
		}
//HUD_init_message(HM_DEFAULT, "Fuelcen %d has %d/%d fuel", segp->value,f2i(Station[segp->value].Capacity),f2i(Station[segp->value].MaxCapacity) );
		return amount;
	} else {
		return 0;
	}
}
#endif

//	--------------------------------------------------------------------------------------------
void disable_matcens(void)
{
	int	i;

	for (i=0; i<Num_robot_centers; i++) {
		Station[i].Enabled = 0;
		Station[i].Disable_time = 0;
	}
}

//	--------------------------------------------------------------------------------------------
//	Initialize all materialization centers.
//	Give them all the right number of lives.
void init_all_matcens(void)
{
	int	i;

	for (i=0; i<Num_fuelcenters; i++)
		if (Station[i].Type == SEGMENT_IS_ROBOTMAKER) {
			Station[i].Lives = 3;
			Station[i].Enabled = 0;
			Station[i].Disable_time = 0;
#ifndef NDEBUG
{
			//	Make sure this fuelcen is pointed at by a matcen.
			int	j;
			for (j=0; j<Num_robot_centers; j++) {
				if (RobotCenters[j].fuelcen_num == i)
					break;
			}
			Assert(j != Num_robot_centers);
}
#endif

		}

#ifndef NDEBUG
	//	Make sure all matcens point at a fuelcen
	for (i=0; i<Num_robot_centers; i++) {
		int	fuelcen_num = RobotCenters[i].fuelcen_num;

		Assert(fuelcen_num < Num_fuelcenters);
		Assert(Station[fuelcen_num].Type == SEGMENT_IS_ROBOTMAKER);
	}
#endif

}

#if defined(DXX_BUILD_DESCENT_II)
void fuelcen_check_for_goal(segment *segp)
{
	Assert( segp != NULL );
	Assert (game_mode_capture_flag());

	if (segp->special==SEGMENT_IS_GOAL_BLUE )	{

			if ((get_team(Player_num)==TEAM_BLUE) && (Players[Player_num].flags & PLAYER_FLAGS_FLAG))
			 {
				multi_send_capture_bonus (Player_num);
				Players[Player_num].flags &=(~(PLAYER_FLAGS_FLAG));
				maybe_drop_net_powerup (POW_FLAG_RED);
			 }
	  	 }
	if ( segp->special==SEGMENT_IS_GOAL_RED) {

			if ((get_team(Player_num)==TEAM_RED) && (Players[Player_num].flags & PLAYER_FLAGS_FLAG))
			 {		
				multi_send_capture_bonus (Player_num);
				Players[Player_num].flags &=(~(PLAYER_FLAGS_FLAG));
				maybe_drop_net_powerup (POW_FLAG_BLUE);
			 }
	  	 }
  }

void fuelcen_check_for_hoard_goal(segment *segp)
{
	Assert( segp != NULL );
	Assert (game_mode_hoard());

   if (Player_is_dead)
		return;

	if (segp->special==SEGMENT_IS_GOAL_BLUE || segp->special==SEGMENT_IS_GOAL_RED  )	
	{
		if (Players[Player_num].secondary_ammo[PROXIMITY_INDEX])
		{
				multi_send_orb_bonus (Player_num);
				Players[Player_num].flags &=(~(PLAYER_FLAGS_FLAG));
				Players[Player_num].secondary_ammo[PROXIMITY_INDEX]=0;
      }
	}

}


/*
 * reads an d1_matcen_info structure from a PHYSFS_file
 */
void d1_matcen_info_read(d1_matcen_info *mi, PHYSFS_file *fp)
{
	mi->robot_flags[0] = PHYSFSX_readInt(fp);
	mi->hit_points = PHYSFSX_readFix(fp);
	mi->interval = PHYSFSX_readFix(fp);
	mi->segnum = PHYSFSX_readShort(fp);
	mi->fuelcen_num = PHYSFSX_readShort(fp);
}
#endif

/*
 * reads a matcen_info structure from a PHYSFS_file
 */
#if defined(DXX_BUILD_DESCENT_I)
void matcen_info_read(matcen_info *mi, PHYSFS_file *fp, int version)
#elif defined(DXX_BUILD_DESCENT_II)
void matcen_info_read(matcen_info *mi, PHYSFS_file *fp)
#endif
{
	mi->robot_flags[0] = PHYSFSX_readInt(fp);
#if defined(DXX_BUILD_DESCENT_I)
	if (version > 25)
		/*mi->robot_flags2 =*/ PHYSFSX_readInt(fp);
#elif defined(DXX_BUILD_DESCENT_II)
	mi->robot_flags[1] = PHYSFSX_readInt(fp);
#endif
	mi->hit_points = PHYSFSX_readFix(fp);
	mi->interval = PHYSFSX_readFix(fp);
	mi->segnum = PHYSFSX_readShort(fp);
	mi->fuelcen_num = PHYSFSX_readShort(fp);
}

static void matcen_info_swap(matcen_info *mi, int swap)
{
	if (!swap)
		return;
	
	mi->robot_flags[0] = SWAPINT(mi->robot_flags[0]);
#if defined(DXX_BUILD_DESCENT_II)
	mi->robot_flags[1] = SWAPINT(mi->robot_flags[1]);
#endif
	mi->hit_points = SWAPINT(mi->hit_points);
	mi->interval = SWAPINT(mi->interval);
	mi->segnum = SWAPSHORT(mi->segnum);
	mi->fuelcen_num = SWAPSHORT(mi->fuelcen_num);
}

/*
 * reads n matcen_info structs from a PHYSFS_file and swaps if specified
 */
void matcen_info_read_n_swap(matcen_info *mi, int n, int swap, PHYSFS_file *fp)
{
	int i;
	
	PHYSFS_read(fp, mi, sizeof(*mi), n);
	
	if (swap)
		for (i = 0; i < n; i++)
			matcen_info_swap(&mi[i], swap);
}

void matcen_info_write(matcen_info *mi, short version, PHYSFS_file *fp)
{
	PHYSFS_writeSLE32(fp, mi->robot_flags[0]);
	if (version >= 27)
#if defined(DXX_BUILD_DESCENT_I)
		PHYSFS_writeSLE32(fp, 0 /*mi->robot_flags[1]*/);
#elif defined(DXX_BUILD_DESCENT_II)
		PHYSFS_writeSLE32(fp, mi->robot_flags[1]);
#endif
	PHYSFSX_writeFix(fp, mi->hit_points);
	PHYSFSX_writeFix(fp, mi->interval);
	PHYSFS_writeSLE16(fp, mi->segnum);
	PHYSFS_writeSLE16(fp, mi->fuelcen_num);
}

static void fuelcen_swap(FuelCenter *fc, int swap)
{
	if (!swap)
		return;
	
	fc->Type = SWAPINT(fc->Type);
	fc->segnum = SWAPINT(fc->segnum);
	fc->Capacity = SWAPINT(fc->Capacity);
	fc->MaxCapacity = SWAPINT(fc->MaxCapacity);
	fc->Timer = SWAPINT(fc->Timer);
	fc->Disable_time = SWAPINT(fc->Disable_time);
	fc->Center.x = SWAPINT(fc->Center.x);
	fc->Center.y = SWAPINT(fc->Center.y);
	fc->Center.z = SWAPINT(fc->Center.z);
}

/*
 * reads n Station structs from a PHYSFS_file and swaps if specified
 */
void fuelcen_read_n_swap(FuelCenter *fc, int n, int swap, PHYSFS_file *fp)
{
	int i;
	
	PHYSFS_read(fp, fc, sizeof(FuelCenter), n);
	
	if (swap)
		for (i = 0; i < n; i++)
			fuelcen_swap(&fc[i], swap);
}
