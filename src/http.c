//implementacion inicial de la linea de solicitud
/*

get index.html http/1.1

parsing de http1.1
met  uri version
que se hace
validamos que tenga 3 campos serpados por espacion
que GET sea el metodo
Que la uri no sea demasiado larga
que la version sea http/1.1
se usa memcpy con limtes explicitos para evitar
overflows de buffer
http parse request line parsea ger index.html http/1.1
http parse headers parsea el bloque de cabeceras
*/
#include "http.h"
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
//helpers privados al modulo
/*
copy field copia un trozo de la cadena con un limite explicito
dst es elbuffer destino
src puntero al inicio del campo de la cadena original
len numero de bytes a copiar
*/
static int copy_field(char *dst,size_t dst_sz,const char *src,size_t len)
{
    if(len +1 > dst_sz) // +1 para el null terminator
    {
        return -1; // campo demasiado largo evitamos overflow
    }
    memcpy(dst,src,len);
    dst[len] = '\0';
    return 0;
}
/*
strncasecmp es una funcion de la biblioteca estandar que compara dos cadenas sin tener en cuenta mayusculas o minusculas
en linux ya existe esto pero se lo implementa para no depender
de extensiones posix y mantener la portabilidad
*/
static int strncasecmp_local(const char *a,const char *b,size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        unsigned char ca =(unsigned char) tolower((unsigned char)a[i]);
        unsigned char cb =(unsigned char) tolower((unsigned char)b[i]);
        if (ca != cb)
        {
            return (int)ca - (int)cb;
        }
        if (ca == '\0') // si llegamos al final de la cadena
        {
            return 0; 
        }
    }
    return 0; // son iguales
}
// trim left spaces avanza el puntero saltando espacios/tabs iniciales
static const char *trim_left_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }
    return s;
}
//trim right len devuelve la longitud que es util de la cadena 
//ignorando espacios/tabs/cr al fina
static size_t trim_right_len(const char *s,size_t len)
{
    while (len > 0)
    {
        char c = s[len -1];
        if (c == ' ' || c == '\t' || c == '\r')
        {
            len--;;
        }
        else
        {
            break;
        }
    }
    return len;
}
//api publica parsing de la linea de solicitud
http_parse_result_t http_parse_request_line(const char *line, http_request_t *req)
{
    const char *p, *sp1, *sp2;
    size_t method_len, uri_len, version_len;
    if (line == NULL || req == NULL)
    {
        return HTTP_PARSE_BAD_REQUEST;
    }
    //se limpia el struct destino
    memset(req,0,sizeof(*req));
    //buscar los 2 espacios separan los 3 campos 
    // metodo<sp>uri<sp>version
    p = line;
    sp1 = strchr(p,' ');
    if (sp1 == NULL)
    {
        return HTTP_PARSE_BAD_REQUEST; // formato invalido no hay espacios  
    }
    sp2 = strchr(sp1 +1,' ');
    if (sp2 == NULL)
    {
        return HTTP_PARSE_BAD_REQUEST; // formato invalido no hay 2 espacios

    }
    //no debe haber un tercer espcio
    if (strchr(sp2 +1,' ') != NULL)
    {
        return HTTP_PARSE_BAD_REQUEST; // formato invalido hay mas de 2 espacios
    }
    method_len = (size_t)(sp1 - p);
    uri_len = (size_t)(sp2 - sp1 -1);
    version_len = strlen(sp2 +1);
    //validar las lonagitudes minimas
    if (method_len == 0 || uri_len == 0 || version_len == 0)
    {
        return HTTP_PARSE_BAD_REQUEST; // formato invalido campos vacios
    }
    /*validar que la uri no sea enorme
    proteccion contra ataques*/
    if (uri_len > HTTP_MAX_URI)
    {
        return HTTP_PARSE_URI_TOO_LONG; // uri demasiado larga
    }
    //copy method
    if (copy_field(req->method,sizeof(req->method),p,method_len) < 0)
    {
        return HTTP_PARSE_METHOD_NOT_ALLOWED; // metodo no permitido
        //si es muy largo el metodo puede ser un intento de ataque
    }
    //solo se acepta el metodo GET
    if (strcmp(req->method,"GET") != 0)
    {
        return HTTP_PARSE_METHOD_NOT_ALLOWED; // metodo no permitido
    }
    //copy uri
    if (copy_field(req->uri,sizeof(req->uri),sp1 +1,uri_len) < 0)
    {
        return HTTP_PARSE_URI_TOO_LONG; // uri demasiado larga      
    }
    //copy version
    if (copy_field(req->version,sizeof(req->version),sp2 +1,version_len) < 0)
    {
        return HTTP_PARSE_VERSION_NOT_SUPPORTED; // version no soportada
    }
    if (strcmp(req->version,"HTTP/1.1") != 0 && strcmp(req->version,"HTTP/1.0") != 0)
    {
        return HTTP_PARSE_VERSION_NOT_SUPPORTED; // version no soportada
    }
    return HTTP_PARSE_OK;
}
//parsing de las cabeceras 
/*
parse one header procesa una sola linea de header name y value
line puntero al  inicoi de la linea usa line len no termina\0
req struc destino
retorna 0 si la linea es valida sea conocida o ignorada
-1 si la linea es mal formada no tiene :
*/
static int parse_one_header(const char *line,size_t line_len,http_request_t *req)
{
    const char *colon ,*value_start;
    size_t name_len, value_len;
    //recortar espacios  cr al final
    line_len = trim_right_len(line,line_len);
    if (line_len == 0)
    {
        return 0; // linea vacia es el final de los headers
        //se ignora
    }
    colon = memchr(line,':',line_len);
    if (colon == NULL)
    {
        return -1; // formato invalido no hay :
        //cabecera mal formada
    }
    name_len = (size_t)(colon - line);
    value_start = trim_left_spaces(colon +1);
    value_len = line_len -(size_t) (value_start - line);
    if (name_len ==0){
        return -1; // formato invalido nombre de header vacio
    }

    //identificar las cabeceras que nos interesan casos insensitivos
   if (name_len ==4 && strncasecmp_local(line, "Host",4) == 0)
    {
        copy_field(req->host,sizeof(req->host),value_start,value_len);
        return 0; // ignoramos el host por ahora no lo necesitamos
    }
    if (name_len == 10 && strncasecmp_local(line,"Connection",10) == 0)
    {
        copy_field(req->connection,sizeof(req->connection),value_start,value_len);
        return 0; // ignoramos la conexion por ahora no lo necesitamos
    }
    if (name_len == 10 && strncasecmp_local(line,"User-Agent",10) == 0)
    {
        copy_field(req->user_agent,sizeof(req->user_agent),value_start,value_len);
        return 0; // ignoramos el user agent por ahora no lo necesitamos
    }
    //otras cabeceras se ignoran sin error
    return 0;
}

/*
compute keep alive determina el valor de req keep alive segun la version
http y el header connection
*/
static void compute_keep_alive(http_request_t *req)
{
    int is_http_1_1 = strcmp(req->version,"HTTP/1.1") == 0;
    if (req->connection[0] != '\0')
    {
        if (strncasecmp_local(req->connection,"close",5) == 0)
        {
            req->keep_alive = 0;
            return; // el cliente quiere cerrar la conexion
        }
        if (strncasecmp_local(req->connection,"keep-alive",10) == 0)
        {
            req->keep_alive = 1;
            return; // el cliente quiere mantener viva la conexion
        }
        
    }
    req->keep_alive = is_http_1_1 ? 1 : 0; // por defecto http/1.1 mantiene viva la conexion
}
http_parse_result_t http_parse_headers(const char *headers, http_request_t *req)
{
    const char *p;
    if (headers == NULL || req == NULL)
    {
        return HTTP_PARSE_BAD_REQUEST;
    }
    p = headers;
    while (*p != '\0')
    {
        const char *line_end;
        size_t line_len;
        //buscar el final de la linea aceptando \r\n y \n por robustez
        line_end = strstr(p,"\r\n");
        if (line_end == NULL)        {
            line_end = strchr(p,'\n');
            if (line_end == NULL){
                line_end = p + strlen(p); // ultima linea sin salto de linea    
            }
        }
        line_len = (size_t)(line_end - p);
        //una linea vacia marca el final de las cabeceras
        if (line_len == 0|| (line_len == 1 && (p[0] == '\r')))
        {
            break; // fin de las cabeceras
        }
        if (parse_one_header(p,line_len,req) < 0)
        {
            return HTTP_PARSE_BAD_REQUEST; // formato invalido en las cabeceras
        }
        //avanzar al inicio de la siguiente linea
        if(*line_end == '\0')
        {
            break; // llegamos al final de la cadena
        }
        if (line_end[0] == '\r' && line_end[1] == '\n')
        {
            p = line_end + 2; // salto de linea \r\n
        }
        else
        {
            p = line_end + 1; // salto de linea \n
        }
    }
    
    compute_keep_alive(req);
    return HTTP_PARSE_OK;
}
//generacion de respuestas http
const char *http_status_text(http_status_t status)
{
    switch (status)
    {
        case HTTP_STATUS_OK:
            return "OK";
        case HTTP_STATUS_BAD_REQUEST:
            return "Bad Request";
        case HTTP_STATUS_METHOD_NOT_ALLOWED:
            return "Method Not Allowed";
        case HTTP_STATUS_FORBIDDEN:
            return "Forbidden";
        case HTTP_STATUS_NOT_FOUND:
            return "Not Found";
        case HTTP_STATUS_INTERNAL_SERVER_ERROR:
            return "Internal Server Error";
        default:
            return "Unknown Status";
    }
}
int http_build_response_header(char *buf,size_t buf_sz,http_status_t status,const char *content_type,size_t content_length,int keep_alive)
{
    int written;
    const char *status_text;
    const char *conn_value;
    if (buf == NULL || content_type == NULL)
    {
        return -1; // parametros invalidos
    }
    status_text = http_status_text(status);
    conn_value = keep_alive ? "keep-alive" : "close";
    /*
    snprintf un version segura de sprintf que 
    nunca escribe mas alla del tamaño del buffer especificado
    si la cabecera no cabe devuelve un valor mayor o igual a buf_sz
    y nosotros considramos esro un error

    */
   written = snprintf(buf,buf_sz,
   "HTTP/1.1 %d %s\r\n"
   "Server: MiniHTTPd\r\n"
   "Content-Type: %s\r\n"
   "Content-Length: %zu\r\n"
   "Connection: %s\r\n"
   "\r\n",
   (int)status,status_text,content_type,content_length,conn_value);
    if (written < 0 || (size_t)written >= buf_sz)
    {
        return -1; // error al escribir la cabecera o no cabe en el buffer
    }
    return written; // numero de bytes escritos en el buffer
}