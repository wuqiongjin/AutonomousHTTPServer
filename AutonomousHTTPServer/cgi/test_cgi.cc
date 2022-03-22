#include "Common_cgi.hpp"

int main()
{
  cerr << "------------------ CGI ----------------" << endl;
  //string method = getenv("METHOD");
  //string query_string;
  //if(method == "GET"){
  //  query_string = getenv("QUERY_STRING");
  //  cerr << "GET: CGI is running!" << endl;  
  //  cerr << "Query_String: " << query_string << endl;
  //  
  //  //1: 向管道中写入
  //  cout << "Your Query_String is: " << query_string << endl;
  //}
  //else if(method == "POST"){
  //  cerr << "POST: CGI is running!" << endl;  
  //  int content_length = atoi(getenv("CONTENT_LENGTH"));
  //  char buff[content_length];
  //  ssize_t s = read(0, buff, content_length);
  //  if(s > 0){
  //    query_string += buff;
  //    cerr << "Post RequestBody: " << query_string << endl;
  //  }
  //  else if(s == 0){

  //  }
  //  else{

  //  }
  //}
  
  std::string query_string;
  if(!GetQueryString(query_string))
    return -1;
  
  std::string s1, s2;
  CutString(query_string, s1, s2, "&");

  std::string key1, value1;
  CutString(s1, key1, value1, "=");

  std::string key2, value2;
  CutString(s2, key2, value2, "=");
  
  int x = atoi(value1.c_str());
  int y = atoi(value2.c_str());

  cout << "<html>";
  cout << "<head> <meta charset=\"UTF-8\">";
  cout << "<title>wuqiongjin's web</title>";
  cout << "<body>";
  cout << "<h4>" << x << " + " << y << " = " << x + y << "</h4>";
  cout << "<h4>" << x << " - " << y << " = " << x - y << "</h4>";
  cout << "<h4>" << x << " * " << y << " = " << x * y << "</h4>";
  cout << "<h4>" << x << " / " << y << " = " << x / y << "</h4>";
  cout << "</body>";
  cout << "</html>";
  
  return 0;
}
