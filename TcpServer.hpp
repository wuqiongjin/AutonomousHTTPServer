#pragma once
#include "Common.h"
#include "Log.hpp"

#define BACKLOG 5

//设计成单例模式
class TcpServer
{
  public:
    static TcpServer* GetInstance(int port)
    {
      if(nullptr == _svr)
      {
         Lock();
         if(nullptr == _svr)
         {
           _svr = new TcpServer(port);
         }
         UnLock();
      }
      return _svr;
    }

    void InitServer()
    {
      CreateSocket();
      Bind();
      Listen();
    }

    void CreateSocket()
    {
      _lsock = socket(AF_INET, SOCK_STREAM, 0);
      if(_lsock < 0)
      {
        LOG(FATAL, "socket create error!");
        exit(1);
      }
    }

    void Bind()
    {
      struct sockaddr_in local;
      local.sin_family = AF_INET;
      local.sin_port = htons(_port);
      local.sin_addr.s_addr = htonl(INADDR_ANY);
      if(bind(_lsock, reinterpret_cast<struct sockaddr*>(&local), sizeof(local)) < 0)
      {
        LOG(FATAL, "bind error!");
        exit(2);
      }
    }

    void Listen()
    {
      if(listen(_lsock, BACKLOG) < 0)
      {
        LOG(FATAL, "listen error!");
        exit(3);
      }
    }

    int GetListenSock()
    {
      return _lsock;
    }

    ~TcpServer()
    {}
  private:
    TcpServer(int port)
      :_port(port)
    {}
    
    static void Lock() { pthread_mutex_lock(&mtx); }
    static void UnLock() { pthread_mutex_unlock(&mtx); }
  private:
    int _port;
    int _lsock;
    static pthread_mutex_t mtx;
    static TcpServer* _svr;
};

pthread_mutex_t TcpServer::mtx = PTHREAD_MUTEX_INITIALIZER;
TcpServer* TcpServer::_svr = nullptr;
