#include "HttpServer.hpp"

void Usage()
{
  cout << "Usage" << endl;
  cout << '\t' << "./HttpServer port" << endl;
}

int main(int argc, char* argv[])
{
  if(argc != 2)
  {
    Usage();
  }
  HttpServer* httpsvr = new HttpServer(atoi(argv[1]));
  httpsvr->InitServer();
  httpsvr->LoopWork();
  
  return 0;
}
