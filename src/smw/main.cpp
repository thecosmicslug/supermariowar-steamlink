/* This file is part of SMW.

    SMW is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Foobar is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SMW.  If not, see <http://www.gnu.org/licenses/>. */

#ifdef _XBOX
#include <xtl.h>
#endif

#include "global.h"				//all the global stuff
#include "linfunc.h"

#include "MatchTypes.h"
#include "FPSLimiter.h"
#include "GSSplashScreen.h"

#include <time.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

//now it's really time for an "engine" (aka resource manager)
// TODO: check SDL2 Windows libs
#ifdef _WIN32
    #pragma comment(lib, "SDL_image.lib")

    #ifndef _XBOX
        #pragma comment(lib, "SDL.lib")
        #pragma comment(lib, "SDLmain.lib")
        #pragma comment(lib, "SDL_mixer.lib")

        #define WIN32_LEAN_AND_MEAN
        #include <windows.h>

        #ifdef _MSC_VER
        #include <crtdbg.h>
        #endif

    #endif
#endif


//------ system stuff ------
#ifdef USE_SDL2
    SDL_Window       *window;        //the window
    SDL_Renderer     *renderer;      //screen -> texture -> renderer -> window
    SDL_Texture      *screenAsTexture;
#endif
SDL_Surface		*screen;		//for gfx (maybe the gfx system should be improved -> resource manager)
SDL_Surface		*blitdest;		//the destination surface for all drawing (can be swapped from screen to another surface)

short			g_iCurrentDrawIndex = 0;

short			x_shake = 0;
short			y_shake = 0;


//------ game relevant stuff ------
CPlayer			*list_players[4];
short			list_players_cnt = 0;

CScore			*score[4];

short			score_cnt;


//Locations for swirl spawn effects
short g_iSwirlSpawnLocations[4][2][25];


CGameMode			*gamemodes[GAMEMODE_LAST];
CGM_Bonus			*bonushousemode = NULL;
CGM_Pipe_MiniGame	*pipegamemode = NULL;
CGM_Boss_MiniGame	*bossgamemode = NULL;
CGM_Boxes_MiniGame	*boxesgamemode = NULL;

short		currentgamemode = 0;

float CapFallingVelocity(float vel)
{
    //if (vel < -MAXVELY)
    //    return -MAXVELY;

    if (vel > MAXVELY)
        return MAXVELY;

    return vel;
}

float CapSideVelocity(float vel)
{
    if (vel < -MAXSIDEVELY)
        return -MAXSIDEVELY;

    if (vel > MAXSIDEVELY)
        return MAXSIDEVELY;

    return vel;
}

//Adds music overrides to the music lists
void UpdateMusicWithOverrides();

void SetupDefaultGameModeSettings();


/*
void EnterBossMode(short type)
{
    if (game_values.gamestate == GS_GAME && game_values.gamemode->gamemode != game_mode_boss_minigame)
    {
        bossgamemode->SetBossType(type);

        game_values.screenfade = 2;
        game_values.screenfadespeed = 2;

        rm->backgroundmusic[0].stop();
        ifsoundonstop(rm->sfx_invinciblemusic);
        ifsoundonstop(rm->sfx_timewarning);
        ifsoundonstop(rm->sfx_slowdownmusic);

        game_values.gamestate = GS_START_GAME;
    }
}*/


#ifdef __EMSCRIPTEN__
void gameloop_frame();
#endif

void gameloop()
{
    SplashScreenState::instance().init();
    GameStateManager::instance().currentState = &SplashScreenState::instance();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(gameloop_frame, 60, 1);
}

void gameloop_frame()
#else
    while (game_values.gamestate != GS_QUIT)
#endif
    {
        FPSLimiter::instance().frameStart();

        GameStateManager::instance().currentState->update();

        FPSLimiter::instance().beforeFlip();

#ifdef USE_SDL2
        SDL_UpdateTexture(screenAsTexture, NULL, screen->pixels, screen->pitch);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, screenAsTexture, NULL, NULL);
        SDL_RenderPresent(renderer);
#else
        SDL_Flip(screen);
#endif

        FPSLimiter::instance().afterFlip();
    }

#ifndef __EMSCRIPTEN__
}
#endif


#ifdef	WIN32
int CALLBACK WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
#ifdef _MSC_VER
    // this will print important debugging information to debug console
    _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_DEBUG );
#endif

    if (strlen(lpCmdLine) > 0)
        RootDataDirectory = lpCmdLine;
#else
// ------ MAIN ------
int main(int argc, char *argv[])
{
    if (argc >= 2)
        RootDataDirectory = argv[1];
#endif


    // this instance will contain the other relevant objects
    smw = new CGame(RootDataDirectory);
	rm = new CResourceManager();
#pragma warning ("delete these or use boost GC shared_ptr")

    g_map = new CMap();
    g_tilesetmanager = new CTilesetManager();

    filterslist = new FiltersList();
    maplist = new MapList(false);

    //TODO: add proper test via size
    if (maplist->IsEmpty()) {
        throw "Empty map directory!";
    }

    skinlist = new SkinList();
    musiclist = new MusicList();
    worldmusiclist = new WorldMusicList();
    soundpacklist = new SoundsList();
    announcerlist = new AnnouncerList();
    tourlist = new TourList();
    worldlist = new WorldList();

    menugraphicspacklist = new GraphicsList();
    worldgraphicspacklist = new GraphicsList();
    gamegraphicspacklist = new GraphicsList();

    printf("-------------------------------------------------------------------------------\n");
    printf(" %s %s\n", TITLESTRING, VERSIONNUMBER);
    printf("-------------------------------------------------------------------------------\n");
    printf("\n---------------- startup ----------------\n");

	gfx_init(smw->ScreenWidth, smw->ScreenHeight, false);		//initialize the graphics (SDL)
    blitdest = screen;

#if	0
    //Comment this in to performance test the preview map loading
    MI_MapField * miMapField = new MI_MapField(&rm->spr_selectfield, 70, 165, "Map", 500, 120);

    for (int k = 0; k < 100; k++) {
        game_values.playerInput.outputControls[3].menu_right.fPressed = true;
        miMapField->SendInput(&game_values.playerInput);
        //printf("Map over-> %s\n", maplist->currentFilename());
    }

    return 0;
#endif

    sfx_init();                     //init the sound system
    net_init();                     //init the networking


    //Joystick-Init
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    joystickcount = (short)SDL_NumJoysticks();
    joysticks = new SDL_Joystick*[joystickcount];

    for (short i = 0; i < joystickcount; i++)
        joysticks[i] = SDL_JoystickOpen(i);

    SDL_JoystickEventState(SDL_ENABLE);

    //currently this only sets the title, not the icon.
    //setting the icon isn't implemented in sdl ->  i'll ask on the mailing list
    char title[128];
    sprintf(title, "%s %s", TITLESTRING, VERSIONNUMBER);
#ifdef USE_SDL2
    SDL_SetWindowTitle(window, title);
    SDL_SetRelativeMouseMode(SDL_TRUE);
#else
    SDL_WM_SetCaption(title, "smw.ico");
    SDL_ShowCursor(SDL_DISABLE);
#endif

    printf("\n---------------- loading ----------------\n");

    game_values.init();

    //Set the default powerup weights for bonus wheel and [?] boxes
    for (short iPreset = 0; iPreset < NUM_POWERUP_PRESETS; iPreset++) {
        for (short iPowerup = 0; iPowerup < NUM_POWERUPS; iPowerup++) {
            g_iCurrentPowerupPresets[iPreset][iPowerup] = g_iDefaultPowerupPresets[iPreset][iPowerup];
        }
    }

    UpdateMusicWithOverrides();

    announcerlist->SetCurrent(0);
    musiclist->SetCurrent(0);
    worldmusiclist->SetCurrent(0);
    menugraphicspacklist->SetCurrent(0);
    worldgraphicspacklist->SetCurrent(0);
    gamegraphicspacklist->SetCurrent(0);
    soundpacklist->SetCurrent(0);

    //set game modes
    gamemodes[0] = new CGM_Classic();
    gamemodes[1] = new CGM_Frag();
    gamemodes[2] = new CGM_TimeLimit();
    gamemodes[3] = new CGM_Jail();
    gamemodes[4] = new CGM_Coins();
    gamemodes[5] = new CGM_Stomp();
    gamemodes[6] = new CGM_Eggs();
    gamemodes[7] = new CGM_CaptureTheFlag();
    gamemodes[8] = new CGM_Chicken();
    gamemodes[9] = new CGM_Tag();
    gamemodes[10] = new CGM_Star();
    gamemodes[11] = new CGM_Domination();
    gamemodes[12] = new CGM_KingOfTheHill();
    gamemodes[13] = new CGM_Race();
    gamemodes[14] = new CGM_Owned();
    gamemodes[15] = new CGM_Frenzy();
    gamemodes[16] = new CGM_Survival();
    gamemodes[17] = new CGM_Greed();
    gamemodes[18] = new CGM_Health();
    gamemodes[19] = new CGM_Collection();
    gamemodes[20] = new CGM_Chase();
    gamemodes[21] = new CGM_ShyGuyTag();

    currentgamemode = 0;
    game_values.gamemode = gamemodes[currentgamemode];

    //Special modes
    bonushousemode = new CGM_Bonus();
    pipegamemode = new CGM_Pipe_MiniGame();
    bossgamemode = new CGM_Boss_MiniGame();
    boxesgamemode = new CGM_Boxes_MiniGame();

    game_values.SetupDefaultGameModeSettings();
	game_values.ReadBinaryConfig();

    //Assign the powerup weights to the selected preset
    for (short iPowerup = 0; iPowerup < NUM_POWERUPS; iPowerup++) {
        game_values.powerupweights[iPowerup] = g_iCurrentPowerupPresets[game_values.poweruppreset][iPowerup];
    }

#ifdef _XBOX
    gfx_setresolution(smw->ScreenWidth, smw->ScreenHeight, false); //Sets flicker filter
    SDL_SetHardwareFilter(game_values.hardwarefilter);
    blitdest = screen;
#else
    if (game_values.fullscreen) {
        gfx_setresolution(smw->ScreenWidth, smw->ScreenHeight, true);
        blitdest = screen;
    }
#endif

    //Calculate the swirl spawn effect locations
    float spawnradius = 100.0f;
    float spawnangle = 0.0f;

    for (short i = 0; i < 25; i++) {
        g_iSwirlSpawnLocations[0][0][i] = (short)(spawnradius * cos(spawnangle));
        g_iSwirlSpawnLocations[0][1][i] = (short)(spawnradius * sin(spawnangle));

        float angle = spawnangle + HALF_PI;
        g_iSwirlSpawnLocations[1][0][i] = (short)(spawnradius * cos(angle));
        g_iSwirlSpawnLocations[1][1][i] = (short)(spawnradius * sin(angle));

        angle = spawnangle + PI;
        g_iSwirlSpawnLocations[2][0][i] = (short)(spawnradius * cos(angle));
        g_iSwirlSpawnLocations[2][1][i] = (short)(spawnradius * sin(angle));

        angle = spawnangle + THREE_HALF_PI;
        g_iSwirlSpawnLocations[3][0][i] = (short)(spawnradius * cos(angle));
        g_iSwirlSpawnLocations[3][1][i] = (short)(spawnradius * sin(angle));

        spawnradius -= 4.0f;
        spawnangle += 0.1f;
    }

    //Load the gfx color palette
    gfx_loadpalette();

    //Call to setup input optimization
    game_values.playerInput.CheckIfMouseUsed();

    srand((unsigned int)time(NULL));
/*
    bool fLoadOK = LoadAndSplashScreenState();

    if (!fLoadOK) {
        printf("\n---------------- EXIT DURING LOADING ----------------\n\n");
        sfx_close();
        gfx_close();
        net_close();
        return 0;
    }
*/


    //**********************************************************


    gameloop(); // all the game logic happens here


    //**********************************************************


    printf("\n---------------- shutdown ----------------\n");

    for (short i = 0; i < GAMEMODE_LAST; i++)
        delete gamemodes[i];

#ifdef _XBOX
    for (i = 0; i < joystickcount; i++)
        SDL_JoystickClose(joysticks[i]);

    delete[] joysticks;
#endif

    sfx_close();
    gfx_close();
    net_close();

    //Delete player skins
    for (short k = 0; k < MAX_PLAYERS; k++) {
        for (short j = 0; j < PGFX_LAST; j++) {
            delete rm->spr_player[k][j];
            delete rm->spr_shyguy[k][j];
            delete rm->spr_chocobo[k][j];
            delete rm->spr_bobomb[k][j];
        }

        delete [] rm->spr_player[k];
        delete [] rm->spr_shyguy[k];
        delete [] rm->spr_chocobo[k];
        delete [] rm->spr_bobomb[k];

        delete score[k];
    }

    delete [] game_values.pfFilters;
    delete [] game_values.piFilterIcons;

//Return to dash on xbox
#ifdef _XBOX
    LD_LAUNCH_DASHBOARD LaunchData = { XLD_LAUNCH_DASHBOARD_MAIN_MENU };
    XLaunchNewImage( NULL, (LAUNCH_DATA*)&LaunchData );
#endif

	// release all resources
	delete rm;
    delete smw;

    return 0;
}


/*
#ifdef _XBOX

void reconnectjoysticks()
{
    for (int i = 0; i < joystickcount; i++)
        SDL_JoystickClose(joysticks[i]);

    delete[] joysticks;

    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);

    joystickcount = SDL_NumJoysticks();
    joysticks = new SDL_Joystick*[joystickcount];

    for (i = 0; i < joystickcount; i++)
        joysticks[i] = SDL_JoystickOpen(i);

    SDL_JoystickEventState(SDL_ENABLE);
}

#endif
*/


void UpdateMusicWithOverrides()
{
    FILE * file = fopen(convertPath("music/Overrides.txt").c_str(), "r");

    if (!file)
        return;

    short iAddToCategory = 0;
    char szBuffer[1024];
    while (fgets(szBuffer, 1024, file)) {
        //Ignore comment lines
        if (szBuffer[0] == '#' || szBuffer[0] == '\n' || szBuffer[0] == '\r' || szBuffer[0] == ' ' || szBuffer[0] == '\t')
            continue;

        //Chop off line ending
        int stringLength = strlen(szBuffer);
        for (short k = 0; k < stringLength; k++) {
            if (szBuffer[k] == '\r' || szBuffer[k] == '\n') {
                szBuffer[k] = '\0';
                break;
            }
        }

        //If we found a category header
        if (szBuffer[0] == '[') {
            if (!strCiCompare(szBuffer, "[maps]"))
                iAddToCategory = 1;
            else if (!strCiCompare(szBuffer, "[worlds]"))
                iAddToCategory = 2;

            continue;
        }

        //If we're not in a category, ignore this line
        if (iAddToCategory == 0)
            continue;

        char * pszName = strtok(szBuffer, ",\n");

        if (!pszName)
            continue;

        if (iAddToCategory == 1) {
            MapMusicOverride * override = new MapMusicOverride();

            override->mapname = pszName;

            char * pszMusic = strtok(NULL, ",\n");
            while (pszMusic) {
                std::string sPath = convertPath(pszMusic);

                if (File_Exists(sPath.c_str())) {
                    override->songs.push_back(sPath);
                }

                pszMusic = strtok(NULL, ",\n");
            }

            //Don't add overrides that have no songs
            if (override->songs.size() == 0) {
                delete override;
                continue;
            }

            mapmusicoverrides.push_back(override);
        } else if (iAddToCategory == 2) {
            WorldMusicOverride * override = new WorldMusicOverride();

            override->worldname = pszName;

            char * pszMusic = strtok(NULL, ",\n");
            if (pszMusic) {
                std::string sPath = convertPath(pszMusic);

                if (File_Exists(sPath.c_str())) {
                    override->song = sPath;
                    worldmusicoverrides.push_back(override);
                }
            }
        }
    }

    musiclist->UpdateEntriesWithOverrides();
    worldmusiclist->UpdateEntriesWithOverrides();

    fclose(file);
}
