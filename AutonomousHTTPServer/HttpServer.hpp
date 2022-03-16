#pragma once 
#include "Common.h"
#include "TcpServer.hpp"
#include "Protocol.hpp"
#include <signal.h>

class HttpServer 
{
  public:
    HttpServer(int port)
      :_port(port)
    {}

    void InitServer()
    {
      signal(SIGPIPE, SIG_IGN); //忽略SIGPIPE信号, 防止出现写入数据时, 由于客户端那边关闭了读端文件描述符, 从而导致服务器线程接收到OS的SIGPIPE从而使服务器崩溃
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
        pthread_t tid;
        pthread_create(&tid, nullptr, Entrance::HandlerRequest, reinterpret_cast<void*>(sockfd));
        pthread_detach(tid);
      }
    }
  private:
    int _port;
    bool _stop;
};
