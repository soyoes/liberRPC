# liberTaskQueue
a CPP task queue server for web server background/asynchronized task such as PHP

## Install (MacOS)
```
> g++ main.cpp -o liber_queue_server -Wall -Wextra -std=c++11
> ./liber_queue_server
```

## Usage

* Command Syntax
  * add URL_STRING DATA_STRING
  //Add task to Queue
  * quit
  //Quit from Telnet
  
* HTTP curl 
  ```Bash
  > curl --data 'add http://localhost/test.php data=1&data2=2' 127.0.0.1:8511
  ```

* Telnet 
  ```Bash
  > telnet 127.0.0.1 8511
  $ add http://localhost/test.php data=1&data2=2
  $ quit
  ```
