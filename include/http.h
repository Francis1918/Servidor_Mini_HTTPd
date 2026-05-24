#ifndef HTTP_H
#define HTTP_H
#include <stddef.h>
//limites y constantes
/*\
con esto evitamos un overflow de buffer y los clientes maliciosos no 
puedan enviar solicitudes de penticiones gigantes
*/
#define HTTP_MAX_METHOD 16
#define HTTP_MAX_URI 2048 
#define HTTP_MAX_VERSION 16 //con esto hacemos con HTTP/1.1
#define HTTP_MAX_HEADER 1024 //la longitud de una sola cabecera
#define HTTP_MAX_REQUEST 8192 // Tamaño de la solicitud
//limites para  los heades
#define  HTTP_MAX_HOST 256
#define HTTP_MAX_CONNECTION 32 
#define HTTP_MAX_USER_AGENT 256

// tamano maximo de la cambezera de respuesta
//Status line  + headers + \r\n\r\n
#define HTTP_MAX_RESPONSE_HEADER 1024

//--- codigos de estado HTTP
/*
Codigos de estado que soporta httpd
estan definidos en el enum para que el compilador 
nos avise si pasamos un modelo magico nosoportado*/

typedef enum {
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_INTERNAL_SERVER_ERROR = 500
} http_status_t;

/*
Resultado del parsing cuando el parser falla devolvemos con uno de estos
que mas tarde mapaeremos un status de http
*/
typedef enum{
    HTTP_PARSE_OK =0,  /* todo va bien */
    HTTP_PARSE_BAD_REQUEST =1, /* formato invalido -. cod 400 */
    HTTP_PARSE_METHOD_NOT_ALLOWED =2, /* metodo distinto get ->405 */
    HTTP_PARSE_URI_TOO_LONG =3, /* url demasiada larga 400 */
    HTTP_PARSE_VERSION_NOT_SUPPORTED =4 /*version no soportada 400 */
} http_parse_result_t;
/*
representa una solicitud http parseada

*/
typedef struct {
    char method[HTTP_MAX_METHOD]; //Get
    char uri[HTTP_MAX_URI];// '/' o '/index.html'
    char version[HTTP_MAX_VERSION]; //http 1.1
//Cabeceras parseadas cadenas vacias en caso de que no esten presentes
    char host[HTTP_MAX_HOST]; //valor del host
    char connection[HTTP_MAX_CONNECTION]; //sigue vivo o cierra la conexion
    char user_agent[HTTP_MAX_USER_AGENT]; //valor del agente usuario
/*
sigue vivo 1 cuando la conexion se mantiene abierta despues de la respuesta
reglas del http 1.1 
con connection: close -> 0
sin conection o con sigue vivo -> 1 defecto
sin connection sigue vivo -> 1 defecto
con connection: keep-alive -> 1
*/
    int keep_alive;
} http_request_t;

//estructura del request
/*
prototype init

*/
http_parse_result_t http_parse_request_line(const char *line, http_request_t *req);
// PARSING de las solicitudes http v1.1 y gen de respuestas
/*
headers cadenas separadas por \r\n
termina cuando aparaece una linea vacia \r\n\r\n en el original
esta cadena puede tener el \r\n final ya cortado
req struc donde se rellelean los host conexiones user agent y 
keep alive, version debe estar fijado por http_parse_request_line para 
que keep alive se calcule bien

--
retorna http parse ok o http parse bad request si hay headers mal formados o no se pueden parsear
*/
http_parse_result_t http_parse_headers(const char *headers, http_request_t *req);
/*
HTTP STATUS text mapea un codigo de estado al mensaje standar 
status codigo http 200 404
retorna una cadena estatica como ok not found etc*/
const char *http_status_text(http_status_t status);
/*
http buid response header construye la cabecera  de la respuesta
buf buffer destino donde se escribe la cabecera
buf sz tamano del buffer destino
status codigo de respuesta http
content type valor del header content type text/html
content length tamano del cuerpo en bytes
keep alive 1 si si la resp llevara conexion keep alive
retorma el numero de bytes escrtos en buf o -1 si la cabecera no cabe
nota no incluye el cuerpo el server se encarga de enviar aparte
*/
int http_build_response_header(char *buf,size_t buf_sz,http_status_t status,const char *content_type,size_t content_length,int keep_alive);

#endif //http.h