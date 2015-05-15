
#include <time.h>
#include <curses.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gi18n.h>

char * host;
char * user;
char * password;
char * sport;
char * currentRoom;
int port;
pthread_t thread;
GtkWidget *tree_view;
GtkWidget *text_view;
GtkWidget *text_entry;
GtkTextBuffer *messageBuffer;
GtkTextBuffer *sendBuffer;
GtkTreeStore *treeModel;
gboolean continueRefresh;
#define MAX_MESSAGES 100
#define MAX_MESSAGE_LEN 300
#define MAX_RESPONSE (20 * 1024)

int lastMessage = 0;

int open_client_socket(char * host, int port) {
	// Initialize socket address structure
	struct  sockaddr_in socketAddress;
	
	// Clear sockaddr structure
	memset((char *)&socketAddress,0,sizeof(socketAddress));
	
	// Set family to Internet 
	socketAddress.sin_family = AF_INET;
	
	// Set port
	socketAddress.sin_port = htons((u_short)port);
	
	// Get host table entry for this host
	struct  hostent  *ptrh = gethostbyname(host);
	if ( ptrh == NULL ) {
		perror("gethostbyname");
		exit(1);
	}
	
	// Copy the host ip address to socket address structure
	memcpy(&socketAddress.sin_addr, ptrh->h_addr, ptrh->h_length);
	
	// Get TCP transport protocol entry
	struct  protoent *ptrp = getprotobyname("tcp");
	if ( ptrp == NULL ) {
		perror("getprotobyname");
		exit(1);
	}
	
	// Create a tcp socket
	int sock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}
	
	// Connect the socket to the specified server
	if (connect(sock, (struct sockaddr *)&socketAddress,
		    sizeof(socketAddress)) < 0) {
		perror("connect");
		exit(1);
	}
	
	return sock;
}

int sendCommand(char * host, int port, char * command, char * user1,
		char * password1, char * args, char * response) {
	int sock = open_client_socket( host, port);

	// Send command
	write(sock, command, strlen(command));
	write(sock, " ", 1);
	write(sock, user1, strlen(user1));
	write(sock, " ", 1);
	write(sock, password1, strlen(password1));
	write(sock, " ", 1);
	write(sock, args, strlen(args));
	write(sock, "\r\n",2);

	// Keep reading until connection is closed or MAX_REPONSE
	int n = 0;
	int len = 0;
	while ((n=read(sock, response+len, MAX_RESPONSE - len))>0) {
		len += n;
	}

	close(sock);
}

void printUsage()
{
	printf("Usage: talk-client host port user password\n");
	exit(1);
}

int add_user() {
	// Try first to add user in case it does not exist.
	char response[ MAX_RESPONSE ];
	sendCommand(host, port, "ADD-USER", user, password, "", response);
	char * newResponse = (char *) malloc(strlen(response)*sizeof(char));
	char * s = newResponse;
	char * response1 = response;
	while (*response1 != '\n') {
		*s = *response1;
		response1++;
		s++;
	}
	*s = '\n';
	s++;
	*s = '\0';
	if (!strcmp(response,"OK\r\n")) {
		printf("User %s added\n", user);
		return 1;
	}
	return 0;
}
int login() {
	char response[ MAX_RESPONSE ];
	sendCommand(host, port, "LOGIN", user, password, "", response);
	printf("%s\n", response);
	char * newResponse = (char *) malloc(strlen(response)*sizeof(char));
	char * s = newResponse;
	char * response1 = response;
	while (*response1 != '\n') {
		*s = *response1;
		response1++;
		s++;
	}
	*s = '\n';
	s++;
	*s = '\0';
	if (!strcmp(newResponse,"OK\r\n")) {
		printf("User %s exists\n", user);
		free(newResponse);
		return 1;
	}
	free(newResponse);
	return 0;
}

void listRooms() {
	GtkTreeIter toplevel, child;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    struct Room {
    	char * name;
    	char ** usersInRoom;
    	int nUsers;
    };
    typedef struct Room Room;
    Room * roomArray = (Room *) malloc(MAX_RESPONSE * sizeof(Room));
	char * response = (char *) malloc(MAX_RESPONSE * sizeof(char));
	char * responsePoint = response;
	char * msg = (char *) malloc(MAX_RESPONSE * sizeof(char));
	char * s = msg;
	int nRooms = 0;
	gboolean first = TRUE;
	gtk_tree_store_clear(treeModel);
	gboolean iterFirst = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(treeModel), &toplevel);
	sendCommand(host, port, "LIST-ROOMS", user, password, "", response);

	while (responsePoint) {
		*s = *responsePoint;
		if (*s == '\r') {
			*s = 0;
			if(strlen(msg) <= 0) {
				break;
			}
			int newEntry = 0;
			roomArray[nRooms].name = strdup(msg);
			roomArray[nRooms].usersInRoom = (char **) malloc(MAX_RESPONSE * sizeof(char*));
        	s = msg;
        	responsePoint++;
        	responsePoint++;
        	char * response2 = (char *) malloc(MAX_RESPONSE * sizeof(char));
        	char *responsePoint2 = response2;
        	sendCommand(host, port, "GET-USERS-IN-ROOM", user, password, msg, response2);
        	roomArray[nRooms].nUsers = 0;
        	while (responsePoint2) {
        		*s = *responsePoint2;
				if (*s == '\r') {
					*s = 0;
					if(strlen(msg) <= 0) {
						break;
					}
					roomArray[nRooms].usersInRoom[roomArray[nRooms].nUsers++] = strdup(msg);
		        	s = msg;
		        	responsePoint2++;
		        	responsePoint2++;
		        	continue;
		        }
		        s++;
		        responsePoint2++;
		    }
		    free(response2);
		    nRooms++;
		    continue;
		}
		s++;
		responsePoint++;
	}
	
	free(response);
	char * tempString;
	gboolean parentExists;
	gboolean childExists;
	GtkTreeIter parent;
		for (int i = 0; i < nRooms; i++) {
				gtk_tree_store_append (treeModel, &toplevel, NULL);
				gtk_tree_store_set (treeModel, &toplevel, 0, roomArray[i].name, -1);

			for (int j = 0; j < roomArray[i].nUsers; j++) {
					gtk_tree_store_append (treeModel, &child, &toplevel);
					gtk_tree_store_set (treeModel, &child, 0, roomArray[i].usersInRoom[j], -1);
					free(roomArray[i].usersInRoom[j]);
			}
			free(roomArray[i].name);
		}
		return;
	
	free(msg);
	free(roomArray);
}

void enterRoom(char * roomName) {
	char response[ MAX_RESPONSE ];
	sendCommand(host, port, "ENTER-ROOM", user, password, roomName, response);
	if (!strcmp(response,"OK\r\n")) {
		printf("User %s added to %s\n", user, roomName);
	}
}

void leaveRoom(GtkWidget * widget) {
	continueRefresh = FALSE;

	GtkTextIter start;
	GtkTextIter end;
    messageBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
    gtk_text_buffer_get_start_iter (messageBuffer, &start);
    gtk_text_buffer_get_end_iter (messageBuffer, &end);
    gtk_text_buffer_delete (messageBuffer, &start, &end);

	char response[ MAX_RESPONSE ];
	char message[ MAX_RESPONSE ];
	sprintf(message, "%s SYSTEM %s has left the room", currentRoom, user);
	sendCommand(host, port, "SEND-MESSAGE", user, password, message, response);
	sendCommand(host, port, "LEAVE-ROOM", user, password, currentRoom, response);
	if (!strcmp(response,"OK\r\n")) {
		printf("User %s left room %s\n", user, currentRoom);
	}
	
}

void getMessages() {
	char response[ MAX_RESPONSE ];
	char args[ MAX_RESPONSE ];
	if (lastMessage == -1)
		sprintf(args, "%d %s", 0, currentRoom);
	else
		sprintf(args, "%d %s", lastMessage + 1, currentRoom);
	sendCommand(host, port, "GET-MESSAGES", user, password, args, response);

	if (!strcmp(response,"NO NEW MESSAGES\r\n")) {
		return;
	}
	char * final = (char *) malloc(MAX_RESPONSE*sizeof(char));
	char * responsePoint = response;
	char * userSent = (char *) malloc(MAX_RESPONSE*sizeof(char));
	char * line;
	char * tempMessage = (char *) malloc(MAX_RESPONSE*sizeof(char));
	char * timestr = (char *) malloc(10*sizeof(char));
	int charCount;
	int messageNum;
	line = strtok(responsePoint, "\r\n");
	
	strcpy(final, "");
	while (line != NULL) {
		if (sscanf(line, "%d %s %s%n", &lastMessage, userSent, timestr, &charCount) < 2) {
			break;
		}

		line += charCount;
		if (!strcmp(timestr, "SYSTEM")) {
			sprintf(tempMessage, "         %s\r\n", line);
		}
		else 
			sprintf(tempMessage, "[%s] <%s> %s\r\n", timestr, userSent, line);
		strcat(final, tempMessage);
		line = strtok(NULL, "\r\n");
		
	}



	GtkTextIter start;
	GtkTextIter end;
    messageBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
    gtk_text_buffer_get_start_iter (messageBuffer, &start);
    gtk_text_buffer_get_end_iter (messageBuffer, &end);

    gtk_text_buffer_insert (messageBuffer, &end, (gchar*) final, -1);
    free(final);
    free(userSent);
    free(tempMessage);
}
char * timestr() {
	char * buffer = (char*) malloc(20*sizeof(char));
  	
  	time_t curtime;
  	struct tm *loctime;

  	curtime = time(NULL);
  	loctime = localtime(&curtime);
  	strftime(buffer, 256, "%T", loctime);
  	return buffer;
}

void sendMessage(GtkWidget * widget) {
	GtkTextIter start;
	GtkTextIter end;
	sendBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW(text_entry));
	gtk_text_buffer_get_start_iter(sendBuffer, &start);
	gtk_text_buffer_get_end_iter(sendBuffer, &end);

	char * getText = (char *) gtk_text_buffer_get_text(sendBuffer, &start, &end, FALSE);
	char * message = (char *) malloc(MAX_RESPONSE*sizeof(char));
	char * timestamp = timestr();
	sprintf(message, "%s %s %s", currentRoom, timestamp, getText);
	char response[ MAX_RESPONSE ];
	sendCommand(host, port, "SEND-MESSAGE", user, password, message, response);
	
	if (!strcmp(response,"OK\r\n")) {
		printf("Message sent\n", user);
	}
	gtk_text_buffer_delete(sendBuffer, &start, &end);
	getMessages();
	g_free(getText);
	free(message);
	free(timestamp);
}
gboolean on_key_press (GtkWidget * widget, GdkEventKey* pKey) {
	if (pKey->type == GDK_KEY_PRESS) {
		if (pKey->keyval == GDK_KEY_Return) {
			sendMessage(widget);
			return TRUE;
		}

	}
	return FALSE;
}

void createRoom(GtkWidget * widget, GtkWidget *mainWindow) {
	GtkWidget * dialog = gtk_dialog_new_with_buttons ("Create Room",
		GTK_WINDOW(mainWindow), 
		(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
		_("_OK"), GTK_RESPONSE_ACCEPT, _("_Cancel"),
		GTK_RESPONSE_REJECT, NULL);

	GtkWidget * contentArea = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	GtkWidget * roomLabel = gtk_label_new ("Enter Room Name");
	GtkWidget * roomName = gtk_entry_new();

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_entry_set_activates_default(GTK_ENTRY(roomName), TRUE);

	gtk_container_add(GTK_CONTAINER(contentArea), roomLabel);
	gtk_container_add(GTK_CONTAINER(contentArea), roomName);

	gtk_widget_show_all(dialog);
	GtkResponseType result = gtk_dialog_run(GTK_DIALOG(dialog));

	if (result == GTK_RESPONSE_ACCEPT) {
		char * room = gtk_entry_get_text(GTK_ENTRY(roomName));
		char response[ MAX_RESPONSE ];
		sendCommand(host, port, "CREATE-ROOM", user, password, room, response);
	
		if (!strcmp(response,"OK\r\n")) {
			printf("Created room %s", room);
		}
	}
	listRooms();
	gtk_widget_destroy(dialog);

}

void createAccount(GtkWidget * window) {
	GtkWidget * dialog = gtk_dialog_new_with_buttons ("Create Account",
		NULL, (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
		_("_Create Account"), 1, _("_Login"), 2, NULL);

	GtkWidget * contentArea = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	GtkWidget * userLabel = gtk_label_new ("Username");
	GtkWidget * userName = gtk_entry_new();
	GtkWidget * passwordLabel = gtk_label_new("Password");
	GtkWidget * passwordEntry = gtk_entry_new();

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	gtk_container_add(GTK_CONTAINER(contentArea), userLabel);
	gtk_container_add(GTK_CONTAINER(contentArea), userName);
	gtk_container_add(GTK_CONTAINER(contentArea), passwordLabel);
	gtk_container_add(GTK_CONTAINER(contentArea), passwordEntry);

	gtk_widget_show_all(dialog);
	while(1) {
		GtkResponseType result = gtk_dialog_run(GTK_DIALOG(dialog));

		if (result == 1) {
			user = strdup(gtk_entry_get_text(GTK_ENTRY(userName)));
			password = strdup(gtk_entry_get_text(GTK_ENTRY(passwordEntry)));
			printf("%s %s\n", user, password);
			if (add_user()) {
				gtk_widget_destroy(dialog);
				gtk_widget_show(window);
				return;
			}
			else {
				GtkWidget * errorDialog = gtk_message_dialog_new(GTK_WINDOW(dialog),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_WARNING,
					GTK_BUTTONS_OK,
					"ERROR: User exists");
				gtk_window_set_title(GTK_WINDOW(errorDialog), "Error");
				gtk_dialog_run(GTK_DIALOG(errorDialog));
				gtk_widget_destroy(errorDialog);
			}
		}

		if (result == 2) {
			user = strdup(gtk_entry_get_text(GTK_ENTRY(userName)));
			password = strdup(gtk_entry_get_text(GTK_ENTRY(passwordEntry)));
			if (login()) {
				gtk_widget_destroy(dialog);
				gtk_widget_show(window);
				return;
			}
			else {
				GtkWidget * errorDialog = gtk_message_dialog_new(GTK_WINDOW(dialog),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_WARNING,
					GTK_BUTTONS_OK,
					"ERROR: User doesn't exist");
				gtk_window_set_title(GTK_WINDOW(errorDialog), "Error");
				gtk_dialog_run(GTK_DIALOG(errorDialog));
				gtk_widget_destroy(errorDialog);
			}
		}

	}

}

void logout(GtkWidget * widget, GtkWidget * window) {
	continueRefresh = FALSE;
	free(user);
	free(password);
	gtk_widget_hide(window);
	createAccount(window);
}

void printPrompt() {
	printf("talk> ");
	fflush(stdout);
}

void printHelp() {
	printf("Commands:\n");
	printf(" -who   - Gets users in room\n");
	printf(" -users - Prints all registered users\n");
	printf(" -help  - Prints this help\n");
	printf(" -quit  - Leaves the room\n");
	printf("Anything that does not start with \"-\" will be a message to the chat room\n");
}

gboolean refreshFunc(GtkWidget *widget) {
	listRooms();
	getMessages();
	return continueRefresh;
}

void startGetMessageThread()
{
	g_timeout_add_seconds(5, (GSourceFunc) refreshFunc, (gpointer) text_view);
}

static GtkWidget *create_text1()
{
   GtkWidget *scrolled_window;

   text_view = gtk_text_view_new();
   messageBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
   gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view),GTK_WRAP_WORD);
   gtk_text_view_set_indent (GTK_TEXT_VIEW (text_view), -15);
   gtk_text_view_set_left_margin (GTK_TEXT_VIEW (text_view), 10);
   scrolled_window = gtk_scrolled_window_new (NULL, NULL);
   gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                   GTK_POLICY_AUTOMATIC,
           GTK_POLICY_AUTOMATIC);

   gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);

   gtk_widget_show_all (scrolled_window);

   return scrolled_window;
}

static GtkWidget *create_text2()
{
   GtkWidget *scrolled_window;

   text_entry = gtk_text_view_new();
   sendBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_entry));
   gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_entry),GTK_WRAP_WORD);
   gtk_text_view_set_indent (GTK_TEXT_VIEW (text_entry), -15);
   gtk_text_view_set_left_margin (GTK_TEXT_VIEW (text_entry), 10);
   scrolled_window = gtk_scrolled_window_new (NULL, NULL);
   gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                   GTK_POLICY_AUTOMATIC,
           GTK_POLICY_AUTOMATIC);

   gtk_container_add (GTK_CONTAINER (scrolled_window), text_entry);

   gtk_widget_show_all (scrolled_window);

   return scrolled_window;
}

void roomSelected(GtkWidget *widget, gpointer textView) 
{
  	GtkTreeIter iter;
  	GtkTreeIter parent;
	GtkTreeModel *model;
  	char *value;
  	gchar *text;

  	if (gtk_tree_selection_get_selected(
      	GTK_TREE_SELECTION(widget), &model, &iter)) {
    	if (gtk_tree_model_iter_parent(model, &parent, &iter))
        	return;
    	gtk_tree_model_get(model, &iter, 0, &value,  -1);
    	GtkTextIter start;
		GtkTextIter end;
    	messageBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
    	gtk_text_buffer_get_start_iter (messageBuffer, &start);
    	gtk_text_buffer_get_end_iter (messageBuffer, &end);
    	enterRoom(value);
    	gtk_text_buffer_delete (messageBuffer, &start, &end);
    	currentRoom = strdup(value);
    	lastMessage = -1;
    	continueRefresh = TRUE;
    	g_free(value);   
  	}
  	getMessages();
  	startGetMessageThread();
}

static GtkWidget * create_list()
{
    GtkWidget *scrolled_window;
    GtkTreeIter toplevel, child;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;

    int i;
   	printf("Create List\n");
    /* Create a new scrolled window, with scrollbars only if needed */
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
            GTK_POLICY_AUTOMATIC, 
            GTK_POLICY_AUTOMATIC);
   
    treeModel = gtk_tree_store_new (1, G_TYPE_STRING);
    tree_view = gtk_tree_view_new ();
    gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
    gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (treeModel));
    gtk_widget_show (tree_view);
   
    /* Add some messages to the window */
    
   
    cell = gtk_cell_renderer_text_new ();

    column = gtk_tree_view_column_new_with_attributes ("Chat Rooms",
                                                       cell,
                                                       "text", 0,
                                                       NULL);
  
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
                 GTK_TREE_VIEW_COLUMN (column));
    printf("Create List Finished\n");
    return scrolled_window;
}

int main(int argc, char **argv) {

	char line[MAX_MESSAGE_LEN+1];
	
	if (argc < 3) {
		printUsage();
	}

	host = argv[1];
	sport = argv[2];

	printf("\nStarting talk-client %s %s\n", host, sport);

	// Convert port to number
	sscanf(sport, "%d", &port);

	GtkWidget *window;
  	GtkWidget *table;
  	GtkWidget *bigPane;
  	GtkWidget *hpaned;
  	GtkWidget *vpaned;

  	GtkWidget *toolbar;
  	GtkToolItem *refresh;
  	GtkToolItem *createRoomButton;
  	GtkToolItem *leaveRoomButton;
  	GtkToolItem *logoutButton;

  	GtkWidget *textEntry;
  	GtkWidget *textView;
  	GtkWidget *button;
  	GtkWidget *roomList;
  	GtkWidget *text;
  	GtkTreeSelection *roomSelection;

  	gtk_init(&argc, &argv);

  	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_widget_set_size_request (window, 600, 400);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	gtk_window_set_title(GTK_WINDOW(window), "IRC Client");
	gtk_container_set_border_width(GTK_CONTAINER(window), 15);

	bigPane = gtk_vpaned_new();
	gtk_container_add(GTK_CONTAINER(window), bigPane);

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_container_set_border_width(GTK_CONTAINER(toolbar), 2);
	gtk_container_add(GTK_CONTAINER(bigPane), toolbar);

	refresh = gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
	createRoomButton = gtk_tool_button_new_from_stock(GTK_STOCK_ADD);
	leaveRoomButton = gtk_tool_button_new_from_stock(GTK_STOCK_QUIT);
	logoutButton = gtk_tool_button_new_from_stock(GTK_STOCK_DISCONNECT);

	gtk_widget_set_tooltip_text(GTK_WIDGET(refresh), "Refresh");
	gtk_widget_set_tooltip_text(GTK_WIDGET(createRoomButton), "Create Room");
	gtk_widget_set_tooltip_text(GTK_WIDGET(leaveRoomButton), "Leave Room");
	gtk_widget_set_tooltip_text(GTK_WIDGET(logoutButton), "Logout");

	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), createRoomButton, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), refresh, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), leaveRoomButton, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), logoutButton, -1);

	vpaned = gtk_vpaned_new();
	gtk_container_add(GTK_CONTAINER(bigPane), vpaned);

	hpaned = gtk_hpaned_new ();
  	gtk_container_add (GTK_CONTAINER (vpaned), hpaned);

  	roomList = create_list ();
  	gtk_widget_set_size_request(roomList, 130, 225);
  	gtk_paned_add1 (GTK_PANED (hpaned), roomList);

  	roomSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
  	textView = create_text1();
  	gtk_paned_add2 (GTK_PANED (hpaned), textView);

	table = gtk_table_new(4, 2, FALSE);
	gtk_container_add (GTK_CONTAINER(vpaned), table);
	textEntry = create_text2();

	button = gtk_button_new_with_label ("Send");
	gtk_table_attach(GTK_TABLE(table), button, 3, 4, 0, 1, 0, 0, 1, 1);
	gtk_table_attach(GTK_TABLE(table), textEntry, 0, 3, 0, 1,
      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 1, 1);

	g_signal_connect_swapped(G_OBJECT(window), "destroy",
        G_CALLBACK(gtk_main_quit), G_OBJECT(window));
  	g_signal_connect(roomSelection, "changed", 
    	G_CALLBACK(roomSelected), text);

  	g_signal_connect(refresh, "clicked", 
    	G_CALLBACK(refreshFunc), (gpointer) NULL);

  	g_signal_connect(leaveRoomButton, "clicked", 
    	G_CALLBACK(leaveRoom), (gpointer) NULL);

  	g_signal_connect(logoutButton, "clicked", 
    	G_CALLBACK(logout), window);

  	g_signal_connect(createRoomButton, "clicked", 
    	G_CALLBACK(createRoom), (gpointer) window);

	g_signal_connect(button, "clicked", 
    	G_CALLBACK(sendMessage), (gpointer) NULL);

	g_signal_connect(text_entry, "key_press_event",
    	G_CALLBACK(on_key_press), (gpointer) NULL);

  	gtk_widget_show_all(window);
  	gtk_widget_hide(window);
  	createAccount(window);
  	char windowName[MAX_RESPONSE];
  	sprintf(windowName, "IRC Client - %s", user);
  	gtk_window_set_title(GTK_WINDOW(window), windowName);

  	listRooms();


  	gtk_main();

	printf("TEST ENDS\n");
	return 0;
}
