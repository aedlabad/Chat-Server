
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

void list_init_user(UserList *list);
	
void list_init_rooms(RoomList *list);

void add_user(UserList *list, char *username, char *password);

int user_exists(UserList *list, char *userName);

void add_room(RoomList *list, char *roomName);

int room_exists(RoomList *list, char *roomName);

void add_user_to_room(RoomList *rlist, UserList *ulist, char *roomname, char *username); 
