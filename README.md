# Time-Travel Debugger

## Build
    g++ server.cpp -o server
    g++ load_balancer.cpp -o load_balancer -pthread
    g++ client.cpp -o client

## Run
Terminal 1:

    ./load_balancer

Terminal 2:

    ./client 127.0.0.1 5000

The load balancer starts `./server 5001 &` by itself, so the `server` file must be in the same folder.

## Commands
    start <file>     run a source file and show step 1
    next             one step forward
    prv              one step back
    jmp next <n>     n steps forward
    jmp prv <n>      n steps back
    help             show commands
    quit             leave