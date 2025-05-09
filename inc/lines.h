#include <unistd.h>


int exist_dir(char *user);
int sendMessage(int socket, char *buffer, int len);
int recvMessage(int socket, char *buffer, int len);
ssize_t readLine(int fd, void *buffer, size_t n);
void send_double(int socket, double d);
double recv_double(int socket);
int get_user_information(char *user, char *ip, int *port);
