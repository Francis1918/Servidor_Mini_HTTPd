#ifndef FILES_H
#define FILES_H
//acceso con sendfile()
#include <stddef.h>

/*
files.h acceso seguro a archivos reciente 
se encargad de:
construir la ruta completa al archivo entro del directorio www/
abrir y leer el archivo del disco
genera la respuesta http con content type correcto
maneja los errores 404 403 500
valida contra directorio transversal  realpath
el server llama a files_serve() despues de parsear la solicitud http*/
// tamaño maximo de la ruta absoluta
#define FILES_MAX_PATH 4096
/*
directorio rais que el servidor expone al exterio
es una ruta relativa al directorio desde donde se lanza ./minihttpd
mas tarde se resuelve la ruta absoluta con realpath()*/
#define FILES_WWW_ROOT "www"
#define FILES_DEFAULT_INDEX "index.html"

/*
files init incializa el directorio raiz absoluto
resuelve  FILES_WWW_ROOT a la ruta absoluta usando realpath()
y guarda internamente 
esta ruta se usa luego en files_serve para validar que cualquier archivo
solicitado este dentro del www/ u permita un ataque de directory traversal
debe llamarse una sola vez al inicio del server antes de aceptar las conexiones
retona 0 si es exitoso
y -1 si no se puede resolver el directorio en ese caso ni arranca el server */
int files_init(void);
/*
files serve rsirve un archivo estatico al cliente
client fd socket del cliente al escribir la respuesta
uri la uri solicitada ej / /index.html, /style.css
keep alive 1 si la conexion debera quedar abierta tras la respuesta

esta funcion se encarga de generar la cabecera http como la de enviar el cuerpo
en caso de error envia de respuesta los codigos de error correspondientes

no retorna nada porque el control del cliente sigue a cargo de server.c*/
void files_serve(int client_fd,const char *uri,int keep_alive);
#endif //fin de files.h
