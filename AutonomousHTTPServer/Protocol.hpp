#pragma once
#include "Common.h"
#include "Log.hpp"

class HttpRequest
{
  private:
};

class HttpResponse
{
  private:
};


//IO通信类: 读取请求; 分析请求; 处理请求; 构建响应;
class EndPoint
{
  public:
    void RecvRequest()
    {

    }

    void AnalyzeRequest()
    {

    }

    void BuildResponse()
    {

    }

    void SendResponse()
    {

    }
  private:
    HttpRequest http_request;
    HttpResponse http_reponse;
};

//线程的入口
class Entrance
{
  public:
    static void* HandlerRequest(void* arg)
    {
      LOG(INFO, "[[Entrance BEGIN:]]");

      int* sockfd = reinterpret_cast<int*>(arg);
      EndPoint* ep = new EndPoint;
      ep->RecvRequest();
      ep->AnalyzeRequest();
      ep->BuildResponse();
      ep->SendResponse();

      LOG(INFO, "[[Entrance END:]]");

      delete ep;
      return nullptr;
    }
};
