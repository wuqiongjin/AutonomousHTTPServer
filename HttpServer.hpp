#pragma once 
#include "Common.h"
#include "TcpServer.hpp"
#include "Protocol.hpp"

class HttpServer 
{
  public:
    HttpServer(int port)
      :_port(port)
    {}

    void InitServer()
    {
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
      }
    }
  private:
    int _port;
    bool _stop;
};
