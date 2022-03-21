#pragma once 
#include "Common.h"
#include "TcpServer.hpp"
//#include "Protocol.hpp"
#include "Task.hpp" //Task当中通过回调函数进行到HandlerRequest中处理请求, 这里实现了解耦，让HttpServer不需要考虑底层协议处理
#include "ThreadPool.hpp"
#include <signal.h>

class HttpServer 
{
  public:
    HttpServer(int port)
      :_port(port)
    {}

    void InitServer()
    {
      signal(SIGPIPE, SIG_IGN); //忽略SIGPIPE信号, 防止出现写入数据时, 由于客户端那边关闭了读端文件描述符, 从而导致服务器线程接收到OS的SIGPIPE从而使服务器崩溃(对方关闭连接，我们正常往下走就行)
      TcpServer::GetInstance(_port)->InitServer();
    }

    void LoopWork()
    {
      TcpServer* tcpsvr = TcpServer::GetInstance(_port);
      LOG(INFO, "Start Loop~~~~");
      while(!_stop)
      {
        struct sockaddr_in peer;
        socklen_t len;
        int* sockfd = new int(accept(tcpsvr->GetListenSock(), reinterpret_cast<sockaddr*>(&peer), &len));
        if(*sockfd < 0)
        {
          LOG(ERROR, "accept faild!");
          continue;
        }

        Task* t = new Task(*sockfd);
        ThreadPool::GetInstance(*sockfd)->PushTask(t);
        delete sockfd;
        //pthread_t tid;
        //pthread_create(&tid, nullptr, Entrance::HandlerRequest, reinterpret_cast<void*>(sockfd));
        //pthread_detach(tid);
      }
    }
  private:
    int _port;
    bool _stop;
};
