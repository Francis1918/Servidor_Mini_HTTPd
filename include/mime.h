#ifndef MIME_H
#define MIME_H

// LISTA DE MAPEO DE LAS EXTENSIONES MIME
/*
el server debe enviar la cabecera content type para que
el navegador interprete correctamente el contenido
para los archivos sin extension conocida se usa application/octet-stream
bytes genericos que el navegador suele descargarlo
\


/////////////////////////////////////
mime get type determina el tipo de mime seguin la extension del archivo
filename nombre o ruta del archivo ej index.html /www/img.png
retorna una cadena estatica con el mime no libera con free()
si la extension no es conocida retorna application/octet-stream 
la comparacion de las extensiones son case-insensitive
.HTML a .html*/
const char *mime_get_type(const char *filename);

#endif //mime.h

