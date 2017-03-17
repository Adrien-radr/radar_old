#include "common.h"

#include <stdlib.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <time.h>

bool Log::log_opened = false;
double Log::log_time = 0.0;
std::string Log::log_name = "Radar_engine.log";
std::stringstream Log::log_ss = std::stringstream();
std::ofstream Log::log_file = std::ofstream();

#if 0
#ifdef RADAR_WIN32
void Sleep
#else
#ifdef RADAR_UNIX
#endif
#endif
#endif

void get_date_time( char *buffer, u32 bsize, const char *fmt )
{
	time_t ti = time( NULL );
	struct tm *lt = localtime( &ti );
	strftime( buffer, bsize, fmt, lt );
}

void Log::Init()
{
	if ( !log_opened )
	{
		log_opened = true;
		log_file = std::ofstream( log_name );
		if ( !log_file.is_open() )
		{
			std::cout << "Error while opening log " << log_name << std::endl;
			exit( 1 );
		}
		log_ss.str( std::string() );
		log_ss.setf( std::ios::fixed, std::ios::floatfield );
		log_ss.precision( 2 );

		char da[64], ti[64];
		get_date_time( da, 64, DEFAULT_DATE_FMT );
		get_date_time( ti, 64, DEFAULT_TIME_FMT );
		std::string dastr( da );
		std::string tistr( ti );
		std::string dt = dastr + " - " + tistr;
		LogInfo( "\t    Radar Log v", RADAR_MAJOR, ".", RADAR_MINOR, ".", RADAR_PATCH );
		LogInfo( "\t", dt.c_str() );
		LogInfo( "================================" );
	}
}

void Log::Close()
{
	if ( log_opened )
	{
		log_opened = false;
		log_file.close();
	}
}

double Log::get_engine_time()
{
	return glfwGetTime();
}
