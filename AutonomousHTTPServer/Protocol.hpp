#pragma once
#include "Common.h"
#include "Log.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#define SEP ": "
#define HTTP_VERSION "HTTP/1.0" //基于短链接
#define LINE_END "\r\n"   //构建http响应的时候的换行符

#define WEB_ROOT "wwwroot"
#define HOME_PAGE "index.html"

#define OK 200
#define BAD_REQUEST 400
#define NOT_FOUND 404
#define SERVER_ERROR 500

#define PAGE_404 "404.html"
#define PAGE_400 "400.html"
#define PAGE_500 "500.html"

static std::string Code2Desc(int code)
{
  switch(code)
  {
    case OK:
      return "OK";
    case BAD_REQUEST:
      return "Bad Request";
    case NOT_FOUND:
      return "Not Found";
    case SERVER_ERROR:
      return "Internal Server Error";
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
  std::string request_blank = LINE_END;
  std::string request_body;

  //切割请求行, 并将内容存储起来
  std::string _method;
  std::string _uri;
  std::string _version;

  //URI的组成
  std::string _path;
  std::string _query_string;

  int content_length = 0;
  std::string suffix = ".html";

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
  
  int fd = -1;   //请求的资源文件,所使用的文件描述符
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
      if(RecvHttpLine() || RecvHttpHeader()){
        return; 
      }
      AnalyzeRequest();
      RecvRequestBody(); //它里面也涉及了read_stop的判断, 在RecvRequest函数的后面会继续判断是否出现读错误
    }

    bool IsReadStop()
    {
      return read_stop;
    }
    
  private:
    //分析请求(请求行+请求报头)
    void AnalyzeRequest()
    {
      AnalyzeRequestLine();   //将请求行拆分到各个字段中
      AnalyzeRequestHeader(); //将报头存储到header_map中
    }

  public:
    //构建响应的步骤:
    //1. 分析请求行的请求方法GET/POST, 如果是GET方法, 对URI的进行进一步的判断; POST方法...
    //2. 分析请求行的URI, 将URI进行拆分, 然后进一步判断资源是否存在, 最终把资源存储到响应报文中
    //3. 组合与填充响应结构体的字段
    void BuildResponse()
    {
      std::string root = WEB_ROOT;
      auto& path = http_request._path;
      auto& query_string = http_request._query_string;
      bool with_arg = false;
      int& code = http_response._status_code;
      
      //1. 判断请求方法
      if(http_request._method != "GET" && http_request._method != "POST"){
        //非法请求
        LOG(WARNING, "method wrong!");
        code = BAD_REQUEST;
        goto END;
      }
      
      //GET和POST方法都要让path = uri，然后再进行后续处理
      http_request._path = http_request._uri; 
      if(http_request._method == "POST"){
        http_request.cgi = true;  //POST方法需要进行CGI处理, GET方法还需要继续进行判断
      } 

      
      //2. 处理URI
      path = http_request._path;
      query_string = http_request._query_string;
      with_arg = Util::CutString(http_request._uri, path, query_string, "?");

      //2.1 path携带参数, 需要CGI处理(GET和POST)
      if(with_arg == true)
        http_request.cgi = true;
      else 
        path = http_request._uri; //path不带参数, 需要在这里设置一下path = uri; 因为CutString在找不到分隔符时, 不会对输出型参数处理的
      

      //2.2 给path添加WEB_ROOT, 添加根目录的名称
      //root在最开头定义了
      root += path;
      path.swap(root);
      
      //2.3 判断path是否合法: 
      //1. 资源是否存在(path最后以'/'结尾的需要先特殊处理, 比如根目录"/") 
      //2. 资源是否可访问(目录、可执行文件)
      if(path[path.size() - 1] == '/')
      {
        path += HOME_PAGE;
      }

      //2.3.1 判断资源是否存在:
      struct stat st;
      
      //浏览器会同时请求favicon资源, 此时资源可能不存在
      //cout << path << endl;
      if(stat(path.c_str(), &st) == 0)
      {
        http_response.size = st.st_size;  //保存所打开文件的大小(后面sendfile时需要使用这个参数)
        //2.3.2.a 判断该资源是否是目录, 如果是目录, 需要让它去访问该目录下的默认页
        if(S_ISDIR(st.st_mode))
        {
          path += "/";  //这种情况:/a/b + "/HOME_PAGE"
          path += HOME_PAGE; 
          stat(path.c_str(), &st);  //刷新st, 以便获取HOME_PAGE的字节数
          http_response.size = st.st_size;  //更新资源的大小
        }
        else if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) //2.3.2.b 是否具有可执行权限
        {
          http_request.cgi = true;
        }


        //处理响应报头的Content-Type字段
        std::string& suffix = http_request.suffix;
        size_t pos = path.rfind(".");
        if(pos != std::string::npos){
          suffix = path.substr(pos); //如.css (需要加.的)
        }

      }
      else//资源不存在 
      {
        http_response._status_code = NOT_FOUND; //404 NOT FOUND
        LOG(WARNING, "resoure not exist!");
        code = NOT_FOUND;
        goto END;
      }
      

      //走到这里的, 要么是POST方法, 要么是请求资源是正确的
      if(http_request.cgi == true){
        code = ProcessCGI();
      }
      else {
        //在ProcessNonCGI函数内, 我们来构建响应并且进行字段的填充
        //cout << "[" << http_request._path << "]" << " " << "[" << http_request._query_string << "]" << endl;
        code =  ProcessNonCGI();  //进行非CGI处理: 打开资源文件
      }


END:
      BuildResponseHelper();
      return;
    }

  private:

    //进行响应的构建
    void BuildResponseHelper()
    {
      int& code = http_response._status_code;
      std::string& response_line  = http_response.response_line;
      response_line = HTTP_VERSION;
      response_line += " ";
      response_line += std::to_string(code);
      response_line += " ";
      response_line += Code2Desc(code);
      response_line += LINE_END;

      //如果是HandlerError, 要拼接上wwwroot目录
      std::string path = WEB_ROOT;
      path += "/";;
      switch(code){
        case OK:
          BuildOKResponse();
          break;
        case BAD_REQUEST:
          HandlerError(path + PAGE_400);
          break;
        case NOT_FOUND:
          HandlerError(path + PAGE_404); //可以定义其它类型的错误, 然后同样使用HandlerError函数进行处理
          break;
        case SERVER_ERROR:
          HandlerError(path + PAGE_500);
          break;
        default:
          break;
      }
    }

    //构建OK的响应(响应报头 + 响应正文<cgi要区分处理>)
    void BuildOKResponse()
    {
      auto& response_header = http_response.response_header;
      std::string line = "Content-Type: ";
      line += MapSuffix2Type(http_request.suffix);
      line += LINE_END;
      response_header.push_back(line);

      line = "Content-Length: ";
      if(http_request.cgi){
        //CGI   --->  CGI程序通过管道向Server发送了数据，这个数据就成为了request_body
        line += std::to_string(http_response.response_body.size());
      }
      else {
        //不带参数的GET方法 (非CGI)   --->  直接返回的是静态网页资源, 因此之前存储过该资源的大小, 存入到了http_response.size当中
        line += std::to_string(http_response.size);
      }

      line += LINE_END; //细节错误2
      response_header.push_back(line);
    } 

    void HandlerError(std::string page)
    {
      http_request.cgi = false;
      auto& response_header = http_response.response_header;
      int fd = open(page.c_str(), O_RDONLY);
      if(fd > 0){
        http_response.fd = fd;
        std::string line = "Content-Type: ";
        line += MapSuffix2Type(http_request.suffix);
        line += LINE_END;
        response_header.push_back(line);

        line = "Content-Length: ";
        struct stat st;
        stat(page.c_str(), &st);
        http_response.size = st.st_size;
        line += std::to_string(st.st_size);
        line += LINE_END;
        response_header.push_back(line);
      }
    }

  public:
    void SendResponse()
    {
      //cout << "response_line: " << http_response.response_line;
       
      send(sockfd, http_response.response_line.c_str(), http_response.response_line.size(), 0); 
      for(auto& it : http_response.response_header)
      {
        //cout << it.c_str() << ":"<< it.size()<< endl;
        send(sockfd, it.c_str(), it.size(), 0);
        //cout << it;
      }
      
      send(sockfd, http_response.response_blank.c_str(), http_response.response_blank.size(), 0);

      if(http_request.cgi){
        //发送响应正文 
        size_t size = http_response.response_body.size();
        const char* start = http_response.response_body.c_str();
        size_t offset = 0;
        size_t ret = 0;
        
        //while(1){
        //  ret = send(sockfd, start + offset, size - offset, 0);
        //  if(ret <= 0){
        //    break;
        //  }
        //  offset += size;
        //}

        //小错误: ()没匹配上去, 导致条件判断出错
        while( offset < size && (ret = send(sockfd, start + offset, size - offset, 0)) > 0)
        {
          offset += ret;
        }
      }
      else{
        sendfile(sockfd, http_response.fd, nullptr, http_response.size);
        close(http_response.fd);
      }
    }
  private:
    //读取请求行, 并将请求行存储到http_line中 
    bool RecvHttpLine()
    {
      std::string& line = http_request.request_line;
      if(Util::ReadLine(sockfd, line) > 0)
      {
        line.resize(line.size() - 1); //删除'\n'
      }
      else 
      {
        LOG(ERROR, "Recv Error!");
        read_stop = true;  
      }

      return read_stop;
    }
    
    bool RecvHttpHeader()
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
          read_stop = true;
          LOG(ERROR, "Recv Error!");
          break;
        }
      }
#ifdef DEBUG
      for(auto& e : http_request.request_header)
        cout << e << endl;
#endif

      http_request.request_blank = line;  //空行
      return read_stop;
    }
    

    //1. 首先判断请求方法是否为POST, 这里就认定GET方法无请求正文;
    //2. 其次需要Body-Length字段
    //读取请求正文前, 需要先对请求报头的内容进行分析和拆解(存入到header_map中), 然后利用Body-Length字段确定大小
    bool RecvRequestBody()
    {
      if(IsNeedRequestBody())
      {
        cout << "Recv Http Body" << endl;
        auto& header_map = http_request.header_map;
        if(header_map.find("Content-Length") != header_map.end())
        {
          http_request.content_length = stoi(header_map["Content-Length"]);

          char ch;
          for(int i = 0; i < http_request.content_length; ++i)
          {
            ssize_t s = recv(sockfd, &ch, 1, 0);
            if(s < 0){
              LOG(ERROR, "Recv request body error");
              read_stop = true;
              break;
            }
            http_request.request_body += ch;
          }

          //错误1:
          //char buff[http_request.content_length];
          //recv(sockfd, buff, sizeof(buff),0);
          //http_request.request_body += buff;  //这么写是错的! 这样写了之后，打印出来的时候不是以"\0"为结尾的
          //cout << buff <<endl;
          //cout << "DEBUG: " << http_request.request_body << "    content_length: "<< http_request.content_length << endl;
        }
      }
      return read_stop;
    }

    bool IsNeedRequestBody()
    {
      cout << "Method!: " << http_request._method << endl;
      if(http_request._method == "POST")
      {
        return true;
      }
      else  //GET没正文
      {
        return false;
      }
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


    //非CGI: 这个函数中, 我们只需要打开静态资源即可。响应行、响应报头、响应正文的处理在BuildResponseHelper与BuildOKResponse或HandlerError
    int ProcessNonCGI()
    {
      ////构建响应行
      //http_response.response_line = HTTP_VERSION;
      //http_response.response_line += " ";
      //http_response.response_line += std::to_string(http_response._status_code);
      //http_response.response_line += " ";
      //http_response.response_line += Code2Desc(http_response._status_code);
      //http_response.response_line += LINE_END;

      //构建响应报头
      //size_t pos = http_request._path.rfind(".");
      //if(pos != std::string::npos)
      //{
  
      //std::string& suffix = http_request.suffix;
      //http_response.response_header.emplace_back(std::string("Content-Length") + SEP + std::to_string(http_response.size) + LINE_END);
      //http_response.response_header.emplace_back(std::string("Content-Type") + SEP + MapSuffix2Type(suffix) + LINE_END); 
  
       //}
      //else 
      //{
      //  
      //  return NOT_FOUND;
      //}

      //打开文件, 读取文件的内容, 将其作为http_response的body
      //优化: 我们在这里读取文件的本质就是将文件的内容从内核缓冲区拷贝到用户缓冲区(因为文件存在外设, 需要调用系统调用接口read来读去)
      //      然而我们最终还需要把http_response的响应body从用户缓冲区发送到内核缓冲区, 这个过程有些浪费
      //    因此我们可以借助一个系统调用函数sendfile, 它能够直接将内容不经由用户空间, 在内核空间进行转发, 以此提高效率!
      http_response.fd = open(http_request._path.c_str(), O_RDONLY);
      if(http_response.fd > 0){
        return OK;
      }

      return NOT_FOUND;
    }

    int ProcessCGI()
    {
      int& code = http_response._status_code;     //状态码
      auto& bin = http_request._path;             //CGI程序的路径
      auto& method = http_request._method;        //作为环境变量传过去, CGI程序根据它判断请求方法
      auto& query_string = http_request._query_string;  //作为GET+参数的环境变量
      int content_length = http_request.content_length; //作为POST时的环境变量

      cout << "DEBUG bin: " << bin << endl;
      //左边写给右边, 右边从左边当中读取
      int FatherToChild[2];
      int ChildToFather[2];
      if(pipe(FatherToChild) < 0){
        LOG(ERROR, "pipe open error");
        code = SERVER_ERROR;
        return code;
      }
      if(pipe(ChildToFather) < 0){
        LOG(ERROR, "pipe open error");
        code = SERVER_ERROR;
        return code;
      }
      

      pid_t id = fork();
      if(id == 0)
      {
        close(FatherToChild[1]);
        close(ChildToFather[0]);
        
        //把method通过环境变量导入, 以便CGI程序能够判断到底是哪个方法
        std::string env_method = "METHOD=";
        env_method += method;
        putenv(const_cast<char*>(env_method.c_str()));
        
        //通过环境变量导入GET方法的参数
        if(method == "GET"){
          std::string env_query_string = "QUERY_STRING=";
          env_query_string += query_string;
          putenv(const_cast<char*>(env_query_string.c_str()));
        }

        if(method == "POST"){
          std::string env_content_length = "CONTENT_LENGTH=";
          env_content_length += std::to_string(content_length);
          putenv(const_cast<char*>(env_content_length.c_str()));
        }

        //将CGI程序的0和1号文件描述符重定向到管道中(0表示从Father读, 1表示写入Father)
        dup2(FatherToChild[0], 0);
        dup2(ChildToFather[1], 1);

        execl(bin.c_str(), bin.c_str(), NULL);
      }
      else//父进程等待 
      {
        close(FatherToChild[0]);
        close(ChildToFather[1]);

        //POST方法的请求正文的数据, 需要通过管道传输给CGI程序
        if(method == "POST"){
          char* start = const_cast<char*>(http_request.request_body.c_str());  
          //int content_length = http_request.content_length; //最前面定义了
          int offset = 0;
          while(1){
            ssize_t ret = write(FatherToChild[1], start + offset, content_length - offset);
            offset += ret;
            if(ret <= 0 || offset > content_length)
              break;
          }
        }
        
        //父进程阻塞式接收来自CGI程序的管道数据
        char ch;
        ssize_t ret = 0;
        while((ret = read(ChildToFather[0], &ch, 1)) > 0)
        {
          http_response.response_body.push_back(ch);
        }
        //cout << "cgi 管道接受到的body:"<< http_response.response_body << endl;
        
        //千万别忘记关了!!! 文件描述符资源有限
        close(FatherToChild[1]);
        close(ChildToFather[0]);

        int status = 0;
        id = waitpid(id, &status, 0); 
        if(WIFEXITED(status)){
          code = OK;
          if(WEXITSTATUS(status) == 0){ //看子进程的退出码，因为子进程也可能因为崩溃退出
            code = OK;
          }
          else {
            code = BAD_REQUEST;
          }
        } 
        else {
          code = SERVER_ERROR;
        }
      }

      return code;
    }

  private:
    int sockfd;
    HttpRequest http_request;
    HttpResponse http_response;
    bool read_stop = false;
};

//线程的入口
class CallBack
{
  public:
    //让CallBack类定义的对象成为仿函数
    void operator()(int sockfd)
    {
      HandlerRequest(sockfd);
    }

    //static void* HandlerRequest(void* arg)
    void HandlerRequest(int sockfd)
    {
      LOG(INFO, "[[Entrance BEGIN:]]");

      //int* sockfd = reinterpret_cast<int*>(arg);
      //int sock = *sockfd;
      //delete sockfd;  //可以释放了

      EndPoint* ep = new EndPoint(sockfd);
      ep->RecvRequest();
      if(!ep->IsReadStop()){
        ep->BuildResponse();
        ep->SendResponse();
      }

      LOG(INFO, "[[Entrance END:]]");

      delete ep;
      //return nullptr;
    }
};
