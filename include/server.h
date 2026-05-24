#ifndef SERVER_H
#define SERVER_H
/*  Este archivo es el mini servidor TCP con epoll()
funciones para crear el socket  y manejar la concurrencia
*/
//server.h
#define DEFAULT_PORT 8080 //Socket para aceptar la conexion 
#define LISTEN_BACKLOG 64
#define MAX_EVENTS 64 // numero de eventos por iteracion de epollwait
#define READ_BUF_SIZE 4096
//tiempo maximo de inactividad Segundos antes de cerrar la conexion
//keep-alive timeout
#define KEEPALIVE_TIMEOUT_SEC 30

/*frecuencia con la que el bucle epoll revisa los timeouts milisegundos*/
#define EPOLL_TICK_MS 1000
/*numero maximo de clientes simultaneos que rastreamos con el timeout*/
#define MAX_CLIENTS 1024
 //socket de escucha
// arranque del server en el puerto que se indico  0 si es exito
// o -1 si falla
int server_run(int port);


#endif  
