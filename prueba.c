#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>

#define SERVER_ADDRESS "127.0.1.1"

#define CONNECT 0
#define MORE_MSGS 1
#define ONE_MSG 2
#define END_MSG 3

#define MAX_LENGTH_MSG 4096
#define MAX_LENGTH_DATA 4096-3*sizeof(int)

//Para poder trabajar con todos los datos que nos pide el protocolo, nos creamos la estructura del enunciado
struct message
{
    int message_length;
    int message_type;
    int pid;
    int data_length;
    char data[MAX_LENGTH_DATA];
};

int sockfd;

int main(int argc, char* argv[])
{
    int port = atoi(argv[1]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int connect_with_server;

    if (argc < 2)
    {
        printf("No hay suficientes argumentos. \n");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS); 
    serv_addr.sin_port = htons(port);

    /* Intentamos conectar el cliente con el servidor*/

    connect_with_server = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (connect_with_server != 0) 
    { 
        printf("connection with the server failed...\n");  
        exit(EXIT_FAILURE);
    }
    printf("Cliente conectado al servidor. \n"); 

    struct message msg;
    msg.message_length = 3*sizeof(int);
    msg.data_length = 0;
    msg.message_type = CONNECT;
    msg.pid = getpid();

    char buffer[MAX_LENGTH_MSG]; 

    //Enviamos mensaje de conexion
    //sprintf(buffer, "%4d%4d%4d", msg.message_length, msg.message_type, msg.pid);
    write(sockfd, &msg.message_length, sizeof(int));
    write(sockfd, &msg.message_type, sizeof(int));
    write(sockfd, &msg.pid, sizeof(int));
    write(1, &buffer, strlen(buffer));
    printf("\nHemos enviado el mensaje de conexion. \n");

    char buf;

    while(1)
    {
        //Inicializamos todo a 0 para poder determinar que mensaje debemos enviar al servidor y como
        msg.message_length = 0;
        msg.message_type = ONE_MSG;
        msg.data_length = 0;
        buf = '\0';


        //Leemos el buffer de entrada desde el terminal de Linux
        while (msg.data_length < (MAX_LENGTH_DATA - 1) && buf != '\n')
        {
            read(1, &buf, sizeof(buf)); //el 1 es el canal del terminal de Linux
            msg.data[msg.data_length] = buf;
            msg.data_length++;
        }
        
        printf("Data length equivale: %d\n", msg.data_length); //Hasta aqui bien

        for (int i = 0; i < msg.data_length; i++)
        {
            if (msg.data[i] == 'E')
            {
                if (msg.data[i+1] == 'N')
                {
                    if (msg.data[i+2] == 'D')
                    {
                        msg.message_type = END_MSG;
                    }
                }
            }
        }

        char msg_send[MAX_LENGTH_MSG];

        if (msg.message_type == END_MSG)
        {
            printf("END MSG\n");
            msg.message_length = 2*sizeof(int);
            //sprintf(msg_send, "%4d%4d", msg.message_length, msg.message_type);
            write(sockfd, &msg.message_length, sizeof(int));
            write(sockfd, &msg.message_type, sizeof(int));
            //write(sockfd, &msg_send, strlen(msg_send));
            exit(0);
        }
        else if (msg.data_length == MAX_LENGTH_DATA-1)
        {
            printf("MORE MSG\n");
            msg.message_type = MORE_MSGS;
            msg.message_length = MAX_LENGTH_MSG;
            msg.data[msg.data_length] = '\0';
            write(sockfd, &msg.message_length, sizeof(int));
            write(sockfd, &msg.message_type, sizeof(int));
            write(sockfd, &msg.data_length, sizeof(int));
            write(sockfd, &msg.data, msg.data_length);
        }
        else if (msg.data_length < MAX_LENGTH_DATA) 
        {
            printf("ONE MSG\n");
            msg.message_length = msg.data_length + 3*sizeof(int); 
            msg.message_type = ONE_MSG;
            printf("El message type es: %d\n", msg.message_type);
            //sprintf(msg_send, "%4d%4d%4d%s", msg.message_length, msg.message_type, msg.data_length, msg.data);
            //write(sockfd, &msg_send, msg.message_length);
            write(sockfd, &msg.message_length, sizeof(int));
            write(sockfd, &msg.message_type, sizeof(int));
            write(sockfd, &msg.data_length, sizeof(int));
            write(sockfd, &msg.data, msg.data_length);
            write(1, &msg_send, msg.message_length);
        }
    }

    close(sockfd); 
    return 0;
}