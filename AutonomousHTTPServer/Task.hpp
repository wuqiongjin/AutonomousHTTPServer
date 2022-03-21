#pragma once 
#include <iostream>
#include "Protocol.hpp"

class Task{
  public:
    Task(int sockfd)
      :_sockfd(sockfd)
    {}
    
    void ProcessOn()
    {
      _handler(_sockfd);
    }
  private:
    int _sockfd;
    CallBack _handler;
};
