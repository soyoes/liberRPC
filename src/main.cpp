#include <iostream>
#include <cstdlib>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 
#include <unistd.h>
#include <vector>
#include <regex>
#include <queue> 

using namespace std;

#define PORT    8511
#define CLIENT_INPUT_LIMIT 1024

#define NUM_THREADS 10

pthread_t task_runner_thread;
unsigned int interval=1000;
int s_sock;

struct json_rpc_req {
  int id;
  string method;
  string params;
};

struct json_rpc_res {
  int id;
  string success;
  string error;
};

std::queue<json_rpc_req> tasks;

vector<string> str_split(char* str,const char* delim){
    char* saveptr;
    char* token = strtok_r(str,delim,&saveptr);
    vector<string> result;
    while(token != NULL){
        result.push_back(token);
        token = strtok_r(NULL,delim,&saveptr);
    }
    return result;
}

int curl(string url, string data){
  //FIXME support other HTTP methods 
  string cmd = "curl --data '"+data+"' -X GET -s -o /tmp/liber_rpc_tmp.html -w '%{http_code}' "+url;
  FILE * f = popen( cmd.c_str(), "r" );
  if(f == 0){
    cout << "ERR : Failed to exec curl()" << endl;
    return 1;
  }
  const int BUFSIZE = 100;
  char buffer[ BUFSIZE ];
  fgets( buffer, BUFSIZE,  f );
  string msg = (strcmp(buffer,"200")==0)? "Success":"Error";
  cout << "Task : " << msg <<endl;

  pclose(f);
  return 0;
}

void * task_worker(void *d){
  //assert(!tasks.empty());
  //read new tasks
  if(tasks.empty()){
    cout << "task_worker : NO Task "  << endl;  
  }else{
    json_rpc_req req = (struct json_rpc_req) tasks.front();
    tasks.pop();
    cout << "task_worker : " << req.method << " - " << req.params << endl;
    curl (req.method, req.params);
    //@TODO exec req.
  }
  usleep(interval*5000);
  task_worker(d);
  //pthread_exit(NULL);
  return 0;
}



void add_queue(string url,string data){
  // cout << "add_queue URL = " << url << endl;
  // cout << "add_queue DATA = " << data << endl;
  // @TODO save to somewhere
  tasks.push((json_rpc_req){
    .id=0, //FIXME,id
    .method=url,
    .params=data
  });
}

void json_rpc_callback (json_rpc_res res){
  //TODO send json_rpc_res to the client server.
}


int main(){

  //create task runner worker thread.
  if(pthread_create(&task_runner_thread, NULL, task_worker, NULL)){
    cout << "ERR : Failed to create thread for worker" << endl;
    exit(-1);
  }

  /*
   pthread_t threads[NUM_THREADS];
   struct thread_data td[NUM_THREADS];
   int rc, i;
   for(i=0; i < NUM_THREADS; i++ ){
      cout <<"main() : creating thread, " << i << endl;
      td[i].thread_id = i;
      td[i].message = "This is message";
      rc = pthread_create(&threads[i], NULL,
                          walker, (void *)&td[i]);
      if (rc){
         cout << "Error:unable to create thread," << rc << endl;
         exit(-1);
      }
   }
   cout << "DONE" <<endl;
   pthread_exit(NULL);
   return 1;
  */

  int c_sock;
  s_sock = socket(AF_INET,SOCK_STREAM,0); //server socket
  if(s_sock == -1){
      cout << "ERR : Failed to create server socket" << endl;
      exit(-1);
  }

  //define server socket
  struct sockaddr_in s_addr,c_addr; //server addr & client addr
  bzero((char *)&s_addr,sizeof(s_addr));
  s_addr.sin_family=AF_INET;
  s_addr.sin_port=htons(PORT);
  s_addr.sin_addr.s_addr=INADDR_ANY;

  if(::bind(s_sock,(struct sockaddr *)&s_addr,sizeof(s_addr))<0){
      cout << "ERR : Failed to ::bind()" <<endl;
      exit(-1);
  }

  if (::listen(s_sock, 50) == -1){
      cout << "ERR : ::listen() error" <<endl;
      exit(-1);
  }
      
  cout << "Start listen" <<endl;

  //to check if its http request or socket
  regex http_pattern("^(GET|POST)[\\s\\/]*HTTP(.*)");
  smatch sm;

  socklen_t c_addr_size=sizeof(c_addr);

  while(1){
    char *msg;
    string cmd;

    c_sock=accept(s_sock,(struct sockaddr *)&c_addr,&c_addr_size);
    
    //client input lenth 
    int c_closed = 0;
    //client inputs 
    char buffer[CLIENT_INPUT_LIMIT];
   
    while (c_closed==0){

      if(recv(c_sock, buffer, CLIENT_INPUT_LIMIT, 0)<=0)
        cout << "ERR : ::recv() error!" << endl ;

      /* input to lines */
      vector<string> rows = str_split(buffer,"\r\n");
      string first_row(rows.front());
      vector<string> words;

      //TODO add suport of JSON-RPC call and JSON-RPC response.
 
      /* http or socket-client */
      if(regex_match(first_row,sm,http_pattern)){ //HTTP REQUEST
        // cout << "HTTP REQ" << endl;
        words = str_split(const_cast<char*>(rows.back().c_str())," ");
        //TODO check length
        add_queue(words[1],words[2]);
        msg = (char*)"HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK";
        send(c_sock, msg, strlen(msg), 0);  
        // close(c_sock);
        c_closed=1;
      } else { //COMMAND, socket client
        // cout << "CMD REQ" << endl;
        words = str_split(const_cast<char*>(first_row.c_str())," ");
        string first_keyword = words[0];
        if(first_keyword.compare("quit")==0){ // QUIT
          cout << "QUIT" << endl;
          msg = (char*)"> BYE";
          send(c_sock, msg, strlen(msg), 0);  
          c_closed=1;
        }else if(first_keyword.compare("add")==0){ //ADD 
          msg = (char*)"> ADD";
          send(c_sock, msg, strlen(msg), 1);  
          add_queue(words[1],words[2]);
        }else{ //UNKOWN CMD
          cout << "ERR:What ? " << first_row << endl;
          msg = (char*)"> WHAT?";
          send(c_sock, msg, strlen(msg), 1);  
        }
      }
      vector<string>().swap(words); //free memory
      vector<string>().swap(rows);//free memory
    }//end while c_close
    close(c_sock);
  }
  //close(s_sock);
}
