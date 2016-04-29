# MiniChat

## Client Side:  
The client sends text messages to the server.  
The client accepts the following commands from the keyboard:  
1. _connect <name> <machine> <port>: this is to connect to the server by name declaration  
2. _quit: the client disconnects from the server  
3. _who: asks the server for a list of connected users  

## Server Side:  
The server publish all text messages received from any client to all the connected clients.  
The server accepts the following commands:  
a. from a client or keyboard:  
	1. _who: same as _who in the client section above  
b. from keyboard:  
	1. _kill <name>: disconnect the client <name> and inform all connected clients  
	2. _shutdown: stop the server  
c. from client side:  
	1. _connect: the server inform all connected clients that a specific client has connected  
	2. _quit: the server inform all connected clients that a specific client has disconnected  
	
