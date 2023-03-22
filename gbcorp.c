/*
			GB Corp.

	It's time to put your Game Boy collection to work!

						by Dr. Ludos (2023)

	Get all my other games:
			http://drludos.itch.io/
	Support my work and get access to betas and prototypes:
			http://www.patreon.com/drludos

	Gameplay music: krümel (crumb)#0723 - In The Town
	Public domain (GB Studio Community Assets):
	https://github.com/DeerTears/GB-Studio-Community-Assets/tree/master/Music

*/

// INCLUDE GBDK FUNCTION LIBRARY
#include <gbdk/platform.h> // includes gb.h, sgb.h, cgb.h
// INCLUDE HANDY HARDWARE REFERENCES
#include <gb/hardware.h>
// INCLUDE RANDOM FUNCTIONS
#include <rand.h>
//INCLUDE FONT (+CUSTOM COLOR) AND TEXT DISPLAY FUNCTIONS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gbdk/font.h>
#include <gb/drawing.h>
#include <gbdk/console.h>


//INCLUDE MUSIC PLAYER (GBT Player by AntonioND)
#include "gbt_player.h"
extern const unsigned char * song_Data[];

//DEFINE SUPPORTED GB MODELS LIST (do it now as it's also used by the Serial Link 2 player mode source file included below)
// For serial link exchange, max supported model value is 0x07u
#define MODEL_EMPTY 0u
#define MODEL_DMG   1u
#define MODEL_MGB   2u
#define MODEL_CGB   3u
#define MODEL_SGB   4u
#define MODEL_SG2   5u
#define MODEL_GBA   6u
#define MODEL_DISCONNECTED 7u
#define MODEL_MIN (MODEL_DMG)
#define MODEL_MAX (MODEL_GBA)

// Serial link 2-Player support
#include "serial_link.c"




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
void detectModel();
void initGame();
void refreshGUI();
void buildBG();
void updateMenu();

// VARIABLE DECLARATIONS - STORED IN RAM
//Looping variables (unsigned 8 bit)
UINT8 i, j, k, l;
//Temp variables (usigned 16 bit)
UINT16 m;
//Temp variables (signed 8 bit)
INT8 si, sj;

//Current program state
UINT8 STATE; //0: Gameplay / 1: Pause screen / 2: How to play (go back to pause screen) / 3: Goals list (go back to gameplay) / 4: Goals list (go back to pause menu) / 10: Title screen / 11: how to play (go back to title screen) / 12: Goals list (go back to title screen)

//Timer to countdown for how long the music can't use a channel as a SFX is playing
UINT8 music_unmute_channels;


//Gameplay (non SRAM)
UINT8 pad; //current player actions (holds the value of the pressed/released state for all the buttons)
UINT8 padOld; //previous player actions state (used to detect new keypresses only once)
UINT8 model; //current model we are running on (used in detection code, can be different from the value saved in SRAM at startup!)
UINT8 model_linked; // 2 player serial linked model, if applicable
UINT8 keep_link; //If > 0 we have to avoid reseting the linked connection (used when we restore gameplay after quitting pause menu, to avoid reseting an active network connection)
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

//Animations (background anims)
UINT8 anim_ticks; //Timer before next animation sequence
UINT8 anim_step; //current animation sequence

//Menus (title screen, goals checklist...)
UINT8 menu_ticks; //animation timer used in menu states
UINT8 menu_step; //step counter used in menu states
UINT8 menu_newgoal; //if > 0 a new goal have just been achieved (so display animation)


//SRAM save
//Constant used to check whether the SRAM contains a save or not
#define SRAM_CHECK  0xDD05B055

//Struct containing the save game (will be mapped to the SRAM through a pointer, see below)
typedef struct savegame {
	UINT32 sram_check;   //check if the SRAM contains a savegame or not (if yes, contains the value of the "SRAM_CHECK" constant, else it will contains anything else)
	UINT8 model;   //previous GB model we were running on (0: uninitialized / 1: DMG / 2: MGB / 3: GBC / 4: SGB / 5: SG2 / 6: GBA)
	UINT32 score;   //the current player score
	UINT8 type[64];   //The current type of each foe (0: empty / 1: DMG / 2: MGB / 3: GBC / 4: SGB / 5: SG2 / 6: GBA)
	UINT8 power[64];   //The current power for each foe
	UINT16 powerUP[64];   //The current price to refill power for each foe
	UINT8 level[64];   //The current xp level for each foe
	UINT16 levelUP[64];   //The current price to increase xp level for each foe
	UINT16 hireCost;   //How will it cost us to hire the next foe (it double with each hire)
	UINT8 loopIndex;   //Current loop step (i.e. foe index) as we don't process them all on every frame
	UINT8 foeIndex;   //Current foe selected (info displayed on GUI and target for player actions)
	UINT8 goals[10]; //List of goals obtained (0 = no goal / 1 = goal achieved).
	UINT8 count_foes; //Current number of foes we have hired (0-64)
	UINT8 count_types[7]; //If > 0 we have hired at least one foe on the matching type (see foe type list above, values 1-6) (NB the array[0] is indeed unused in game, but it's easier to do that way)
	UINT8 count_models[7]; //If > 0 we have run the game on the matching console type (see foe type list above to get the values of actual consoles, values 1-6)
	UINT8 count_maxed; //Count the number of foes we have at max level (150), values (0-64)
	UINT8 count_recharges; //Count the number of foes we have at max level (150), values (0-64)
} savegame;

//Map the savegame struct to the beginning of SRAM (0xA000) for persistent gameplay!
savegame __at (0xA000) save;

/* MEMO : GOALS list (indexes are from the "goals" array in SRAM save, so 0-based)
	0: max score reached
	1: 64 foes hired
	2: 0 foes hired (everyone has been fired, including the first "free" employee!)
	3: Hired 1 foe of every console variant
	4: 1 foe reached level 150
	5: 64 foes reached level 150
	6: switched cartridge on 2 different consoles
	7: switched cartridge on all 6 different consoles
	8: performed 200 power recharges manually
	9: connected two console through link cable
*/


//RICH SFX player (i.e. custom handmade DIY music player)
//Here is a kind of "quick'n dirty" custom music player made using solely the Channel 2 of the GB (coming straight out of my first GB game "Sheep It Up!")
//The main parameters of the channel are set manually, then this table defines "notes", as in "frequencies" to play that will go to the NR2X registers

//Here is the "note table". Each note is defined by 4 hex numbers, directly setting the values for NR21, NR22, NR23 and NR24 registers, in that order
const UINT8 music_notes [] ={
	//NR21, NR22=Initial volume [0: silence / F: max] / Volume duration, NR23=Frequency 1/2, NR24=Note Duration (C=short / 8=long) / Frequency 2/2 
	0x82, 0x47, 0x16, 0x84 //[0] C4 long
	,0x82, 0x47, 0x55, 0x83 //[1] A3 long
	,0x82, 0x47, 0xDA, 0x83 //[2] B3 long 
	,0x82, 0x47, 0xC5, 0x81 //[3] E3 long
	,0x82, 0x47, 0x16, 0xC4 //[4] C4 cut
	,0x82, 0x47, 0x55, 0xC3 //[5] A3 cut
	,0x82, 0x47, 0xDA, 0xC3 //[6] B3 cut
	,0x82, 0x47, 0xC5, 0xC1 //[7] E3 cut
	,0x82, 0xA5, 0x0A, 0x86 //[8] C5 long
	,0x82, 0xA5, 0xAC, 0x85 //[9] A4 long
	,0x82, 0xA5, 0xED, 0x85 //[10] B4 long 
	,0x82, 0xA5, 0xE5, 0x84 //[11] E4 long
	,0x82, 0xA2, 0x0A, 0x86 //[12] C5 short
	,0x82, 0xA2, 0xAC, 0x85 //[13] A4 short
	,0x82, 0xA2, 0xED, 0x85 //[14] B4 short 
	,0x82, 0xA2, 0xE5, 0x84 //[15] E4 short
	,0x82, 0xA5, 0x42, 0x86 //[16] D5 long
	,0x82, 0xA5, 0x72, 0x86 //[17] E5 long
	,0x82, 0xA5, 0x89, 0x86 //[18] F5 long
	,0x82, 0xA5, 0xB2, 0x86 //[19] G5 long
	,0x82, 0xA5, 0xD6, 0x86 //[20] A5 long
	,0x82, 0xA5, 0xF7, 0x86 //[21] B5 long
};

//Short Victory music when you achieve a new goal (see before for how this music "partition" is coded)
const UINT8 music_goal [] ={
	13, 7 //A4 (cut)
	,13, 7 //A4 (cut)
	,14, 7 //B4 (cut)
	,8, 30  //C5 (long)
};

//Counter used to keep track of the current "note" index in the music table sheet (it's an UINT16, because we want to play more than 256 / 2 = 128 notes in each song :))
UINT16 music_note;
//Counter used to track the delay between each "note" of the song
UINT8 music_tempo;
//Trigger to see it music is muted or not
UINT8 music_mute;



//=================================
//==========  MAIN LOOP ============
//=================================
void main(){

	//Startup init (one time only)
	
	//Launch the function that will detect the current GB model we are running on (called only once at startup)
	detectModel();
	
	//If the SRAM doesn't contains the "magic key", it means that no game is started yet
	//Enable SRAM use
	ENABLE_RAM_MBC5;
	if( save.sram_check != SRAM_CHECK ){
		
		//Launch the function that will init the gameplay (needed to have a clean startup state)
		initGame();
		
		//Enable SRAM use
		ENABLE_RAM_MBC5;
		
		//Voluntarily corrupt the SRAM magic key again, as "InitGame" did have inited a new valid game save state
		save.sram_check = 0x00000000;
		
		//Disable SRAM use (for now)
		DISABLE_RAM_MBC5;
		
		//Set the current state as "Title Screen"
		STATE=10;
	} 
	//Else, a game session / save is currently active, so we skip the title menu and go straight to gameplay
	else {
		
		//Launch the function that will init the gameplay
		initGame();
	}

	
	//If we are not in gameplay state, launch the "menu states" function (to display goals completion screen when "cartridge swapped" or the title screen on 1st boot for example)
	if( STATE != 0 ){

		//Call the function that handle the menu states (endless loop with its own vblbank(), that will "initGame" to go back to normal gameplay before exiting)
		updateMenu();
	}
	
	
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

		
		//Update serial link cable connection (2 Players mode)
		if( link_update(model, &(model_linked)) ){

			//If a state change occurred, update the display
			ENABLE_RAM_MBC5; //SRAM access needed here
			
			//Refresh the bg maps since the linked GB model may enable/disable some workers
			buildBG();
			
			//If the spot isn't empty, update the GUI too 
			//To save CPU time (as building the BG is quite CPU intensive already), we only refresh the first line ("Power") : it can switch from active to inactive depending on the linked GB model
			if( save.type[save.foeIndex] != 0 ){
		
				//Erase the text (only the first line!)
				fill_win_rect(2, 2, 18, 1, 0x00);
		
				//-= FOE POWER (and update price) =-
		
				//If the foe is currently active ?
				if ((save.type[save.foeIndex] == save.model) ||
				    (save.type[save.foeIndex] == model_linked)) {
		
					//Display power static string
					set_win_tiles(2, 2, 5, 1, msg_power);
		
					//Display foe power
		
					//Convert the number to an ASCII string
					uitoa(save.power[save.foeIndex], txt_buffer, 10);
		
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
					uitoa(save.powerUP[save.foeIndex], txt_buffer, 10);
		
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
			}
			
			//GOALS
			//Check goal "2 Player link mode used" if not already unlocked
			if( save.goals[9] == 0 ){
				//Record this goal as achieved!
				save.goals[9]=1;
				//Display menu showing the newly unlocked goal!
				STATE=3;
				menu_newgoal=9; //set this variable to indicate a newly unlocked goal!
			}
			
			DISABLE_RAM_MBC5; //END SRAM USE
			
			//Display connected GB model at the bottom of the status area (will display blank tile if linked model is disconnected or empty)
			set_win_tiles(0, 5, 5, 1, &(map_linked_buddy[model_linked][0]) );
		}


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
		if( save.type[save.foeIndex] != MODEL_EMPTY ){

			//Pressing A : Recharge batteries
			if (pad & J_A){

				//If we have enough money left and if the foe is currently active (via user GB or Linked GB)
				if (((save.type[save.foeIndex] == save.model) ||
					 (save.type[save.foeIndex] == model_linked)) &&
					 (save.score > save.powerUP[save.foeIndex]) ) {

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
					uitoa( save.power[save.foeIndex], txt_buffer, 10);

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
					uitoa(save.powerUP[save.foeIndex], txt_buffer, 10);

					//Compute its length (before messing with it!)
					j=strlen(txt_buffer);

					//Apply a -32 offset to all letters of the string to map the ASCII index to the one of our embeded font
					for( i=0 ; i < j ; ++i){ txt_buffer[i] -= 32; }

					//Add a trailing "]"
					txt_buffer[j]=0x3D;

					//Display the value (start afte the previous string, and add +1 char at the end for the trailing "]" sign)
					set_win_tiles(l, 2, j+1, 1, txt_buffer);

					//GOALS
					//Update manual recharges counter
					if( save.count_recharges < 255 ){
						++save.count_recharges;
					}
					//Check goal "200 manual recharges performed" if not already unlocked
					if( save.goals[8] == 0 && save.count_recharges >= 200 ){
						//Record this goal as achieved!
						save.goals[8]=1;
						//Display menu showing the newly unlocked goal!
						STATE=3;
						menu_newgoal=8; //set this variable to indicate a newly unlocked goal!
					}
					
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
					
					//GOALS
					//Update maxed foes counter if the current foes reached max level
					if( save.level[save.foeIndex] == 150 && save.count_maxed < 255 ){ 
						++save.count_maxed;
					}
					//Check goal "1 foe maxed" if not already unlocked
					if( save.goals[4] == 0 && save.count_maxed > 0 ){
						//Record this goal as achieved!
						save.goals[4]=1;
						//Display menu showing the newly unlocked goal!
						STATE=3;
						menu_newgoal=4; //set this variable to indicate a newly unlocked goal!
					}
					//Check goal "64 foe maxed" if not already unlocked
					if( save.goals[5] == 0 && save.count_maxed >= 64 ){
						//Record this goal as achieved!
						save.goals[5]=1;
						//Display menu showing the newly unlocked goal!
						STATE=3;
						menu_newgoal=5; //set this variable to indicate a newly unlocked goal!
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

					//GOALS
					//Update foes types counter (do it now because we'll clear the foes variables right after that!)
					if( save.count_types[save.type[save.foeIndex]] > 0 ){
						--save.count_types[save.type[save.foeIndex]];
					}
					//Update maxed foes counter if the current foes reached max level (do it now too because we'll clear the foes variables right after that!)
					if( save.level[save.foeIndex] == 150 && save.count_maxed > 0 ){ 
						--save.count_maxed;
					}
					
					//Fire the current foe
					save.type[save.foeIndex] = MODEL_EMPTY;
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
					
					//GOALS
					//Update foes counter
					if( save.count_foes > 0){ 
						--save.count_foes;
					}
					//Check goal "0 foes" if not already unlocked
					if( save.goals[2] == 0 && save.count_foes == 0 ){
						//Record this goal as achieved!
						save.goals[2]=1;
						//Display menu showing the newly unlocked goal!
						STATE=3;
						menu_newgoal=2; //set this variable to indicate a newly unlocked goal!
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

			//Pressing B : hire new foe! (and not "A" to avoid accidentaly hiring a foe when spamming recharge batteries button over all the foes)
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
						// save.type[save.foeIndex]=1+(rand()&3);
						save.type[save.foeIndex] = ((unsigned char)rand() % (MODEL_MAX - MODEL_MIN + 1)) + MODEL_MIN;
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
					if( save.type[j] == MODEL_DMG ){

						//Is this model currently running?
						if( (save.model == MODEL_DMG) || (model_linked == MODEL_DMG) ){
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
					else if( save.type[j] == MODEL_MGB ){

						//Is this model currently running?
						if( (save.model == MODEL_MGB) || (model_linked == MODEL_MGB) ){
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
					else if( save.type[j] == MODEL_CGB ){

						//Is this model currently running?
						if( (save.model == MODEL_CGB) || (model_linked == MODEL_CGB) ){
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
					else if( save.type[j] == MODEL_SGB ){

						//Is this model currently running?
						if( (save.model == MODEL_SGB) || (model_linked == MODEL_SGB) ){
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_sgb_on);
						}
						//Else, draw the "sleeping" version
						else {
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_sgb_off);
						}
					}
					
					//else if the foe is a SG2 model
					else if( save.type[j] == MODEL_SG2 ){

						//Is this model currently running?
						if( (save.model == MODEL_SG2) || (model_linked == MODEL_SG2) ){
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_sg2_on);
						}
						//Else, draw the "sleeping" version
						else {
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_sg2_off);
						}
					}
					
					//else if the foe is a GBA model
					else if( save.type[j] == MODEL_GBA ){

						//Is this model currently running?
						if( (save.model == MODEL_GBA) || (model_linked == MODEL_GBA) ){
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_gba_on);
						}
						//Else, draw the "sleeping" version
						else {
							//Draw the foe tilemap on the foe coordinates
							set_bkg_tiles( k, l, 4, 4, map_gba_off);
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
					
					//GOALS
					//Update foes counter
					if( save.count_foes < 255){ 
						++save.count_foes;
					}
					//Update foes types counter
					if( save.count_types[save.type[j]] < 255 ){
						++save.count_types[save.type[j]];
					}
					//Check goal "64 foes" if not already unlocked
					if( save.goals[1] == 0 && save.count_foes >= 64 ){
						//Record this goal as achieved!
						save.goals[1]=1;
						//Display menu showing the newly unlocked goal!
						STATE=3;
						menu_newgoal=1; //set this variable to indicate a newly unlocked goal!
					}
					//Check goal "All foes types" if not already unlocked
					if( save.goals[3] == 0 ){
						//If we are currently hiring at least one foe of each type
						if( save.count_types[1] != 0 && save.count_types[2] != 0 && save.count_types[3] != 0 && save.count_types[4] != 0 && save.count_types[5] != 0 && save.count_types[6] != 0 ){
							//Record this goal as achieved!
							save.goals[3]=1;
							//Display menu showing the newly unlocked goal!
							STATE=3;
							menu_newgoal=3; //set this variable to indicate a newly unlocked goal!
						}
					}	
					//Check also if the "all foes types" might have been reached too

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
			if ((save.type[j] == save.model) ||
			    (save.type[j] == model_linked)) {

				//If the current foe have some power left
				if( save.power[j] > 0 ){

					//Increase score by the current foe recharge price
					save.score += save.powerUP[j];

					//Don't go over the max score value! (4 294 967 295 for a UINT32)
					if( save.score > 4242424242 ){
						
						//Go back to the arbitrary max value I chose (H2G2 FTW!)
						save.score = 4242424242;
						
						//GOALS
						//Check goal "Max score" if not already unlocked
						if( save.goals[0] == 0 ){
							//Record this goal as achieved!
							save.goals[0]=1;
							//Display menu showing the newly unlocked goal!
							STATE=3;
							menu_newgoal=0; //set this variable to indicate a newly unlocked goal!
						}
					}


					//Decrease foe power by 1
					--save.power[j];

					//If we are currently displaying this foe info on GUI, update the POWER number only (optimization!)
					if( save.foeIndex == j ){

						//Convert the foe power to an ASCII string
						uitoa( save.power[save.foeIndex], txt_buffer, 10);

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
		ultoa(save.score, txt_score, 10);

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

		
		//Pressing START : go to pause screen (menus)
		if (pad & J_START ){
			
			//Set the state to 1 (pause screen)
			STATE=1;
			//We'll enter the menu state at the end of the current step of the gameplay loop
		}
		
		//If we asked for a "non-gameplay" state (menu, goals list, etc.), we display it now (so we are sure that the game has completed a full gameplay loop before switching to menus)
		if( STATE != 0 ){
			
			//And call the function that handle the menu states (endless loop with its own vblbank(), that will "initGame" back to normal before exiting)
			updateMenu();
			
			//Special case: if we reseted the Savegame, we are still in STATE = 10 after this function, so we launch it again to display the title menu
			if( STATE == 10 ){
				//Display the title menu, and as we'll exit we will be playing with the new game save
				updateMenu();
			}
		}
		
		//WAIT FOR VBLANK TO FINISH - ENSURES 60 FRAMES PER SECOND MAXIMUM
		wait_vbl_done();
	}
	//GAME LOOP END
}




//==============================
//======== FUNCTIONS  ===========
//==============================

//This function detect the GB model we are currently running on (without saving it to the save data - that part is done in the initGame function)
//Params ()
void detectModel(){
	
	//Wait 4 frames
	//For PAL SNES this delay is required on startup
	for (i = 4; i != 0; --i){ wait_vbl_done(); }

	//Shut down screen while init
	DISPLAY_OFF;

	//==== MODEL DETECTION ====

	//Detect the current gb model we are running on! (without saving it, as the SRAM is not initialized yet)

	//If running on a SGB
	if ( sgb_check() != NULL ){

		//DMG cpu (SGB1)
		if( _cpu == DMG_TYPE ){

			//Record the current model in a local var for now
			model = MODEL_SGB;
		}
		//MGB cpu (SGB2)
		else if( _cpu == MGB_TYPE ){

			//Record the current model in a local var for now
			model = MODEL_SG2;
		}

	}
	//Else, not running on a SGB
	else {
		//DMG model (Fat)
		if( _cpu == DMG_TYPE ){

			//Record the current model in a local var for now
			model = MODEL_DMG;
		}
		//MGB model (Pocket)
		else if( _cpu == MGB_TYPE ){

			//Record the current model in a local var for now
			model = MODEL_MGB;
		}
		//CGB model (Color)
		else if( _cpu == CGB_TYPE ){

			// GBA model (AGB/AGS Game Boy Advance, Game Boy Advance SP)
			if (_is_GBA == GBA_DETECTED) {
				//Record the current model in a local var for now
				model = MODEL_GBA;
			}
			// Else actual GBC model
			else {
				//Record the current model in a local var for now
				model = MODEL_CGB;
			}

			//Enable default grayscale palette when running on CGB (so we don't get rendered by the ugly red/green default palettes)
			cgb_compatibility();
			
			//Set the first background color as pure white, as the compatibilty mode add a slight touch of red that doesn't sit well with modded "backlit" screen for CGB or AGB models
			set_bkg_palette_entry(0,0, 0xFFFF);

			//Set the second sprite color as white, to reflect what we do with the non-CGB sprite palette
			set_sprite_palette_entry(0,1, 0xFFFF);
			
		}
	}
	
	//Serial Linked model always resets to disconnected on startup, regardless of save data
	//Use MODEL_DISCONNECTED instead of MODEL_EMPTY to ensure
	//there aren't accidental matches in the disconnected state.
	model_linked = MODEL_DISCONNECTED;
}


//This function init the game at startup or after exiting a menu and going back to gameplay
//Params ()
void initGame(){

	//Shut down screen while init
	DISPLAY_OFF;

	//==== SRAM INIT  ====

	//Begin SRAM use
	ENABLE_RAM_MBC5;
	
	//Init the SRAM if it's the first game run (i.e no save file found)
	
	//By default, set the game to a "gameplay" state
	STATE=0;
	
	//Variable used to record the lastest unlocked goal to emphasize it on goals list (we reset it here as it can be used to complete a goal right after switching consoles)
	menu_newgoal=0; 

	//If the SRAM doesn't contains the "magic key", we "reset / init" it to have a clear and controlled startup state
	if( save.sram_check != SRAM_CHECK ){

		//Mark the SRAM as checked to avoid erasing it in the future!
		save.sram_check = SRAM_CHECK;

		//Reset the GB model (it'll be stored right after that section)
		save.model = MODEL_EMPTY;

		//Reset the score
		save.score = 0;

		//Reset the foes values (type, energy and level all set to base values)
		for( i=0 ; i < 64 ; ++i){
			save.type[i] = MODEL_EMPTY;
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
		
		//Reset the goals (achievements) list
		for( i=0 ; i < 10 ; ++i){
			save.goals[i]=0; //mark the goal as "not reached yet"
		}
		
		//Init the various counters used for goals detection
		save.count_foes=1; //as we have 1 free employee at start
		save.count_maxed=0;
		save.count_recharges=0;
		//Reset the types / console model counters
		for( i=0 ; i < 7 ; ++i){
			save.count_types[i]=0;
			save.count_models[i]=0;
		}
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
		
		//GOALS
		//Update models types counter
		if( save.count_models[save.model] < 255 ){
			++save.count_models[save.model];
		}
		//Count the number of different models we used so far, using the "j" variable as temp counter
		j=0;
		//For each console model (values 1-6)
		for( i=1 ; i < 7 ; ++i){
			//If we used them at least once
			if( save.count_models[i] != 0 ){
				++j; //increase total number of consoles used
			}
		}
		//Check goal "2 consoles used" if not already unlocked
		if( save.goals[6] == 0 && j >= 2 ){
			//Record this goal as achieved!
			save.goals[6]=1;
			//Display menu showing the newly unlocked goal!
			STATE=3;
			menu_newgoal=6; //set this variable to indicate a newly unlocked goal!
		}
		//Check goal "6 consoles used" if not already unlocked
		if( save.goals[7] == 0 && j >= 6 ){
			//Record this goal as achieved!
			save.goals[7]=1;
			//Display menu showing the newly unlocked goal!
			STATE=3;
			menu_newgoal=7; //set this variable to indicate a newly unlocked goal!
		}
		
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
	set_bkg_data(0x80, 126, tiles_data);

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
	/*
	//Display the signature logo on bottom right corner
	set_win_tile_xy(15, 5, 0xD2);
	set_win_tile_xy(16, 5, 0xD3);
	set_win_tile_xy(17, 5, 0xD4);
	set_win_tile_xy(18, 5, 0xD5);
	set_win_tile_xy(19, 5, 0xD6);
	*/
	//Display the number of goals achieved in the bottom right corner
	//set_win_tile_xy(17, 5, 0x10); //zero character
	set_win_tile_xy(18, 5, 0xFD); //the "tick" character
	set_win_tile_xy(19, 5, 0x10); //zero character
	
	//Populate the GUI by refreshing it once (enable SRAM access just for this, as we'll need values from SRAM)
	ENABLE_RAM_MBC5;
	refreshGUI();
	//Set the variable to detect when to refresh the GUI too
	foeIndexOld=save.foeIndex;
	
	//GOALS
	//Count the number of different goals we have completed so far, using the "j" variable as temp counter
	j=0;
	//For each goal (values 0-9)
	for( i=0 ; i < 10 ; ++i){
		//If we completed it
		if( save.goals[i] != 0 ){
			++j; //increase total number of goals achieved
		}
	}
	
	//If we have 10 goals or more completed (two digits instead of one)
	if( j >= 10 ){
		//Add a heading "1" to the number of goals completed! (me handle the 2-digits number display manually here)
		set_win_tile_xy(18, 5, 0x11); //one character
		//And move the tick character one step to the left
		set_win_tile_xy(17, 5, 0xFD); //the "tick" character
		//And remove 10 from the number of goals completed so we can display the last digit manually
		j -= 10;
	}
	
	//Display the current number of goals achieved in the bottom right corner of the GUI
	j += 0x10; //add an offset to map to the number 1 character in the tile table
	set_win_tile_xy(19, 5, j);
	
	DISABLE_RAM_MBC5;

	//Display connected GB model at the bottom left of the GUI (will display blank tiles if linked model is empty or disconnected)
	set_win_tiles(0, 5, 5, 1, &(map_linked_buddy[model_linked][0]) );
	
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
	move_sprite(0, 69, 45);
	set_sprite_tile(1, 1);
	move_sprite(1, 77, 45);
	set_sprite_tile(2, 2);
	move_sprite(2, 69, 53);

	//Top-Right corner
	set_sprite_tile(3, 0);
	move_sprite(3, 99, 45);
	set_sprite_prop(3, S_FLIPX);
	set_sprite_tile(4, 1);
	move_sprite(4, 91, 45);
	set_sprite_prop(4, S_FLIPX);
	set_sprite_tile(5, 2);
	move_sprite(5, 99, 53);
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

	//Screen on as initial setup is completed!
	DISPLAY_ON;

	//Start serial link status broadcasting and receiving
	link_init();
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
		if ((save.type[save.foeIndex] == save.model) ||
		    (save.type[save.foeIndex] == model_linked)) {

			//Display power static string
			set_win_tiles(2, 2, 5, 1, msg_power);

			//Display foe power

			//Convert the number to an ASCII string
			uitoa(save.power[save.foeIndex], txt_buffer, 10);

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
			uitoa(save.powerUP[save.foeIndex], txt_buffer, 10);

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
		uitoa(save.level[save.foeIndex], txt_buffer, 10);

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
		uitoa(save.levelUP[save.foeIndex], txt_buffer, 10);

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
		uitoa(save.hireCost, txt_buffer, 10);

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
		uitoa(save.hireCost, txt_buffer, 10);

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
		if( save.type[i] == MODEL_DMG ){

			//Is this model currently running?
			if ((save.model == MODEL_DMG) || (model_linked == MODEL_DMG)) {

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
		else if( save.type[i] == MODEL_MGB ){

			//Is this model currently running?
			if ((save.model == MODEL_MGB) || (model_linked == MODEL_MGB)) {
				
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
		else if( save.type[i] == MODEL_CGB ){

			//Is this model currently running?
			if ((save.model == MODEL_CGB) || (model_linked == MODEL_CGB)) {
				
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
		else if( save.type[i] == MODEL_SGB ){

			//Is this model currently running?
			if ((save.model == MODEL_SGB) || (model_linked == MODEL_SGB)) {
				
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
		
		//else if the foe is a SG2 model
		else if( save.type[i] == MODEL_SG2 ){

			//Is this model currently running?
			if ((save.model == MODEL_SG2) || (model_linked == MODEL_SG2)) {
				
				//Draw the foe tilemap on the foe coordinates
				set_bkg_tiles( k, l, 4, 4, map_sg2_on);

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
				set_bkg_tiles( k, l, 4, 4, map_sg2_off);
			}
		}
		
		//else if the foe is a GBA model
		else if( save.type[i] == MODEL_GBA ){

			//Is this model currently running?
			if ((save.model == MODEL_GBA) || (model_linked == MODEL_GBA)) {
				//Draw the foe tilemap on the foe coordinates
				set_bkg_tiles( k, l, 4, 4, map_gba_on);

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
				set_bkg_tiles( k, l, 4, 4, map_gba_off);
			}
		}
		//Else, the cubicle is empty!
		else {
			//Draw the foe tilemap on the foe coordinates
			set_bkg_tiles(k, l, 4, 4, map_empty);
		}
	}
}




//================================
//========  MENU SCREENS ===========
//================================	
	
//This function handle the title screen and other "non gameplay" states
//Params ()
void updateMenu(){
	
	//Shut down screen while initing the menu screens
	DISPLAY_OFF;
	
	//Hide the sprites
	HIDE_SPRITES;
	
	//Hide the windows
	HIDE_WIN;
	
	//Reset the scrolling
	move_bkg(0, 0);
	
	//Init the menu related variables
	menu_ticks=0;
	menu_step=0;
	
	//Stop the music
	gbt_stop();
	gbt_update();
	
	//Turn sound back on (to play SFX while in menus)
	NR52_REG = 0x80;
	//Enable all sounds channels (1: 0x11 / 2: 0x22 / 3: 0x44 / 4: 0x88 / All : 0xFF)
	NR51_REG = 0xFF;
	//Set volume (max = 0x77, min = 0x00)
	NR50_REG = 0x77;
	
	//Repeat endlessly while the STATE isn't set to 0 (Gameplay) to exit the loop and this function
	while( STATE != 0 ){
		
		//Keep on updating the Link mode if it was enabled (else it'll pause for both players whenever someone enters a menu!)
		link_update(model, &(model_linked));
		
		//Read the player gamepad for actions
		pad=joypad();
		//AND mask it using the previous state to get only new keypresses
		pad &= ~padOld;
		//And store the full pad state for ANDing in the next frame
		padOld = joypad();
		
		
		//====== TITLE SCREEN ========
		
		//If the state is "Title Screen" 
		if( STATE == 10 ){
			
			//Init the screen (menu step 0)
			if( menu_step == 0 ){
				
				//Send all the background tile data (graphics assets) into VRAM
				set_bkg_data(0x80, 57, title_logo_tiles);
				
				//Clear the background (use the "empty" tile, 0x00 in the tiles data)
				fill_bkg_rect(0, 0, 32, 32, 0x00);
				
				//Draw the title logo on screen
				set_bkg_tiles(5, 0, 10, 6, title_logo_map);
				
				//Display credits
				gotoxy(0,16);
				printf("music by");
				gotoxy(0,17);
				printf("KRUMEL");
				gotoxy(11,16);
				printf("a game by");
				gotoxy(11,17);
				printf("Dr. LUDO");
				//Set the final bottom-right tile manually (here, a "S"), to avoid triggering the "auto-scroll" feature of GBDK console
				set_bkg_tile_xy( 19, 17, 0x33);
				
				//set the var so we don't init the screen twice
				menu_step=1;
				
				//And enable display again
				DISPLAY_ON;
			}
			//Update the screen
			else {
				
				//UPDATE SCREEN DISPLAY
				//force the ticks to stop at 31, as we use a modulo 32 to make the text blink
				++menu_ticks;
				if( menu_ticks == 32 ){ menu_ticks = 0; }
				
				//PLAYER MENU ACTIONS
				
				//Menu step 1 (push button to open menu!)
				if( menu_step == 1 ){
					
					//Display instructions blinking
					if( (menu_ticks & 0x1F) < 16 ){
						gotoxy(4, 9); 
						printf("press button"); 
					} else {
						gotoxy(4, 9); 
						printf("            "); 
					}
					
					//When A, B or start button is pressed
					if( pad & J_A || pad & J_B || pad & J_START ){
						
						//Go to the next menu step
						menu_step=2;
						
						//Display the start game option
						gotoxy(4, 7); 
						printf("> NEW GAME   ");
						
						//Display the how to play option (also, clear the "press button" message thanks to the spaces at beginning :))
						gotoxy(4, 9); 
						printf("  HOW TO PLAY");
						
						//Display the how to play option 
						gotoxy(4, 11); 
						printf("  GOALS LIST ");
						
						//Play SFX (upgrade)
						NR10_REG = 0x26;
						NR11_REG = 0x85;
						NR12_REG = 0xF3;
						NR13_REG = 0x84;
						NR14_REG = 0x00;
						NR14_REG = 0x83;
					}
				
				}
				//Menu step 2 (main menu) : menu_step > 0 is used to keep track of current menu option selected!
				else if( menu_step <= 10) {
					
					//Move in the menu up and down
					//Up
					if( pad & J_UP ){
						//Clear previous cursor display
						gotoxy(4, 7+((menu_step-2) << 1)); 
						printf(" ");
						
						//Decrease menu option (and wrap)
						if( menu_step > 2 ){ --menu_step; }
						else { menu_step=4; }
						
						//Update menu option cursor display
						gotoxy(4, 7+((menu_step-2) << 1)); 
						printf(">");
						
						//Play SFX (can't do that)
						NR10_REG = 0x21;
						NR11_REG = 0xA3;
						NR12_REG = 0xF1;
						NR13_REG = 0x0A;
						NR14_REG = 0x00;
						NR14_REG = 0x80;
					}
					//Down
					else if( pad & J_DOWN ){
						//Clear previous cursor display
						gotoxy(4, 7+((menu_step-2) << 1)); 
						printf(" ");
						
						//Increase menu option (and wrap)
						if( menu_step < 4 ){ ++menu_step; }
						else { menu_step=2; }
						
						//Update menu option cursor display
						gotoxy(4, 7+((menu_step-2) << 1)); 
						printf(">");
						
						//Play SFX (can't do that)
						NR10_REG = 0x21;
						NR11_REG = 0xA3;
						NR12_REG = 0xF1;
						NR13_REG = 0x0A;
						NR14_REG = 0x00;
						NR14_REG = 0x80;
					}
				
					//If fire or start button is pressed
					if( pad & J_A || pad & J_B || pad & J_START ){
					
						//Menu item #1 : Start Game
						if( menu_step == 2 ){
							
							//Set the state back to Gameplay (it'll break the endless menu loop)
							STATE=0;
						}
						//Menu item #2 : How to play
						else if( menu_step == 3 ){
							
							//Set the state to the "how to play" option (use the state 11 that will go back to title screen, and not state 2 that will go back to pause screen)
							STATE=11;
							
							//Reset menu variables too
							menu_step=0;
							menu_ticks=0;
							
							//Play SFX (hire)
							NR10_REG = 0x13;
							NR11_REG = 0x8A;
							NR12_REG = 0xF5;
							NR13_REG = 0x0E;
							NR14_REG = 0x00;
							NR14_REG = 0x81;
						}
						//Menu item #3 : Goals list
						else if( menu_step == 4 ){
							
							//Set the state to the "goals list" option (use the state 12 that will go back to title screen, and not state 4 that will go back to pause screen or 3 that will go back to gameplay)
							STATE=12;
							
							//Reset menu variables too
							menu_step=0;
							menu_ticks=0;
							
							//Play SFX (hire)
							NR10_REG = 0x13;
							NR11_REG = 0x8A;
							NR12_REG = 0xF5;
							NR13_REG = 0x0E;
							NR14_REG = 0x00;
							NR14_REG = 0x81;
						}
					}
				}
			}
			
		}
		
		//====== PAUSE SCREEN ========
		
		//Else if the state is "Pause Screen" 
		else if( STATE == 1 ){
			
			//Init the screen (menu step 0)
			if( menu_step == 0 ){
				
				//Send all the background tile data (graphics assets) into VRAM
				set_bkg_data(0x80, 57, title_logo_tiles);
				
				//Clear the background (use the "empty" tile, 0x00 in the tiles data)
				fill_bkg_rect(0, 0, 32, 32, 0x00);
				
				//Draw the title logo on screen
				set_bkg_tiles(5, 0, 10, 6, title_logo_map);
				
				//Display credits
				gotoxy(0,16);
				printf("music by");
				gotoxy(0,17);
				printf("KRUMEL");
				gotoxy(11,16);
				printf("a game by");
				gotoxy(11,17);
				printf("Dr. LUDO");
				//Set the final bottom-right tile manually (here, a "S"), to avoid triggering the "auto-scroll" feature of GBDK console
				set_bkg_tile_xy( 19, 17, 0x33);
				
				//Display the start game option
				gotoxy(4, 7); 
				printf("> CONTINUE  ");
				
				//Display the goals list option (also, clear the "press button" message thanks to the spaces at beginning :))
				gotoxy(4, 9); 
				printf("  GOALS LIST ");
				
				//Display the how to play option (also, clear the "press button" message thanks to the spaces at beginning :))
				gotoxy(6, 11); 
				printf("HOW TO PLAY");
				
				//Display the clear save option
				gotoxy(6, 13); 
				printf("RESET SAVE");
				
				//set the var so we don't init the screen twice (set it to "2" because we use the same code as title screen, that have another step "press button to open menu" lacking here)
				menu_step=2;
				
				//And enable display again
				DISPLAY_ON;
				
				//Play SFX (hire)
				NR10_REG = 0x13;
				NR11_REG = 0x8A;
				NR12_REG = 0xF5;
				NR13_REG = 0x0E;
				NR14_REG = 0x00;
				NR14_REG = 0x81;
			}
			//Update the screen
			else {
				
				//PLAYER MENU ACTIONS
				
				//Menu step 1  : menu_step > 0 is used to keep track of current menu option selected!
				if( menu_step <= 10) {
					
					//Move in the menu up and down
					//Up
					if( pad & J_UP ){
						//Clear previous cursor display
						gotoxy(4, 7+((menu_step-2) << 1)); 
						printf(" ");
						
						//Decrease menu option (and wrap)
						if( menu_step > 2 ){ --menu_step; }
						else { menu_step=5; }
						
						//Update menu option cursor display
						gotoxy(4, 7+((menu_step-2) << 1)); 
						printf(">");
						
						//Play SFX (can't do that)
						NR10_REG = 0x21;
						NR11_REG = 0xA3;
						NR12_REG = 0xF1;
						NR13_REG = 0x0A;
						NR14_REG = 0x00;
						NR14_REG = 0x80;
					}
					//Down
					else if( pad & J_DOWN ){
						//Clear previous cursor display
						gotoxy(4, 7+((menu_step-2) << 1)); 
						printf(" ");
						
						//Increase menu option (and wrap)
						if( menu_step < 5 ){ ++menu_step; }
						else { menu_step=2; }
						
						//Update menu option cursor display
						gotoxy(4, 7+((menu_step-2) << 1)); 
						printf(">");
						
						//Play SFX (can't do that)
						NR10_REG = 0x21;
						NR11_REG = 0xA3;
						NR12_REG = 0xF1;
						NR13_REG = 0x0A;
						NR14_REG = 0x00;
						NR14_REG = 0x80;
					}
				
					//If fire or start button is pressed
					if( pad & J_A || pad & J_B || pad & J_START ){
					
						//Menu item #1 : Start Game
						if( menu_step == 2 ){
							
							//Set the state back to Gameplay (it'll break the endless menu loop)
							STATE=0;
						}
						//Menu item #2 : Goals list
						else if( menu_step == 3 ){
							
							//Set the state to the "goals" list, but showing the pause menu again after exit instead of gameplay
							STATE=4;
							
							//Reset menu variables too
							menu_step=0;
							menu_ticks=0;
							
							//Play SFX (hire)
							NR10_REG = 0x13;
							NR11_REG = 0x8A;
							NR12_REG = 0xF5;
							NR13_REG = 0x0E;
							NR14_REG = 0x00;
							NR14_REG = 0x81;
						}
						//Menu item #3 : How to play
						else if( menu_step == 4 ){
							
							//Set the state to the "how to play" option
							STATE=2;
							
							//Reset menu variables too
							menu_step=0;
							menu_ticks=0;
							
							//Play SFX (hire)
							NR10_REG = 0x13;
							NR11_REG = 0x8A;
							NR12_REG = 0xF5;
							NR13_REG = 0x0E;
							NR14_REG = 0x00;
							NR14_REG = 0x81;
						}
						//Menu item #4 : Clear save
						else if( menu_step == 5 ){
							
							//Open a submenu to confirm "are you sure ?"
							
							//Go to the desired menu step (>10 => clear save confirm submenu)
							menu_step=12; //Player safeguard trick: we select "no" instead of "yes" by default :)
							
							//Erase the selection cursor from the main menu item we were in
							gotoxy(4, 11); 
							printf(" ");
							
							//Display the menu message
							gotoxy(7, 14); 
							printf("ARE YOU SURE?");
							
							//Display the "yes" option
							gotoxy(7, 15); 
							printf("  YES");
							
							//Display the "no" option
							gotoxy(15, 15); 
							printf("> NO");
							
							//Play SFX (fire employee)
							NR10_REG = 0x4E;
							NR11_REG = 0x96;
							NR12_REG = 0xA7;
							NR13_REG = 0xBB;
							NR14_REG = 0x00;
							NR14_REG = 0x85;
						}
						
					}
				}
				
				//Menu step 2 (clear save confirm submenu) : menu_step > 10 is used to keep track of current menu option selected!
				else if( menu_step > 10) {
					
					//Move in the menu up and down
					//Up
					if( pad & J_UP || pad & J_LEFT ){
						//Clear previous cursor display
						gotoxy(7+((menu_step-11)*8), 15);
						printf(" ");
						
						//Decrease menu option (and wrap)
						if( menu_step > 11 ){ --menu_step; }
						else { menu_step=12; }
						
						//Update menu option cursor display
						gotoxy(7+((menu_step-11)*8), 15); 
						printf(">");
						
						//Play SFX (can't do that)
						NR10_REG = 0x21;
						NR11_REG = 0xA3;
						NR12_REG = 0xF1;
						NR13_REG = 0x0A;
						NR14_REG = 0x00;
						NR14_REG = 0x80;
					}
					//Down
					else if( pad & J_DOWN || pad & J_RIGHT ){
						//Clear previous cursor display
						gotoxy(7+((menu_step-11)*8), 15);
						printf(" ");
						
						//Increase menu option (and wrap)
						if( menu_step < 12 ){ ++menu_step; }
						else { menu_step=11; }
						
						//Update menu option cursor display
						gotoxy(7+((menu_step-11)*8), 15);
						printf(">");
						
						//Play SFX (can't do that)
						NR10_REG = 0x21;
						NR11_REG = 0xA3;
						NR12_REG = 0xF1;
						NR13_REG = 0x0A;
						NR14_REG = 0x00;
						NR14_REG = 0x80;
					}
				
					//If fire or start button is pressed
					if( pad & J_A || pad & J_B || pad & J_START ){
					
						//Menu item #1 : Clear Save
						if( menu_step == 11 ){
							
							//Enable SRAM use
							ENABLE_RAM_MBC5;
							
							//Voluntarily corrupt the SRAM integrity check to force the game to set a new one at startup
							save.sram_check = 0x00000000;
							
							//Also reset the "Goals List" in case the player want to checks it before restarting a new game
							for( i=0 ; i < 10 ; ++i){
								save.goals[i]=0; //mark the goal as "not reached yet"
							}
							
							//End SRAM use
							DISABLE_RAM_MBC5;
			
							//Set the state to Title Screen, as we are starting a new game, without breaking this loop to avoid triggering a "InitGame" that would actually set up a new and valid game save data
							STATE=10;
							
							//Init the menu related variables
							menu_ticks=0;
							menu_step=0;

						}
						//Menu item #2 : Don't clear save and go back to main menu
						else if( menu_step == 12 ){
							
							//Erase the submenu text by filling with the blank tile
							fill_bkg_rect(7, 14, 15, 2, 0x00);
							
							//Display the selection cursor back in front of the main menu item we were in
							gotoxy(4, 13); 
							printf(">");
							
							//Go back to the main menu selection
							menu_step=5;
							
							//Play SFX (can't do that)
							NR10_REG = 0x21;
							NR11_REG = 0xA3;
							NR12_REG = 0xF1;
							NR13_REG = 0x0A;
							NR14_REG = 0x00;
							NR14_REG = 0x80;
						}
					}

				}
				
				
			}
		
		}			
			
		//====== HOW TO PLAY SCREEN ========
		
		//Else if the state is "How to play" 
		else if( STATE == 2 || STATE == 11 ){
			
			//Init the screen (menu step 0 - Page 1)
			if( menu_step == 0 ){
				
				//Clear the background (use the "empty" tile, 0x00 in the tiles data)
				fill_bkg_rect(0, 0, 32, 32, 0x00);
				
				//Add a border
				fill_bkg_rect(0, 0, 20, 1, 0x60);
				
				//Display the screen title option
				gotoxy(1, 1); 
				printf("-= HOW TO PLAY =-");
				
				//Add a border
				fill_bkg_rect(0, 2, 20, 1, 0x60);
				
				//Display the text
				gotoxy(1, 4); 
				printf("GB Corp is an idle\n -incremental game\n rewarding you for\n playing on various\n Game Boy models.\n\n Hire consoles to\n work at GB Corp!\n\n Recharge their\n batteries when\n needed and upgrade\n them to increase\n your earnings.");
				
				//set the var so we don't init the screen twice
				++menu_step;
				
				//And enable display again
				DISPLAY_ON;
			}
			//Init the screen (menu step 2 - Page 1)
			else if( menu_step == 2 ){
				
				//DISABLE DISPLAY TO REPLACE THE WHOLE SCREEN
				DISPLAY_OFF;
				
				//Clear the background (use the "empty" tile, 0x00 in the tiles data)
				fill_bkg_rect(0, 0, 32, 32, 0x00);
				
				//Add a border
				fill_bkg_rect(0, 0, 20, 1, 0x60);
				
				//Display the screen title option
				gotoxy(1, 1); 
				printf("-= HOW TO PLAY =-");
				
				//Add a border
				fill_bkg_rect(0, 2, 20, 1, 0x60);
				
				//Display the text
				gotoxy(1, 4); 
				printf("Consoles only work\n if you are playing\n on the SAME MODEL:\n use a DMG to wake\n DMGs in the game!\n\n Each time you swap\n the game cartridge\n to a different Game Boy model everyone\n will get recharged\n FOR FREE!");
				
				//Add a border
				fill_bkg_rect(0, 17, 20, 1, 0x60);
				
				//set the var so we don't init the screen twice
				++menu_step;
				
				//And enable display again
				DISPLAY_ON;
			}
			//Init the screen (menu step 4 - Page 2)
			else if( menu_step == 4 ){
				
				//DISABLE DISPLAY TO REPLACE THE WHOLE SCREEN
				DISPLAY_OFF;
				
				//Clear the background (use the "empty" tile, 0x00 in the tiles data)
				fill_bkg_rect(0, 0, 32, 32, 0x00);
				
				//Add a border
				fill_bkg_rect(0, 0, 20, 1, 0x60);
				
				//Display the screen title option
				gotoxy(4, 1); 
				printf("-= TIPS =-");
				
				//Add a border
				fill_bkg_rect(0, 2, 20, 1, 0x60);
				
				//Display the text
				gotoxy(1, 4); 
				printf("Upgrading employees is key: each active employee will earn\n twice its recharge\n cost per second.\n\n You can also link\n two different GB\n models using a\n LINK CABLE to have\n 2 types of active\n employees!");
				
				//Add a border
				fill_bkg_rect(0, 17, 20, 1, 0x60);
				
				//set the var so we don't init the screen twice
				++menu_step;
				
				//And enable display again
				DISPLAY_ON;
			}

			//PLAYER MENU ACTIONS

			//If fire or start button is pressed
			if( pad & J_A || pad & J_B || pad & J_START ){
				
				//Next Menu Page (if we haven't displayed all the pages yet) - menu_step is used to keep track of current page 
				if( menu_step < 5 ){
					//Increase menu step to display next page
					++menu_step;
					
					//Play SFX (recharge)
					NR10_REG = 0x27;
					NR11_REG = 0xD4;
					NR12_REG = 0xF2;
					NR13_REG = 0x40;
					NR14_REG = 0x00;
					NR14_REG = 0x86;					
				}
				//Else Quit screen when pressed and back to menu
				else  {
			
					//Menu item #1 : back to menu where we came from (1: pause or 10: title)
					//Go back to title screen
					if( STATE == 11 ){ 
						STATE = 10;
						
						//Play SFX (hire)
						NR10_REG = 0x13;
						NR11_REG = 0x8A;
						NR12_REG = 0xF5;
						NR13_REG = 0x0E;
						NR14_REG = 0x00;
						NR14_REG = 0x81;
					}
					//else go back to pause screen
					else {
						STATE = 1;
					}
					
					//Reset menu variables too
					menu_step=0;
					menu_ticks=0;
				}
				
				
			}

		}
		
		//====== GOALS SCREEN ========
		
		//Else if the state is "Goals list" 
		else if( STATE == 3 || STATE == 4 || STATE == 12 ){
			
			//Init the screen (menu step 0 - Page 1)
			if( menu_step == 0 ){
				
				//Clear the background (use the "empty" tile, 0x00 in the tiles data)
				fill_bkg_rect(0, 0, 32, 32, 0x00);
				
				//Add a border
				fill_bkg_rect(0, 0, 20, 1, 0x60);
				
				//Display the screen title option
				gotoxy(5, 1); 
				printf("-= GOALS =-");
				
				//Add a border
				fill_bkg_rect(0, 2, 20, 1, 0x60);
				
				ENABLE_RAM_MBC5; //Save data access needed here
				
				//Display the goals list (checkmark + descriptions)
				gotoxy(0, 3);
				
				printf("\n   Money is KING");
				set_bkg_tile_xy( 1, 4, (save.goals[0] != 0) ? 0xFD : 0x0D);
				
				printf("\n   No Jobs Opening");
				set_bkg_tile_xy( 1, 5, (save.goals[1] != 0) ? 0xFD : 0x0D);	
				
				printf("\n   Empty Office");
				set_bkg_tile_xy( 1, 6, (save.goals[2] != 0) ? 0xFD : 0x0D);	
				
				printf("\n   Diversity Day");
				set_bkg_tile_xy( 1, 7, (save.goals[3] != 0) ? 0xFD : 0x0D);
				
				printf("\n   The Expert");
				set_bkg_tile_xy( 1, 8, (save.goals[4] != 0) ? 0xFD : 0x0D);	
				
				printf("\n   Top #1 Company");
				set_bkg_tile_xy( 1, 9, (save.goals[5] != 0) ? 0xFD : 0x0D);
				
				printf("\n   Cartridge Swap");
				set_bkg_tile_xy( 1, 10, (save.goals[6] != 0) ? 0xFD : 0x0D);
				
				printf("\n   True Collector");
				set_bkg_tile_xy( 1, 11, (save.goals[7] != 0) ? 0xFD : 0x0D);
				
				printf("\n   Battery Hog");
				set_bkg_tile_xy( 1, 12, (save.goals[8] != 0) ? 0xFD : 0x0D);
				
				printf("\n   Connecting People");
				set_bkg_tile_xy( 1, 13, (save.goals[9] != 0) ? 0xFD : 0x0D);
				
				DISABLE_RAM_MBC5;
				
				//Add a border
				fill_bkg_rect(0, 15, 20, 1, 0x60);
				
				//set the var so we don't init the screen twice
				++menu_step;
				
				//Play Some Music
				music_note = 0;
				music_tempo = 10; //add a small delay so it doesn't play immediately
				
				//And enable display again
				DISPLAY_ON;
			}

			//ANIMATION
			
			//force the ticks to stop at 31, as we use a modulo 32 to make the text blink
			++menu_ticks;
			if( menu_ticks == 32 ){ menu_ticks = 0; }
			
			//If we are in "new goal achieved" state (STATE == 3)
			if( STATE == 3 ){
			
				//Make the "check" sign blink on the newly completed goal
				//The menu_newgoal variable is set beforehand during gameplay to indicate the ID of the newly achieved goal
				//On
				if( menu_ticks == 0 ){ set_bkg_tile_xy( 1, 4+menu_newgoal, 0xFD); }
				//Off
				else if( menu_ticks == 16 ){ set_bkg_tile_xy( 1, 4+menu_newgoal, 0x00); }
				
				
				//Play some music (actually, a short "achievement unlocked" jingle)
				if( music_note < sizeof(music_goal)){
					
					//If the previous note has finished playing
					if( music_tempo == 0){
						
						//Play current note ( << 2 is like a *4 (bitshift is faster than * for the GB), because music note is 0,1,2,3,4... while our actual note array uses 4 entry per note, 
						//so we have to multiply note by 4 to get the actual registers index each note)
						NR21_REG = music_notes[ music_goal[music_note]<<2 ];
						NR22_REG = music_notes[ (music_goal[music_note]<<2)+1 ];
						NR23_REG = music_notes[ (music_goal[music_note]<<2)+2 ];
						NR24_REG = music_notes[ (music_goal[music_note]<<2)+3 ];
						
						//Set the new delay to wait
						music_tempo = music_goal[ music_note+1 ];
						
						//Skip to the next note
						music_note += 2;
					}
					//Else wait for the next note to play
					else {
						music_tempo--;
					}
				}
			}
			
			
			//PLAYER MENU ACTIONS
			
			//If waiting time isn't over over (to avoid quitting it immediately without having time to read
			if( menu_step < 80 ){
				++menu_step;
				//Increase wait counter immediately if we are coming from the pause or title menu (and not a "goal unlocked" from gameplay screen)
				if( STATE != 3 ){
					menu_step += 79;
				}
			}
			//Else enable player actions
			else {
				
				//Display "press button" message
				gotoxy(4, 17);
				//If not in "new goal unlocked" mode, make it blink
				if( STATE != 3 ){
					if( menu_ticks < 16 ){ printf("PRESS BUTTON"); }
					else { printf("            "); }
				}
				//Else make it static
				else {
					printf("PRESS BUTTON"); 
				}
				
				//If fire or start button is pressed
				if( pad & J_A || pad & J_B || pad & J_START ){
					
					//Menu item #1 : back to menu where we came from (1: pause, 10: title or 0: gameplay)
					//Go back to pause screen
					if( STATE == 4 ){ 
						STATE = 1;
					}
					//Else go back to title screen
					else if( STATE == 12 ){ 
						STATE = 10;
						
						//Play SFX (hire)
						NR10_REG = 0x13;
						NR11_REG = 0x8A;
						NR12_REG = 0xF5;
						NR13_REG = 0x0E;
						NR14_REG = 0x00;
						NR14_REG = 0x81;
					}
					//else go back to gameplay
					else {
						STATE = 0;
					}
					
					//Reset menu variables too
					menu_step=0;
					menu_ticks=0;
				}
			}

		}
		
		//Wait for vblank (as we are in self-contained loop for menu screen, we don't use the one from the main loop)
		wait_vbl_done();	
	}
	
	//Before exiting this Menu function and going back to regular  Gameplay, we "init" the game again so it resume gameplay back were it was before going to menus (build BG, GUI, set scroll, etc.)
	initGame();
}