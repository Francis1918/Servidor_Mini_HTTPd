/*
acceso a archivos estaticos del directorio www/
en esta se implementa
construccion de la ruta www + uri sanitizando la  / inicial
apertura y lectura del archivo con open(), fstat(), read()
generacion de la respuesta http con el mime correcto
manejo de errores 404 403 500
se agrego el  realpath() para validar contra directorio transversal
*/
#include "files.h"
#include "mime.h"
#include "http.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>     // PATH_MAX

/*
 * Directorio raiz absoluto del servidor, resuelto por realpath() al arrancar.
 * Sera algo como "/home/bravo/minihttpd/www".
 * El terminador '\0' lo agrega realpath().
 */
static char g_www_root_abs[FILES_MAX_PATH] = {0};
static size_t g_www_root_abs_len = 0;

/*helpers privados al modulo
send error response enviar una respuesta de error http corta
client fd socket destino
status codigo http 404 403 500
keep alive 1 si la conexion sigue abierta a la respuesta*/
static void send_error_response(int client_fd,http_status_t status,int keep_alive)
{
    char header[HTTP_MAX_RESPONSE_HEADER];
    char body[256];
    int header_len;
    int body_len;
    const char *status_text;

    status_text = http_status_text(status); // inicializar antes de usar
    /*
    construir un body simple en html
    snprintf siempre limita por sizeof evitando el overflow*/
    body_len = snprintf(body,sizeof(body),
        "<!DOCTYPE html>\n"
        "<html><head><title>%d %s</title></head>\n"
        "<body><h1>%d %s</h1></body></html>\n",
        (int) status, status_text,
        (int) status, status_text);
        if (body_len <0 || (size_t) body_len >= sizeof(body))
        {
            fprintf(stderr,"Error al construir el cuerpo de la respuesta de error\n");
            return; // salio algo raro se aborta
        }
        header_len = http_build_response_header(header,sizeof(header),status,"text/html",(size_t) body_len,keep_alive);
        if (header_len <0)
        {
            return; // error al construir la cabecera se aborta
        }
        if (write(client_fd,header,(size_t) header_len)<0)
        {
            perror("write error header");
            return;
        }
        if (write(client_fd,body,(size_t) body_len)<0)
        {
            perror("write error body");
            return;
        }
}
/*build file system path combina files www root uri en una ruta del disco
reglas
si la uri es / se considera index.html
si la uri es /algo.html se devuelve www/algo.html
retonrna 0 si al la ruta cabe en out path -1 si no cabe
*/
static int build_filesystem_path(const char *uri,char *out_path,size_t out_sz)
{
    int written;
    if (uri == NULL || out_path == NULL || out_sz == 0)
    {
        return -1; // parametros invalidos
    }
    //si la uri es / se envia el archivo por defecto
    if (strcmp(uri,"/") == 0)
    {
        written = snprintf(out_path, out_sz, "%s/%s",FILES_WWW_ROOT, FILES_DEFAULT_INDEX);
    }else if (uri[0] == '/')
    {
        //la uri ya inicia  con / asi que la pegamos despues de "www".*/
        written = snprintf(out_path,out_sz,"%s%s",FILES_WWW_ROOT,uri);

    }else
    {
        written = snprintf(out_path,out_sz,"%s/%s",FILES_WWW_ROOT,uri);
        //url sin / inicial  se inserta /
    }
    if (written <0 || (size_t) written >= out_sz)
    {
        return -1; // la ruta no cabe en el buffer
    }
    return 0; // exito
}
/*
erno to http status convierte erno tras open() en un estatus de HTTP
enoent archivo no existe 404  not found
eacces permiso 403 forbidden
cualquier otro 500 internal server error*/
static http_status_t errno_to_http_status(int err)
{
    switch (err)
    {
        case ENOENT:
            return HTTP_STATUS_NOT_FOUND;
        case EACCES:
            return HTTP_STATUS_FORBIDDEN;
        default:
            return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }
}

/*
  is_inside_root - Verifica que una ruta absoluta este dentro del directorio
 www/ resuelto al inicio.
 
 Compara el inicio de "abs_path" contra g_www_root_abs. Pero ojo: si
 g_www_root_abs es "/home/bravo/minihttpd/www" y la ruta solicitada es
 "/home/bravo/minihttpd/wwwbackup/secreto", el simple prefijo coincidiria
  por accidente. Por eso despues del prefijo debe venir '/' o '\0'.
 
 Retorna 1 si esta dentro, 0 si no.
 */
static int is_inside_root(const char *abs_path)
{
    if (abs_path == NULL || g_www_root_abs_len == 0) return 0;

    /* El prefijo debe coincidir caracter por caracter. */
    if (strncmp(abs_path, g_www_root_abs, g_www_root_abs_len) != 0) {
        return 0;
    }

    /* Despues del prefijo, debe venir '/' (subdirectorio o archivo)
       o '\0' (es exactamente el directorio raiz). */
    char next = abs_path[g_www_root_abs_len];
    return (next == '\0' || next == '/');
}

//api publica

int files_init(void)
{
    char *resolved = realpath(FILES_WWW_ROOT, NULL);
    if (resolved == NULL) {
        fprintf(stderr, "files_init: no se pudo resolver '%s': %s\n",
                FILES_WWW_ROOT, strerror(errno));
        return -1;
    }

    size_t len = strlen(resolved);
    if (len + 1 > sizeof(g_www_root_abs)) {
        fprintf(stderr, "files_init: ruta absoluta demasiado larga (%zu)\n", len);
        free(resolved);
        return -1;
    }

    memcpy(g_www_root_abs, resolved, len + 1);
    g_www_root_abs_len = len;
    free(resolved);

    printf("[files] directorio raiz: %s\n", g_www_root_abs);
    return 0;
}

void files_serve(int client_fd,const char *uri,int keep_alive)
{
    char path[FILES_MAX_PATH];
    char *resolved_path = NULL;
    struct stat st;
    int fd = -1;
    char header[HTTP_MAX_RESPONSE_HEADER];
    int header_len;
    const char *mime;
    ssize_t n;
    char buf[8192];

    // construir la ruta del archivo en el sistema de archivos
    if (build_filesystem_path(uri,path,sizeof(path))<0)
    {
        send_error_response(client_fd,HTTP_STATUS_BAD_REQUEST,keep_alive);
        return;
    }

    /*
     realpath() resuelve "..", ".", "//" y links simbolicos a una ruta
     absoluta canonica. Si el archivo no existe, retorna NULL con errno
     = ENOENT.
     */
    resolved_path = realpath(path, NULL);
    if (resolved_path == NULL) {
        http_status_t status = errno_to_http_status(errno);
        fprintf(stderr, "[files] realpath fallo para '%s': %s\n",
                path, strerror(errno));
        send_error_response(client_fd, status, keep_alive);
        return;
    }

    /*
     Verificacion de Directory Traversal: la ruta resuelta DEBE estar
     dentro del directorio raiz www/. Si no, es un intento de ataque.
     */
    if (!is_inside_root(resolved_path)) {
        fprintf(stderr, "[files] BLOQUEADO directory traversal: '%s' -> '%s'\n",
                path, resolved_path);
        send_error_response(client_fd, HTTP_STATUS_FORBIDDEN, keep_alive);
        free(resolved_path);
        return;
    }

    //abrir el archivo solo como lectura
    fd = open(resolved_path,O_RDONLY);
    if (fd <0)
    {
        http_status_t status = errno_to_http_status(errno);
        fprintf(stderr,"Error al abrir el archivo '%s': %s\n",resolved_path,strerror(errno));
        send_error_response(client_fd,status,keep_alive);
        free(resolved_path);
        return;
    }
    //obtener la metadata del archivo peso y tipo
    if (fstat(fd,&st)<0)
    {
        perror("fstat");
        close(fd);
        free(resolved_path);
        send_error_response(client_fd,HTTP_STATUS_INTERNAL_SERVER_ERROR,keep_alive);
        return;
    }
    //verificar  que sea un archivo regular no un directorio o disp
    if (!S_ISREG(st.st_mode))
    {
        fprintf(stderr,"[files] '%s' no es un archivo regular\n",resolved_path);
        close(fd);
        free(resolved_path);
        send_error_response(client_fd,HTTP_STATUS_FORBIDDEN,keep_alive);
        return;
    }
    //detectar el tipo de mime en base a la extension
    mime = mime_get_type(resolved_path);
    //construir la cabecera de respuesta http
    header_len = http_build_response_header(header,sizeof(header),HTTP_STATUS_OK,mime,(size_t) st.st_size,keep_alive);
    if (header_len <0)    {
        close(fd);
        free(resolved_path);
        send_error_response(client_fd,HTTP_STATUS_INTERNAL_SERVER_ERROR,keep_alive);
        return;
    }
    if (write(client_fd,header,(size_t) header_len)<0)
    {
        perror("write header");
        close(fd);
        free(resolved_path);
        return;
    }
    //leer un archivo en bloques y enviarlo al socket
    while ((n = read(fd,buf,sizeof(buf))) >0)
    {
        ssize_t total_written = 0;
        while (total_written < n)
        {
            ssize_t w = write(client_fd,buf + total_written,(size_t)(n - total_written));
            if (w <0)
            {
                if(errno==EINTR){
                    continue;  // EINTR = reintentar, no abortar
                }
                perror("write(body)");
                close(fd);
                free(resolved_path);
                return;
            }
            total_written += w;
        }
    }
    if (n <0)
    {
        perror("read file");
    }
    close(fd);
    printf("[Files] sirvio '%s'(%lld bytes, %s)\n",resolved_path, (long long)st.st_size,mime);
    free(resolved_path);
}