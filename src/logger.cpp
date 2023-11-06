#include "logger.hpp"

MyLogger* MyLogger::logger_= nullptr;

MyLogger *MyLogger::GetInstance(const string file_name){
	if(logger_ == nullptr){
		logger_ = new MyLogger(file_name);
	}
	return logger_;
}

void MyLogger::deleteInstance(){
	fprintf(fptr, "~~~ Log File Closed ~~~");
	fclose(fptr);
	delete logger_;
	fptr = nullptr;
	logger_ = nullptr;
}


string MyLogger::getLogLevelName(LogLevel level){
	switch(level){
		case LogLevel::LOG_OFF: return "OFF";
		case LogLevel::LOG_CRITICAL: return "CRITICAL";
		case LogLevel::LOG_ERROR: return "ERROR";
		case LogLevel::LOG_WARNING: return "WARNING";
		case LogLevel::LOG_INFO: return "INFO";
		case LogLevel::LOG_DEBUG: return "DEBUG";
		case LogLevel::LOG_ALL: return "ALL";
		default: return "can't happen";
	}
}

void MyLogger::logIt(LogLevel level, const char * format, ...){
	if(logLevel < level) return;
	
	string levelName = getLogLevelName(level);
	va_list args;

	va_start(args, format);
	fprintf(fptr, "%-9s: ", levelName.c_str());

	vfprintf(fptr, format, args);
	fprintf(fptr, "\n");
	va_end(args);
	fflush(fptr);
}

void MyLogger::setLogLevel(LogLevel level){
	logLevel = level;
}
