//
// Created by root on 1/8/17.
//

#ifndef DRAW_CUBE_TRACE_TIME_H
#define DRAW_CUBE_TRACE_TIME_H

#include <string>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define BeginTrace(func_name) \
      RecordCurrentTime(func_name);
#define EndTrace(func_name) \
      __android_log_print(ANDROID_LOG_DEBUG, "TraceTime",               \
                        "%s,%ld,line:%d", func_name, getConsumeWalkClockTime(), \
                         __LINE__);                                             \
      output(func_name, getConsumeWalkClockTime(), __LINE__);
//      __android_log_print(ANDROID_LOG_DEBUG, "TraceTime",               \
//                        "Time consuming: %ld, Function Name: %s, File[%s], line[%d]", getConsumeWalkClockTime(), func_name, \
//                        __FILENAME__, __LINE__);

// Vulkan call wrapper, no return value
#define CALL_VK_CHECK(func)                                                 \
  BeginTrace(getFunctionName(#func));                                 \
  if (VK_SUCCESS != (func)) {                                         \
    __android_log_print(ANDROID_LOG_ERROR, "Tutorial ",               \
                        "Vulkan error. File[%s], line[%d]", __FILE__, \
                        __LINE__);                                    \
    assert(false);                                                    \
  }                                                                   \
  EndTrace(getFunctionName(#func));

#define CALL_VK(func)                                                 \
  BeginTrace(getFunctionName(#func));                                 \
  (func);                                                             \
  EndTrace(getFunctionName(#func));

// Vulkan call with return value
#define CALL_VK_RET(func) ({ \
    BeginTrace(getFunctionName(#func)); \
    VkResult res = (func);   \
    EndTrace(getFunctionName(#func)); \
    res;                     \
})

using namespace std;

extern std::string android_file_directory;

extern int currentMultiple;

long int getCurrentTimeMillis();

void RecordCurrentTime(std::string func_name);
long int getConsumeWalkClockTime();

void EndTraceA(std::string func_name);

long int getConsumeProcessorTime();
void RecordProcessorClock(std::string func_name);

const char* getFunctionName(std::string func);

std::string long2string(long number);
std::string getFileName();
void initFile();
void closeFile();
void output(std::string funcName, int timeConsuming, int codeLineNum);

bool saveFPS(int fps);
void calcAvgAndDeviation(int arr[], int size);
void saveAvgAndDeviation();

void setFileDirectory(string fileDir);
void setCurrentMultiple(int multiple);

void startTimer();
extern void timerCall();
void stopTimer();

#endif //TUTORIAL04_FIRST_WINDOW_VULKANFUNCNAME_H


