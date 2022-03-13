#pragma once 
#include "Common.h"

//日志级别
#define INFO    1 //显示普通信息
#define WARNING 2 //警告
#define ERROR   3 //错误
#define FATAL   4 //致命错误

//日志级别必须用#define, 不然LOG宏这里无法再预处理阶段被替换掉
#define LOG(level, msg) Log(#level, msg, __FILE__, __LINE__)  //#level表示

//日志打印的形式 [日志级别][时间戳][日志信息][当前文件名称][当前行]
void Log(std::string level, std::string msg, std::string filename, int line)
{
  cout << "[" << level << "]" << "[" << time(nullptr) << "]" << "[" << msg << "]" \
    << "[Filename:" << filename << "]" << "[line:" << line << "]" << endl;
}
