#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <string>
using namespace std;

int main()
{
  cerr << "------------------ CGI ----------------" << endl;
  string method = getenv("METHOD");

  if(method == "GET"){
    string query_string = getenv("QUERY_STRING");
    cerr << "GET: CGI is running!" << endl;  
    cerr << "Query_String: " << query_string << endl;
    
    //1: 向管道中写入
    cout << "Your Query_String is: " << query_string << endl;
  }
  else if(method == "POST"){
    cerr << "POST: CGI is running!" << endl;  
    int content_length = atoi(getenv("CONTENT_LENGTH"));
    std::string request_body;
    char buff[content_length];
    ssize_t s = read(0, buff, content_length);
    if(s > 0){
      request_body += buff;
      cerr << "Post RequestBody: " << request_body << endl;
    }
    else if(s == 0){

    }
    else{

    }
  }

  return 0;
}
