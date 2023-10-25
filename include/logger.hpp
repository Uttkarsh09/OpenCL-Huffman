#pragma once
#ifndef LOGGER
#define LOGGER
#include <iostream>
#include <stdarg.h>

using namespace std;

class MyLogger{
public:
	enum LogLevel{
		LOG_OFF,
		LOG_CRITICAL,
		LOG_ERROR,
		LOG_WARNING,
		LOG_INFO,
		LOG_DEBUG,
		LOG_ALL
	};

	void operator=(const MyLogger&) = delete;
	
	/// @brief Get the logger object
	/// @param value log file path
	/// @return `MyLogger`
	static MyLogger *GetInstance(const string value);
	
	/// @brief 
	/// @param level - Log level
	/// @param format - C string to be logged which can contain embedded fromat specifiers that are replaced by the values specified in subsequent additional arguments
	/// @param (optional) variable names (if) used in the format string
	void logIt(LogLevel level, const char * format, ...);

	void setLogLevel(LogLevel level);
	string getLogLevelName(LogLevel level);
	void deleteInstance();
	
	MyLogger() = delete;
	

protected:
	static MyLogger* logger_;
	LogLevel logLevel;
	string value_;
	FILE *fptr;
	
	// ? inaccessable constructor
	MyLogger(const string file_name){
		fptr = fopen(file_name.c_str(), "w");
		logLevel = LogLevel::LOG_ALL;

		if(fptr == NULL){
			printf("ERROR - Cannot open file %s\n", file_name.c_str());
			exit(EXIT_FAILURE);
		}
	}
	
};

#endif // LOGGER
