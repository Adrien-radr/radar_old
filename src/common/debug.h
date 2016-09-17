#pragma once

// memory Leaks debugging
#ifdef _DEBUG
#include <vld.h>
#endif


#include <iostream>
#include <sstream>
#include <fstream>

class Log {
public:
	static void Init();
	static void Close();


	template <typename... M>
	static void Err(const char *file, int line, const M &...msg_list) {
		std::string path(file);
#ifdef _WIN32
		std::string filename = path.substr(path.find_last_of('\\')+1);
#else
		std::string filename = path.substr(path.find_last_of('/')+1);
#endif
		log_ss.precision(2);
		LogMsg("<", get_engine_time(), "> EE (", filename, ":", line, ") ");
		log_ss.precision(4);
		LogMsg( msg_list..., "\n");
	}

	template <typename... M>
	static void Info(const char *file, int line, const M &...msg_list) {
		std::string path(file);
#ifdef _WIN32
		std::string filename = path.substr(path.find_last_of('\\')+1);
#else
		std::string filename = path.substr(path.find_last_of('/')+1);
#endif
		log_ss.precision(2);
		LogMsg("<", get_engine_time(), "> II (", filename, ":", line, ") ");
		log_ss.precision(4);
		LogMsg(msg_list..., "\n");
	}

private:
	template <typename T>
	static void LogMsg(const T& value) {
		log_ss << value;// << std::endl;

		std::cout << log_ss.str();
		log_file << log_ss.str();

		log_file.flush();
		log_ss.str(std::string());
	}

	template <typename U, typename... T>
	static void LogMsg(const U& head, const T&... tail)
	{
		log_ss << head;
		LogMsg(tail...);
	}

	static double get_engine_time();

private:
	static bool log_opened;
	static std::string log_name;

	static std::ofstream log_file;
	static std::stringstream log_ss;
	static double log_time;
};

#ifdef _DEBUG
#define D(x) x
#else
#define D(x)
#endif


#define LogInfo(...) do {\
	Log::Info(__FILE__, __LINE__, ##__VA_ARGS__);\
} while(0)

#define LogErr(...) do {\
	Log::Err(__FILE__, __LINE__, ##__VA_ARGS__);\
} while(0)

#define LogDebug(...) do {\
	D(LogInfo("[DEBUG] ", ##__VA_ARGS__);)\
} while(0)


#define Assert(cond) \
{\
D(\
	do {\
		if(!(cond)) Log::Err(__FILE__, __LINE__, "Assertion \"", #cond, "\" failed.");\
		} while(0);\
)\
}
