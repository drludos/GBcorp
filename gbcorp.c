/*
			GB Corp.
			
	A game boy game for the GB Compo 2021!
	
						by Dr. Ludos (2021)
	
	Get all my other games: 
			http://drludos.itch.io/
	Support my work and get access to betas and prototypes:
			http://www.patreon.com/drludos

	Gameplay music: krümel (crumb)#0723 - In The Town
	Public domain (GB Studio Community Assets):
	https://github.com/DeerTears/GB-Studio-Community-Assets/tree/master/Music
	
*/

// INCLUDE GBDK FUNCTION LIBRARY														
#include <gb/gb.h>
// INCLUDE HANDY HARDWARE REFERENCES
#include <gb/hardware.h>
// INCLUDE RANDOM FUNCTIONS
#include <rand.h>
//INCLUDE FONT (+CUSTOM COLOR) AND TEXT DISPLAY FUNCTIONS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gb/font.h>
#include <gb/drawing.h>
#include <gb/console.h>
#include <gb/sgb.h>
#include <gb/cgb.h>

//INCLUDE MUSIC PLAYER (GBT Player by AntonioND)
#include "gbt_player.h"
extern const unsigned char * song_Data[];


//================================
//========  GRAPHICS ==============
//================================

//Import all graphics assets from external file
#include "assets.c"

//The font used is GBDK default one, loaded and colorized by using the functions in <gb/font.h>




//==========================================
//==========  VARIABLES & HEADERS ============
//==========================================

// FUNCTION DECLARATIONS
void initGame();
void refreshGUI();
void buildBG();

// VARIABLE DECLARATIONS - STORED IN RAM
//Looping variables (unsigned 8 bit)
UINT8 i, j, k, l;
//Temp variables (usigned 16 bit)
UINT16 m;
//Temp variables (signed 8 bit)
INT8 si, sj;

//Timer to countdown for how long the music can't use a channel as a SFX is playing
UINT8 music_unmute_channels;

//Counter to reset the save data when we hold START for several seconds
UINT8 reset_save; 

//Gameplay (non SRAM)
UINT8 pad; //current player actions (holds the value of the pressed/released state for all the buttons)
UINT8 padOld; //previous player actions state (used to detect new keypresses only once)
UINT8 model; //current model we are running on (used in detection code, can be different from the value saved in SRAM at startup!)
UINT8 txt_score[10]; //The string that will be used to display the score (as it's a long (usigned 32 bits), it's not possible to use sprintf directly here)
UINT8 txt_buffer[20]; //The string buffer that will be used to display various stuffs (printf target BKG only, but we want to draw on the WINDOW using sprintf instead)
UINT8 foeIndexOld; //Store the previous foe Index, to refresh only after a change (as it's a costly operation in CPU time with all those 16 bits numbers to display)
INT8 scrollX; //Amount of BG scrolling remaining to do in X
INT8 scrollY; //Amount of BG scrolling remaining to do in Y


//Text strings (constants manually defined to save CPU time when refreshing GUI, so we simply use set_win_tiles instead of printf)
//The format below is to use the ASCII code for each letter, minus an offset (-32) to "map it" to the matching reduced font set used in the game
//New hire [$
const UINT8 msg_hire[]={ 0x4e-32, 0x65-32, 0x77-32, 0x20-32, 0x68-32, 0x69-32, 0x72-32, 0x65-32, 0x20-32, 0x5b-32, 0x24-32};
//Power
const UINT8 msg_power[]={ 0x50-32, 0x6f-32, 0x77-32, 0x65-32, 0x72-32};
//Level
const UINT8 msg_level[]={ 0x4c-32, 0x65-32, 0x76-32, 0x65-32, 0x6c-32};
//Fire it [$
const UINT8 msg_fire[]={ 0x46-32, 0x69-32, 0x72-32, 0x65-32, 0x20-32, 0x69-32, 0x74-32, 0x20-32, 0x5b-32, 0x24-32};
//-- Inactive --
const UINT8 msg_sleeping[]={ 0x2d-32, 0x2d-32, 0x20-32, 0x49-32, 0x6e-32, 0x61-32, 0x63-32, 0x74-32, 0x69-32, 0x76-32, 0x65-32, 0x20-32, 0x2d-32, 0x2d-32};
//RESET IN 3
const UINT8 msg_clearsave[]={ 0x52-32, 0x45-32, 0x53-32, 0x45-32, 0x54-32, 0x20-32, 0x49-32, 0x4e-32, 0x20-32, 0x33-32};
	
//Animations (background anims)
UINT8 anim_ticks; //Timer before next animation sequence
UINT8 anim_step; //current animation sequence

	
	
//SRAM save
//Constant used to check whether the SRAM contains a save or not
#define SRAM_CHECK  0xDD05B055

//Struct containing the save game (will be mapped to the SRAM through a pointer, see below)
typedef struct savegame {
	UINT32 sram_check;   //check if the SRAM contains a savegame or not (if yes, contains the value of the "SRAM_CHECK" constant, else it will contains anything else)
	UINT8 model;   //previous GB model we were running on (0: uninitialized / 1: DMG / 2: MGB / 3: GBC / 4: SGB)
	UINT32 score;   //the current player score
	UINT8 type[64];   //The current type of each foe (0: empty / 1: DMG / 2: MGB / 3: GBC / 4: SGB)
	UINT8 power[64];   //The current power for each foe
	UINT16 powerUP[64];   //The current price to refill power for each foe
	UINT8 level[64];   //The current xp level for each foe
	UINT16 levelUP[64];   //The current price to increase xp level for each foe
	UINT16 hireCost;   //How will it cost us to hire the next foe (it double with each hire)
	UINT8 loopIndex;   //Current loop step (i.e. foe index) as we don't process them all on every frame
	UINT8 foeIndex;   //Current foe selected (info displayed on GUI and target for player actions)
} savegame;

//Map the savegame struct to the beginning of SRAM (0xA000) for persistent gameplay!
savegame __at (0xA000) save;


//=================================
//==========  MAIN LOOP ============
//=================================
void main(){
	
	//Launch the function that will init the game
	initGame();
	
	//-= GAME LOOP START =-
	while(1){
		
		//======= BACKGROUND SCROLLING =========
		//We do it BEFORE anything else to avoid any kind of screen tearing due to the scrolling (here we are right after the VBLANK)
		
		//If we have to scroll the background, do it now
		//Scroll both X and Y
		if( scrollX != 0 && scrollY != 0 ){
			
			//ScrollX
			//If the scrolling value is too low to be shifted, set it to 1
			if( scrollX == 1 || scrollX == 2 || scrollX == 3 ){
				si=1;
			}
			//Or to -1 in the opposite direction
			else if ( scrollX == -1 || scrollX == -2 || scrollX == -3 ){
				si=-1;
			}
			//Else, we remove a quarter of the remaining amount to scroll to have a smooth "easing" effect on the scrolling
			else {
				//We simply shift the remaining value 2 times to the right, as it's the same as dividing it by 4
				si=scrollX >> 2;
			}
			//Substract the current amount of scrolling done from the remaining total to do
			scrollX -= si;
			
			//ScrollY
			//If the scrolling value is too low to be shifted, set it to 1
			if( scrollY == 1 || scrollY == 2 || scrollY == 3 ){
				sj=1;
			}
			//Or to -1 in the opposite direction
			else if ( scrollY == -1 || scrollY == -2 || scrollY == -3 ){
				sj=-1;
			}
			//Else, we remove a quarter of the remaining amount to scroll to have a smooth "easing" effect on the scrolling
			else {
				//We simply shift the remaining value 2 times to the right, as it's the same as dividing it by 4
				sj=scrollY >> 2;
			}
			//Substract the current amount of scrolling done from the remaining total to do
			scrollY -= sj;
			
			
			//And apply both of the the current amounts to the background scrolling
			scroll_bkg(si, sj);
		}
		//Scroll X only
		else if( scrollX != 0 ){
			
			//If the scrolling value is too low to be shifted, set it to 1
			if( scrollX == 1 || scrollX == 2 || scrollX == 3 ){
				si=1;
			}
			//Or to -1 in the opposite direction
			else if ( scrollX == -1 || scrollX == -2 || scrollX == -3 ){
				si=-1;
			}
			//Else, we remove a quarter of the remaining amount to scroll to have a smooth "easing" effect on the scrolling
			else {
				//We simply shift the remaining value 2 times to the right, as it's the same as dividing it by 4
				si=scrollX >> 2;
			}
			//Substract the current amount of scrolling done from the remaining total to do
			scrollX -= si;
			
			//And apply the current amount to the background scrolling
			scroll_bkg(si, 0);
		}
		//Scroll Y only
		else if( scrollY != 0 ){
			
			//If the scrolling value is too low to be shifted, set it to 1
			if( scrollY == 1 || scrollY == 2 || scrollY == 3 ){
				sj=1;
			}
			//Or to -1 in the opposite direction
			else if ( scrollY == -1 || scrollY == -2 || scrollY == -3 ){
				sj=-1;
			}
			//Else, we remove a quarter of the remaining amount to scroll to have a smooth "easing" effect on the scrolling
			else {
				//We simply shift the remaining value 2 times to the right, as it's the same as dividing it by 4
				sj=scrollY >> 2;
			}
			//Substract the current amount of scrolling done from the remaining total to do
			scrollY -= sj;
			
			//And apply the current amount to the background scrolling
			scroll_bkg(0, sj);
		}
		
		
		
		//======= ANIMATIONS =========
		
		//Working foes: both hands pushing buttons
		//If the animation timer is still ticking, count down
		if( anim_ticks > 0 ){
			--anim_ticks;
		}
		//Else, it's time to change the animation!
		else {
			//Increase the animation step
			++anim_step;
			//Make the animation loop
			if( anim_step == 8 ){ anim_step = 0; }
			
			//And use it to change the current animation (send new tile data in VRAM so all foes are updated instantly!)
			if( anim_step == 0 ){ set_bkg_data(0xAD, 4, tiles_working0); }
			else if( anim_step == 1 ){ set_bkg_data(0xAD, 4, tiles_working1); }
			else if( anim_step == 2 ){ set_bkg_data(0xAD, 4, tiles_working2); }
			else if( anim_step == 3 ){ set_bkg_data(0xAD, 4, tiles_working3); }
			else if( anim_step == 4 ){ set_bkg_data(0xAD, 4, tiles_working4); }
			else if( anim_step == 5 ){ set_bkg_data(0xAD, 4, tiles_working3); }
			else if( anim_step == 6 ){ set_bkg_data(0xAD, 4, tiles_working2); }
			else if( anim_step == 7 ){ set_bkg_data(0xAD, 4, tiles_working1); }
		
			//And reset the timer
			anim_ticks=4;
		}
		
		
		
		//======= BACKEND =========

		//Are some music channels disabled because a SFX is playing?
		if( music_unmute_channels > 0 ){
			
			//Decrease countdown timer
			--music_unmute_channels;
			
			//If the timer has run out
			if( music_unmute_channels == 0 ){
				
				//Re-enable all music channels
				gbt_enable_channels(GBT_CHAN_1 | GBT_CHAN_2 | GBT_CHAN_3 | GBT_CHAN_4);
			}
		}
		
		//Play the music
		gbt_update(); // This will change to ROM bank 1.

		
		//Read the player gamepad for actions
		pad=joypad();
		//AND mask it using the previous state to get only new keypresses
		pad &= ~padOld;
		//And store the full pad state for ANDing in the next frame
		padOld = joypad();
		
		
		
		
		//Enable SRAM use
		ENABLE_RAM_MBC5;
		
		//====== PLAYER ACTIONS ========
		
		//Move LEFT
		if (pad & J_LEFT){
			
			//Browse the current row of 8 foes to the left
			--save.foeIndex;
			
			//Use a bitmask modulo to wrap around the same 8 cells (if we go the last one of the previous row, we loop)
			if( (save.foeIndex & 7) == 7){
				save.foeIndex +=8;
			}
			
			//Add the amount background movement needed to the scroll variables
			scrollX -= 32;
		}
		//Move RIGHT
		else if (pad & J_RIGHT){
			
			//Browse the current row of 8 foes to the left (use a bitmask modulo to wrap)
			++save.foeIndex;
			
			//Use a bitmask modulo to wrap around the same 8 cells (if we go the first one of the next row, we loop)
			if( (save.foeIndex & 7) == 0){
				save.foeIndex -=8;
			}
			
			//Add the amount background movement needed to the scroll variables
			scrollX += 32;
		}
		
		//Move UP
		if (pad & J_UP){
			
			//Browse the current column of 8 foes upward (use a bitmask modulo to wrap)
			save.foeIndex = (save.foeIndex-8) & 63;
			
			//Add the amount background movement needed to the scroll variables
			scrollY -= 32;
		}
		//Move DOWN
		else if (pad & J_DOWN){
			
			//Browse the current column of 8 foes downward (use a bitmask modulo to wrap)
			save.foeIndex = (save.foeIndex+8) & 63;
			
			//Add the amount background movement needed to the scroll variables
			scrollY += 32;
		}
		
		
		//Refresh GUI when needed (don't do everytime as it costs A LOT of CPU time, so this part is quite optimized)
		//Does the foeIndex have changed (do it here after all the movements have been processed to avoid refreshing the GUI twice in case the player moved in diagonal)
		if( foeIndexOld != save.foeIndex ){
			
			//Refresh the GUI
			refreshGUI();
			
			//And store the new foe index value to avoid refreshing it before the next change
			foeIndexOld=save.foeIndex;
		}
		
		
		//If a foe is currently active in the slot
		if( save.type[save.foeIndex] != 0 ){
		
			//Pressing A : Recharge batteries
			if (pad & J_A){
				
				//If we have enough money left and if the foe is currently active
				if( save.type[save.foeIndex] == save.model && save.score > save.powerUP[save.foeIndex] ){
				
					//Restore the power on the Foe
					save.power[save.foeIndex]=99;
					
					//Take the money from player!
					save.score -= save.powerUP[save.foeIndex];
					
					//Restore the "hands moving" animation on the foe tilemap now that he's powered up again!
					
					//Compute the X/Y coordinates of the foe tilemap in the BG (and skip the first row to point directly to the top-left corner of the left hand tilemap)
					k=(save.foeIndex & 7) << 2;
					l=((save.foeIndex >> 3) << 2)+1;
					
					//Replace the "hands" tiles in the foe tilemap so they move again!
					//Left hand
					set_bkg_tile_xy( k, l, 0xAD);
					set_bkg_tile_xy( k, l+1, 0xAE);
					
					//Right hand
					set_bkg_tile_xy( k+3, l, 0xAF);
					set_bkg_tile_xy( k+3, l+1, 0xB0);
					
					//For the GUI, update the POWER number only (optimization!) instead of the full GUI, as the other elements aren't modified here, and the CPU usage is already heavy due to the BG tilemap modification
					
					//Convert the foe power to an ASCII string
					utoa( save.power[save.foeIndex], txt_buffer);
					
					//Compute its length before messing with it
					l=strlen(txt_buffer);
					
					//Apply a -32 offset to all letters of the string to map the ASCII index to the one of our embeded font
					for( k=0 ; k < l ; ++k){ txt_buffer[k] -= 32; }
					
					//Add a trailing "%"
					txt_buffer[l]=0x05;
					
					//And then a trailing " " after that (in case the number goes from two to one digit)
					txt_buffer[l+1]=0x00;
					
					//Add a trailing "["
					txt_buffer[l+2]=0x3B;
					
					//Add a trailing "$"
					txt_buffer[l+3]=0x04;
					
					//Finally, display the tiles onscreen! (strlen + 4 as we have 4 extra trailing chars to display)
					set_win_tiles(8, 2, l+4, 1, txt_buffer);
					
					//Display foe power update price 
					//Store the starting index of this value by adding the previous string length (j+4) to its starting position (x=8)
					l=l+12;
					
					//Convert the number to an ASCII string
					utoa(save.powerUP[save.foeIndex], txt_buffer);
					
					//Compute its length (before messing with it!)
					j=strlen(txt_buffer);
					
					//Apply a -32 offset to all letters of the string to map the ASCII index to the one of our embeded font
					for( i=0 ; i < j ; ++i){ txt_buffer[i] -= 32; }
					
					//Add a trailing "]"
					txt_buffer[j]=0x3D;
					
					//Display the value (start afte the previous string, and add +1 char at the end for the trailing "]" sign)
					set_win_tiles(l, 2, j+1, 1, txt_buffer);
					
					//Play SFX
					NR10_REG = 0x27;
					NR11_REG = 0xD4;
					NR12_REG = 0xF2;
					NR13_REG = 0x40;
					NR14_REG = 0x00;
					NR14_REG = 0x86;
					//Prevent music from using the SFX audio channel for a short time so the SFX can play without being interrupted
					gbt_enable_channels(GBT_CHAN_2 | GBT_CHAN_3 | GBT_CHAN_4);
					music_unmute_channels=90;
				}
				//Else, can't do that!
				else {
					//Play SFX
					NR10_REG = 0x21;
					NR11_REG = 0xA3;
					NR12_REG = 0xF1;
					NR13_REG = 0x0A;
					NR14_REG = 0x00;
					NR14_REG = 0x80;
					//Prevent music from using the SFX audio channel for a short time so the SFX can play without being interrupted
					gbt_enable_channels(GBT_CHAN_2 | GBT_CHAN_3 | GBT_CHAN_4);
					music_unmute_channels=25;
				}
			}
			
			
			//Pressing B : Increase XP level
			else if (pad & J_B ){
				
				//If we have enough money left, and Foe is not maxed out!
				if( save.score > save.levelUP[save.foeIndex] && save.level[save.foeIndex] < 150 ){
				
					//Increase the foe Level
					++save.level[save.foeIndex];
					
					//Take the money from player!
					save.score -= save.levelUP[save.foeIndex];
					
					//Increase the Power charge price (be careful not to go over the UINT16 limit!)
					if( save.powerUP[save.foeIndex] < 62000 ){
						save.powerUP[save.foeIndex] += 1+(save.powerUP[save.foeIndex] >> 4);
					}
					//The last price increase steps are fixed here 
					else if( save.powerUP[save.foeIndex] < 63000 ){
						save.powerUP[save.foeIndex] = 63297;
					}
					else if( save.powerUP[save.foeIndex] < 63500 ){
						save.powerUP[save.foeIndex] = 63599;
					}
					else if( save.powerUP[save.foeIndex] < 64000 ){
						save.powerUP[save.foeIndex] = 64135;
					}
					else if( save.powerUP[save.foeIndex] < 64500 ){
						save.powerUP[save.foeIndex] = 64630;
					}
					//Else, set maximum price (it won't change after that)
					else {
						save.powerUP[save.foeIndex] = 65378;
					}
					
					//Increase the XP upgrade price  (be careful not to go over the UINT16 limit!)
					if( save.levelUP[save.foeIndex] < 64000 ){
						save.levelUP[save.foeIndex] += 1+(save.levelUP[save.foeIndex] >> 3);
					}
					//Else, set maximum price (it won't change after that)
					else {
						save.levelUP[save.foeIndex] = 65519;
					}
					
					//Refresh the GUI
					refreshGUI();
					
					//Play SFX
					NR10_REG = 0x26;
					NR11_REG = 0x85;
					NR12_REG = 0xF3;
					NR13_REG = 0x84;
					NR14_REG = 0x00;
					NR14_REG = 0x83;
					//Prevent music from using the SFX audio channel for a short time so the SFX can play without being interrupted
					gbt_enable_channels(GBT_CHAN_2 | GBT_CHAN_3 | GBT_CHAN_4);
					music_unmute_channels=120;
				}
				//Else, can't do that!
				else {
					//Play SFX
					NR10_REG = 0x21;
					NR11_REG = 0xA3;
					NR12_REG = 0xF1;
					NR13_REG = 0x0A;
					NR14_REG = 0x00;
					NR14_REG = 0x80;
					//Prevent music from using the SFX audio channel for a short time so the SFX can play without being interrupted
					gbt_enable_channels(GBT_CHAN_2 | GBT_CHAN_3 | GBT_CHAN_4);
					music_unmute_channels=25;
				}
				
			}
			
			//Pressing SELECT : fire the foe! (use the hiring too for this one)
			else if ( (pad & J_SELECT) ){
				
				//If we have enough money left
				if( save.score > save.hireCost ){
					
					//Take the money from player!
					save.score -= save.hireCost;
				
					//Fire the current foe
					save.type[save.foeIndex]=0; 
					//Reset all the other vars too for future slot use
					save.power[save.foeIndex]=0; 
					save.powerUP[save.foeIndex]=1; 
					save.level[save.foeIndex]=1; 
					save.levelUP[save.foeIndex]=3; 
					
					//Change the display background to remove the current foe
					set_bkg_tiles( ((save.foeIndex & 7) << 2), ((save.foeIndex >> 3) << 2), 4, 4, map_empty);
					
					//Increase the hiring price (be careful not to go over the UINT16 limit!)
					if( save.hireCost < 60000 ){
						save.hireCost += 1+(save.hireCost >> 2);
					}
					//Else, set maximum price (it won't change after that)
					else {
						save.hireCost = 65432;
					}
					
					//Refresh the GUI
					refreshGUI();
					
					//Play SFX
					NR10_REG = 0x4E; //or 1E or 1D for louder sound / 2E / 3E / 4E... for more "vibe"
					NR11_REG = 0x96;
					NR12_REG = 0xA7; //B7, C7, D7...F7 for longer sound
					NR13_REG = 0xBB;
					NR14_REG = 0x00;
					NR14_REG = 0x85;
					//Prevent music from using the SFX audio channel for a short time so the SFX can play without being interrupted
					gbt_enable_channels(GBT_CHAN_2 | GBT_CHAN_3 | GBT_CHAN_4);
					music_unmute_channels=120;
				}
				//Else, can't do that!
				else {
					//Play SFX
					NR10_REG = 0x21;
					NR11_REG = 0xA3;
					NR12_REG = 0xF1;
					NR13_REG = 0x0A;
					NR14_REG = 0x00;
					NR14_REG = 0x80;
					//Prevent music from using the SFX audio channel for a short time so the SFX can play without being interrupted
					gbt_enable_channels(GBT_CHAN_2 | GBT_CHAN_3 | GBT_CHAN_4);
					music_unmute_channels=25;
				}
			}
		}
		
		//Else, the slot is empty, and we can hire
		else {
			
			//Pressing B : hire new foe! (and not "A" to avoid accidendatly hiring a foe when spamming recharge batteries over all the foes)
			if ( (pad & J_B) ){
				
				//If we have enough money left
				if( save.score > save.hireCost ){
					
					//Take the money from player!
					save.score -= save.hireCost;
				
					//Hire a new foe of a random type
					//First, 12.5% chance to hire the model we are running on (or 100% if it's the first hire we are making)
					if( (rand() & 7) == 0 || save.hireCost == 1 ){
						save.type[save.foeIndex]=save.model; 
					}
					//Else, truly random model selection among all the available models (including the current one)
					else {
						save.type[save.foeIndex]=1+(rand()&3); 
					}
					
					//Set his power to the max (even if it can't run now because it's not the right model)
					save.power[save.foeIndex]=99; 
					//All the other vars were inited during first startup in SRAM or when firing a previous foe
					
					
					//Change the display background to display the new hire!
					j=save.foeIndex;
					
					//Compute the X/Y coordinates of the foe tilemap in the BG once for all cases
					k=(j & 7) << 2;
					l=((j >> 3) << 2);
					
					//If the foe is a DMG model
					if( save.type[j] == 1 ){
						
						//Is this model currently running?
						if( save.model == 1 ){
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_dmg_on);
						}
						//Else, draw the "sleeping" version
						else {
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_dmg_off);
						}
					}
					//else if the foe is a MGB model
					else if( save.type[j] == 2 ){
						
						//Is this model currently running?
						if( save.model == 2 ){
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_mgb_on);
						}
						//Else, draw the "sleeping" version
						else {
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_mgb_off);
						}
					}
					//else if the foe is a CGB model
					else if( save.type[j] == 3 ){
						
						//Is this model currently running?
						if( save.model == 3 ){
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_cgb_on);
						}
						//Else, draw the "sleeping" version
						else {
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_cgb_off);
						}
					}
					//else if the foe is a SGB model
					else if( save.type[j] == 4 ){
						
						//Is this model currently running?
						if( save.model == 4 ){
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_sgb_on);
						}
						//Else, draw the "sleeping" version
						else {
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_sgb_off);
						}
					}
					
					//Increase the hiring price (be careful not to go over the UINT16 limit!)
					if( save.hireCost < 60000 ){
						save.hireCost += 1+(save.hireCost >> 2);
					}
					//Else, set maximum price (it won't change after that)
					else {
						save.hireCost = 65432;
					}
					
					//Refresh the GUI
					refreshGUI();
					
					//Play SFX
					NR10_REG = 0x13;
					NR11_REG = 0x8A;
					NR12_REG = 0xF5;
					NR13_REG = 0x0E;
					NR14_REG = 0x00;
					NR14_REG = 0x81;
					//Prevent music from using the SFX audio channel for a short time so the SFX can play without being interrupted
					gbt_enable_channels(GBT_CHAN_2 | GBT_CHAN_3 | GBT_CHAN_4);
					music_unmute_channels=30;
				}
				//Else, can't do that!
				else {
					//Play SFX
					NR10_REG = 0x21;
					NR11_REG = 0xA3;
					NR12_REG = 0xF1;
					NR13_REG = 0x0A;
					NR14_REG = 0x00;
					NR14_REG = 0x80;
					//Prevent music from using the SFX audio channel for a short time so the SFX can play without being interrupted
					gbt_enable_channels(GBT_CHAN_2 | GBT_CHAN_3 | GBT_CHAN_4);
					music_unmute_channels=25;
				}
			}
		}
		
		
		
		//======= FOE UPDATE =========
		
		//Process only 2 foes at a time so the score doesn't increase too fast :)
		for( i=0 ; i < 2 ; ++i){
			
			//Increase loop index (so we don't process all the foes each frame)
			++save.loopIndex;
			//And loop it to the current number of foes we can handle (64 foes maximum)
			if( save.loopIndex > 63 ){
				save.loopIndex=0;
			}
			
			//Copy the current loop index in a temp var to avoid unnecessary struct member access (optimization)
			j=save.loopIndex;
			
			//If the foe is of the current model we are running on
			if( save.type[j] == save.model ){
				
				//If the current foe have some power left
				if( save.power[j] > 0 ){
					
					//Increase score by the current foe recharge price
					save.score += save.powerUP[j];
					
					//Don't go over the max score value! (4 294 967 295 for a UINT32)
					if( save.score > 4242424242 ){
						//Go back to the arbitrary max value I chose (H2G2 FTW!)
						save.score = 4242424242;
					}
					//TODO: have a "game won" state?
					/*if( save.score >= 4242424242 ){
						//go to "game won" state
					}*/
					
					
					//Decrease foe power by 1
					--save.power[j];
					
					//If we are currently displaying this foe info on GUI, update the POWER number only (optimization!)
					if( save.foeIndex == j ){
					
						//Convert the foe power to an ASCII string
						utoa( save.power[save.foeIndex], txt_buffer);
						
						//Compute its length before messing with it
						l=strlen(txt_buffer);
						
						//Apply a -32 offset to all letters of the string to map the ASCII index to the one of our embeded font
						for( k=0 ; k < l ; ++k){ txt_buffer[k] -= 32; }
						
						//Add a trailing "%"
						txt_buffer[l]=0x05;
						
						//And then a trailing " " after that (in case the number goes from two to one digit)
						txt_buffer[l+1]=0x00;
						
						//Finally, display the tiles onscreen! (strlen + 2 as we have 2 extra trailing chars to display)
						set_win_tiles(8, 2, l+2, 1, txt_buffer);
					}
					
					//If the foe has just run out of energy (i.e. it was its last power point)
					if( save.power[j] == 0 ){

						//Compute the X/Y coordinates of the foe tilemap in the BG (and skip the first row to point directly to the top-left corner of the left hand tilemap)
						k=(j & 7) << 2;
						l=((j >> 3) << 2)+1;
						
						//Replace the "hands" tiles in the foe tilemap so they no longer move!
						//Left hand
						set_bkg_tile_xy( k, l, 0xB1);
						set_bkg_tile_xy( k, l+1, 0xB2);
						
						//Right hand
						set_bkg_tile_xy( k+3, l, 0xB3);
						set_bkg_tile_xy( k+3, l+1, 0xB4);
					}
				}				
			}
			
		}
		
	
		
		
		//======= SCORE DISPLAY =========
		
		//Convert the 32 unsigned integer score value to a string to display it (the sprintf function cannot handle more than 16 bit value here)
		ultoa(save.score, txt_score);
		
		//Retrieve the score string length (used for accurate positioning)
		j=strlen(txt_score);
		
		//The ultoa string holds the actual ASCII values, but we don't have the full ASCII table here, only a subset
		//So for every digit of the score (using the strlen to avoid messing with the trailing zeroes)
		for( i=0 ; i < 10 ; ++i){
			
			//if the digit is part of the current score (using strlen)
			if( i < j ){
				//Apply an offset to "remap" the real ASCII value to the ones we'll use in our limited subset
				txt_score[i] -= 32;
			}
			//Else, replace the digit with the "gui border" to erase any previous character when score decreases and loses a digit
			else {
				txt_score[i] = 0x60;
			}
		}
		
		//Display the current score "string" with the ascii tiles in memory (use the length for accurate positioning)
		set_win_tiles(7, 0, 10, 1, txt_score);
		
		//Disable SRAM use (for now)
		DISABLE_RAM_MBC5;
		
		
		//Pressing START : clear save data when hold for > 3 seconds
		if ( (joypad() & J_START) ){ //don't read the "pad" value, as it holds only new keypresses, but the actual joypad state so we can see if a key is held down
				
			//Did we finish the countdown ?
			if( reset_save ==  200 ){
				
				//Voluntarily corrupt the SRAM integrity check to force the game to set a new one at startup
				ENABLE_RAM_MBC5;
				save.sram_check = 0x00000000;
				DISABLE_RAM_MBC5;
				
				//Clear the countdown to reset message
				fill_win_rect(5, 5, 10, 1, 0x0);
				
				//Launch the game init function, that will restore a blank save data as the magic key isn't present in the SRAM integrity check variable
				initGame(); //this function will shutdown screen and return it back on after the init is completed
			}
			//Else, keep counting
			else {
				//Increase the counter
				++reset_save;
				
				//Display countdown
				if( reset_save == 30 ){
					//Display full message
					set_win_tiles(5, 5, 10, 1, msg_clearsave);
				}
				else if( reset_save == 90 ){
					//Update the counter only
					set_win_tile_xy(14, 5, 0x32-32);
				}
				else if( reset_save == 150 ){
					//Update the counter only
					set_win_tile_xy(14, 5, 0x31-32);
				}
			}

		}
		//Else, if start is not pressed but we did start to count for a reset (and the player didn't hold the button long enough)
		else if ( reset_save != 0 ){
			
			//Clear the countdown message
			fill_win_rect(5, 5, 10, 1, 0x0);
			
			//reset the variable
			reset_save=0;
		}
		


		// WAIT FOR VBLANK TO FINISH - ENSURES 60 FRAMES PER SECOND MAXIMUM
		wait_vbl_done();														
	}
	// GAME LOOP END
}




//==============================
//======== FUNCTIONS  ===========
//==============================

//This function init the game at startup
//Params ()
void initGame(){

	//Shut down screen while init
	DISPLAY_OFF;
	
	//==== SRAM INIT & MODEL DETECTION ====
	
	//Begin SRAM use
	ENABLE_RAM_MBC5;

	//Detect the current gb model we are running on! (without saving it, as the SRAM is not initialized yet)
	
	//If running on a SGB
	if ( sgb_check() != NULL ){
		
		//Record the current model in a local var for now
		model=4;
	}
	//Else, not running on a SGB
	else {
		//DMG model (Fat)
		if( _cpu == DMG_TYPE ){
			
			//Record the current model in a local var for now
			model=1;
		} 
		//MGB model (Pocket)
		else if( _cpu == MGB_TYPE ){
			
			//Record the current model in a local var for now
			model=2;
		}
		//CGB model (Color)
		else if( _cpu == CGB_TYPE ){
			
			//Record the current model in a local var for now
			model=3;
			
			//Enable default grayscale palette when running on CGB (so we don't get rendered by the ugly red/green default palettes)
			cgb_compatibility();
			
			//Set the second sprite color as white, to reflect what we do with the non-CGB sprite palette
			set_sprite_palette_entry(0,1, 0xFFFF);
		}
	}
	
	//Init the SRAM if it's the first game run (i.e no save file found)
	
	//If the SRAM doesn't contains the "magic key", we "reset / init" it to have a clear and controlled startup state
	if( save.sram_check != SRAM_CHECK ){
		
		//Mark the SRAM as checked to avoid erasing it in the future!
		save.sram_check = SRAM_CHECK;
		
		//Reset the GB model (it'll be stored right after that section)
		save.model = 0;
		
		//Reset the score
		save.score = 0;
		
		//Reset the foes values (type, energy and level all set to base values)
		for( i=0 ; i < 64 ; ++i){
			save.type[i]=0; 
			save.power[i]=0; 
			save.powerUP[i]=1; 
			save.level[i]=1; 
			save.levelUP[i]=3; 
		}
		
		//Reset the current foeIndex to the first one
		save.foeIndex=0;
		
		//Set the cost of your first hire
		save.hireCost=1; 
		
		//Reset the loop step
		save.loopIndex=0; 
		
		//Enable the first foe for free, of the same type as the current model
		save.type[0]=model;
	}
	
	//If we plugged the cartridge to a different console model, every active foe gets a free power charge!
	if( model != save.model ){
		
		//For each foe
		for( i=0 ; i < 64 ; ++i){
			//If the foe is of the current model we are running on
			if( save.type[i] == model ){
				//Set its power back to max for free!
				save.power[i]=99; 
			}
		}

		//And record the new current model back to the saved data
		save.model=model;
	}
	
	//RANDOM INITIALISATION
	//Init the random sequence (use the current hiring cost, as it's never 0, it changes on every hire, and the random values are only used for hiring foes)
	initrand(save.hireCost);
	
	//End SRAM use (for now)
	DISABLE_RAM_MBC5;
	
	
	//==== MUSIC ====
	
	//Init the GBT music player
	gbt_play(song_Data, 1, 4);
	gbt_loop(0);
	set_interrupts(VBL_IFLAG);
	enable_interrupts();	
	
	//Init the channels muting variable
	music_unmute_channels=0;
	
	
	//==== GRAPHICS ====
	
	//Load the font tiles into VRAM
	font_init();
	font_load( font_ibm );
	DISPLAY_OFF; //turns off the display again as the font function will enable it automatically
	
	//Send all the background / window tile data (graphics assets) into VRAM
	set_bkg_data(0x80, 90, tiles_data);
	
	//Then call the function that will build the BG using the SRAM data and the tiles above (enable SRAM access just for this, as we'll need values from SRAM)
	ENABLE_RAM_MBC5;
	buildBG();
	//And Move the background to show the currently selected foe
	move_bkg( ((save.foeIndex & 7)*32)-64, ((save.foeIndex / 8)*32)-32 );
	DISABLE_RAM_MBC5;
	
	//Init background scrolling values too
	scrollX=0;
	scrollY=0;
	
	//Init the animation variables
	anim_ticks = 4;
	anim_step = 0;

	//Display the BG
	SHOW_BKG;
	
	
	//==== GUI ====
	
	//Init the window that will be used to display GUI with a blank tile
	init_win( 0x0 );
	//Add a top border
	fill_win_rect(0, 0, 20, 1, 0x60);
	//Add a dollar sign before the score inside the top border
	set_win_tile_xy(6, 0, 0x04);
	
	//Display the various buttons (A, B and SELECT) on the left part of the GUI so they face the matching actions
	set_win_tile_xy(0, 2, 0xCF); //A Button
	set_win_tile_xy(0, 3, 0xD0); //B Button
	set_win_tile_xy(0, 4, 0xD1); //SELECT Button
	/*
	//Display the game title on bottom left corner
	set_win_tile_xy(12, 5, 0xD7); 
	set_win_tile_xy(13, 5, 0xD8); 
	set_win_tile_xy(14, 5, 0xD9); 
	*/
	//Display the signature logo on bottom right corner
	set_win_tile_xy(15, 5, 0xD2); 
	set_win_tile_xy(16, 5, 0xD3); 
	set_win_tile_xy(17, 5, 0xD4); 
	set_win_tile_xy(18, 5, 0xD5); 
	set_win_tile_xy(19, 5, 0xD6); 
	
	//Populate the GUI by refreshing it once (enable SRAM access just for this, as we'll need values from SRAM)
	ENABLE_RAM_MBC5;
	refreshGUI();
	//Set the variable to detect when to refresh the GUI too
	foeIndexOld=save.foeIndex;
	DISABLE_RAM_MBC5;
	
	//And move it where it belongs
	move_win( 7, 96);
	
	//Display the window
	SHOW_WIN;
	
	
	//==== SPRITES (part of GUI) ====
	
	//Modify the Object palette 0 to have the following colors : Light Grey / White / Dark Grey / Black
	OBP0_REG=0b11100001; //This is a binary literal (2 bit per palette colour), the leftest number being the last entry in the palette
	
	//Send all the background / window tile data (graphics assets) into VRAM
	set_sprite_data(0, 3, tiles_sprites);

	//Display the "crosshair" in the middle of the screen 
	//Each crosshair part is made of 3 sprites, laid down once and for all (the BG moves, but not the sprites)
	
	//Top-Left corner
	set_sprite_tile(0, 0);
	move_sprite(0, 70, 46);
	set_sprite_tile(1, 1);
	move_sprite(1, 78, 46);
	set_sprite_tile(2, 2);
	move_sprite(2, 70, 54);
	
	//Top-Right corner
	set_sprite_tile(3, 0);
	move_sprite(3, 99, 46);
	set_sprite_prop(3, S_FLIPX);
	set_sprite_tile(4, 1);
	move_sprite(4, 91, 46);
	set_sprite_prop(4, S_FLIPX);
	set_sprite_tile(5, 2);
	move_sprite(5, 99, 54);
	set_sprite_prop(5, S_FLIPX);

	//Bottom-Left corner
	set_sprite_tile(6, 0);
	move_sprite(6, 69, 75);
	set_sprite_prop(6, S_FLIPY);
	set_sprite_tile(7, 1);
	move_sprite(7, 77, 75);
	set_sprite_prop(7, S_FLIPY);
	set_sprite_tile(8, 2);
	move_sprite(8, 69, 67);
	set_sprite_prop(8, S_FLIPY);

	//Bottom-Right corner
	set_sprite_tile(9, 0);
	move_sprite(9, 99, 75);
	set_sprite_prop(9, S_FLIPX | S_FLIPY);
	set_sprite_tile(10, 1);
	move_sprite(10, 91, 75);
	set_sprite_prop(10, S_FLIPX | S_FLIPY);
	set_sprite_tile(11, 2);
	move_sprite(11, 99, 67);
	set_sprite_prop(11, S_FLIPX | S_FLIPY);
	
	//Display the sprites
	SHOW_SPRITES;
	
	
	//==== GAMEPLAY INIT ====	
	
	//Variable used to count how long start is pressed to reset the score
	reset_save = 0;
	
	//Screen on as initial setup is completed!
	DISPLAY_ON;
}




//This function redraw the whole GUI to display the infos from a new "foe" slot (called each time foeIndex change!)
//Params ()
void refreshGUI(){
	
	//If the spot isn't empty, display current foe stats
	if( save.type[save.foeIndex] != 0 ){
		
		//Erase the text
		fill_win_rect(2, 2, 18, 3, 0x00);
		
		//-= FOE POWER (and update price) =-
		
		//If the foe is currently active ? 
		if( save.type[save.foeIndex] == save.model ){
			
			//Display power static string
			set_win_tiles(2, 2, 5, 1, msg_power);
			
			//Display foe power
			
			//Convert the number to an ASCII string
			utoa(save.power[save.foeIndex], txt_buffer);
			
			//Compute its length (before messing with it!)
			j=strlen(txt_buffer);
			
			//Apply a -32 offset to all letters of the string to map the ASCII index to the one of our embeded font
			for( i=0 ; i < j ; ++i){ txt_buffer[i] -= 32; }
			
			//Add a trailing "%"
			txt_buffer[j]=0x05;
			
			//Add a trailing " "
			txt_buffer[j+1]=0x00;
		
			//Add a trailing "["
			txt_buffer[j+2]=0x3B;
			
			//Add a trailing "$"
			txt_buffer[j+3]=0x04;
			
			//Display the value (+4 chars at the end for the trailing signs)
			set_win_tiles(8, 2, j+4, 1, txt_buffer);
		
			
			//Display foe power update price 
			//Store the starting index of this value by adding the previous string length (j+4) to its starting position (x=8)
			l=j+12;
			
			//Convert the number to an ASCII string
			utoa(save.powerUP[save.foeIndex], txt_buffer);
			
			//Compute its length (before messing with it!)
			j=strlen(txt_buffer);
			
			//Apply a -32 offset to all letters of the string to map the ASCII index to the one of our embeded font
			for( i=0 ; i < j ; ++i){ txt_buffer[i] -= 32; }
			
			//Add a trailing "]"
			txt_buffer[j]=0x3D;
			
			//Display the value (start afte the previous string, and add +1 char at the end for the trailing "]" sign)
			set_win_tiles(l, 2, j+1, 1, txt_buffer);
		}
		//Else, the foe is currently inactive (and we CAN'T recharge its batteries (you must change console for a free charge and the need to manually power the active consoles)
		else {
			//Display inactive static string
			set_win_tiles(2, 2, 14, 1, msg_sleeping);
		}
		
		
		//-= FOE LEVEL (and update price) =-
		
		//Display level static string
		set_win_tiles(2, 3, 5, 1, msg_level);
		
		//Display foe level
		
		//Convert the number to an ASCII string
		utoa(save.level[save.foeIndex], txt_buffer);
		
		//Compute its length (before messing with it!)
		j=strlen(txt_buffer);
		
		//Apply a -32 offset to all letters of the string to map the ASCII index to the one of our embeded font
		for( i=0 ; i < j ; ++i){ txt_buffer[i] -= 32; }
		
		//Add a trailing " "
		txt_buffer[j]=0x00;
	
		//Add a trailing "["
		txt_buffer[j+1]=0x3B;
		
		//Add a trailing "$"
		txt_buffer[j+2]=0x04;
		
		//Display the value (+3 chars at the end for the trailing signs)
		set_win_tiles(8, 3, j+3, 1, txt_buffer);
	
		
		//Display foe level update price 
		//Store the starting index of this value by adding the previous string length (j+3) to its starting position (x=8)
		l=j+11;
		
		//Convert the number to an ASCII string
		utoa(save.levelUP[save.foeIndex], txt_buffer);
		
		//Compute its length (before messing with it!)
		j=strlen(txt_buffer);
		
		//Apply a -32 offset to all letters of the string to map the ASCII index to the one of our embeded font
		for( i=0 ; i < j ; ++i){ txt_buffer[i] -= 32; }
		
		//Add a trailing "]"
		txt_buffer[j]=0x3D;
		
		//Display the value (start after the previous string, and add +1 char at the end for the trailing "]" sign)
		set_win_tiles(l, 3, j+1, 1, txt_buffer);
		
		
		
		//-= FOE FIRING =-
		
		//Display firing static string
		set_win_tiles(2, 4, 10, 1, msg_fire);
		
		//Display firing price (same as hiring one)
		
		//Convert the number to an ASCII string
		utoa(save.hireCost, txt_buffer);
		
		//Compute its length (before messing with it!)
		j=strlen(txt_buffer);
		
		//Apply a -32 offset to all letters of the string to map the ASCII index to the one of our embeded font
		for( i=0 ; i < j ; ++i){ txt_buffer[i] -= 32; }
		
		//Add a trailing "]"
		txt_buffer[j]=0x3D;
		
		//Display the price (+1 char at the end for the trailing "]" sign)
		set_win_tiles(12, 4, j+1, 1, txt_buffer);
		
	}
	
	//Else, display a text (and price!) for hiring a new one
	else {
		
		//-= FOE HIRING =-
		
		//Erase the text
		fill_win_rect(2, 2, 18, 3, 0x00);
		
		//Display hiring static string
		set_win_tiles(2, 3, 11, 1, msg_hire);
		
		//Display hiring price 
		
		//Convert the number to an ASCII string
		utoa(save.hireCost, txt_buffer);
		
		//Compute its length (before messing with it!)
		j=strlen(txt_buffer);
		
		//Apply a "-32" offset to map the ASCII code to the GB font, and replace string end char by a "]")
		for( i=0 ; i < 6 ; ++i){ //the price value will never exceed 5 digits + 1 trailing "]" sign, as it's a 16 bit number
			//If the character is part of the string, apply the offset
			if( i < j){ txt_buffer[i] -= 32; } 
			//Else, replace all the trailing characters by a "]" sign
			else { txt_buffer[i] = 0x3D; } 
		}
		
		//Display the price (+1 char at the end for the trailing "]" sign)
		set_win_tiles(13, 3, j+1, 1, txt_buffer);
	}
}




//This function draw the whole game area at startup (all the cubicles and the GBs in them) on the background layer
//Params ()
void buildBG(){
	
	//====== DRAW FOE ON BG LAYER ========

	//For each foe
	for( i=0 ; i < 64 ; ++i ){
		
		//Compute the X/Y coordinate of the foe tilemap in the BG
		k=(i & 7) << 2;
		l=(i >> 3) << 2;
		
		//If the foe is a DMG model
		if( save.type[i] == 1 ){
			
			//Is this model currently running?
			if( save.model == 1 ){
				
				//Draw the foe tilemap on the foe coordinates
				set_bkg_tiles( k, l, 4, 4, map_dmg_on);
				
				//If the foe is out of energy, disable the "hands working" animation by replacing part of the tilemap (both hands)
				if( save.power[i] == 0 ){
					//Replace the "hands" tile in the foe tilemap so they no longer move!
					//Left hand
					set_bkg_tile_xy( k, l+1, 0xB1);
					set_bkg_tile_xy( k, l+2, 0xB2);
					//Right hand
					set_bkg_tile_xy( k+3, l+1, 0xB3);
					set_bkg_tile_xy( k+3, l+2, 0xB4);
				}
			}
			//Else, draw the "sleeping" version
			else {
				//Draw the foe tilemap on the foe coordinates
				set_bkg_tiles( k, l, 4, 4, map_dmg_off);
			}
		}
		//else if the foe is a MGB model
		else if( save.type[i] == 2 ){
			
			//Is this model currently running?
			if( save.model == 2 ){
				//Draw the foe tilemap on the foe coordinates
				set_bkg_tiles( k, l, 4, 4, map_mgb_on);
				
				//If the foe is out of energy, disable the "hands working" animation by replacing part of the tilemap (both hands)
				if( save.power[i] == 0 ){
					//Replace the "hands" tile in the foe tilemap so they no longer move!
					//Left hand
					set_bkg_tile_xy( k, l+1, 0xB1);
					set_bkg_tile_xy( k, l+2, 0xB2);
					//Right hand
					set_bkg_tile_xy( k+3, l+1, 0xB3);
					set_bkg_tile_xy( k+3, l+2, 0xB4);
				}
			}
			//Else, draw the "sleeping" version
			else {
				//Draw the foe tilemap on the foe coordinates
				set_bkg_tiles( k, l, 4, 4, map_mgb_off);
			}
		}
		//else if the foe is a CGB model
		else if( save.type[i] == 3 ){
			
			//Is this model currently running?
			if( save.model == 3 ){
				//Draw the foe tilemap on the foe coordinates
				set_bkg_tiles( k, l, 4, 4, map_cgb_on);
				
				//If the foe is out of energy, disable the "hands working" animation by replacing part of the tilemap (both hands)
				if( save.power[i] == 0 ){
					//Replace the "hands" tile in the foe tilemap so they no longer move!
					//Left hand
					set_bkg_tile_xy( k, l+1, 0xB1);
					set_bkg_tile_xy( k, l+2, 0xB2);
					//Right hand
					set_bkg_tile_xy( k+3, l+1, 0xB3);
					set_bkg_tile_xy( k+3, l+2, 0xB4);
				}
			}
			//Else, draw the "sleeping" version
			else {
				//Draw the foe tilemap on the foe coordinates
				set_bkg_tiles( k, l, 4, 4, map_cgb_off);
			}
		}
		//else if the foe is a SGB model
		else if( save.type[i] == 4 ){
			
			//Is this model currently running?
			if( save.model == 4 ){
				//Draw the foe tilemap on the foe coordinates
				set_bkg_tiles( k, l, 4, 4, map_sgb_on);
				
				//If the foe is out of energy, disable the "hands working" animation by replacing part of the tilemap (both hands)
				if( save.power[i] == 0 ){
					//Replace the "hands" tile in the foe tilemap so they no longer move!
					//Left hand
					set_bkg_tile_xy( k, l+1, 0xB1);
					set_bkg_tile_xy( k, l+2, 0xB2);
					//Right hand
					set_bkg_tile_xy( k+3, l+1, 0xB3);
					set_bkg_tile_xy( k+3, l+2, 0xB4);
				}
			}
			//Else, draw the "sleeping" version
			else {
				//Draw the foe tilemap on the foe coordinates
				set_bkg_tiles( k, l, 4, 4, map_sgb_off);
			}
		}
		//Else, the cubicle is empty!
		else {	
			//Draw the foe tilemap on the foe coordinates
			set_bkg_tiles(k, l, 4, 4, map_empty);
		}
	}
	
	
}
