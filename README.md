# Servidor MiniHTTPd

Servidor HTTP/1.1 con lenguaje C, sin uso de bibliotecas HTTP externas.

Proyecto integrador de Computacion Distribuida

## Caracteristicas

- Servidor HTTP/1.1 basico que sirve archivos estaticos desde el directorio `www/`.
- Bucle de eventos con `epoll()` para manejar multiples clientes concurrentemente.
- Conexiones con Keep-Alive con timeout de inactividad modificables.
- Parsing manual del protocolo HTTP/1.1 (solicitud y cabeceras).
- Soporte de los siguientes tipos MIME: HTML, CSS, JS, PNG, JPG.
- Codigos de estado HTTP: 200 OK, 400 Bad Request, 403 Forbidden, 404 Not Found,
  405 Method Not Allowed, 500 Internal Server Error.
- Proteccion contra Directory Traversal usando `realpath()`.
- Rechazo de metodos distintos a GET con codigo 405.
- Validacion de tamaños de buffer para prevenir overflows.

## Requisitos

- Sistema operativo Linux (probado en Ubuntu 26.04 LTS bajo WSL2).
- Compilador `gcc` con soporte para C11 (probado con gcc 15.2.0).
- `GNU make`.

## Estructura del proyecto

```text
minihttpd/
├── Makefile
├── README.md
├── include/
│   ├── http.h
│   ├── server.h
│   ├── mime.h
│   └── files.h
├── src/
│   ├── main.c
│   ├── server.c
│   ├── http.c
│   ├── mime.c
│   └── files.c
└── www/
   ├── index.html
   ├── style.css
   └── image.png
```

## Descripción de archivos

- `Makefile`: Reglas de compilación (`make`, `make clean`) y targets para construir el proyecto.
- `README.md`: Documentación del proyecto (este archivo).
- `include/`:
   - `http.h`: Declaraciones para el parseo y representación de solicitudes/respuestas HTTP.
   - `server.h`: Prototipos y estructuras relacionadas con el bucle de eventos y la gestión de conexiones.
   - `mime.h`: Declaraciones para detección y mapeo de tipos MIME.
   - `files.h`: Interfaces para resolver rutas y servir archivos estáticos.
- `src/`:
   - `main.c`: Punto de entrada; inicializa la configuración y arranca el servidor.
   - `server.c`: Manejo de sockets, `epoll`, aceptación de clientes y gestión del bucle principal.
   - `http.c`: Parsing de la línea de solicitud y cabeceras, y construcción de respuestas básicas.
   - `mime.c`: Determina el tipo MIME de un archivo por su extensión.
   - `files.c`: Resuelve rutas con `realpath()`, valida acceso dentro de `www/` y envía el contenido al cliente.
- `www/`:
   - `index.html`, `style.css`, `image.png`: Ejemplos de contenido estático servible por el servidor.

## Compilacion

Desde la raiz del proyecto:

```bash
make
```

Esto genera el binario `minihttpd` en el directorio actual y los archivos
objeto en `build/`.

Para limpiar los archivos compilados:

```bash
make clean
```

## Ejecucion

Lanzar el servidor (por defecto escucha en el puerto 8080):

```bash
./minihttpd
```

Salida esperada:
```bash
bravo@MSI:~/minihttpd$ ./minihttpd
MiniHTTPd: iniciando...
[files] directorio raiz: /home/bravo/minihttpd/www
MiniHTTPD escuchando en el puerto 8080 (Ctrl+C para salir)
keep-alive timeout: 30 segundos
```
Para detener el servidor: `Ctrl+C`.

## Test

Con el servidor ejecutandose, en otra terminal ejecutar:

```bash
# Pagina principal
curl http://localhost:8080/

# Archivo CSS
curl http://localhost:8080/style.css

# Imagen
curl http://localhost:8080/image.png --output /tmp/imagen.png

# Archivo inexistente (debe devolver 404)
curl -v http://localhost:8080/no_existe.html

# Metodo no permitido (debe devolver 405)
curl -v -X POST http://localhost:8080/
```

Tambien se puede abrir desde un navegador en `http://localhost:8080/`.

## Pruebas de seguridad

Intentos de Directory Traversal (deben devolver 403 cuando el archivo
resuelto cae fuera del directorio `www/`):

```bash
curl --path-as-is "http://localhost:8080/../../../../etc/passwd"
curl --path-as-is "http://localhost:8080/../Makefile"
```

## Arquitectura

El servidor sigue un modelo **event-driven** de un solo hilo:

1. `main.c` invoca `server_run()` con el puerto por defecto.
2. `server.c` crea el socket de escucha, configura `epoll` y entra en el bucle principal.
3. Cuando llega una nueva conexion, se acepta y se registra en `epoll`.
4. Cuando un cliente envia datos, se parsea la solicitud HTTP con
   `http.c`, se determina el archivo solicitado y se delega a `files.c`.
5. `files.c` resuelve la ruta con `realpath()`, valida que este dentro
   de `www/`, abre el archivo y lo envia al cliente con su tipo MIME
   correcto (determinado por `mime.c`).
6. Si el cliente pidio Keep-Alive, la conexion queda abierta para mas
   solicitudes; un barrido periodico cierra conexiones inactivas tras
   30 segundos.

## Autor

Bravo Francis - Proyecto Integrador MiniHTTPd.