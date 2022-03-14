#pragma once
#include "Common.h"
#include "Log.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#define SEP ": "
#define HTTP_VERSION "HTTP/1.0"
#define LINE_END "\r\n"   //构建http响应的时候的换行符

#define WEB_ROOT "wwwroot"
#define HOME_PAGE "index.html"

#define OK 200
#define NOT_FOUND 404

static std::string Code2Desc(int code)
{
  switch(code)
  {
    case OK:
      return "OK";
    case NOT_FOUND:
      return "NOT FOUND";
    default:
      break;
  }
  return "";
}

//文件后缀转Content-Type
static std::string MapSuffix2Type(const std::string& suffix)
{
  static std::unordered_map<std::string, std::string> SuffixConvertTable{
    {".html", "text/html"},
    {".css", "test/css"},
    {".js", "application/javascript"},
    {".xml", "application/xml"}
  };
  auto it = SuffixConvertTable.find(suffix);
  if(it != SuffixConvertTable.end())
  {
    return it->second;
  }
  return "text/html"; //没找到一律按照html处理
}

//HTTP请求结构
struct HttpRequest
{
  std::string request_line;
  std::vector<std::string> request_header;
  std::string request_blank;
  std::string request_body;

  //切割请求行, 并将内容存储起来
  std::string _method;
  std::string _uri;
  std::string _version;

  //URI的组成
  std::string _path;
  std::string _query_string;

  std::unordered_map<std::string, std::string> header_map;  //请求报头的kv键值对 

  bool cgi;
};

//HTTP响应结构
struct HttpResponse
{
  std::string response_line;
  std::vector<std::string> response_header;
  std::string response_blank = LINE_END;  //"\r\n"
  std::string response_body;

  std::string _version = HTTP_VERSION;
  int _status_code = OK; //状态码
  //std::string _status_desc; //状态码描述
  
  int fd;   //请求的资源文件,所使用的文件描述符
  int size; //所打开的文件资源的大小
};


//IO通信类: 读取请求; 分析请求; 处理请求; 构建响应;
class EndPoint
{
  public:
    EndPoint(int sock)
      :sockfd(sock)
    {}

    void RecvRequest()
    {
      //按行读取
      RecvHttpLine();
      RecvHttpHeader();
      RecvHttpBody();
    }

    //构建响应的步骤:
    //1. 分析请求(请求行的拆分 + 请求报头的录入)
    //2. 分析请求行的请求方法GET/POST, 如果是GET方法, 对URI的进行进一步的判断; POST方法...
    //3. 分析请求行的URI, 将URI进行拆分, 然后进一步判断资源是否存在, 最终把资源存储到响应报文中
    //4. 组合与填充响应结构体的字段
    void BuildResponse()
    {
      AnalyzeRequest(); //1.先分析请求
      //2. 分析请求方法
      if(http_request._method == "GET")
      {
        //3. 判断URI
        auto& path = http_request._path;
        auto& query_string = http_request._query_string;
        bool with_arg = Util::CutString(http_request._uri, path, query_string, "?");

        //path携带参数, 需要CGI处理
        if(with_arg)
          http_request.cgi = true;
        else 
          path = http_request._uri; //path不带参数, 需要在这里设置一下path = uri; 因为CutString在找不到分隔符时, 不会对输出型参数处理的
        

        //给path添加WEB_ROOT, 添加根目录的名称
        std::string root = WEB_ROOT;
        root += path;
        path.swap(root);
        
        //判断path是否合法: 
        //1. 资源是否存在(path最后以'/'结尾的需要先特殊处理, 比如根目录"/") 
        //2. 资源是否可访问(目录、可执行文件)
        if(path[path.size() - 1] == '/')
        {
          path += HOME_PAGE;
        }

        //判断资源是否存在:
        struct stat st;
        
        //浏览器会同时请求favicon资源, 此时资源可能不存在
        //cout << path << endl;
        if(stat(path.c_str(), &st) == 0)
        {
          http_response.size = st.st_size;  //保存所打开文件的大小(后面sendfile时需要使用这个参数)
          //判断该资源是否是目录, 如果是目录, 需要让它去访问该目录下的默认页
          if(S_ISDIR(st.st_mode))
          {
            path += "/";  //这种情况:/a/b + "/HOME_PAGE"
            path += HOME_PAGE; 
            stat(path.c_str(), &st);  //刷新st, 以便获取HOME_PAGE的字节数
            http_response.size = st.st_size;  //更新资源的大小
          }
          else if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) //是否具有可执行权限
          {
            http_request.cgi = true;
            goto END;
          }

        }
        else//资源不存在 
        {
          http_response._status_code = NOT_FOUND; //404 NOT FOUND
          LOG(ERROR, "resoure not exist!");
          goto END;
        }
      }
      else if(http_request._method == "POST")
      {
        http_request.cgi = true;
        goto END;
      }
      else 
      {
        //DO NOTHING
      }

      //走到这里的, 要么是POST方法, 要么是请求资源是正确的
      if(http_request.cgi == true)
      {
        ProcessCGI();
      }
      else 
      {
        //在ProcessNonCGI函数内, 我们来构建响应并且进行字段的填充
        //cout << "[" << http_request._path << "]" << " " << "[" << http_request._query_string << "]" << endl;
        http_response._status_code =  ProcessNonCGI();  //进行非CGI处理: 打开资源文件
      }

END:
      return;
    }

    void SendResponse()
    {
      //cout << http_response.response_line;
       

      send(sockfd, http_response.response_line.c_str(), http_response.response_line.size(), 0); 
      for(auto& it : http_response.response_header)
      {
        send(sockfd, it.c_str(), it.size(), 0);
        //cout << it;
      }
      
      send(sockfd, http_response.response_blank.c_str(), http_response.response_blank.size(), 0);
      sendfile(sockfd, http_response.fd, nullptr, http_response.size);
      close(http_response.fd);
    }
  private:
    //读取请求行, 并将请求行存储到http_line中 
    void RecvHttpLine()
    {
      std::string& line = http_request.request_line;
      if(Util::ReadLine(sockfd, line) > 0)
      {
        line.resize(line.size() - 1); //删除'\n'
      }
      else 
      {
        LOG(ERROR, "ReadLine failed!");
      }
    }
    
    void RecvHttpHeader()
    {
      std::string line;
      while(1)
      {
        line.clear(); 
        if(Util::ReadLine(sockfd, line) > 0)
        {
          if(line == "\n")
            break;
          line.resize(line.size() - 1); //删除'\n'
          http_request.request_header.push_back(line);
        }
        else 
        {

        }
      }
#ifdef DEBUG
      for(auto& e : http_request.request_header)
        cout << e << endl;
#endif
      http_request.request_blank = line;  //空行
    }

    //1. 首先判断请求方法是否为POST, 这里就认定GET方法无请求正文;
    //2. 其次需要Body-Length字段
    //读取请求正文前, 需要先对请求报头的内容进行分析和拆解(存入到header_map中), 然后利用Body-Length字段确定大小
    void RecvHttpBody()
    {
      if(IsNeedHttpBody())
      {
        auto& header_map = http_request.header_map;
        if(header_map.find("Content-Length") != header_map.end())
        {
          char buff[stoi(header_map["Content-Length"])];
          recv(sockfd, buff, sizeof(buff),0);
          http_request.request_body = buff;
        }
      }
    }

    inline bool IsNeedHttpBody()
    {
      if(http_request._method == "POST")
        return true;
      else  //GET没正文
        return false;
    }

    //分析请求(请求行+请求报头)
    void AnalyzeRequest()
    {
      AnalyzeRequestLine();   //将请求行拆分到各个字段中
      AnalyzeRequestHeader(); //将报头存储到header_map中
    }

    //将请求行拆分, 分别存储到3个字段当中
    void AnalyzeRequestLine()
    {
      auto& line = http_request.request_line;
      std::stringstream ss(line);
      ss >> http_request._method >> http_request._uri >> http_request._version;
      
      std::string& method = http_request._method;
      std::transform(method.begin(), method.end(), method.begin(), ::toupper); //保证请求方法的URI一定为大写的
    }

    void AnalyzeRequestHeader()
    {
      std::string s1, s2;
      for(size_t i = 0; i < http_request.request_header.size(); ++i)
      {
        if(Util::CutString(http_request.request_header[i], s1, s2, SEP))
        {
          http_request.header_map.insert({s1, s2}); //将header的内容全部存入header_map
        }
      }
    }


    int ProcessNonCGI()
    {
      //构建响应行
      http_response.response_line = HTTP_VERSION;
      http_response.response_line += " ";
      http_response.response_line += std::to_string(http_response._status_code);
      http_response.response_line += " ";
      http_response.response_line += Code2Desc(http_response._status_code);
      http_response.response_line += LINE_END;

      //构建响应报头
      size_t pos = http_request._path.rfind(".");
      if(pos != std::string::npos)
      {
        std::string suffix = http_request._path.substr(pos);
        http_response.response_header.emplace_back(std::string("Content-Length") + SEP + std::to_string(http_response.size) + LINE_END);
        http_response.response_header.emplace_back(std::string("Content-Type") + SEP + MapSuffix2Type(suffix) + LINE_END); 
      }
      else 
      {
        
        return NOT_FOUND;
      }

      //打开文件, 读取文件的内容, 将其作为http_response的body
      //优化: 我们在这里读取文件的本质就是将文件的内容从内核缓冲区拷贝到用户缓冲区(因为文件存在外设, 需要调用系统调用接口read来读去)
      //      然而我们最终还需要把http_response的响应body从用户缓冲区发送到内核缓冲区, 这个过程有些浪费
      //    因此我们可以借助一个系统调用函数sendfile, 它能够直接将内容不经由用户空间, 在内核空间进行转发, 以此提高效率!
      http_response.fd = open(http_request._path.c_str(), O_RDONLY);
      if(http_response.fd > 0)
      {
        return OK;
      }
      return NOT_FOUND;
    }

    int ProcessCGI()
    {
      return 0;
    }

  private:
    int sockfd;
    HttpRequest http_request;
    HttpResponse http_response;
};

//线程的入口
class Entrance
{
  public:
    static void* HandlerRequest(void* arg)
    {
      LOG(INFO, "[[Entrance BEGIN:]]");

      int* sockfd = reinterpret_cast<int*>(arg);
      int sock = *sockfd;
      delete sockfd;  //可以释放了

      EndPoint* ep = new EndPoint(sock);
      ep->RecvRequest();
      ep->BuildResponse();
      ep->SendResponse();

      LOG(INFO, "[[Entrance END:]]");

      delete ep;
      return nullptr;
    }
};
