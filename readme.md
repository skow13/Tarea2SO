# Tarea 2 Sistemas Operativos: Torneo de Komikan Concurrente

Universidad Diego Portales - Escuela de Informática y Telecomunicaciones
Integrantes: Diego Briones, Matias Rojas Lara

Simulador de un torneo de Komikan concurrente usando hilos POSIX (pthreads). Un servidor recibe a N jugadores, los empareja (un Puma con un equipo de Perros) y los asigna a mesas, con un máximo de M partidas simultáneas. Cada mesa tiene un juez que valida y aplica los movimientos de forma serializada, de modo que el tablero nunca queda en un estado inconsistente aunque muchas fichas intenten moverse al mismo tiempo. El programa tolera desconexiones abruptas de jugadores y reacciona a SIGINT (Ctrl+C).

## Contenido del repositorio

- `Torneo.c`: implementación completa del servidor, las mesas, el emparejamiento y el ejecutor de casos de prueba.
- `Makefile`: compila el programa y limpia los archivos generados.
- `test_cases.txt`: suite de 12 casos de prueba.
- `readme.md`: este documento, con la compilación, el uso, el formato de configuración y las decisiones de sincronización.
- `torneo.log`: se genera al ejecutar. Contiene la misma salida que se imprime por pantalla.

## Compilación

Es necesario utilizar el siguiente comando:

```
gcc -Wall -Wextra -std=c17 -O2 -pthread Torneo.c -o torneo
```

También se puede usar `make`, que ejecuta el mismo comando. `make clean` borra el binario y `torneo.log`.

Requiere Linux, ya que usa mutex robustos (`pthread_mutexattr_setrobust`), `sigtimedwait` y semáforos POSIX. Fue probado en Ubuntu (WSL).

## Modo de uso

```
./torneo N M [config.txt]
```

- `N`: cantidad de jugadores que llegan al servidor (entero mayor a cero).
- `M`: cantidad máxima de mesas (partidas) simultáneas (entero mayor a cero).
- `config.txt`: archivo opcional. Si no se entrega, se usan los valores por defecto.

Los jugadores con id par llegan como Puma y los de id impar como Perros, por lo que se forman parejas de a un Puma y un equipo de Perros. Si sobra un jugador sin pareja al terminar las llegadas, queda marcado como "sin pareja".

Ejemplos:

```
./torneo 20 4                       # torneo de 20 jugadores con máximo 4 mesas
./torneo 20 4 config.txt            # igual, pero con configuración propia
./torneo 2 1 test_cases.txt         # ejecuta la suite de 12 casos de prueba
```

Para simular la interrupción del servidor basta con presionar Ctrl+C mientras se ejecuta. El servidor deja de aceptar jugadores, avisa a todas las mesas activas, las cierra ordenadamente, libera los recursos y muestra el resumen final.

El código de salida es `0` si todo salió bien, `1` si algún caso o verificación falló y `2` si hubo un error de uso o no se pudo abrir el archivo de configuración.

## Formato del archivo de configuración

Las líneas vacías se ignoran y todo lo que sigue a un `#` es un comentario. Las coordenadas se escriben como `x y` (0 a 4) o con los nombres `TRONO_TOP`, `TRONO_LEFT` y `TRONO_RIGHT` para las tres casillas del trono.

Parámetros de configuración (en una misma línea o en líneas separadas):

| Parámetro | Significado | Valor por defecto |
|---|---|---|
| `SEED n` | Semilla del generador aleatorio. Con la misma semilla las decisiones aleatorias de cada ficha son reproducibles. | 1 |
| `DELAY_MIN ms` | Retardo mínimo entre jugadas de una ficha. | 50 |
| `DELAY_MAX ms` | Retardo máximo entre jugadas de una ficha. | 300 |
| `PUMA_INIT x y` | Casilla inicial del Puma. | casilla 12 (2,2) |
| `DOG_INIT id x y` | Casilla inicial del perro `id` (0 a 11). | disposición estándar |

Ejemplo: `CONFIG SEED 42 DELAY_MIN 50 DELAY_MAX 300 PUMA_INIT 2 2`

## Formato de la suite de pruebas

```
SUITE_START 12

TEST_CASE 1 "Partida Normal - Victoria del Puma"
CONFIG SEED 42 DELAY_MIN 50 DELAY_MAX 300 PUMA_INIT 2 2
EVENT MOVE PUMA 0 FROM 2 2 TO 2 3
EVENT MOVE DOG 4 FROM 1 3 TO 2 3
TEST_END 1

SUITE_END
```

Eventos soportados:

| Evento | Descripción |
|---|---|
| `EVENT MOVE PUMA 0 FROM x y TO x y` | Movimiento del Puma. |
| `EVENT MOVE DOG id FROM x y TO x y` | Movimiento del perro `id`. |
| `EVENT MOVE PUMA 0 FROM ... TO ... CAPTURE DOG id AT x y` | Salto del Puma que captura al perro `id`. |
| `EVENT SPAWN_PLAYERS COUNT n MESAS_MAX m` | Lanza `n` jugadores con un máximo de `m` mesas. |
| `EVENT WAIT_QUEUED_PLAYERS` | Espera a que todos los jugadores en cola terminen. |
| `EVENT DISCONNECT PLAYER_PUMA` / `PLAYER_DOG ID n` | Desconexión abrupta de un jugador. |
| `EVENT SIGNAL_SIGINT` | Envía SIGINT al propio proceso. |
| `EVENT BURST_RANDOM_MOVES COUNT n` | Ráfaga de `n` solicitudes aleatorias con retardo mínimo (stress test). |

## Reglas del juego implementadas

El tablero es una grilla de 5x5 con adyacencia en 8 direcciones, más un trono de 3 casillas (`TRONO_TOP`, `TRONO_LEFT`, `TRONO_RIGHT`) conectado a la casilla (2,4). Hay 12 perros y 1 Puma.

- Un movimiento es válido si la casilla destino está libre y es adyacente al origen.
- El Puma además puede saltar sobre un perro adyacente hacia la casilla libre que está justo detrás y eso captura al perro.
- El Puma gana si captura 6 perros. Los Perros ganan si el Puma queda sin movimientos posibles (acorralado).
- Si una partida en modo automático llega a 4000 jugadas aceptadas, se declara empate por límite de jugadas.
- Un movimiento rechazado se informa con su motivo: casilla ocupada, no adyacente, salto sin perro, ficha no está en el origen, partida finalizada o casilla inválida.

## Estructura del programa

El programa está organizado en cuatro capas:

- **Tablero** (`tab_init`, `dist`): construye las tablas de adyacencia y de saltos de las 28 casillas.
- **Mesa** (`mesa_ini`, `juzgar`, `procesar`, `chequear`, `verificar`, `th_juez`, `th_ficha`): lógica de una partida. `juzgar` valida y aplica un movimiento, `verificar` comprueba que el arreglo de casillas y el de posiciones de las fichas coincidan.
- **Servidor** (`srv_nuevo`, `th_cli`, `th_mm`, `srv_detener`, `srv_resumen`): llegada de jugadores, emparejamiento y asignación de mesas.
- **Pruebas** (`cargar`, `pev`, `run_test`): lee el archivo de configuración y ejecuta los eventos de cada caso.

Hilos que existen durante la ejecución:

| Hilo | Cantidad | Función |
|---|---|---|
| Cliente (`th_cli`) | N | Representa a un jugador que llega y espera su partida. |
| Matchmaker (`th_mm`) | 1 | Empareja a un Puma con un equipo de Perros y reserva una mesa. |
| Juez (`th_juez`) | 1 por mesa | Único hilo que modifica el tablero de su mesa. |
| Ficha (`th_ficha`) | 13 por mesa | Una por ficha (Puma y 12 perros). Elige y envía movimientos al juez. |
| Señales (`th_sig`) | 1 | Atiende SIGINT. |

## Decisiones de sincronización

**Un juez por mesa serializa todos los movimientos.** Las fichas no tocan el tablero directamente. Cada ficha encola su solicitud (`enviar`) en una cola circular FIFO de la mesa y espera la respuesta. El juez las atiende de a una en el orden de llegada. Así, si dos fichas intentan entrar a la misma casilla a la vez, la primera en la cola gana y la otra recibe "casilla ocupada". Esto evita race conditions sobre el tablero y hace que el resultado de las colisiones sea determinista una vez fijado el orden de llegada.

**Un mutex por mesa.** Todo el estado de una mesa (casillas, posiciones, cola, contadores) está protegido por su propio `mutex`. Las mesas son independientes entre sí, por lo que no compiten por el mismo lock y se pueden jugar en paralelo.

**Dos variables de condición por mesa, sin busy waiting.** `qcv` despierta al juez cuando llega una solicitud o cuando la partida debe terminar. `rcv` despierta a las fichas cuando el juez ya respondió su solicitud. Todas las esperas se hacen con `pthread_cond_wait` dentro de un `while` que reevalúa la condición, para tolerar despertares espurios. Los hilos nunca dan vueltas en un ciclo consultando el estado.

**Semáforo para limitar las mesas a M.** El matchmaker hace `sem_wait` sobre un semáforo inicializado en M antes de crear una mesa y la mesa hace `sem_post` al terminar (`mesa_fin`). Los jugadores que no encuentran mesa disponible quedan en cola de espera pasiva, bloqueados sin consumir CPU. El resumen final verifica que `simultaneas_max <= M`.

**Cola de emparejamiento con mutex y variable de condición.** El servidor guarda una cola de Pumas y otra de Perros bajo un mismo mutex (`srv->mu`). El matchmaker espera con `cv` hasta que haya al menos uno de cada rol o hasta que lleguen todos los jugadores, ahí marca como "sin pareja" a los que sobran.

**Orden de adquisición de locks para evitar deadlock.** Cuando hay que tomar ambos mutex, siempre se toma primero el del servidor y después el de la mesa (por ejemplo, en `srv_detener`). El juez nunca mantiene el mutex de su mesa mientras toma el del servidor, lo suelta antes de llamar a `mesa_fin`.

**Mutex robustos para las desconexiones abruptas.** Los mutex de las mesas se crean con `PTHREAD_MUTEX_ROBUST`. Si un hilo muere mientras tiene el lock (lo que simula el caso de prueba de desconexión abrupta), el siguiente que lo toma recibe `EOWNERDEAD`, lo marca como consistente con `pthread_mutex_consistent` y la mesa se marca como fallida. El juez entonces da la victoria al rival del jugador desconectado, despierta a todas las fichas con un `broadcast` y la mesa termina sin dejar hilos colgados ni bloquear al resto del torneo.

**SIGINT manejado por un hilo dedicado.** La señal se bloquea con `pthread_sigmask` en el hilo principal, de modo que todos los hilos creados después la heredan bloqueada. Un único hilo (`th_sig`) la espera con `sigtimedwait`, evitando ejecutar lógica compleja dentro de un manejador asíncrono. Al recibirla, `srv_detener` marca el servidor como detenido, marca cada mesa activa como abortada y hace `broadcast` sobre las variables de condición para despertar a todos los hilos bloqueados. Luego se hace `join` de todos los hilos y se destruyen los mutex, variables de condición y el semáforo.

**Variables atómicas para banderas globales.** `gfin`, `gscript` y `gabort` son `atomic_int`, ya que las leen y escriben hilos distintos sin pasar por un mutex.

**Log protegido por mutex.** Todas las escrituras a pantalla y a `torneo.log` pasan por un mutex global (`glg`), para que las líneas de hilos distintos no se mezclen.

**Terminación limpia.** Al terminar una mesa, el juez espera con `pthread_join` a las 13 fichas de esa mesa y el matchmaker espera a todos los jueces. Al final de `main` se espera al hilo de señales y se libera toda la memoria. No quedan hilos sin recoger.

## Pruebas realizadas

La suite completa (`./torneo 2 1 test_cases.txt`) termina con `SUITE: 12/12 casos OK`. En cada caso el resumen final comprueba que el tablero quedó consistente (`tablero=consistente`) y que nunca hubo más de M mesas activas a la vez. Además:

- Con colisiones simultáneas en una misma casilla (casos 3 y 4), exactamente una ficha ocupa la casilla y las demás reciben "casilla ocupada".
- Con 20 jugadores y 2 mesas (caso 5), las partidas se juegan de a dos como máximo y el resto espera en cola hasta que se libera una mesa.
- Con desconexión abrupta del Puma o de un Perro (casos 9 y 10), la mesa termina con abandono y el torneo continúa.
- Con SIGINT durante un torneo de 10 jugadores y 3 mesas (caso 11), se aborta todo, se liberan los recursos y se muestra el resumen.
- Con 500 solicitudes aleatorias y retardo de 1 ms (caso 12), el tablero se mantiene consistente (500 solicitudes: 490 aceptadas, 10 rechazadas, 9 colisiones, 12 capturas).