#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <stack>

#include "BitmapManager.h"
#include "DataManager.h"
#include "FontManager.h"
#include "GUIManager.h"
#include "Logger.h"
#include "Path.h"
#include "State.h"
#include "SoundManager.h"

#if defined(_MSC_VER)
#include "stdint.h"
#include <direct.h>
#define mkdir(A,B)	_mkdir(A)
#elif defined(_WIN32)
#include <io.h>
#define mkdir(A,B)	mkdir(A)
#else
#include <unistd.h>
#include <errno.h>
#endif

namespace OT
{
	class Application
	{
		friend class State;
		friend class WindowRender;
		
	public:
		Application(int argc, char * argv[]);
		
		Path getPath() const { return path; }
		
		Logger logger;
		
		sf::RenderWindow window;
		sf::VideoMode videoMode;
		
		DataManager   data;
		GUIManager    gui;
		GUI *         rootGUI;
		BitmapManager bitmaps;
		FontManager   fonts;
		SoundManager  sounds;
		
		int run();
		
	private:
		Path path;
		void setPath(const Path & p);
		
		bool running;
		int exitCode;
		
		void init();
		void loop();
		void cleanup();
		
		std::stack<State *> states;
		void pushState(State * state);
		void popState();

		bool dumpResources;
		Path dumpResourcesPath;
	};

	class WindowRender {
	public:
		static void render(void * userData);
	};

	extern Application * App;
}
