# MatCom-Guard-SO-Project-
En este vasto reino digital, los virus y amenazas informáticas son como plagas y ejércitos invasores que buscan corromper tus tierras y saquear tus recursos. MatCom Guard es tu muralla y tu guardia real, un sistema diseñado para vigilar y proteger tu reino (máquina virtual) basado en UNIX de cualquier intruso o actividad sospechosa.

Para compilado ejecucion:
bash~    make && ./matcom-guard

    Recursos:

https://beej.us/guide/bgnet/
https://www.gtk.org/docs/
https://man7.org/linux/man-pages/man5/proc.5.html

    Bibliotecas:

GTK+: Para la interfaz gráfica (alternativa: Qt si prefieres).
libudev (Linux): Para monitorear dispositivos USB.
libprocps: Para leer información de procesos desde /proc.
Sockets (sys/socket.h): Para el escaneo de puertos.
Debugging: gdb y valgrind para detectar errores de memoria.

    Interfaz Gráfica (GTK+)

Widgets clave:
GtkTextView para logs.
GtkProgressBar para uso de CPU/memoria.
Botones para iniciar/parar escaneos.

    Escaneo de Puertos

Enfoque:
Usar sockets TCP para conectar a localhost:puerto.
Si connect() retorna 0, el puerto está abierto.


    Monitoreo de Procesos

Enfoque:
Leer /proc/[pid]/stat y /proc/[pid]/status para CPU y memoria.
Comparar valores entre iteraciones (ej.: si CPU > 90% por 5 segundos, alertar).

    Detección de Dispositivos USB

Enfoque:
Usar inotify (Linux) para monitorear /media/ o /mnt/ en tiempo real.
Ejecutar lsblk o udevadm para listar dispositivos conectados.
Escanear archivos con fopen() y stat() para detectar cambios.