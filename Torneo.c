#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
 
#define NG 25
#define NN 28
#define TTOP 25
#define TLEFT 26
#define TRIGHT 27
#define NP 12
#define NS 13
#define SP 12
#define LIBRE -1
#define CPUMA -2
#define MAXMOV 4000

enum{Q_OK,Q_CAP,Q_OCUP,Q_NOADJ,Q_NOCAP,Q_OBS,Q_FIN,Q_INV};
static const char*RT[]={"Aceptado","Captura","Rechazado: casilla ocupada","Rechazado: no adyacente","Rechazado: salto sin perro","Rechazado: ficha no esta en origen","Rechazado: partida finalizada","Rechazado: casilla invalida"};
enum{E_ESP,E_JUG,E_FIN,E_CANC,E_SINPAR};
enum{E_MOVE,E_SPAWN,E_WAITQ,E_DPUMA,E_DDOG,E_SIGINT,E_BURST};

static struct timespec t0;
static long ms(void){
    struct timespec t
    ;clock_gettime(CLOCK_MONOTONIC,&t)
    ;return(long)(t.tv_sec-t0.tv_sec)*1000+(t.tv_nsec-t0.tv_nsec)/1000000;
}
static void dormir(int d){
    if(d<=0)return;
    struct timespec t={d/1000,(long)(d%1000)*1000000L},r;
    while(nanosleep(&t,&r)==-1&&errno==EINTR)t=r;
}
static pthread_mutex_t glg=PTHREAD_MUTEX_INITIALIZER;
static FILE*glf;
static void lgraw(const char*s){
    pthread_mutex_lock(&glg);
    fputs(s,stdout);fflush(stdout);
    if(glf){f
        puts(s,glf);fflush(glf);
    }
    pthread_mutex_unlock(&glg);
}
static void lg(const char*f,...){
    char b[640],c[720];
    va_list a;
    va_start(a,f);
    vsnprintf(b,sizeof b,f,a);
    va_end(a);
    snprintf(c,sizeof c,"[%6ld] %s\n",ms(),b);
    lgraw(c);
}
static char*cn(int c,char*b){
    if(c==TTOP)strcpy(b,"TRONO_TOP");
    else if(c==TLEFT)strcpy(b,"TRONO_LEFT");
    else if(c==TRIGHT)strcpy(b,"TRONO_RIGHT");
    else if(c>=0&&c<NG)sprintf(b,"%d,%d",c%5,c/5);
    else strcpy(b,"?");return b;
}
static void pnom(int sl,char*b){
    if(sl==SP)strcpy(b,"PUMA");
    else sprintf(b,"PERRO%d",sl);
}

//Dimensiones tablero
static int ady[NN][8],nady[NN],sal[NN][8][2],nsal[NN],CX[NN],CY[NN];
static void ad1(int a,int b){
    ady[a][nady[a]++]=b;
}
static void ad(int a,int b){
    ad1(a,b);ad1(b,a);
}
static void jp(int a,int o,int t){
    sal[a][nsal[a]][0]=o;
    sal[a][nsal[a]][1]=t;
    nsal[a]++;
}
static void tab_init(void){
 for(int y=0;y<5;y++)for(int x=0;x<5;x++){
  int c=y*5+x;CX[c]=x;CY[c]=y;
  for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
   if(!dx&&!dy)continue;
   int x1=x+dx,y1=y+dy,x2=x+2*dx,y2=y+2*dy;
   if(x1<0||x1>4||y1<0||y1>4)continue;
   ad1(c,y1*5+x1);
   if(x2>=0&&x2<=4&&y2>=0&&y2<=4)jp(c,y1*5+x1,y2*5+x2);
  }
 }
 CX[TTOP]=2;CY[TTOP]=5;CX[TLEFT]=1;CY[TLEFT]=6;CX[TRIGHT]=3;CY[TRIGHT]=6;
 ad(TTOP,22);ad(TTOP,TLEFT);ad(TTOP,TRIGHT);ad(TLEFT,TRIGHT);
 jp(17,22,TTOP);jp(TTOP,22,17);
}
static int dist(int a,int b){
    int dx=CX[a]-CX[b],dy=CY[a]-CY[b];
    if(dx<0)dx=-dx;
    if(dy<0)dy=-dy;
    return dx>dy?dx:dy;
}