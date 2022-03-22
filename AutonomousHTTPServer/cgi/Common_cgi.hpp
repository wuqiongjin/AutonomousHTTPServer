#pragma once 
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <string>
using namespace std;

//query_string输出型参数
bool GetQueryString(std::string& query_string)
{
  std::string method = getenv("METHOD");
  if("GET" == method){
    cerr << "[GET]: CGI is Running!" << endl;
    query_string = getenv("QUERY_STRING");
  }
  else if("POST" == method){
    cerr << "[POST]: CGI is Running!" << endl;
    int content_length = atoi(getenv("CONTENT_LENGTH"));
    char ch;
    while(content_length --){
      ssize_t s = read(0, &ch, 1);
      if(s > 0)
        query_string += ch;
      else if(s  == 0)
        break;
      else
        return false;
    }
  }
  return true; 
}

void CutString(std::string& query_string, std::string& s1, std::string& s2, const std::string& sep)
{
  int pos = query_string.find(sep);
  s1 = query_string.substr(0, pos);
  s2 = query_string.substr(pos + sep.size());
}
