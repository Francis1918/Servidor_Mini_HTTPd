/* 
Servidor tcp con epoll
v0.2 un solo hilo mas un bucle de eventos avisando de los descriptores
vigilados nueva conexion en el socket de escucha o datos disponibles 
en un socket
ahora con conexiones persistentes keep-alive y timeout de inactividad
------------------------------------------------------------------------
*/
#include "server.h"
#include "http.h"
#include "files.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
tabla simple de clientes activos para llevar el timestamp de la 
ultima actividad. el indice = file descriptor (funciona porque linux
asigna fds pequeños).
last_activity[fd] = time() de la ultima vez que recibimos datos.
si vale 0 -> ese fd no es un cliente activo
*/
static time_t last_activity[MAX_CLIENTS] = {0};

// helpers privados al modulo
static int set_nonblocking(int fd)
{
	int flags = fcntl(fd,F_GETFL,0);
	if (flags <0)
	{
		perror("fcntl(F_GETFL)");
		return -1;
	}
	if (fcntl(fd,F_SETFL,flags| O_NONBLOCK)<0)
	{
		perror("fcntl(F_SETFL)");
		return -1;
	}
	return 0;
}
static int create_listen_socket(int port)
{
	int fd;
	int opt=1;
	struct sockaddr_in addr;
	fd = socket(AF_INET, SOCK_STREAM,0);
	if (fd<0)
	{
		perror("socket");
		return -1;
	}
	if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0)
	{
		perror("setsockopt(SO_REUSEADDR)");
		close(fd);
		return -1;
	}
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port =htons((uint16_t)port);
	if (bind(fd,(struct sockaddr *)&addr,sizeof(addr))<0)
	{
		perror("bind");
		close(fd);
		return -1;
	}
	if (listen(fd, LISTEN_BACKLOG)<0)
	{
		perror("listen");
		close(fd);
		return -1;
	}
	if (set_nonblocking(fd)<0)
	{
		close(fd);
		return -1;
	}
	return fd;
}

/*
close_client cierra un cliente y limpia su entrada de la tabla
de last_activity
*/
static void close_client(int client_fd, int epoll_fd)
{
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
	close(client_fd);
	if (client_fd >= 0 && client_fd < MAX_CLIENTS)
	{
		last_activity[client_fd] = 0;
	}
}

static void accept_new_connection(int listen_fd,int epoll_fd)
{
	while (1)
	{
		struct sockaddr_in client_addr;
		socklen_t cli_len = sizeof(client_addr);
		char ip[INET_ADDRSTRLEN];
		struct epoll_event ev;
		int cfd;
		cfd = accept(listen_fd,(struct sockaddr *)&client_addr,&cli_len);
		if (cfd<0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return;
			}
			perror("accept");
			return;
		}
		/*
		si el fd asignado es muy grande no cabe en nuestra tabla
		de timeouts, asi que rechazamos esa conexion
		*/
		if (cfd >= MAX_CLIENTS)
		{
			fprintf(stderr,"Demasiados clientes (fd=%d >= %d), cerrando\n", cfd, MAX_CLIENTS);
			close(cfd);
			continue;
		}
		if (set_nonblocking(cfd)<0)
		{
			close(cfd);
			continue;
		}
		ev.events = EPOLLIN;
		ev.data.fd = cfd;
		if (epoll_ctl(epoll_fd,EPOLL_CTL_ADD,cfd,&ev)<0)
		{
			perror("epoll_ctl(ADD cliente)");
			close(cfd);
			continue;
		}
		// registrar el momento de la conexion para el timeout
		last_activity[cfd] = time(NULL);

		if (inet_ntop(AF_INET,&client_addr.sin_addr,ip,sizeof(ip))!=NULL)
		{
			printf("[+] Cliente conectado fd=%d desde %s:%d\n",cfd,ip,ntohs(client_addr.sin_port));
		}
	}
}

// error del parser status http
static http_status_t http_parse_result_to_status(http_parse_result_t r)
{
	switch (r)
	{
		case HTTP_PARSE_METHOD_NOT_ALLOWED:
			return HTTP_STATUS_METHOD_NOT_ALLOWED;
		case HTTP_PARSE_URI_TOO_LONG:
			return HTTP_STATUS_BAD_REQUEST; // uri demasiado larga se considera una mala solicitud
		case HTTP_PARSE_VERSION_NOT_SUPPORTED:
			return HTTP_STATUS_BAD_REQUEST; // version no soportada se considera una mala solicitud
		default:
			return HTTP_STATUS_BAD_REQUEST; // error desconocido
	}
}

// construir y enviar una respuesta
/*
send response  genera y envia una respuesta http completa
client fd socket del cliente
status codigo http
content_type tipo de contenido a enviar
body cuerpo de la respuesta puede ser null si esta vacia
body len logitud del cuerpo
keep alive  1 si la conexion debe quedar abierta
*/
static void send_response(int client_fd,http_status_t status,const char *content_type,const char *body,size_t body_len,int keep_alive)
{
	char header[HTTP_MAX_RESPONSE_HEADER];
	int header_len;
	header_len = http_build_response_header(header,sizeof(header),status,content_type,body_len,keep_alive);
	if (header_len <0)
	{
		fprintf(stderr,"Error al construir la cabecera de respuesta\n");
		return;
	}
	if (write(client_fd,header,(size_t)header_len)<0)
	{
		perror("write header");
		return;
	}
	if(body != NULL && body_len >0)
	{
		if (write(client_fd,body,body_len)<0)
		{
			perror("write body");
		}
	}
}

/*
handle_client_data procesa una peticion http del cliente
retorna 1 si la conexion debe mantenerse abierta keep-alive
retorna 0 si la conexion debe cerrarse
*/
static int handle_client_data(int client_fd)
{
	char buf[READ_BUF_SIZE];
	char request_line[HTTP_MAX_REQUEST];
	size_t request_line_len;
	ssize_t n;
	char *first_line_end;
	http_request_t req;
	http_parse_result_t pr;
	n = read(client_fd,buf,sizeof(buf)-1);
	if (n==0)
	{
		printf("Cliente fd=%d desconectado\n",client_fd);
		return 0;
	}
	if (n<0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return 1; // no hay datos por ahora, mantener abierta
		}
		perror("read");
		return 0;
	}
	buf[n] = '\0'; // asegurar que el buffer es una cadena null-terminada
	first_line_end = strstr(buf,"\r\n");
	if (first_line_end == NULL)
	{
		send_response(client_fd,HTTP_STATUS_BAD_REQUEST,"text/plain","Bad Request",11,0);
		return 0;
	}
	request_line_len = (size_t)(first_line_end - buf);
	if(request_line_len + 1  > sizeof(request_line))
	{
		send_response(client_fd,HTTP_STATUS_BAD_REQUEST,"text/plain","Bad Request",11,0);
		return 0;
	}
	memcpy(request_line,buf,request_line_len);
	request_line[request_line_len] = '\0';
	//parsear la linea de solicitud
	pr = http_parse_request_line(request_line,&req);
	if (pr != HTTP_PARSE_OK)
	{
		http_status_t status = http_parse_result_to_status(pr);
		const char *msg = http_status_text(status);
		send_response(client_fd,status,"text/plain",msg,strlen(msg),0);
		printf("[fd=%d] Error parseando request line: %d -> %d %s\n",client_fd,pr,(int)status,msg);
		return 0;
	}
	/* Parsear cabeceras (a partir de despues de "\r\n"). */
	//printf("[DEBUG] buf despues de request line: [[[%s]]]\n", first_line_end + 2);
    pr = http_parse_headers(first_line_end + 2, &req);
    if (pr != HTTP_PARSE_OK) {
        send_response(client_fd, HTTP_STATUS_BAD_REQUEST,
                      "text/plain", "Bad Request", 11, 0);
        return 0;
    }

    /* Log de lo parseado. */
    printf("[fd=%d] %s %s %s  Host='%s' UA='%s' keep_alive=%d\n",
           client_fd, req.method, req.uri, req.version,
           req.host, req.user_agent, req.keep_alive);

    /* servir archivo pasando el keep_alive negociado con el cliente */
    files_serve(client_fd, req.uri, req.keep_alive);

    /* si el cliente pidio keep-alive dejamos la conexion abierta */
    return req.keep_alive ? 1 : 0;
}

/*
sweep_timeouts recorre la tabla de clientes y cierra los inactivos
que llevan mas de KEEPALIVE_TIMEOUT_SEC segundos sin actividad
*/
static void sweep_timeouts(int epoll_fd)
{
	time_t now = time(NULL);
	int i;
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (last_activity[i] == 0) continue; // slot vacio
		if ((now - last_activity[i]) >= KEEPALIVE_TIMEOUT_SEC)
		{
			printf("[timeout] cerrando fd=%d inactivo por %lds\n",
			       i, (long)(now - last_activity[i]));
			close_client(i, epoll_fd);
		}
	}
}

int server_run(int port)
{
	int listen_fd,epoll_fd;
	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];
	//inicializo la raiz del server resuelve www/ con realpath 
	if (files_init()<0)
	{
		fprintf(stderr,"Server_run: Error al inicializar el directorio\n");
		return -1;
	}
	listen_fd = create_listen_socket(port);
	if (listen_fd<0)
	{
		return -1;
	}
	epoll_fd = epoll_create1(0);
	if (epoll_fd<0)
	{
		perror("epoll_create1");
		close(listen_fd);
		return -1;
	}
	ev.events = EPOLLIN;
	ev.data.fd = listen_fd;
	if (epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_fd,&ev)<0)
	{
		perror("epoll_ctl(ADD listen)");
		close(listen_fd);
		close(epoll_fd);
		return -1;
	}
	printf("MiniHTTPD escuchando en el puerto %d (Ctrl+C para salir)\n",port);
	printf("keep-alive timeout: %d segundos\n", KEEPALIVE_TIMEOUT_SEC);

	while (1)
	{
		/*
		ahora usamos un timeout de EPOLL_TICK_MS para poder revisar
		periodicamente las conexiones inactivas
		*/
		int n = epoll_wait(epoll_fd,events,MAX_EVENTS,EPOLL_TICK_MS);
		if (n<0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			perror("epoll_wait");
			break;
		}
		// procesar eventos
		for (int i=0;i<n;i++)
		{
			int fd = events[i].data.fd;
			if (fd == listen_fd)
			{
				accept_new_connection(listen_fd,epoll_fd);
			}
			else
			{
				int keep = handle_client_data(fd);
				if (keep)
				{
					// actualizar timestamp para el timeout
					last_activity[fd] = time(NULL);
				}
				else
				{
					close_client(fd, epoll_fd);
				}
			}
		}
		// revisar timeouts independiente de si hubo eventos
		sweep_timeouts(epoll_fd);
	}
	close(listen_fd);
	close(epoll_fd);
	return 0;
}