#include <iostream>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h> 
#include <thread>
#include <string>
#include <vector>
#include <regex>
#include <queue> 
#include <chrono>
#include <array>
#include <fstream>

using namespace std;

#define PORT                    8511
#define CLIENT_INPUT_LIMIT      1024
#define LISTENER_WORKER_LIMIT   10
#define LISTENER_WORKER_INERVAL 5000
#define DB_FILE                 "tasks.data"

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

int       s_sock;
thread    runner;
array<int,LISTENER_WORKER_LIMIT>     listeners_stats;
queue<json_rpc_req> tasks;
ofstream  fout;


vector<string> str_split(char* str,const char* delim);
int   curl(string url, string data);
void  runner_start();
void  runner_worker();
void  listener_start(int c_sork);
void  listener_worker(int c_sock, int thread_idx);
void  add_queue(string url,string data,bool sync_file);
void  server_start();
void  server_stop();
void data_clear();
void data_puts(json_rpc_req req);
void data_gets();


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

/**
* call remote server while worker start to exec 
*/
int curl(string url, string data){
  //FIXME support other HTTP methods 
  string cmd = "curl --data '"+data+"' -X POST -s -o /tmp/liber_rpc_tmp.html -w '%{http_code}' "+url;
  FILE * f = popen( cmd.c_str(), "r" );
  if(f == 0){
    cout << "ERR : Failed to exec curl(), " << url << endl;
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

/**
* batch proc thread, worker program to exec tasks in queue.
*/
void runner_worker(){
  //assert(!tasks.empty());
  //read new tasks
  if(tasks.empty()){
    cout << "runner_worker : NO Task "  << endl;  
  }else{
    std::vector<json_rpc_req> errs;
    while(!tasks.empty()){
      json_rpc_req req = (struct json_rpc_req) tasks.front();
      tasks.pop();
      cout << "runner_worker : " << req.method << " - " << req.params << endl;
      int res = curl (req.method, req.params);
      if(res==1)//failed
        errs.push_back(req);
    }
    data_clear();
    if(!errs.empty()){
      for(json_rpc_req o : errs){
        add_queue(o.method, o.params,true);
      }
      vector<json_rpc_req>().swap(errs); //free memory
    }
  }
  //usleep(LISTENER_WORKER_INERVAL * 1000);
  this_thread::sleep_for(chrono::milliseconds(LISTENER_WORKER_INERVAL));
  runner_worker();
}

/*
 * start a new listenr thread. 
 */
void lister_start(int c_sock){
  int assigned = 0;
  for(int i=0;i<LISTENER_WORKER_LIMIT;i++){
    if(listeners_stats[i]==0){
      listeners_stats[i]=1;
      //create listener thread.
      thread t = thread(listener_worker, c_sock, listeners_stats[i]); 
      t.join();
      assigned = 1;
      break;
    }
  }
  if(assigned==0){ //if thread pool is full , wait 20ms to do it again
    usleep(20 * 1000);
    lister_start(c_sock); 
  }
}

/**
* listener works threads, which listen to new task request.
*/
void listener_worker(int c_sock, int thread_idx){
  char *msg;
  string cmd;

  //to check if its http request or socket
  regex http_pattern("^(GET|POST)[\\s\\/]*HTTP(.*)");
  smatch sm;

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

    cout << first_row << endl;

    //TODO add suport of JSON-RPC call and JSON-RPC response.

    /* http or socket-client */
    if(regex_match(first_row,sm,http_pattern)){ //HTTP REQUEST
      // cout << "HTTP REQ" << endl;
      words = str_split(const_cast<char*>(rows.back().c_str())," ");
      //TODO check length
      add_queue(words[1],words[2],true);
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
        add_queue(words[1],words[2],true);
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
  listeners_stats[thread_idx] = 0;
}


void add_queue(string url,string data,bool sync_file){
  json_rpc_req req = (json_rpc_req){
    .id=0, //FIXME,id
    .method=url,
    .params=data
  };
  tasks.push(req);
  if(sync_file)
    data_puts(req);
}

void data_clear(){
  std::ofstream ofs;
  ofs.open(DB_FILE, std::ofstream::out | std::ofstream::trunc);
  ofs.close();
}

/**
 * save tasks to data file.
 * @param data : request data
 */
void data_puts(json_rpc_req req){
  fout << req.method << " " << req.params << endl;
}

/**
 * check data file of the last time. and load to tasks queue
 */
void data_gets(){
  ifstream fin(DB_FILE);
  if(fin.is_open()){//file exists
    string line;
    while (getline(fin, line)){
      vector<string> words = str_split(const_cast<char*>(line.c_str())," ");
      add_queue(words[0],words[1],false);
    }
  }
}


void server_start(){
  s_sock = socket(AF_INET,SOCK_STREAM,0); //server socket
  if(s_sock == -1){
      cout << "ERR : Failed to create server socket" << endl;
      exit(-1);
  }
  //define server socket
  struct sockaddr_in s_addr; //server addr & client addr
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
  cout << "RPC server start listening ..." <<endl;
}

void server_stop(){
  close(s_sock);
}

int main(){

  //check data file of the last time. and load to tasks queue
  data_gets();

  //init file handler
  fout = ofstream(DB_FILE);
  
  //create task runner worker thread.
  runner = thread(runner_worker);
  runner.detach(); //run as deamon
  
  //init lister status
  listeners_stats.fill(0);

  //start server socket
  server_start();
  
  sockaddr_in c_addr;
  socklen_t c_addr_size=sizeof(c_addr);
  while(true){
    int c_sock = accept(s_sock,(struct sockaddr *)&c_addr,&c_addr_size);
    lister_start(c_sock);
  }
}
