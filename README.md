# liberRPC
A task queue, RPC server written in CPP.<BR>
for the purpose of background/asynchronized/time-consuming tasks<BR>
Support JSON-RPC spec and simple command line mode.

## Install (MacOS)
```
> g++ main.cpp -o liber_rpc_server -Wall -Wextra -std=c++11
> ./liber_rpc_server
```

## Usage

* Command Syntax
  * add URL_STRING DATA_STRING
  //Add task to Queue
  * quit
  //Quit from Telnet
  
* HTTP curl 
  ```Bash
  > curl --data 'add http://example.dev/ arg1=1&arg2=2' 127.0.0.1:8511
  ```

* Telnet 
  ```Bash
  > telnet 127.0.0.1 8511
  $ add http://example.dev/ data1=1&data2=2
  $ quit
  ```
