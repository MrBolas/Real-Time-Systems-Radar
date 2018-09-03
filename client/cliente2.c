//////////////////////////////////////////////////////////////////////////////////////////
//                                     2013/2014                                        //
//                                Sistemas a Tempo Real                                 //
//                                                                                      //
//                                Fábio Cabrita n37030                                  //
//                                 João Bolas n40651                                    //
//                               Vitaliy Parkula n37047                                 //
//                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h> //exit
#include <unistd.h>
#include <errno.h>
#include "socketlib.h"
#include <stdio.h>
#include <sys/mman.h> //mlockall
#include <native/task.h>
#include <native/timer.h>
#include "socketlib.h"
#include <ncurses.h> //display

#define MAXLEN 200 //buffer space for the 0-199 array

#define TASK_PRIO 60 // 99 is Highest RT priority, 0 is Lowest
#define TASK_MODE 0 // No flags
#define TASK_STKSZ 0 // default Stack size
#define TASK_PERIOD 50000000 // 500 ms = 500.000.000ns = 200*2.5ms

RT_TASK tSOCKETREAD; // task name
RT_TASK tDISPLAY; // task name
RT_TASK tRJ45STATE; // task name

int e1, e2, e3, e4, e5; // possible errors from socketread rt functions
int d2, d3, d4, d5; // possible errors from display rt functions
int c2, c3, c4, c5; // possible errors from rj45state rt functions

int sock, port, DATALEN;
int ack_env=1;
int ack_rec;
char *servername;

static int old_buf [MAXLEN];
static int new_buf [MAXLEN];
static int buf [MAXLEN];

int i,j,x,y;
int nbits;
int c,k; //c - line bits count, k - bits losted counter
int old_bit;
int new_bit;
int len;
int disc=0;

//for target colision identification
int new_bit_prev;
int new_bit_next;
int old_bit_prev;
int old_bit_next;
int checkpoint; //checks if there is any moving target on the screen, otherwhise colision detected

//state inicialization
int mti=0; //view mode
int alert=0; //target warning
int lost=0; //fail warning

int key=0; // value of key

//display dinamic messages aka warnings
static char *view[]={"Normal View",
    "Advance View"};
static char *target[]={"No problem with targets",
    "ALERT: IMINENT COLISION!",
    "ALERT: COLISION DETECTED!!"};
static char *fail[]={"Connected",
    "Disconnected"};

void display () {
    
	initscr(); //Init ncurses
	if(has_colors() == FALSE) { endwin(); curs_set(1); rt_task_delete(&tSOCKETREAD); rt_task_delete(&tDISPLAY); exit(1); } //check terminal manipulation on colors
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
	init_pair(7, COLOR_RED, COLOR_WHITE);  // menu alert start_socket_control
    
	for(;;){
        
		d4 = rt_task_set_periodic(&tDISPLAY, TM_NOW, rt_timer_ns2ticks(TASK_PERIOD)); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		
		if (d4) {
			endwin(); //restore the saved shell terminal mode
			curs_set(1); //show cursor
			fprintf (stderr, "-EINVAL is returned if task inew_bit_prevs not a task descriptor, or period is different from TM_INFINITE but shorter than the scheduling latency value for the target system, as available from /proc/xenomai/latency.\n-EIDRM is returned if task is a deleted task descriptor.\n-ETIMEDOUT is returned if idate is different from TM_INFINITE and represents a date in the past.\n-EWOULDBLOCK is returned if the system timer is not active.\n-EPERM is returned if task is NULL but not called from a task context.");
			rt_task_delete(&tSOCKETREAD);
			rt_task_delete(&tRJ45STATE);
			rt_task_delete(&tDISPLAY);
			
			exit(1);
		}
        
		d5 = rt_task_wait_period(NULL); //deschedule until next period.
        
		if (d5) {
			endwin(); //restore the saved shell terminal mode
			curs_set(1); //show cursor
			fprintf (stderr, "-EWOULDBLOCK is returned if rt_task_set_periodic() has not previously been called for the calling task.\n-EINTR is returned if rt_task_unblock() has been called for the waiting task before the next periodic release point has been reached. In this case, the overrun counter is reset too.\n-ETIMEDOUT is returned if a timer overrun occurred, which indicates that a previous release point has been missed by thestart_socket_control calling task. If overruns_r is valid, the count of pending overruns is copied to the pointed memory location.\n-EPERM is returned if this service was called from a context which cannot sleep (e.g. interrupt, non-realtime or scheduler locked).");
			rt_task_delete(&tSOCKETREAD);
			rt_task_delete(&tRJ45STATE);
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
		
		//menu
		attrset(COLOR_PAIR(5));
		for (j=0; j<(10*8); j++) { mvprintw(20, j, "="); }
		mvprintw(22, 0, " MOVING TARGET INDICATOR: %s", view[mti]);
		mvprintw(23, 0, " TARGETS STATUS: %s", target[alert]);
        
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
		mvprintw(24, 0, " CONNECTION STATUS: %s", fail[lost]);
		mvprintw(25, 0, " SHORTKEYS: [n]- Normal View [m]- Advance View [esc]- Close Radar");
        
		//screen
        
		for (i=0; i<20; i++) { //lines
			c=0; //reset bit scrost=ost=een counter per line
			for (j=0; j<10; j++) { //colunmns in bytes
				
				
				if (new_buf[i*10+j]==256) { //lost bits received by socketread
					attrset(COLOR_PAIR(1));
					for (k=c;k<(c+8);k++) {	mvprintw(i,j+k,"X"); }
					c=c+8;
					checkpoint=1; //if there is any lost bits and no moving bits
				}else {
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

void rj45state () {
    int kap,rj45buf[MAXLEN];
	int discThreshold=-30;
    
	for(;;){
        
		c4=rt_task_set_periodic(&tRJ45STATE, TM_NOW, rt_timer_ns2ticks(TASK_PERIOD));
        
		if (c4) {
			endwin(); //restore the saved shell terminal mode
			curs_set(1); //show cursor
			fprintf (stderr, "-EINVAL is returned if task inew_bit_prevs not a task descriptor, or period is different from TM_INFINITE but shorter than the scheduling latency value for the target system, as available from /proc/xenomai/latency.\n-EIDRM is returned if task is a deleted task descriptor.\n-ETIMEDOUT is returned if idate is different from TM_INFINITE and represents a date in the past.\n-EWOULDBLOCK is returned if the system timer is not active.\n-EPERM is returned if task is NULL but not called from a task context.");
			rt_task_delete(&tSOCKETREAD);
			rt_task_delete(&tRJ45STATE);
			rt_task_delete(&tDISPLAY);
			
			exit(1);
		}
        
		c5=rt_task_wait_period(NULL);
        
		if (c5) {
			endwin(); //restore the saved shell terminal mode
			curs_set(1); //show cursor
			fprintf (stderr, "-EWOULDBLOCK is returned if rt_task_set_periodic() has not previously been called for the calling task.\n-EINTR is returned if rt_task_unblock() has been called for the waiting task before the next periodic release point has been reached. In this case, the overrun counter is reset too.\n-ETIMEDOUT is returned if a timer overrun occurred, which indicates that a previous release point has been missed by thestart_socket_control calling task. If overruns_r is valid, the count of pending overruns is copied to the pointed memory location.\n-EPERM is returned if this service was called from a context which cannot sleep (e.g. interrupt, non-realtime or scheduler locked).");
			rt_task_delete(&tSOCKETREAD);
			rt_task_delete(&tRJ45STATE);
			rt_task_delete(&tDISPLAY);
			exit(1);
		}
        
        //Sistema de deteção de disconneção do Cabo Ethernet
		if (disc<discThreshold&&lost==0) {
		    lost=1;
		    for(kap=0;kap<MAXLEN;kap++){
		        rj45buf[kap]=new_buf[kap];
		    }
		}
        
		disc--;
		if(lost==1){
		    for(kap=0;kap<MAXLEN;kap++){
		        if(rj45buf[kap]!=new_buf[kap]){
		            lost=0;
		            disc=0;
		        }
		    }
		}
	}
}

void socketread () {
    
	for (i=0; i<MAXLEN; i++) { new_buf[i]=256; } // new_buf inicialization
    
	sock=connectHost (servername, port, SOCK_STREAM); //TCP
    
	for(;;) {
        
		e4 = rt_task_set_periodic(&tSOCKETREAD, TM_NOW, rt_timer_ns2ticks(TASK_PERIOD));
        
		if (e4) {
			endwin(); //restore the saved shell terminal mode
			curs_set(1); //show cursor
			fprintf (stderr, "-EINVAL is returned if task is not a task descriptor, or period is different from TM_INFINITE but shorter than the scheduling latency value for the target system, as available from /proc/xenomai/latency.\n-EIDRM is returned if task is a deleted task descriptor.\n-ETIMEDOUT is returned if idate is different from TM_INFINITE and represents a date in the past.\n-EWOULDBLOCK is returned if the system timer is not active.\n-EPERM is returned if task is NULL but not called from a task context.");
			rt_task_delete(&tSOCKETREAD);
			rt_task_delete(&tRJ45STATE);
			rt_task_delete(&tDISPLAY);
			
			exit(1);
		}
        
		e5 = rt_task_wait_period(NULL); //deschedule until next period.
        
		if (e5) {
			fprintf (stderr, "-EWOULDBLOCK is returned if rt_task_set_periodic() has not previously been called for the calling task.\n-EINTR is returned if rt_task_unblock() has been called for the waiting task before the next periodic release point has been reached. In this case, the overrun counter is reset too.\n-ETIMEDOUT is returned if a timer overrun occurred, which indicates that a previous release point has been missed by the calling task. If overruns_r is valid, the count of pending overruns is copied to the pointed memory location.\n-EPERM is returned if this service was called from a context which cannot sleep (e.g. interrupt, non-realtime or scheduler locked).");
			rt_task_delete(&tSOCKETREAD);
			rt_task_delete(&tRJ45STATE);
			rt_task_delete(&tDISPLAY);
			exit(1);
		}
        
		read (sock, &ack_rec,sizeof(int));
        
		if (ack_rec==27) { // Read data from the server, print it
			key=27;
		}
        
		write (sock, &ack_env, sizeof(int));
		len = read (sock, buf, sizeof(buf));
        
		if (len>=0) { // Read data from the server, print it
			disc=disc+10;
			for (i=0; i<MAXLEN; i++) { old_buf[i]=new_buf[i]; new_buf[i]=buf[i]; }
		}
	}
}


int main(int argc, char **argv) {
	servername=argv[1];
	// Evaluate command line arguments
	if (argc!=3) {
        fprintf(stderr, "Command line:\n","Cliente <server name or IP> <port>\n");
        exit(-1);
    }
    port = atoi(argv[2]);
	
	RT_TIMER_INFO info;
    
	mlockall(MCL_CURRENT|MCL_FUTURE); // reserve memory space
    
	/*socketread*/
    
	e1 = rt_timer_set_mode(TM_ONESHOT);
	e2 = rt_task_create(&tSOCKETREAD, "socketread", TASK_STKSZ, TASK_PRIO, TASK_MODE); 	//set period
	e3 = rt_task_start(&tSOCKETREAD, &socketread, NULL); //inicilization of socket read
    
	if (e1) {
		endwin(); //restore the saved shell terminal mode
		curs_set(1); //show cursor
		fprintf(stderr, "-ENODEV is returned if the underlying architecture does not support the requested periodic timing. Aperiodic/oneshot timing is always supported.\n");
		rt_task_delete(&tSOCKETREAD);
		rt_task_delete(&tRJ45STATE);
		rt_task_delete(&tDISPLAY);
		exit(1);
	} else if (e2) {
		endwin(); //restore the saved shell terminal mode
		curs_set(1); //show cursor
		fprintf(stderr, "-ENOMEM is returned if the system fails to get enough dynamic memory from the global real-time heap in order to create or register the task. \n-EEXIST is returned if the name is already in use by some registered object.\n -EPERM is returned if this service was called from an asynchronous context.");
		rt_task_delete(&tSOCKETREAD);
		rt_task_delete(&tRJ45STATE);
		rt_task_delete(&tDISPLAY);
		exit(1);
	} else if (e3) {
		endwin(); //restore the saved shell terminal mode
		curs_set(1); //show cursor
		fprintf(stderr, "-EINVAL is returned if task is not a task descriptor.\n-EIDRM is returned if task is a deleted task descriptor.\n-EBUSY is returned if task is already started.\n-EPERM is returned if this service was called from an asynchronous context.");
		rt_task_delete(&tSOCKETREAD);
		rt_task_delete(&tRJ45STATE);
		rt_task_delete(&tDISPLAY);
		exit(1);
	}
    
	/*display*/
    
	d2 = rt_task_create(&tDISPLAY, "display", TASK_STKSZ, (TASK_PRIO/2-10), TASK_MODE); 	//set period
	d3 = rt_task_start(&tDISPLAY, &display, NULL); //inicilization of socket read
    
	if (d2) {
		endwin(); //restore the saved shell terminal mode
		curs_set(1); //show cursor
		fprintf(stderr, "-ENOMEM is returned if the system fails to get enough dynamic memory from the global real-time heap in order to create or register the task. \n-EEXIST is returned if the name is already in use by some registered object.\n -EPERM is returned if this service was called from an asynchronous context.");
		rt_task_delete(&tSOCKETREAD);
		rt_task_delete(&tRJ45STATE);
		rt_task_delete(&tDISPLAY);
		exit(1);
	} else if (d3) {
		endwin(); //restore the saved shell terminal mode
		curs_set(1); //show cursor
		fprintf(stderr, "-EINVAL is returned if task is not a task descriptor.\n-EIDRM is returned if task is a deleted task descriptor.\n-EBUSY is returned if task is already started.\n-EPERM is returned if this service was called from an asynchronous context.");
		rt_task_delete(&tRJ45STATE);
		rt_task_delete(&tSOCKETREAD);
		rt_task_delete(&tDISPLAY);
		exit(1);
	}
    
	c2 = rt_task_create(&tRJ45STATE, "write", TASK_STKSZ, 10, TASK_MODE); 	//set period
	c3 = rt_task_start(&tRJ45STATE, &rj45state, NULL); //inicilization of socket read
	if (c2) {
		endwin(); //restore the saved shell terminal mode
		curs_set(1); //show cursor
		fprintf(stderr, "-ENOMEM is returned if the system fails to get enough dynamic memory from the global real-time heap in order to create or register the task. \n-EEXIST is returned if the name is already in use by some registered object.\n -EPERM is returned if this service was called from an asynchronous context.");
		rt_task_delete(&tRJ45STATE);
		rt_task_delete(&tSOCKETREAD);
		rt_task_delete(&tDISPLAY);
		exit(1);
	} else if (c3) {
		endwin(); //restore the saved shell terminal mode
		curs_set(1); //show cursor
		fprintf(stderr, "-EINVAL is returned if task is not a task descriptor.\n-EIDRM is returned if task is a deleted task descriptor.\n-EBUSY is returned if task is already started.\n-EPERM is returned if this service was called from an asynchronous context.");
		rt_task_delete(&tRJ45STATE);
		rt_task_delete(&tSOCKETREAD);
		rt_task_delete(&tDISPLAY);
		exit(1);
	}
	
	/*end of the program */	
	
	for(;;) {
		if (key==27) {
			write (sock,&key,  sizeof(key));
			rt_task_delete(&tRJ45STATE);
			rt_task_delete(&tDISPLAY);			
			endwin(); //restore the saved shell terminal mode 
			curs_set(1); //show cursor
			rt_task_delete(&tSOCKETREAD); 
			exit(0);
		}	
	}	
    
	return 0;
}
