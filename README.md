# Preforking-Chat-Server
Linux Programming

- Install libraries, gcc, make
<br>```sudo apt update```
<br>```sudo apt install build-essential```

- Build server
<br>```cd ./server```
<br>```make```

- Build client
<br>```cd./client```
<br>```gcc main.c -o client```

- Run
<br>```./server/build/server 8000```
<br>```./client/client 127.0.0.1 8000```

- Client command
<br>```JOIN <name>```
<br>```QUIT```
<br>```MSG <message>```
<br>```PMSG <name> <mesage>```

- Server response
<br>```100 OK```
<br>```200 NICKNAME IN USE```
<br>```888 WRONG STATE```
<br>```999 UNKNOWN ERROR```
<br>```999 UNKNOWN COMMAND```
