
const char * usage =
"                                                               \n"
"IRCServer:                                                   \n"
"                                                               \n"
"Simple server program used to communicate multiple users       \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   IRCServer <port>                                            \n"
"                                                               \n"
"Where 1024 < port < 65536.                                     \n"
"                                                               \n"
"In another window type:                                        \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where talk-server      \n"
"is running. <port> is the port number you used when you run    \n"
"daytime-server.                                                \n"
"                                                               \n";

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "IRCServer.h"
#include "HashTableVoid.cc"

int QueueLength = 5;
HashTableVoid passwords;
struct Message {
	char * user;
	char * message;
	int index;
};
struct ChatRoom {
	char * name;
	Message * messages; //Only 100 messages at once
	char ** usersInRoom; //Resizeable array--initialize to 10
	int nUsers;
	int maxUsers;
	int nMessages;
	int nLists;
};
int nUsers;
int maxMessages;
int maxRooms;
int nRooms;
ChatRoom * roomList; //Resizeable--initialize to 5

int
IRCServer::open_server_socket(int port) {

	// Set the IP address and port for this server
	struct sockaddr_in serverIPAddress; 
	memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
	serverIPAddress.sin_family = AF_INET;
	serverIPAddress.sin_addr.s_addr = INADDR_ANY;
	serverIPAddress.sin_port = htons((u_short) port);
  
	// Allocate a socket
	int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
	if ( masterSocket < 0) {
		perror("socket");
		exit( -1 );
	}

	// Set socket options to reuse port. Otherwise we will
	// have to wait about 2 minutes before reusing the sae port number
	int optval = 1; 
	int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
			     (char *) &optval, sizeof( int ) );
	
	// Bind the socket to the IP address and port
	int error = bind( masterSocket,
			  (struct sockaddr *)&serverIPAddress,
			  sizeof(serverIPAddress) );
	if ( error ) {
		perror("bind");
		exit( -1 );
	}
	
	// Put socket in listening mode and set the 
	// size of the queue of unprocessed connections
	error = listen( masterSocket, QueueLength);
	if ( error ) {
		perror("listen");
		exit( -1 );
	}

	return masterSocket;
}

void
IRCServer::runServer(int port)
{
	int masterSocket = open_server_socket(port);

	initialize();
	
	while ( 1 ) {
		
		// Accept incoming connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );
		int slaveSocket = accept( masterSocket,
					  (struct sockaddr *)&clientIPAddress,
					  (socklen_t*)&alen);
		
		if ( slaveSocket < 0 ) {
			perror( "accept" );
			exit( -1 );
		}
		
		// Process request.
		processRequest( slaveSocket );		
	}
}

int
main( int argc, char ** argv )
{
	// Print usage if not enough arguments
	if ( argc < 2 ) {
		fprintf( stderr, "%s", usage );
		exit( -1 );
	}
	
	// Get the port from the arguments
	int port = atoi( argv[1] );

	IRCServer ircServer;
	
	// It will never return
	ircServer.runServer(port);
	
}

//
// Commands:
//   Commands are started by the client.
//
//   Request: ADD-USER <USER> <PASSWD>\r\n
//   Answer: OK\r\n or DENIED\r\n
//
//   REQUEST: GET-ALL-USERS <USER> <PASSWD>\r\n
//   Answer: USER1\r\n
//            USER2\r\n
//            ...
//            \r\n
//
//   REQUEST: CREATE-ROOM <USER> <PASSWD> <ROOM>\r\n
//   Answer: OK\n or DENIED\r\n
//
//   Request: LIST-ROOMS <USER> <PASSWD>\r\n
//   Answer: room1\r\n
//           room2\r\n
//           ...
//           \r\n
//
//   Request: ENTER-ROOM <USER> <PASSWD> <ROOM>\r\n
//   Answer: OK\n or DENIED\r\n
//
//   Request: LEAVE-ROOM <USER> <PASSWD>\r\n
//   Answer: OK\n or DENIED\r\n
//
//   Request: SEND-MESSAGE <USER> <PASSWD> <MESSAGE> <ROOM>\n
//   Answer: OK\n or DENIED\n
//
//   Request: GET-MESSAGES <USER> <PASSWD> <LAST-MESSAGE-NUM> <ROOM>\r\n
//   Answer: MSGNUM1 USER1 MESSAGE1\r\n
//           MSGNUM2 USER2 MESSAGE2\r\n
//           MSGNUM3 USER2 MESSAGE2\r\n
//           ...\r\n
//           \r\n
//
//    REQUEST: GET-USERS-IN-ROOM <USER> <PASSWD> <ROOM>\r\n
//    Answer: USER1\r\n
//            USER2\r\n
//            ...
//            \r\n
//

void IRCServer::processRequest( int fd ) {
	// Buffer used to store the comand received from the client
	const int MaxCommandLine = 1024;
	char commandLine[ MaxCommandLine + 1 ];
	int commandLineLength = 0;
	int n;
	
	// Currently character read
	unsigned char prevChar = 0;
	unsigned char newChar = 0;
	
	//
	// The client should send COMMAND-LINE\n
	// Read the name of the client character by character until a
	// \n is found.
	//

	// Read character by character until a \n is found or the command string is full.
	while ( commandLineLength < MaxCommandLine &&
		read( fd, &newChar, 1) > 0 ) {

		if (newChar == '\n' && prevChar == '\r') {
			break;
		}
		
		commandLine[ commandLineLength ] = newChar;
		commandLineLength++;

		prevChar = newChar;
	}
	
	// Add null character at the end of the string
	// Eliminate last \r
	commandLineLength--;
        commandLine[ commandLineLength ] = 0;

	printf("RECEIVED: %s\n", commandLine);
	const char * command;
	const char * user;
	const char * password;
	const char * args;
	int lastSpace = 0;
	char * temp = (char *) malloc(1024 * sizeof(char));
	char * t = temp;
	for (int i = 0; i < commandLineLength; i++) {
		if (commandLine[i] == ' ') {
			*t = 0;
			command = strdup(temp);
			lastSpace = i;
			break;
		}
		*t = commandLine[i];
		t++;
	}
	t = temp;
	for (int i = lastSpace + 1; i < commandLineLength; i++) {
		if (commandLine[i] == ' ') {
			*t = 0;
			user = strdup(temp);
			lastSpace = i;
			break;
		}
		*t = commandLine[i];
		t++;
	}
	t = temp;
	for (int i = lastSpace + 1; i <= commandLineLength; i++) {
		if ((commandLine[i] == ' ') || (commandLine[i] == '\0')) {
			*t = 0;
			password = strdup(temp);
			lastSpace = i;
			break;
		}
		*t = commandLine[i];
		t++;
	}
	t = temp;
	for (int i = lastSpace + 1; i < commandLineLength; i++) {
		*t = commandLine[i];
		t++;
	}
	*t = 0;
	args = strdup(temp);
	free(temp);
	printf("command=%s\n", command);
	printf("user=%s\n", user);
	printf( "password=%s\n", password );
	printf("args=%s\n", args);

	if (!strcmp(command, "ADD-USER")) {
		addUser(fd, user, password, args);
	}
	else if (!strcmp(command, "CREATE-ROOM")) {
		createRoom(fd, user, password, args);
	}
	else if (!strcmp(command, "LOGIN")) {
		login(fd, user, password, args);
	}
	else if (!strcmp(command, "LIST-ROOMS")) {
		listRooms(fd, user, password, args);
	}
	else if (!strcmp(command, "ENTER-ROOM")) {
		enterRoom(fd, user, password, args);
	}
	else if (!strcmp(command, "LEAVE-ROOM")) {
		leaveRoom(fd, user, password, args);
	}
	else if (!strcmp(command, "SEND-MESSAGE")) {
		sendMessage(fd, user, password, args);
	}
	else if (!strcmp(command, "GET-MESSAGES")) {
		getMessages(fd, user, password, args);
	}
	else if (!strcmp(command, "GET-USERS-IN-ROOM")) {
		getUsersInRoom(fd, user, password, args);
	}
	else if (!strcmp(command, "GET-ALL-USERS")) {
		getAllUsers(fd, user, password, args);
	}
	else {
		const char * msg =  "UNKNOWN COMMAND\r\n";
		write(fd, msg, strlen(msg));
	}

	close(fd);	
}

void
IRCServer::initialize()
{
	// Open password file
	FILE * passFile = fopen("password.txt","a+");

	// Initialize users in room
	char * temp = (char *) malloc(1024 * sizeof(char));
	char * user;
	char * password;
	char * t = temp;
	int last;
	int c;
	while ((c = fgetc(passFile)) != EOF) {
		if (c == ' ') {
			*t = 0;
			user = strdup(temp);
			t = temp;
			continue;
		}
		else if ((c == '\n') && (last == '\r')) {
			t--;
			*t = 0;
			password = strdup(temp);
			t = temp;
			if ((user != NULL) && (password != NULL))
				addUser(1, user, password, "initialize");
			continue;
		}
		*t = c;
		last = c;
		t++;
	}
	free(temp);
	fclose(passFile);
	// Initalize room list
	maxMessages = 100;
	maxRooms = 5;
	nRooms = 0;
	roomList = (ChatRoom *) malloc(maxRooms * sizeof(ChatRoom));
}

bool
IRCServer::checkPassword(int fd, const char * user, const char * password) {
	char * stored = (char *) malloc(sizeof(password)); //sets aside memory for the password that's stored for the user
	if(passwords.find(user, (void**) &stored)) {  //finds the user in the database
		if (!strcmp(password, stored)) //compares the password they typed in to the one in the database
			return true; //if it's correct, let them continue!
	}
	return false; //if not--you shall not pass!
}

void
IRCServer::addUser(int fd, const char * user, const char * password, const char * args) {
	// Here add a new user. For now always return OK.
	const char * msg;
	char ** temp;
	if(!checkPassword(fd, user, password)) {
		if(!passwords.insertItem(user, (void*) password)) {
			msg =  "OK\r\n";
			nUsers++;
		}
		else
			msg = "DENIED\r\n"; //if not, you're denied!
	}
	else
		msg = "DENIED\r\n"; //another deny for catch-all
	write(fd, msg, strlen(msg)); //sends the variable msg back through the network (either OK or DENIED)
	if (strcmp(args, "initialize") && !strcmp(msg, "OK\r\n")) { //I have a password file that I'm adding the users to, and this makes sure it's not already there
		FILE * passFile = fopen("password.txt","a+"); //open the password file with "append" priviledges
		fprintf(passFile, "%s %s\r\n", user, password); //write the username and password into the file
		fclose(passFile); //close the password file
	}
	return;	//exit the function!
}
void IRCServer::createRoom(int fd, const char * user, const char * password, const char * args) {
	FILE * fssock = fdopen(fd,"r+");
	if (!checkPassword(fd, user, password)) {
		fprintf(fssock,"ERROR (Wrong password)\r\n");
		fclose(fssock);
		return;
	}
	if (nRooms == maxRooms) {
		maxRooms = 2 * maxRooms;
		roomList = (ChatRoom *) realloc(roomList, maxRooms * sizeof(ChatRoom));
	}
	for (int i = 0; i < nRooms; i++) {
		if (!strcmp(args, roomList[i].name)) {
			fprintf(fssock,"DENIED\r\n");
			fclose(fssock);
			return;
		}
	}
	roomList[nRooms].name = strdup(args);
	roomList[nRooms].messages = (Message *) malloc(maxMessages * sizeof(Message));
	roomList[nRooms].nMessages = 0;
	roomList[nRooms].maxUsers = 10;
	roomList[nRooms].usersInRoom = (char **) malloc(roomList[nRooms].maxUsers * sizeof(char *));
	roomList[nRooms].nUsers = 0;
	nRooms++;
	fprintf(fssock,"OK\r\n");
	fclose(fssock);
}
void IRCServer::listRooms(int fd, const char * user, const char * password, const char * args) {
	FILE * fssock = fdopen(fd,"r+");
	if (!checkPassword(fd, user, password)) {
		fprintf(fssock,"ERROR (Wrong password)\r\n");
		fclose(fssock);
		return;
	}
	for (int i = 0; i < nRooms; i++) {
		fprintf(fssock,"%s\r\n", roomList[i].name);
	}
	fprintf(fssock,"\r\n");
	fclose(fssock);

}
void
IRCServer::enterRoom(int fd, const char * user, const char * password, const char * args) {
	FILE * fssock = fdopen(fd,"r+");
	if (!checkPassword(fd, user, password)) {
		fprintf(fssock,"ERROR (Wrong password)\r\n");
		fclose(fssock);
		return;
	}
	for (int i = 0; i < nRooms; i++) {
		if (!strcmp(args, roomList[i].name)) {
			if (roomList[i].nUsers == roomList[i].maxUsers) {
				roomList[i].maxUsers = 2 * roomList[i].maxUsers;
				roomList[i].usersInRoom = (char **) realloc(roomList[i].usersInRoom, roomList[i].maxUsers * sizeof(char *));
			}
			bool already = false;
			for (int j = 0; j < roomList[i].nUsers; j++) {
				if (!strcmp(roomList[i].usersInRoom[j], user)) {
					already = true;
					break;
				}
			}
			if (!already) {
				roomList[i].usersInRoom[roomList[i].nUsers] = strdup(user);
				roomList[i].nUsers++;
			}
			fprintf(fssock,"OK\r\n");
			fclose(fssock);
			return;
			
		}
	}
	fprintf(fssock,"ERROR (No room)\r\n");
	fclose(fssock);
	return;
}

void
IRCServer::leaveRoom(int fd, const char * user, const char * password, const char * args) {
	FILE * fssock = fdopen(fd,"r+");
	if (!checkPassword(fd, user, password)) {
		fprintf(fssock,"ERROR (Wrong password)\r\n");
		fclose(fssock);
		return;
	}
	for (int i = 0; i < nRooms; i++) {
		if (!strcmp(args, roomList[i].name)) {
			for (int j = 0; j < roomList[i].nUsers; j++) {
				if (!strcmp(roomList[i].usersInRoom[j], user)) {
					for (int b = j; b < roomList[i].nUsers - 1; b++) {
						char * temp = roomList[i].usersInRoom[b];
						roomList[i].usersInRoom[b] = roomList[i].usersInRoom[b+1];
						roomList[i].usersInRoom[b+1] = temp;
				
					}
					free(roomList[i].usersInRoom[--roomList[i].nUsers]);
					fprintf(fssock,"OK\r\n");
					fclose(fssock);
					return;
				}
			}
		}
	}
	fprintf(fssock,"ERROR (No user in room)\r\n");
	fclose(fssock);
	return;
}

void
IRCServer::sendMessage(int fd, const char * user, const char * password, const char * args) {
	FILE * fssock = fdopen(fd,"r+");
	if (!checkPassword(fd, user, password)) {
		fprintf(fssock,"ERROR (Wrong password)\r\n");
		fclose(fssock);
		return;
	}
	for (int i = 0; i < nRooms; i++) {
		if (strstr(args, roomList[i].name) == args) {
			if (roomList[i].nMessages == maxMessages) {
				roomList[i].nLists++;
			}
			for (int j = 0; j < roomList[i].nUsers; j++) {
				if (!strcmp(roomList[i].usersInRoom[j], user)) {
					roomList[i].messages[roomList[i].nMessages - (maxMessages * roomList[i].nLists)].message = strdup(args + (sizeof(char) * (strlen(roomList[i].name) + 1)));
					roomList[i].messages[roomList[i].nMessages - (maxMessages * roomList[i].nLists)].user = strdup(user);
					roomList[i].messages[roomList[i].nMessages - (maxMessages * roomList[i].nLists)].index = roomList[i].nMessages;
					roomList[i].nMessages++;
					fprintf(fssock,"OK\r\n");
					fclose(fssock);
					return;
				}
			}
			
		}
	}
	fprintf(fssock,"ERROR (user not in room)\r\n");
	fclose(fssock);
}

void
IRCServer::getMessages(int fd, const char * user, const char * password, const char * args) {
	FILE * fssock = fdopen(fd,"r+");
	if (!checkPassword(fd, user, password)) {
		fprintf(fssock,"ERROR (Wrong password)\r\n");
		fclose(fssock);
		return;
	}
	int lastMessageNum;
	int argLength = strlen(args);
	char * room;
	char * s = strdup(args);
	char * temp = (char *) malloc (argLength * sizeof(char));
	char * t = temp;
	while (*s) {
		if (*s == ' ') {
			*t = 0;
			lastMessageNum = atoi(temp);
			s++;
			room = strdup(s);
			break;
		}
		*t = *s;
		s++;
		t++;
	}
	int n = 0;
	for (int i = 0; i < nRooms; i++) {
		if (strcmp(room, roomList[i].name) == 0) {
			for (int j = 0; j < roomList[i].nUsers; j++) {
				if (!strcmp(roomList[i].usersInRoom[j], user)) {
					if (roomList[i].nLists && (lastMessageNum < roomList[i].nMessages - (maxMessages * roomList[i].nLists))) {
						lastMessageNum = roomList[i].nMessages - (maxMessages * roomList[i].nLists);
					}
					if (lastMessageNum >= maxMessages) {
						n++;
					}
					for (int j = lastMessageNum; j < roomList[i].nMessages; j++) { 
						if (j && ((j % maxMessages) == 0)) {
							n++;
						}
						fprintf(fssock,"%d %s %s\r\n", roomList[i].messages[j-(n*maxMessages)].index, roomList[i].messages[j-(n*maxMessages)].user, roomList[i].messages[j-(n*maxMessages)].message);

					}
					if (lastMessageNum >= roomList[i].nMessages) {
						fprintf(fssock,"NO-NEW-MESSAGES\r\n");
					}
					else
						fprintf(fssock,"\r\n");
					fclose(fssock);
					return;
				}
			}
		}
	}
	fprintf(fssock,"ERROR (User not in room)\r\n");
	fclose(fssock);
	return;
}

void
IRCServer::getUsersInRoom(int fd, const char * user, const char * password, const char * args) {
	FILE * fssock = fdopen(fd,"r+");
	if (!checkPassword(fd, user, password)) {
		fprintf(fssock,"ERROR (Wrong password)\r\n");
		fclose(fssock);
		return;
	}

	for (int i = 0; i < nRooms; i++) {
		if (!strcmp(args, roomList[i].name)) {
			for (int a = 0; a < roomList[i].nUsers; a++) {
				for (int b = 0; b < roomList[i].nUsers - 1; b++) {
					if (strcmp(roomList[i].usersInRoom[b],roomList[i].usersInRoom[b+1]) > 0) {
						char * temp = roomList[i].usersInRoom[b];
						roomList[i].usersInRoom[b] = roomList[i].usersInRoom[b+1];
						roomList[i].usersInRoom[b+1] = temp;
					}
				}
			}
			for (int j = 0; j < roomList[i].nUsers; j++) {
					fprintf(fssock,"%s\r\n",roomList[i].usersInRoom[j]);
			}
			fprintf(fssock,"\r\n");
			fclose(fssock);
			return;
		}
	}
	
	fprintf(fssock,"DENIED\r\n");
	fclose(fssock);
	return;
}

void
IRCServer::getAllUsers(int fd, const char * user, const char * password, const  char * args) {
	HashTableVoidIterator iterator(&passwords);
	FILE * fssock = fdopen(fd,"r+");
	if (!checkPassword(fd, user, password)) {
		fprintf(fssock,"ERROR (Wrong password)\r\n");
		fclose(fssock);
		return;
	}
	const char * key;
	void * data;
	char ** users = (char **) malloc(nUsers * sizeof(char *));
	int n = 0;
	while (iterator.next(key, data)) {
		users[n++] = strdup(key);
	}
	for (int i = 0; i < nUsers; i++) {
		for (int j = 0; j < nUsers - 1; j++) {
			if (strcmp(users[j],users[j+1]) > 0) {
				char * temp = users[j];
				users[j] = users[j+1];
				users[j+1] = temp;
			}
		}
	}
	for (int i = 0; i < nUsers; i++) {
		fprintf(fssock,"%s\r\n",users[i]);
		free(users[i]);
	}
	free(users);
	fprintf(fssock,"\r\n");
	fclose(fssock);
}
void
IRCServer::login(int fd, const char * user, const char * password, const char * args) {
	FILE * fssock = fdopen(fd,"r+");
	const char * msg;
	if (checkPassword(fd, user, password)) {
		msg = "OK\r\n";
	}
	else
		msg = "DENIED\r\n"; //another deny for catch-all
	//sends the variable msg back through the network (either OK or DENIED)
	fprintf(fssock, "%s", msg);
	fclose(fssock);
	return;	//exit the function!
}

