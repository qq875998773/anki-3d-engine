#include <GL/glew.h>
#include <sstream>
#include <SDL/SDL.h>
#include "App.h"
#include "Scene.h"

bool App::isCreated = false;

//=====================================================================================================================================
// Constructor                                                                                                                        =
//=====================================================================================================================================
App::App()
{
	if( isCreated )
		FATAL( "You cannot create a second App instance" )

	isCreated = true;

	scene = new Scene;
	activeCam = NULL;

	windowW = 1280;
	windowH = 720;
	/*windowW = 1440;
	windowH = 900;*/


	timerTick = 1000/40; // in ms. 1000/Hz
	time = 0;
}


//=====================================================================================================================================
// initWindow                                                                                                                         =
//=====================================================================================================================================
void App::initWindow()
{
	INFO( "SDL window initializing..." );
	SDL_Init( SDL_INIT_VIDEO );

	// print driver name
	char charBuff [256];
	if( SDL_VideoDriverName(charBuff, sizeof(charBuff)) != NULL )
	{
		INFO( "Video driver name: " << charBuff );
	}
	else
	{
		ERROR( "Failed to obtain the video driver name" );
	}


	// get desktop size
	const SDL_VideoInfo* info = SDL_GetVideoInfo();
	desktopW = info->current_w;
	desktopH = info->current_h;

	// the icon
	iconImage = SDL_LoadBMP("gfx/icon.bmp");
	if( iconImage == NULL )
	{
		ERROR( "Cannot load window icon" );
	}
	else
	{
		Uint32 colorkey = SDL_MapRGB( iconImage->format, 255, 0, 255 );
		SDL_SetColorKey( iconImage, SDL_SRCCOLORKEY, colorkey );
		SDL_WM_SetIcon( iconImage, NULL );
	}

	// set GL attribs
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 8 ); // WARNING: Set this only in deferred shading
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );

	// set the surface
	mainSurf = SDL_SetVideoMode( windowW, windowH, 24, SDL_HWSURFACE | SDL_OPENGL );

	// move the window
#ifdef WIN32
	SDL_SysWMinfo i;
	SDL_VERSION( &i.version );
	if( SDL_GetWMInfo(&i) )
	{
		HWND hwnd = i.window;
		SetWindowPos( hwnd, HWND_TOP, 10, 25, w, h, NULL );
	}
#endif

	SDL_WM_SetCaption( "AnKi Engine", NULL );


	INFO( "SDL window initialization ends" );
}


//=====================================================================================================================================
// togleFullScreen                                                                                                                    =
//=====================================================================================================================================
void App::togleFullScreen()
{
	SDL_WM_ToggleFullScreen( mainSurf );
}


//=====================================================================================================================================
// quitApp                                                                                                                            =
//=====================================================================================================================================
void App::quitApp( int code )
{
	SDL_FreeSurface( mainSurf );
	SDL_Quit();
	exit(code);
}


//=====================================================================================================================================
// waitForNextFrame                                                                                                                   =
//=====================================================================================================================================
void App::waitForNextFrame()
{
	uint now = SDL_GetTicks();

	if( now - time < timerTick )
	{
		// the new time after the SDL_Delay will be...
		time += timerTick;
		// sleep a little
		SDL_Delay( time - now);
	}
	else
		time = now;

}


//=====================================================================================================================================
// printAppInfo                                                                                                                       =
//=====================================================================================================================================
void App::printAppInfo()
{
	stringstream msg;
	msg << "App info: ";
	#if defined( _DEBUG_ )
		msg << "Debug, ";
	#else
		msg << "Release, ";
	#endif
	msg << "GLEW " << glewGetString(GLEW_VERSION) << ", ";
	const SDL_version* v = SDL_Linked_Version();
	msg << "SDL " << int(v->major) << '.' << int(v->minor) << '.' << int(v->patch);

	INFO( msg.str() )
}


//=====================================================================================================================================
// getTicks                                                                                                                           =
//=====================================================================================================================================
uint App::getTicks()
{
	return SDL_GetTicks();
}

