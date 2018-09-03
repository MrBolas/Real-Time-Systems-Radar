////////////////////////////////////////////////////////////////////////
//                             2013/2014                              //
//                       Sistemas a Tempo Real                        //
//                                                                    //
//                                                                    //
//                                                                    //
//                                                                    //
////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h> //ioperm inb outb
#include "common.h"
#include "socketlib.h"
#include <native/task.h>
#include <sys/mman.h> //mloockall
#include <time.h>
#include <native/timer.h>
#include <string.h>
#include <curses.h>


#define CONLIMIT 100  //Concurrent connection limit
#define MESSLEN  30   //Time string length

#define TASK_PRIO  70 /* 99 is Highest RT priority, 0 is Lowest*/
#define TASK_MODE  0  /* No flags */
#define TASK_STKSZ 0  /* Stack size (use default one) */

#define TASK_PERIOD 2500000
#define DATALEN 200

int numcon=0; //current number of concurrent connections
int chunk[DATALEN];

//variaveis display
int old_buf [DATALEN]={0};
int new_buf [DATALEN];
int buf [DATALEN];
int i,j,z,h;
int nbits;
int c,k; //c - line bits count, k - bits losted counter
int old_bit;
int new_bit;

//Real Time Tasks
RT_TASK tDISPLAY;
RT_TASK tSERVER;
RT_TASK connection[CONLIMIT];

//for target identification
int new_bit_prev,new_bit_next,old_bit_prev,old_bit_next,checkpoint;

//Possible errors for Real Time Tasks
int e1, e2, e3, e4, e5;
int d1, d2, d3, d4, d5;

int mti=0; //view mode
int alert=0; //target warning
int lost=0; //fail warning
int key=0; // value of key
bool shuttingDown=false;

//warnings
static char *view[]={"Normal View",
    "Advance View"};
static char *target[]={"No problem with targets",
    "ALERT: IMINENT COLISION!",
    "ALERT: COLISION DETECTED!!"};
static char *fail[]={"Connected",
    "FAIL, CONNECTION RESET, AND LOST",
    "FAIL, CONNECTION REFUSED, AND LOST",
    "FAIL, CONNECTION ABORTED, AND LOST"};


//Funcao a tempo real de converção da DATAPORT e COUNTPORT para Array
void serve() {

int count=0;
time_t t;
char buf[MESSLEN];

    //Declaraçao da tarefa como periodica
	if (rt_task_set_periodic(NULL,TM_NOW,rt_timer_ns2ticks(TASK_PERIOD))!=0)
	printf("Error creating turning task periodic\n");

	for(;;){
	rt_task_wait_period(NULL);
    	t = time(NULL);
	if (t==-1) sprintf(buf, "%s\n", strerror(errno));
        
    //Converção de I/O ports para array se sincronizado
	if(inb(COUNTPORT)==count){
        count=inb(COUNTPORT);
        chunk[count]=inb(DATAPORT);
	count++;
	}
    
    //Se não estiver sincronizado adiciona o valor 256 que será traduzido para um "X"
	else if(inb(COUNTPORT)>count){
	int offset=inb(COUNTPORT)-count;
		for (offset;offset<inb(COUNTPORT);offset++){
		chunk[offset]=256;
		}
	count++;
	}
        
        //Passar o novo array recebido para o array a ser usado pelo display e o array a ser usado pelo display para um array do frame do display anterior de forma a permitir calculo de movimento
        if (count==200) {

		count=0;

        for(z=0;z<DATALEN;z++){       
               old_buf[z]=new_buf[z]; new_buf[z]=chunk[z];
    	}
     }
  }  
}

//Função a tempo real que mantem a socket aberta entre servidor e cliente e em cada 0,5 segundos
void server(void *arg){
int conNumber=numcon;
int csock = *((int *) arg);
int socket_control,errorr;
int ack=1;
int shutDownSignal=27;

        //Declaração da tarefa como periodica
		if (rt_task_set_periodic(NULL,TM_NOW,rt_timer_ns2ticks(TASK_PERIOD*200))!=0)
	    printf("Error creating turning task periodic\n");

	for(;;){

		rt_task_wait_period(NULL);

        
        /* Sistema de Controlo implementado com ack's para evitar efeito Speedy Gonzalez na reconnecção do cliente. Este efeito e evitado com a implementação de um duplo ack , da mesma forma que funciona o three hadn shake, quando o hand shake falha o servidor pára de enviar de forma a quando a reconeção é feita o cliente e actualizado mas so com os ultimos dados recebidos pelo servidor do radaradc
         */
        
        //Sinal de shutdown para os clientes
        if (shuttingDown) {
            write(csock,&shutDownSignal,sizeof(int));
        }
        
        // Escrita do 3-hand shake
		write(csock,&ack,sizeof(int));
        
        //Leitura do 3-hand shake
		socket_control=0;
		if(errorr=read(csock,&socket_control,sizeof(int))<0){
		}
        
        /*Se a leitura feita for de um inteiro "27" significa que o cliente encerrou dessa forma esta tarefa é desnecessaria (existe uma tarefa destas por ligação a cliente) então termina-se esta tarefa */
		if(socket_control==27){
		rt_task_delete(&connection[--numcon]);
        }
		
        
        //Envio da informação do frame(imagem)
		if(socket_control==1){
        int flag=write (csock, chunk, sizeof(chunk));
		}
	}
}

void display () { 
		
	initscr(); //Init ncurses
	if(has_colors() == FALSE) { endwin(); curs_set(1); /*rt_task_delete(&tSOCKETREAD);*/ rt_task_delete(&tDISPLAY); exit(1); } //check terminal manipulation on colors
	nonl(); //no new line
	noecho(); // no echopc
	curs_set(0); //hide cursor
	nodelay(stdscr, true); 
	start_color(); // Start color mode			
	init_pair(1, COLOR_WHITE, COLOR_BLACK); // background (no target)
	init_pair(2, COLOR_WHITE, COLOR_GREEN); // fixed target
	init_pair(3, COLOR_WHITE, COLOR_RED);   // movingtarget
	init_pair(4, COLOR_WHITE, COLOR_BLUE);  // any target
	init_pair(5, COLOR_BLACK, COLOR_WHITE);  // menu

	init_pair(6, COLOR_YELLOW, COLOR_WHITE);  // menu alert eminent colition
	init_pair(7, COLOR_RED, COLOR_WHITE);  // menu alert 

	for(;;){

		d4 = rt_task_set_periodic(&tDISPLAY, TM_NOW, rt_timer_ns2ticks(TASK_PERIOD)); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		
		if (d4) {
			endwin(); //restore the saved shell terminal mode 
			curs_set(1); //show cursor
			fprintf (stderr, "-EINVAL is returned if task inew_bit_prevs not a task descriptor, or period is different from TM_INFINITE but shorter than the scheduling latency value for the target system, as available from /proc/xenomai/latency.\n-EIDRM is returned if task is a deleted task descriptor.\n-ETIMEDOUT is returned if idate is different from TM_INFINITE and represents a date in the past.\n-EWOULDBLOCK is returned if the system timer is not active.\n-EPERM is returned if task is NULL but not called from a task context.");					
			//rt_task_delete(&tSOCKETREAD); 
			rt_task_delete(&tDISPLAY);
			
			exit(1);
		}

		d5 = rt_task_wait_period(NULL); //deschedule until next period.
	
		if (d5) {
			endwin(); //restore the saved shell terminal mode 
			curs_set(1); //show cursor
			fprintf (stderr, "-EWOULDBLOCK is returned if rt_task_set_periodic() has not previously been called for the calling task.\n-EINTR is returned if rt_task_unblock() has been called for the waiting task before the next periodic release point has been reached. In this case, the overrun counter is reset too.\n-ETIMEDOUT is returned if a timer overrun occurred, which indicates that a previous release point has been missed by the calling task. If overruns_r is valid, the count of pending overruns is copied to the pointed memory location.\n-EPERM is returned if this service was called from a context which cannot sleep (e.g. interrupt, non-realtime or scheduler locked).");		
			//rt_task_delete(&tSOCKETREAD); 
			rt_task_delete(&tDISPLAY);
			exit(1);
		}
	
		//screen inicilization 
		attrset(COLOR_PAIR(1));      
		for (i=0; i<20; i++) { for (j=0; j<(10*8); j++) { mvprintw(i,j," "); }}
		//menu inicilizationSTATUS
		attrset(COLOR_PAIR(5));      
		for (i=21; i<27; i++) { for (j=0; j<(10*8); j++) { mvprintw(i,j," "); }}
		//checkpoint inicialization
		checkpoint=0;
	
		//footnote
		//screen mode selection
		key=getch();	/* check key pressed */	

		if (key==110) { // tecla N (fixed and moved targets with the same colors)
			mti=0;
		} else if (key==109) { // tecla M (fixed and moved targets with diferent colors)
			mti=1; 
		} 

			if (key==27) {
				
                shuttingDown=true;
		}
		
		//menu		
		attrset(COLOR_PAIR(5));  
		for (j=0; j<(10*8); j++) { mvprintw(20, j, "="); }
		mvprintw(22, 0, " MOVING TARGET INDICATOR: %s", view[mti]);
		//mvprintw(23, 0, " TARGETS STATUS: %s", target[alert]);
		if (alert==0) {
			mvprintw(23, 0, " TARGETS STATUS: ");
			attrset(COLOR_PAIR(5));
			mvprintw(23, 17, "%s", target[alert]);
		} else if (alert==1) {
			mvprintw(23, 0, " TARGETS STATUS: ");
			attrset(COLOR_PAIR(6));
			mvprintw(23, 17, "%s", target[alert]);
		} else if (alert==2) {
			mvprintw(23, 0, " TARGETS STATUS: ");
			attrset(COLOR_PAIR(7));
			mvprintw(23, 17, "%s", target[alert]);
		}
		
		attrset(COLOR_PAIR(5));
		mvprintw(24, 0, " NUMBER OF CONNECTIONS: %d", numcon);
		mvprintw(25, 0, " SHORTKEYS: [n]- Normal View [m]- Advance View [esc]- Close Radar");

		//screen

		for (i=0; i<20; i++) { //lines
			c=0; //reset bit screen counter per line
			for (j=0; j<10; j++) { //colunmns in bytes				
				
				
				if (new_buf[i*10+j]==256) { //lost bits received by socketread
					attrset(COLOR_PAIR(1));
					for (k=c;k<(c+8);k++) {	mvprintw(i,j+k,"X"); }
					c=c+8; 
					checkpoint=1; //if there is any lost bits and no moving bits
				}else {
					//while(nbits!=0) { //convert 1 byte to 8 bits					
					for (nbits = 7; nbits >= 0; nbits--) { //convert 1 byte to 8 bits
	
						old_bit = old_buf[i*10+j] >> nbits;
						new_bit = new_buf[i*10+j] >> nbits;

						//target identification
						if ((new_bit & 1)&&(old_bit & 1)) { //fixed targets
							if (mti==0) attrset(COLOR_PAIR(4));
							else if (mti==1) attrset(COLOR_PAIR(2));
    							mvprintw(i,j+c," "); //add targets 
						} else if (new_bit & 1) { //moved targets
							if (mti==0) attrset(COLOR_PAIR(4));    
							else if (mti==1) attrset(COLOR_PAIR(3));  
    							mvprintw(i,j+c," "); //add targets
							checkpoint=1;

							alert=0;//alert reinicilization

							//target colision identification
							if ((nbits!=7)&&(nbits!=0)) {
								new_bit_prev = new_buf[i*10+j] >> (nbits-1);
								new_bit_next = new_buf[i*10+j] >> (nbits+1);
								old_bit_prev = old_buf[i*10+j] >> (nbits-1);
								old_bit_next = old_buf[i*10+j] >> (nbits+1);
								if (((new_bit & 1)&&(new_bit_next & 1))&&(old_bit_prev & 1)) { alert=1; }//moving to right
								if (((new_bit & 1)&&(new_bit_prev & 1))&&(old_bit_next & 1)) { alert=1; }//moving to left
									
							} 	 
						}
		
						c++;
					}
					
				} 
				c=c--;	//to remove last increment on bit counter		
			}
		}
		
		if (checkpoint==0) {alert=2;} // no target has been detected so we have a colision

		refresh();
	} 
	
}

int main (int argc, char **argv) {

int port, maxcon;	//user defined maximum concurrent connections
int csock,sock;
int err;
char id_task[7]="server0";

#pragma - mark input from adc

if(ioperm(COUNTPORT,1,1)) {             //ioperm: obtain�hardware�i/o�access�permission  $
        puts("error: must be root");    //for io port COUNTPORT 
        exit(1); }                            //No need in mode kernel  

if(ioperm(DATAPORT,1,1)) {         //ioperm: obtain�hardware�i/o�access�permission       $
        puts("error");               //for io port DATAPORT 
        exit(1); }                           //No need in mode kernel   

#pragma -mark telnet connection and transfer

        mlockall(MCL_CURRENT|MCL_FUTURE);
	
   	// Analisar inputs e verificar se cumprem com o programa
   	if (argc!=3) {
    	fprintf(stderr, "Command line:\n"
                      	"dtd <Maximum concurrent connections (1-100)> <port>\n");
      	exit(-1);
      	}
   	maxcon=atoi(argv[1]);   
   	port=atoi(argv[2]);   

    // Numero de conexões permitidas
   	if (maxcon<1 || maxcon>CONLIMIT) {
    	fprintf(stderr, "Maximum concurrent connections %d\n", CONLIMIT);
	  	exit (EXIT_FAILURE);
      	}
    
    // Validação da porta
   	if (port<1024 || port>65535) {
      	fprintf(stderr, "Port range: 1024 - 65535\n");
	  	exit (EXIT_FAILURE);
      	}
   
   	// Create stream server internet socket 
   	sock=setSocket (port, SOCK_STREAM);

   	// Inicia o Display
    int err1=rt_task_spawn(&tDISPLAY,"display",TASK_STKSZ,TASK_PRIO/2,TASK_MODE,&display,NULL);
            if (err1<0) {
                printf ("Error creating display task = %d\n", err1);
                rt_task_delete(&tDISPLAY);
            }
    
	//Inicia a Aquisição de dados
    int err2=rt_task_spawn(&tSERVER,"values aquire",TASK_STKSZ,TASK_PRIO,TASK_MODE,&serve,NULL);
            if (err2<0) {
                printf ("Error creating server task = %d\n", err2);
                rt_task_delete(&tSERVER);
            }


    //Loop que avalia novas ligações e cria sockets para as mesmas
    for (;;) {
      	if (numcon < maxcon) {
            
			if ((csock=getClientSocket(sock, 100))>0) {
				err = rt_task_spawn(&connection[numcon++],id_task,TASK_STKSZ,TASK_PRIO,TASK_MODE,&server, &csock); //retorn 0 se OK ou <0 se erro
                
                sprintf(id_task,"server%d",numcon);
				if (err<0) {
					printf ("Error creating task = %d\n", err);
					rt_task_delete(&connection[--numcon]);
                }
              if(shuttingDown){
                sleep(3);
                rt_task_delete(&tSERVER);
                for (h=0; h<numcon; h++) {
		        rt_task_delete(&connection[h]);
		}
		      endwin(); //restore the saved shell terminal mode
			curs_set(1); //show cursor
			rt_task_delete(&tDISPLAY);
			exit(0);
}  
            }
        }
    }   

}
