#pragma once 
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

using std::cout;
using std::endl;

//工具类
class Util
{
  public:
    //按行读取  ---  换行符统一处理为'\n'
    static int ReadLine(int sockfd, std::string& out)
    {
      char ch = 0;
      while(true)
      {
        ssize_t s = recv(sockfd, &ch, 1, 0);
        if(s > 0)
        {
          if(ch == '\n')
          {
            out += ch;
            break;
          }
          else if(ch == '\r')
          {
            //判断是'\r'还是"\r\n"
            recv(sockfd, &ch, 1, MSG_PEEK);  //MSG_PEEK参数:窥探下一个字符, 不会真的读取出来 
            if(ch == '\n')
            {
              recv(sockfd, &ch, 1, 0);  //如果是"\r\n"的换行符, 直接把下一个字符读出来
            }
            out += '\n';  //这里我们统一把换行符处理为'\n'
            break;
          }
          else//正常字符
          {
            out += ch;
          }
        }
        else if(s == 0)
        {
          return 0;
        }
        else//s < 0 
        {
          return -1;
        }
      }
      return out.size();
    }

    static inline bool CutString(const std::string& s, std::string& s1, std::string& s2, std::string sep)
    {
      size_t pos = s.find(sep);
      if(pos != std::string::npos)
      {
        s1 = s.substr(0, pos);
        s2 = s.substr(pos + sep.size());
        return true;
      }
      return false;
    }
};
