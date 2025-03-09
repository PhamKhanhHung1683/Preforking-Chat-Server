# Preforking-Chat-Server
Linux Programming

Install libraries, gcc, make
  sudo apt update
  sudo apt install build-essential

Build server
  cd ./server
  make

Build client
  cd./client
  gcc main.c -o client

Run
  ./server 8000
  ./client 127.0.0.1 8000

Client command
  JOIN <name>
  QUIT
  MSG <message>
  PMSG <name> <mesage>

Server response
  100 OK
  200 NICKNAME IN USE
  888 WRONG STATE
  999 UNKNOWN ERROR
  999 UNKNOWN COMMAND
