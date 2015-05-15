#IRC Client/Server

Running the 'make' command will create the files IRCServer and IRCClient.

##IRC Server

When IRCServer is executed, it will accept a number of commands through network connections and send back responses to the client. The server will process only one request at one time.

The server can be started with the command `IRCServer <port>` where `port` is a designated available network port on the machine.  It can be ended by typing ctrl+c.

The server uses the `HashTableVoid` class provided in `HashTableVoid.h` and `HashTableVoid.cc` to create a hash table of users and passwords. These user/password pairs are then stored in an unencrypted file (hopefully this will be remedied in future updates) called password.txt (I conformed to the specifications of a project at the time of writing) so that all of the users are persistent through reboots of the server.

###Commands

The server can accept the following commands:
   ```
   Request: ADD-USER <USER> <PASSWD>\r\n
   Answer: OK\r\n or DENIED\r\n
   
   Request: GET-ALL-USERS <USER> <PASSWD>\r\n
   Answer:  USER1\r\n
            USER2\r\n
            ...
            \r\n

   Request: CREATE-ROOM <USER> <PASSWD> <ROOM>\r\n
   Answer: OK\n or DENIED\r\n

   Request: LIST-ROOMS <USER> <PASSWD>\r\n
   Answer: room1\r\n
           room2\r\n
           ...
           \r\n

   Request: ENTER-ROOM <USER> <PASSWD> <ROOM>\r\n
   Answer: OK\n or DENIED\r\n
   Request: LEAVE-ROOM <USER> <PASSWD>\r\n
   Answer: OK\r\n or DENIED\r\n

   Request: SEND-MESSAGE <USER> <PASSWD> <MESSAGE> <ROOM>\n
   Answer: OK\n or DENIED\n

   Request: GET-MESSAGES <USER> <PASSWD> <LAST-MESSAGE-NUM> <ROOM>\r\n
   Answer:  MSGNUM1 USER1 MESSAGE1\r\n
            MSGNUM2 USER2 MESSAGE2\r\n
            MSGNUM3 USER2 MESSAGE2\r\n
            ...\r\n
            \r\n

   Request: GET-USERS-IN-ROOM <USER> <PASSWD> <ROOM>\r\n
   Answer:  USER1\r\n
            USER2\r\n
            ...
            \r\n
   ```

##IRC Client   
  IRCClient can be run with the usage `IRCClient <host> <port>`, where `host` is the machine the server is running from, and \<port\> is the port used by the server. It provides a graphical interface for the IRC server using GTK 2.0. 
