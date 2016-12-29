
const char * usage =
"                                                               \n"
"IRCServer:                                                   \n"
"                                                               \n"
"Simple server program used to communicate multiple users       \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   IRCServer <port>                                          \n"
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
#include "HashTableVoid.h"

int QueueLength = 5;
FILE *fl;

struct UserNode {
	char *username;
	char *password;
	char *chatroom[100];
	int roomcount;
	struct UserNode *next;
};

typedef struct UserNode UserNode;

struct RoomNode {
	char *roomName;
	char *messages[100];
	char *users[100];
	int messageCount;
	int userCount;
	struct RoomNode *next;
};

typedef struct RoomNode RoomNode;

struct UserList {
	UserNode *head;
};

typedef struct UserList UserList;

struct RoomList {
	RoomNode *head;
};

typedef struct RoomList RoomList;

void list_init_user(UserList *list) {
	list->head = NULL;
}

void list_init_rooms(RoomList *list) {
	list->head = NULL;
}

UserList *ulist;
RoomList *rlist;

//convert all characters to lower case.
void toLower(char *s) {	
	int i = 0;
	while (s[i] != '\0' ) {
		char ch = s[i];
		if (ch>='A'&&ch<='Z') {
			ch = (ch-'A')+'a';
			s[i]=ch;
		}
		i++;
	}    

}



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
	// have to wait about 2 minutes before reusing the same port number
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
//   Commands are started y the client.
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

	void
IRCServer::processRequest( int fd )
{
	// Buffer used to store the comand received from the client
	const int MaxCommandLine = 102;	//char commandLine[ MaxCommandLine + 1 ];
	char *commandLine = (char *)malloc(sizeof(char)*(MaxCommandLine+1)); 
	int commandLineLength = 0;
	char *word[20];
	int j = 0;
	int i = 0;

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

	//printf("The commandLine has the following format:\n");
	//printf("COMMAND <user> <password> <arguments>. See below.\n");
	//printf("You need to separate the commandLine into those components\n");
	//printf("For now, command, user, and password are hardwired.\n");

	const char * command = strtok(commandLine, " ");
	const char * user = strtok(NULL, " ");
	const char * password = strtok(NULL, " ");
	const char * args = strtok(NULL, "");
	/*const char * command = word[0];
	  const char * user = word[1];
	  const char * password = word[2];
	  const char * args = word[3];*/

	printf("command=%s\n", command);
	printf("user=%s\n", user);
	printf( "password=%s\n", password);
	printf("args=%s\n", args);

	if (!strcmp(command, "ADD-USER")) {
		//printf("Understood\n");
		addUser(fd, user, password, args);
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
	else if (!strcmp(command, "LIST-ROOMS")) {
		listRooms(fd, user, password, args);
	}
	else if (!strcmp(command, "CREATE-ROOM")) {
		createRoom(fd, user, password, args);
	}
	else {
		const char * msg =  "UNKNOWN COMMAND\r\n";
		write(fd, msg, strlen(msg));
	}

	// Send OK answer
	//const char * msg =  "OK\n";
	//write(fd, msg, strlen(msg));

	close(fd);	
}

void
IRCServer::initialize() {
	// Open password file

	// Initialize users in room

	// Initalize message list
	ulist = (UserList *) malloc(sizeof(UserList));
	rlist = (RoomList *) malloc(sizeof(RoomList));
	list_init_user(ulist);
	list_init_rooms(rlist);


}


void add_user(UserList *list, char *username, char *password) {
	UserNode *n = (UserNode *)malloc (sizeof(UserNode));
	//	n->username = (char *)malloc(sizeof(char)*100);
	n->username = strdup(username);
	n->password = strdup(password);
	n->roomcount = 0;
	n->next = list->head;
	list->head = n;
}

int user_exists(UserList *list, char *username) {

	UserNode *e;
	if ( list->head == NULL) {
		return 0;
	}
	else {
		e = list->head;
		while ( e!=NULL) {
			if ( strcmp(e->username,username) == 0 ) 
				return 1;
			e = e->next;

		}
	}
	return 0;
}

void add_room(RoomList *list, char *roomName) {

	RoomNode *n = (RoomNode *)malloc (sizeof(RoomNode));
	n->roomName = roomName;
	n->messageCount = 0;
	n->userCount = 0;
	n->next = list->head;
	list->head = n;
}

int room_exists(RoomList *list, char *roomName) {

	RoomNode *e ;
	if ( list->head == NULL) {
		return 0;
	}
	else {
		e = list->head;
		while ( e!=NULL) {
			if ( strcmp(e->roomName,roomName) == 0 ) 
				return 1;
			e = e->next;

		}
	}
	return 0;
}

void add_user_to_room(UserList *ulist, char *roomname, char *username) {

	UserNode *e = (UserNode *)malloc ( sizeof(UserNode));
	e = ulist->head;
	while ( e!=NULL) {
		if ( strcmp(e->username,username) == 0) {
			e->chatroom[e->roomcount] = roomname;
			e->roomcount++;
			return;
		}
		e = e->next;
	}	
}

bool
IRCServer::checkPassword(int fd, const char * user, const char * password) {
	// Here check the password
	//check whether user exists
	//UserList *list;
	//UserList *list = (UserList *) malloc(sizeof(UserList));
	char *username = strdup(user);
	int check = user_exists(ulist, username);
	if ( check == 0)
		return false;
	if ( check == 1) {
		UserNode *e;
		e = ulist->head;
		while ( e!=NULL) {
			if (strcmp(e->username, user) == 0) {	
				if ( strcmp(e->password, password) == 0) {
					return true;
				}
				else return false;
			}
			e = e->next;	
		}
	}
	return false;
}
	void
IRCServer::addUser(int fd, const char * user, const char * password, const char * args)
{
	// Here add a new user. For now always return OK.

	//UserList *list = (UserList *) malloc(sizeof(UserList));
	char *username = strdup(user);
	char *password1 = strdup(password);
	add_user(ulist, username, password1); 
	const char * msg =  "OK\r\n";
	//const char * check =  "HI\r\n";
	write(fd, msg, strlen(msg));
	fl = fopen("password.txt","w");
	fprintf(fl, "%s %s\n", user, password);
	//write(fd, check, strlen(check));

	return;		
}
	void
IRCServer::createRoom(int fd, const char * user, const char * password, const char * args)
{
	const char *error = "ERROR (Wrong password)\r\n";
	const char *msg = "OK\r\n";
	char *username = strdup(user);
	char *password1 = strdup(password);
	char *argu = strdup(args);
	bool res = checkPassword(fd, username, password1);

	if (res == false) {
		write(fd, error, strlen(error));
		return;
	}

	if (room_exists(rlist, argu)) {
		//write(fd,error,strlen(error));
		return;
	}

	add_room(rlist, argu);
	write(fd,msg,strlen(msg));


}

	void
IRCServer::enterRoom(int fd, const char * user, const char * password, const char * args)
{

	const char *error = "DENIED\r\n";
	const char *msg = "OK\r\n";
	const char *badpassword = "ERROR (Wrong password)\r\n";
	const char *noUser = "ERROR (user not in room)\r\n";
	const char *noRoom = "ERROR (No room)\r\n";
	//UserList *ulist;
	//UserList *ulist = (UserList *) malloc(sizeof(UserList));
	//RoomList *rlist = (RoomList *) malloc(sizeof(RoomList));
	//RoomList *rlist;
	char *username = strdup(user);
	char *password1 = strdup(password);
	char *argu = strdup(args);
	bool res = checkPassword(fd, username, password1);

	if (res == false) {
		write(fd, badpassword, strlen(badpassword));
		return;
	}

	if (!room_exists(rlist, argu)) {
		//add_user_to_room(ulist, argu, username);
		write(fd,noRoom,strlen(noRoom));
	}
	else {
		//add_room(rlist, argu);
		add_user_to_room(ulist, argu, username);
		write(fd,msg,strlen(msg));
	}



}

	void
IRCServer::leaveRoom(int fd, const char * user, const char * password, const char * args)
{
	const char *noUser = "ERROR (No user in room)\r\n";
	const char *error = "DENIED\r\n";
	const char *msg = "OK\r\n";
	const char *badpassword = "ERROR (Wrong password)\r\n";
	//const char *noUser = "ERROR (user not in room)\r\n";
	const char *noRoom = "ERROR (no room)\r\n";
	//const char *check = "NOT PRESENT IN ROOM\r\n";
	//UserList *ulist;
	//RoomList *rlist;
	//UserList *ulist = (UserList *) malloc(sizeof(UserList));
	//RoomList *rlist = (RoomList *) malloc(sizeof(RoomList));
	char *username = strdup(user);
	char *password1 = strdup(password);
	char *argu = strdup(args);
	bool res = checkPassword(fd, user, password);

	if (res == false) {
		write(fd, badpassword, strlen(badpassword));
		return;
	}
	int a = room_exists(rlist, argu);
	if (a!= 1) {
		write(fd, noRoom, strlen(noRoom));
		return;
	}

	UserNode *e;
	int flag = 0;
	if ( ulist->head == NULL) {
		return;
	}
	e = ulist->head;
	while ( e!=NULL) {
		if ( strcmp(e->username,username) == 0 ) {
			int i;
			if ( e->roomcount == 0 ) {
				write(fd, strdup(noUser), strlen(noUser));
				break;
			}
			for( i = 0; i < e->roomcount;i++ ) {
				if ( strcmp(e->chatroom[i],argu) == 0) {
					flag = 1;
					int j;
					for ( j = i; j<e->roomcount-1; j++) {
						e->chatroom[j] = e->chatroom[j+1];
					}
					e->roomcount--;
					write(fd,msg,strlen(msg));
					return;
				}
			}	
			if (flag == 0) {
				write(fd,noUser,strlen(noUser));
				return;
			}

		}
		e = e->next;

	}



}

	void
IRCServer::sendMessage(int fd, const char * user, const char * password, const char * args)
{

	const char *error = "DENIED\r\n";
	const char *msg = "OK\r\n";
	const char *badpassword = "ERROR (Wrong password)\r\n";
	const char *noUser = "ERROR (user not in room)\r\n";
	const char *noRoom = "ERROR (no room)\r\n";
	//UserList *ulist;
	//RoomList *rlist;
	//UserList *ulist = (UserList *) malloc(sizeof(UserList));
	//RoomList *rlist = (RoomList *) malloc(sizeof(RoomList));
	int x = 0;
	int y = 0;
	char *word[3];
	char *username = strdup(user);
	char *password1 = strdup(password);
	char *argu = strdup(args);
	char *a = argu;

	char *room = strtok(argu, " ");
	char *message = strtok(NULL, "");

	bool res = checkPassword(fd, username, password1);

	if (res == false) {
		write(fd, badpassword, strlen(badpassword));
		return;
	}

	if (!room_exists(rlist, argu)) {
		write(fd,noRoom,strlen(noRoom));
		return;
	}

	UserNode *e;
	int flag = 0;
	if ( ulist->head == NULL) {
		return;
	}

	e = ulist->head;
	while ( e!=NULL) {
		if ( strcmp(e->username,username) == 0 ) {
			
			int i;
			
			if ( e->roomcount == 0 ) {
				write(fd, strdup(noUser), strlen(noUser));
				return;
			}

			for( i = 0; i < e->roomcount;i++ ) {
				if ( strcmp(e->chatroom[i],argu) == 0) {
					flag = 1;
					break;
				}
			}
			
			if (flag == 0) {
				write(fd,noUser,strlen(noUser));
				return;


			}
		}

		e = e->next;

	}

	RoomNode *n;
	n = rlist->head;
	while ( n!=NULL) {

		if ( strcmp(n->roomName,room) == 0) {

			if ( n->messageCount >= 100 ) {
				int i;
				for ( i = 0; i < 99; i++ ) {
					n->messages[i] = n->messages[i+1];
					n->users[i] = n->users[i+1];
				}
				n->messages[99] = message;
				n->users[99] = username;
				return;;
			}	

			else { 
				n->messages[n->messageCount] = message;
				n->users[n->userCount] = username;
				n->messageCount++;
				n->userCount++;
				write(fd,msg,strlen(msg));
				return;
			}
		}	

		n = n->next;
	}



}

	void
IRCServer::getMessages(int fd, const char * user, const char * password, const char * args)
{

	const char *error = "DENIED\r\n";
	const char *msg = "OK\r\n";
	const char *indent = "\r\n";  
	const char *badpassword = "ERROR (Wrong password)\r\n";
	const char *noUser = "ERROR (User not in room)\r\n";
	const char *noRoom = "ERROR (no room)\r\n";
	const char *noMessages = "NO-NEW-MESSAGES\r\n";
	//UserList *ulist;
	//RoomList *rlist;
	//UserList *ulist = (UserList *) malloc(sizeof(UserList));
	//RoomList *rlist = (RoomList *) malloc(sizeof(RoomList));
	int number;
	char *room;
	char *username = strdup(user);
	char *password1 = strdup(password);
	char *argu = strdup(args);
	bool res = checkPassword(fd, username, password1);

	if (res == false) {
		write(fd, badpassword, strlen(badpassword));
		return;
	}
	number = atoi(strtok(argu, " " ));
	room = strtok(NULL, "");


	if (!room_exists(rlist, room)) {
		write(fd,noRoom,strlen(noRoom));
		return;
	}
	UserNode *e;
	int flag = 0;
	if ( ulist->head == NULL) {
		return;
	}
	e = ulist->head;
	while ( e!=NULL) {
		if ( strcmp(e->username,username) == 0 ) {
			int i;
			if ( e->roomcount == 0 ) {
				write(fd, strdup(noUser), strlen(noUser));
				return;
			}
			for( i = 0; i < e->roomcount;i++ ) {
				if ( strcmp(e->chatroom[i],room) == 0) {
					flag = 1;
					break;
				}
			}
			if (flag == 0) {
				write(fd,noUser,strlen(noUser));
				return;


			}
		}
		e = e->next;

	}

	const char *space = " ";
	RoomNode *n = (RoomNode *)malloc(sizeof(RoomNode));
	n = rlist->head;
	char *output = (char *) malloc(sizeof(char)*20);
	while ( n!=NULL) {
		if ( strcmp(n->roomName,room) == 0) {
			int c;
			if ( n->messageCount == 0 ) {
				write(fd, noMessages, strlen(noMessages));
				return;
			}
			if ( number >= n->messageCount ) {
				write(fd, noMessages, strlen(noMessages));
				return;
			}
			for ( c = number; c < n->messageCount; c++) {
				output = n->messages[c];
				char check[15];
				sprintf(check, "%d", c);

				write ( fd, check, strlen(check));
				write ( fd, space, strlen(space));
				write(fd, n->users[c], strlen(n->users[c]));
				write(fd, space, strlen(space));
				write(fd, output, strlen(output));
				write(fd, indent, strlen(indent));
			}
		}
		n = n->next;
	}
	write(fd, indent, strlen(indent));
}

	void
IRCServer::getUsersInRoom(int fd, const char * user, const char * password, const char * args)
{

	const char *error = "DENIED\r\n";
	const char *badpassword = "ERROR (Wrong password)\r\n";
	const char *noUser = "ERROR (No user in room)\r\n";
	const char *noRoom = "ERROR (no room)\r\n";
	const char *msg = "OK\r\n";
	const char *indent = "\r\n";  

	char *username = strdup(user);
	char *password1 = strdup(password);
	if ( args == NULL ) {
		write(fd, error, strlen(error));
	}
	char *room = strdup(args);
	char *temp = (char *)malloc(sizeof(char)*20);
	bool res = checkPassword(fd,username, password1);

	if (!res ) {
		write(fd, badpassword, strlen(badpassword));
		return;
	}
	if (!room_exists(rlist, room)) {
		write(fd,noRoom,strlen(noRoom));
		return;
	}
	int counter = 0;
	char *a[100];
	UserNode *e = (UserNode *)malloc(sizeof(UserNode));
	e = ulist->head;
	int i;
	while ( e!=NULL) {
		for ( i = 0; i < e->roomcount; i++) {
			if ( e->roomcount == 0 ) 
				break;

			if ( strcmp(strdup(e->chatroom[i]), room) == 0) {
				a[counter] = strdup(e->username);
				counter++;
				break;
			}
		}
		e = e->next;
	}
	i = 0;
	int j;
	while ( i < counter-1 ) {
		j = i+1;
		while ( j < counter ) {
			if ( strcmp(a[i],a[j]) > 0 ) {
				temp = a[i];
				a[i] = a[j];
				a[j] = temp;
			}
			j++;
		}
		i++;
	}

	for ( j = 0; j < counter; j++) {
		write(fd, a[j], strlen(a[j]));
		write(fd, indent, strlen(indent));
	}

	write(fd, indent, strlen(indent));


}

	void
IRCServer::getAllUsers(int fd, const char * user, const char * password,const  char * args)
{


	const char *badpassword = "ERROR (Wrong password)\r\n";
	const char *msg = "OK\r\n";
	const char *noUser = "ERROR (user not in room)\r\n";
	const char *indent = "\r\n";  
	const char *noRoom = "ERROR (no room)\r\n";


	char *username = strdup(user);
	char *password1 = strdup(password);
	bool res = checkPassword(fd, username, password1);

	if (res == false) {
		write(fd, badpassword, strlen(badpassword));
		return;
	}
	if ( ulist->head == NULL ) {
		write(fd, noUser, strlen(noUser));
	}


	UserNode *e;
	char *temp = (char *)malloc(sizeof(char)*20);
	e = ulist->head;
	char *a[100]; 
	int counter = 0;
	while ( e!= NULL ) {
		a[counter] = e->username;
		e = e->next;
		counter++;
	}
	int i = 0;
	int j;
	while ( i < counter-1 ) {
		j = i+1;
		while ( j < counter ) {
			if ( strcmp(a[i],a[j]) > 0 ) {
				temp = a[i];
				a[i] = a[j];
				a[j] = temp;
			}
			j++;
		}
		i++;
	}

	for ( i = 0; i < counter; i++) {
		write(fd, a[i], strlen(a[i]));
		write(fd, strdup(indent), strlen(indent));
	}
	write(fd, "\r\n", strlen("\r\n"));



}

	void
IRCServer::listRooms(int fd, const char * user, const char * password,const  char * args)
{


	const char *error = "DENIED\r\n";
	const char *msg = "OK\r\n";
	const char *indent = "\r\n";  


	//UserList *list = (UserList *) malloc(sizeof(UserList));
	//UserList *list;
	char *username = strdup(user);
	char *password1 = strdup(password);
	//char *room = strdup(args);
	bool res = checkPassword(fd, username, password1);

	if (res == false) {
		write(fd, error, strlen(error));
		return;
	}
	if ( rlist->head == NULL ) {
		write(fd, error, strlen(error));
	}


	RoomNode *e;
	char *temp = (char *)malloc(sizeof(char)*20);
	e = rlist->head;
	char *a[100]; 
	int counter = 0;
	while ( e!= NULL ) {
		a[counter] = e->roomName;
		e = e->next;
		counter++;
	}
	int i = 0;
	int j;
	while ( i < counter-1 ) {
		j = i+1;
		while ( j < counter ) {
			if ( strcmp(a[i],a[j]) > 0 ) {
				temp = a[i];
				a[i] = a[j];
				a[j] = temp;
			}
			j++;
		}
		i++;
	}

	for ( i = 0; i < counter; i++) {
		write(fd, a[i], strlen(a[i]));
		write(fd, indent, strlen(indent));
	}


}
