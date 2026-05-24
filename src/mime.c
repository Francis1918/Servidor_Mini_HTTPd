/*
implementacion de la deteccion de tipos mime

usamos una tabla estaticoa con pares extension mimie type y 
una busqueda lineal, la tabla es pequeña de 10 entradas asi que la
busqueda es eficiente para este tamaño no se usa hash

la extension se busca desde el final del nombre par que las rutas como
"/www/img.png funcionen correctamente"
*/
#include "mime.h"
#include <string.h>
#include <ctype.h>
#include <stddef.h>

// cuando no se reconoce la extension se retorna este tipo generico
#define MIME_DEFAULT "application/octet-stream"
/*
tabla de mapeo extension tipos mime
las 5 primeras son requisito del proyecto
las otras son extras adicionales para probar con mas
extensiones
*/
struct mime_entry {
    const char *extension; //incluye el punto ej .html
    const char *mime_type;
};
static const struct mime_entry mime_table[] = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    // entradas adicionales
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".txt", "text/plain"},
    {".svg", "image/svg+xml"},
    {".pdf", "application/pdf"},
    {".ico", "image/x-icon"},


    //fin de tabla
    {NULL, NULL}
};
/*
strcasecmp local comparacion case-insensitive de 2 cadenas completas
como ya se lo tiene en http.c hacemos una version simple que compara 
2 cadenas hasta su fin 
no se la expone solo se la usaa de manera interna
*/
static int strcasecmp_local(const char *a, const char *b){
    while (*a && *b){
        unsigned char ca= (unsigned char)tolower((unsigned char)*a);
        unsigned char cb= (unsigned char)tolower((unsigned char)*b);
        if (ca!=cb) return (int) ca - (int)cb;
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
/*
find extension encuentra el ultimo . en el nombre
devuelve un puntero al . incluyendolo o NULL si el archivo no tiene
una extension

solo se conside el ultimo . despues del ultimo /
par que las rutas como
"/path.with.dots/ file"
no se confundan con extension
.with.dots/file*/
static const char *find_extension(const char *filename){
    const char *last_slash;
    const char *last_dot;
    const char *base;
    if (filename == NULL) return NULL;
    //buscar el ultimo separador de path para aislar el nombre base
    last_slash = strrchr(filename, '/');
    base = (last_slash != NULL) ? (last_slash + 1) : filename;
    //buscar el ultimo punto en el nombre base
    last_dot = strrchr(base, '.');
    if (last_dot ==NULL|| last_dot == base) return NULL; //no hay extension o el punto es el primer caracter
    return last_dot; //retorna el punto incluido
}
const char *mime_get_type(const char *filename){
    const char *ext;
    const struct mime_entry *entry;
    ext = find_extension(filename);
    if (ext == NULL) return MIME_DEFAULT; //no hay extension
    //buscar en la tabla de mime
    for (entry = mime_table; entry->extension != NULL; entry++){
        if (strcasecmp_local(ext, entry->extension) == 0){
            return entry->mime_type; //encontrado
        }
    }
    return MIME_DEFAULT; //no se encontro la extension
}