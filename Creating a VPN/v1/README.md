# Problem 1

This program implements ping operation using UDP sockets with some added functionality to simulate retries, sleep and exit.

## Structure
- [mypingsrv.c](mypingsrv.c) is a server that reads requests from a UDP socket. The method `make_response` contains the business logic for the server where it reads the request, perfroms the necessary actions and builds a response to write back to the socket.
- [mypingcli.c](mypingcli.c) is a client that writes requests to a UDP socket and reads the response. It also calculates the round-trip time for each request-response and prints it to stdout.
- [pingparam.dat](pingparam.dat) is the configuration file for [mypingcli.c](mypingcli.c).
- [zigzagconf.c](zigzagconf.c) is a client that reads a configuration file and sends routing instructions to [zigzagrouter.c](zigzagrouter).
- [zzoverlay.dat](zzoverlay.dat) is the configuration file for [zigzagrouter.c](zigzagrouter.c).
- [zigzagrouter.c](zigzagrouter.c) is the router application that forwards packets to specific destinations based on the configuration it receives from [zigzagconf.c](zigzagconf).
- [security.c](security.c) is a utility file for getting the IP address of the currently running application.
- [security.h](security.h) is the header file for [security.c](security.c).

## Usage
### Build
To build the files run
```sh
make
```
After execution, clean up the workspace using
```sh
make clean
```
### Execute
From one window in the terminal, run the server. From the other window, run the client. From the other window(s), run zigzagrouter.
#### Step 1
Fill `zzoverlay.dat`.
#### Step 2
Run the router on all intermediate machines.
```sh
./zigzagrouter <PORT>
```
#### Step 3
Run `zigzagconf` from any machine that can reach the machine running `zigzagrouter`.
#### Step 4
Run the server
```sh
./mypingsrv.bin <SERVER_IP> <SERVER_PORT>
```
#### Step 5
Run the client
```sh
./mypingcli.bin <CLIENT_IP> <SERVER_IP> <SERVER_PORT>
```
### Clean up
The programs generate several intermediate files. To clean them up, run
```sh
make clean
```
