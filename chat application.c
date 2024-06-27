#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 3
#define MAX_MESSAGE_LENGTH 250
int buffersize=250;


typedef struct {
    char name[20];
    char ip[16];
    int port;
} UserInfo;

UserInfo user_info[MAX_CLIENTS] = {
    {"user_1", "127.0.0.1", 50000},
    {"user_2", "127.0.0.1", 50001},
    {"user_3", "127.0.0.1", 50002}
};

int sendEOF(int sockfd){
	char eof[]="\r\n";
	if(send(sockfd,&eof,2,0)<2){
		return -1;
	}
	return 0;
}


int sendmessage(char *msg,int sockfd){
	int sz;
	int i=0;
	int remaininglen=strlen(msg);
	while((sz=send(sockfd,msg+i,((buffersize<remaininglen)?(buffersize):(remaininglen)),0))>0){
		i+=sz;
		remaininglen-=sz;
		if(remaininglen==0){
			if(sendEOF(sockfd)==-1){
				return -1;
			}
			break;
		}
	}
	if(sz==-1){
		return -1;
	}
	return 0;
}

int receivemessage(char *msg,int sockfd){
	int sz;
	int i=0;
	while((sz=recv(sockfd,msg+i,buffersize,0))>0){
		if(msg[i+sz-1]=='\n'&&msg[i+sz-2]=='\r'){
			msg[i+sz-2]='\0';
			break;
		}
		i+=sz;
	}
    if(sz==0)return -1;
	if(sz==-1){
		return -1;
	}

	return 0;
}

int numofusers = 3;

int main(int argc, char *argv[]) {
    int my_index = -1;
    if(argc!=2){
        printf("Usage: %s <user_index>\n", argv[0]);
        return 0;
    }
    my_index = atoi(argv[1]);
    if(my_index>=numofusers||my_index<0){
        printf("Invalid user index\n");
        return 0;
    }
    int server_fd, client_fds[MAX_CLIENTS];
    int unknown_fd=-1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    fd_set read_fds;
    int max_fd, activity, i, valread;
    char message[MAX_MESSAGE_LENGTH];

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set server socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(user_info[my_index].port);

    // Bind server socket to a specific port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on %s:%d\n", user_info[my_index].ip, user_info[my_index].port);

    // Initialize client file descriptors
    for (i = 0; i < MAX_CLIENTS; i++) {
        if(i!=my_index){
            // try to connect to other users
            client_fds[i] = socket(AF_INET, SOCK_STREAM, 0);
            if (client_fds[i] < 0) {
                perror("socket failed");
                exit(EXIT_FAILURE);
            }
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(user_info[i].port);
            if (inet_pton(AF_INET, user_info[i].ip, &server_addr.sin_addr) <= 0) {
                perror("inet_pton failed");
                exit(EXIT_FAILURE);
            }

            if (connect(client_fds[i], (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                // printf("Connection to %s:%d failed\n", user_info[i].ip, user_info[i].port);
                client_fds[i] = 0;
            }
            else{
                printf("Connected to %s:%d\n", user_info[i].ip, user_info[i].port);
                if(sendmessage(user_info[my_index].name,client_fds[i])==-1){
                    // printf("Connection to %s:%d failed\n", user_info[i].ip, user_info[i].port);
                    close(client_fds[i]);
                    client_fds[i] = 0;
                }
            }
        }
        else{
            client_fds[i] = server_fd;
        }
    }

    // Main loop
    while (1) {
        // Clear the socket set
        printf("Waiting for activity...\n");
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        max_fd = server_fd;
        if(unknown_fd!=-1){
            FD_SET(unknown_fd, &read_fds);
            if(unknown_fd>max_fd){
                max_fd=unknown_fd;
            }
        }
        // Add client and server sockets to the set
        for (i = 0; i < MAX_CLIENTS; i++) {
            int client_fd = client_fds[i];
            if (client_fd > 0) {
                FD_SET(client_fd, &read_fds);
                if(client_fd>max_fd){
                    max_fd=client_fd;
                }
            }
        }
        // Wait for activity on any of the sockets

        activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select failed");
            exit(EXIT_FAILURE);
        }
        // Handle new connections
        if (FD_ISSET(server_fd, &read_fds)) {
            int new_client_fd;
            if ((new_client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
                perror("accept failed");
                exit(EXIT_FAILURE);
            }
            // printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            if(unknown_fd!=-1){
                close(unknown_fd);
            }
            unknown_fd = new_client_fd;
            continue;
        }
        if(unknown_fd!=-1&&FD_ISSET(unknown_fd, &read_fds)){
            if(receivemessage(message,unknown_fd)==-1){
                printf("Unknown user disconnected\n");
                close(unknown_fd);
                unknown_fd=-1;
            }
            else{
                for(int i=0;i<numofusers;i++){
                    if(strcmp(message,user_info[i].name)==0){
                        printf("User %s connected\n",message);
                        client_fds[i]=unknown_fd;
                        unknown_fd=-1;
                        break;
                    }
                }
            }
            continue;
        }
        // Handle activity on client sockets
        int cnt=0;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if(client_fds[i]==0){
                continue;
            }
            if(client_fds[i]==server_fd){
                continue;
            }
            int client_fd = client_fds[i];
            if (FD_ISSET(client_fd, &read_fds)) {
                if (receivemessage(message,client_fd)==-1) {
                    // Client disconnected
                    printf("%s disconnected\n", user_info[i].name);
                    close(client_fd);
                    client_fds[i] = 0;
                } else {
                    // Display received message
                    printf("Message from %s: %s\n", user_info[i].name, message);
                }
                cnt++;
                break;
            }
        }
        if(cnt)continue;
        // Handle user input from stdin
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            fgets(message, MAX_MESSAGE_LENGTH, stdin);
            if(strlen(message)>2){
            // Parse user input
                char *friend_name = strtok(message, "/");
                char *msg = strtok(NULL, "\n");
                printf("Friend: %s\n", friend_name);
                printf("Message: %s\n", msg);
                // Find friend's index in user_info table
                int friend_index = -1;
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (strcmp(friend_name, user_info[i].name) == 0) {
                        friend_index = i;
                        break;
                    }
                }
                if(friend_index==my_index){
                    printf("You cannot send message to yourself\n");
                    continue;
                }
                // Send message to friend
                if (friend_index >= 0) {
                    int friend_fd = client_fds[friend_index];
                    if (friend_fd > 0) {
                        if(sendmessage(msg,friend_fd)==-1){
                            printf("Friend is not connected\n");
                        }
                    } else {
                        printf("Friend is not connected\n");
                    }
                } else {
                    printf("Friend not found\n");
                }
            }
        }
    }

    return 0;
}

