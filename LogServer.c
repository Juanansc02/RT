#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#define SERVER_ADDRESS "127.0.1.1"
#define BUFF_SIZE 128

#define CONNECT 0
#define MORE_MSGS 1
#define ONE_MSG 2
#define END_MSG 3

#define MAX_LENGTH_MSG 4096
#define MAX_LENGTH_DATA 4096-3*sizeof(int)

struct message
{
    int message_length;
    int message_type;
    int pid;
    int data_length;
    char data[MAX_LENGTH_DATA];
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int server_socket;
int log_filefd; //File descritor del archivo que se desea leer


void sig_func(int SIG_NUM, siginfo_t *info, void *context)
{
    printf("SERVER CLOSED\n");
    close(server_socket);
    close(log_filefd);
    exit(0);
}


void *func(void *arg)
{
    struct message msg;
    char msg_recieved[MAX_LENGTH_MSG];
    int client_socket = *(int *)arg;
    int cont = 1;

    char *data = calloc(MAX_LENGTH_DATA, sizeof(char)); 


    printf("El cliente ha entrado correctamente al servidor\n");

    while(1)
    {
        struct sigaction hand;
        sigemptyset(&hand.sa_mask);
        hand.sa_flags = SA_SIGINFO;
        hand.sa_sigaction = sig_func;
        sigaction(SIGUSR2, &hand, NULL);

        char pid_msg[64];
        char end_msg[64];

        memset(msg_recieved, 0, sizeof(msg_recieved));//borramos el buffer
        
        //Leemos la longitud del mensaje
        read(client_socket, msg_recieved, sizeof(int));
        msg.message_length = *(int *)msg_recieved;

        //Para que el servidor no se quede pillado
        if (msg.message_length == 0) break;

        //Leemos tipo del mensaje 
        read(client_socket, msg_recieved, sizeof(int));
        msg.message_type = *(int *)msg_recieved;


        memset(msg_recieved, 0, sizeof(msg_recieved));

        //Ahora, en funcion del tipo que sea, seguiremos de una forma u otra
        if (msg.message_type == CONNECT) 
        {
            //Solo hace falta leer el pid una vez se conecta
            read(client_socket, msg_recieved, sizeof(int));
            msg.pid = *(int *)msg_recieved;
            sprintf(pid_msg, "PID: %d\n", msg.pid);

            printf("Client connected: %d %d %d\n", msg.message_length, msg.message_type, msg.pid); 

            //Usamos la exclusion mutua para evitar que varios clientes escriban a la vez en el fichero
            pthread_mutex_lock(&mutex);
            write(log_filefd, "\nCONNECTED\n", 12);
            write(log_filefd, &pid_msg, strlen(pid_msg));
            pthread_mutex_unlock(&mutex);
        }
        else if (msg.message_type == ONE_MSG)
        {
            read(client_socket, msg_recieved, sizeof(int)); 
            msg.data_length = *(int *)msg_recieved;
            memset(msg_recieved, 0, sizeof(msg_recieved));
            sprintf(pid_msg, "PID: %d ->", msg.pid);

            //Leemos data y la añadimos al buffer
            //read(client_socket, msg_recieved, msg.data_length); 
            read(client_socket, msg_recieved, msg.data_length); 
            //Esta funcion la usamos para incluir los datos recibidos en la ubicacion donde guardamos todo
            strcpy(msg.data, msg_recieved);
            strcat(pid_msg, msg.data);

            printf("\n MESSAGE RECIEVED: %s\n", msg.data);

            //Usamos la exclusión mutua para que solo escriba un cliente en el fichero
            pthread_mutex_lock(&mutex);
            //write(log_filefd, &pid_msg, strlen(pid_msg));
            write(log_filefd, &pid_msg, strlen(pid_msg));
            pthread_mutex_unlock(&mutex);
            
            //Redimensionamos la memoria previamente reservada con el calloc
            char *redi = realloc(data, MAX_LENGTH_DATA);
            data = redi;
            memset(data, 0, sizeof(data));
        }
        else if (msg.message_type == MORE_MSGS)
        {
            cont++;

            read(client_socket, msg_recieved, sizeof(int)); 
            msg.data_length = *(int *)msg_recieved;
            //memset(msg_recieved, 0, sizeof(msg_recieved));
            
            //Leemos los datos (ultimo elemento en ser enviado)
            //NO LO HACE BIEN, NO GUARDA NADA
            read(client_socket, msg_recieved, strlen(msg_recieved));

            strcpy(msg.data, msg_recieved);

            char *redi = realloc(data, MAX_LENGTH_DATA*cont);
            data = redi;
            
            /*Como el mensaje es demasiado largo para leerlo en una sola tanda, vamos a ir 
            añadiendo datos al buffer donde guardamos el mensaje para luego poder juntarlo 
            y asi escribirlo en el fichero y/o printarlo por pantalla*/

            pthread_mutex_lock(&mutex);
            strcat(data, msg.data);
            write(log_filefd, &data, strlen(data));
            pthread_mutex_unlock(&mutex);

            /*Al redimensionar el espacio de memoria reservado, lo vamos multiplicando por la variable cont.
            La funcio de esta es determinar cuantas veces se ha tenido que fraccionar el mensaje inicial
            para poder enviarlo. Es por eso que debemos expandir el espacio reservado en funcion de cont 
            ya que, por ejemplo, si cont es 2 eso quiere decir que el mensaje original se ha fraccionado dos
            veces y se han enviado tres mensajes de tipo ONE_MSG. Por este motivo se expande la memoria*/
            printf("\n MESSAGE RECIEVED: %s\n", data); 
            memset(data, 0, sizeof(data));
        }
        else if (msg.message_type == END_MSG)
        {
            sprintf(end_msg, "Client PID: %d DISCONNECTED\n", msg.pid);
            printf("END: %s", end_msg);
            pthread_mutex_lock(&mutex);
            write(log_filefd, &end_msg, strlen(end_msg));
            pthread_mutex_unlock(&mutex);

            //Liberamos el buffer donde almacenamos los datos, liberamos el socket y eliminamos el thread
            free(data);
            close(client_socket);
            pthread_exit(NULL);
        }
       
    }
    pthread_exit(NULL); 
}



int main(int argc, char* argv[])
{
    pthread_t id;
    char *file_name = argv[1];
    int port_number = atoi(argv[2]);
    char buff_read[128];

    if (argc < 3) 
    {
        printf("No hay suficientes argumentos. \n");
        exit(EXIT_FAILURE);
    }

    log_filefd = open(file_name, O_RDWR|O_CREAT|O_TRUNC, 0666);

    server_socket = socket(AF_INET, SOCK_STREAM, 0); //Creamos el socket
    if (server_socket == -1) 
    {
        printf("Error en la creación del socket. \n");
        exit(EXIT_FAILURE);
    }

    //Declaramos un adress para el socket
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_number);
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);

    //Asignamos adress
    int bind_ret = bind(server_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)); 
    if (bind_ret == -1) 
    {
        printf("No se ha asociado correctamente el socket a la IP. \n");
        exit(EXIT_FAILURE);
    } 
    
    if (listen(server_socket, 50) == 0) //A la espera de un nuevo cliente
    {
        printf("[SERVER]: listening on SERV_PORT %d \n\n", port_number);
    }

    while(1) 
    {
        int client_socket = accept(server_socket, NULL, NULL); //Espera a que alguien se conecte al puerto
        pthread_create(&id, NULL, func, (void*)&client_socket); //Creamos thread para leer mensajes que se envien por el socket      
    }

    close(server_socket);
    close(log_filefd);
    return 0;
}