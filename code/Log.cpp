#include "Log.h"
#include <iostream>

Log::Log() {

}

Log::~Log() {

}

void Log::init(const char *fileName, int logLevel) {
    this->pwd = fileName;
    this->level = logLevel;
}

void Log::output(const char *context, int logLevel) {
    if (logLevel < this->level)
        return;
    if (logLevel == 0)
        std::cout << "Info:" << context << std::endl;
    if (logLevel == 1)
        std::cout << "Warning:" << context << std::endl;
    if (logLevel == 2)
        std::cout << "Error:" << context << std::endl;
}