//-Uncomment to compile with arduino support
//#define ARDUINO

//-------------------------------------
//-  Include files
//-------------------------------------
#include <termios.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <rtems.h>
#include <rtems/termiostypes.h>
#include <bsp.h>

#include "displayC.h"

//-------------------------------------
//-  Constants
//-------------------------------------
#define MSG_LEN 9
#define SLAVE_ADDR 0x8
#define AVG_SPEED 55
#define BRK_SPEED 10
#define STOP_DISTANCE 11000
//-------------------------------------
//-  Global Variables
//-------------------------------------
float speed = 0.0;
int distance = 0;
int stp_sensor = 1; //1 - STOP, 0 - GO
int ex_mode = 0; // 0 Normal mode, 1 Braking Mode, 2 Stop mode, 3 Emergency.
int light_lvl = 100;
int lamp_state = 0;
struct timespec time_msg = {0,400000000};
struct timespec time_mix;
struct timespec mix_period = {30,0};


int mix_state = -1;
int fd_serie = -1;



int sc = 0;
int num_sc = 6;
struct timespec sc_time = {5,0};

int get_target_speed(){
	switch (ex_mode) {
		case 1:
			return BRK_SPEED;
		case 3:
			return 0;
		default:
			return AVG_SPEED;
	}
}



//-------------------------------------
//-  Function: read_msg
//-------------------------------------
int read_msg(int fd, char *buffer, int max_size)
{
    char aux_buf[MSG_LEN+1];
    int count=0;
    char car_aux;

    //clear buffer and aux_buf
    memset(aux_buf, '\0', MSG_LEN+1);
    memset(buffer, '\0', MSG_LEN+1);

    while (1) {
        car_aux='\0';
        read(fd_serie, &car_aux, 1);
        // skip if it is not valid character
        if ( ( (car_aux < 'A') || (car_aux > 'Z') ) &&
             ( (car_aux < '0') || (car_aux > '9') ) &&
               (car_aux != ':')  && (car_aux != ' ') &&
               (car_aux != '\n') && (car_aux != '.') &&
               (car_aux != '%') ) {
            continue;
        }
        // store the character
        aux_buf[count] = car_aux;

        //increment count in a circular way
        count = count + 1;
        if (count == MSG_LEN) count = 0;

        // if character is new_line return answer
        if (car_aux == '\n') {
           int first_part_size = strlen(&(aux_buf[count]));
           memcpy(buffer,&(aux_buf[count]), first_part_size);
           memcpy(&(buffer[first_part_size]),aux_buf,count);
           return 0;
        }
    }
    strncpy(buffer,"MSG: ERR\n",MSG_LEN);
    return 0;
}

//-------------------------------------
//-  Function: task_speed
//-------------------------------------
int task_speed()
{
    char request[MSG_LEN+1];
    char answer[MSG_LEN+1];

    //--------------------------------
    //  request speed and display it
    //--------------------------------

    //clear request and answer
    memset(request, '\0', MSG_LEN+1);
    memset(answer, '\0', MSG_LEN+1);

    // request speed
    strcpy(request, "SPD: REQ\n");

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    //Use the simulator
    simulator(request, answer);
#endif

    // display speed
    if (1 == sscanf (answer, "SPD:%f\n", &speed)){
        displaySpeed(speed);
    }
    return 0;
}

//-------------------------------------
//-  Function: task_slope
//-------------------------------------
int task_slope()
{
    char request[MSG_LEN+1];
    char answer[MSG_LEN+1];

    //--------------------------------
    //  request slope and display it
    //--------------------------------

    //clear request and answer
    memset(request,'\0',MSG_LEN+1);
    memset(answer,'\0',MSG_LEN+1);

    // request slope
    strcpy(request, "SLP: REQ\n");

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    //Use the simulator
    simulator(request, answer);
#endif

    // display slope
    if (0 == strcmp(answer, "SLP:DOWN\n")) displaySlope(-1);
    if (0 == strcmp(answer, "SLP:FLAT\n")) displaySlope(0);
    if (0 == strcmp(answer, "SLP:  UP\n")) displaySlope(1);

    return 0;
}



int send_accelerator_signal(int activate){
	char request[MSG_LEN+1];
	char answer[MSG_LEN+1];

	//--------------------------------
	//  request action
	//--------------------------------

	//clear request and answer
	memset(request,'\0',MSG_LEN+1);
	memset(answer,'\0',MSG_LEN+1);

	if(activate){
		strcpy(request, "GAS: SET\n");
	}else{
		strcpy(request, "GAS: CLR\n");
	}

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    //Use the simulator
    simulator(request, answer);
#endif

    if (0 == strcmp(answer, "GAS:  OK\n")) return 0;
    if (0 == strcmp(answer, "MSG: ERR\n")) return -1;
    return -2;
}

//-------------------------------------
//-  Function: task_accelerator
//-------------------------------------
int task_accelerator(){
	int target = get_target_speed();
	if(speed < target){
		if(0==send_accelerator_signal(1)) displayGas(1);
	}else if(speed > target) {
		if(0==send_accelerator_signal(0)) displayGas(0);
	}
	return 0;
}

int send_brake_signal(int activate){
	char request[MSG_LEN+1];
	char answer[MSG_LEN+1];

	//--------------------------------
	//  request action
	//--------------------------------

	//clear request and answer
	memset(request,'\0',MSG_LEN+1);
	memset(answer,'\0',MSG_LEN+1);

	if(activate){
		strcpy(request, "BRK: SET\n");
	}else{
		strcpy(request, "BRK: CLR\n");
	}

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    //Use the simulator
    simulator(request, answer);
#endif

    if (0 == strcmp(answer, "BRK:  OK\n")) return 0;
    if (0 == strcmp(answer, "MSG: ERR\n")) return -1;
    return -2;
}
//-------------------------------------
//-  Function: task_brake
//-------------------------------------
int task_brake(){
	int target = get_target_speed();

	if(speed > target){
		if(0==send_brake_signal(1)) displayBrake(1);
	}else if(speed <= target) {
		if(0==send_brake_signal(0)) displayBrake(0);
	}
	return 0;
}

int send_mix_signal(int activate){
	char request[MSG_LEN+1];
	char answer[MSG_LEN+1];

	//--------------------------------
	//  request action
	//--------------------------------

	//clear request and answer
	memset(request,'\0',MSG_LEN+1);
	memset(answer,'\0',MSG_LEN+1);

	if(activate){
		strcpy(request, "MIX: SET\n");
	}else{
		strcpy(request, "MIX: CLR\n");
	}

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    //Use the simulator
    simulator(request, answer);
#endif

    if (0 == strcmp(answer, "MIX:  OK\n")) return 0;
    if (0 == strcmp(answer, "MSG: ERR\n")) return -1;
    return -2;
}


//-------------------------------------
//-  Function: task_mixer
//-------------------------------------
int task_mixer(){

	if(mix_state == -1){ // Mixer has not been initialized.
		clock_gettime(CLOCK_REALTIME, &time_mix); // Set mixing time to current time.
		mix_state=1; //start mixer
		if(0==send_mix_signal(mix_state)) displayMix(mix_state);
		return 0;
	}

	struct timespec current;
	clock_gettime(CLOCK_REALTIME, &current); // Get current time.

	struct timespec diff;
	diffTime(current, time_mix, &diff);

	if(compTime(diff, mix_period)>=0){ // 30 seconds has passed
		mix_state = (mix_state+1) % 2; // Toggle mixer
		if(0==send_mix_signal(mix_state)) displayMix(mix_state);
		clock_gettime(CLOCK_REALTIME, &time_mix); // Set mixing time to current time.
	}



	return 0;
}



int task_light(){

    char request[MSG_LEN+1];
    char answer[MSG_LEN+1];

    //--------------------------------
    //  request light level and display it
    //--------------------------------

    //clear request and answer
    memset(request, '\0', MSG_LEN+1);
    memset(answer, '\0', MSG_LEN+1);

    // request speed
    strcpy(request, "LIT: REQ\n");

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    //Use the simulator
    simulator(request, answer);
#endif

    // display speed
    if (1 == sscanf (answer, "LIT: %2d%%\n", &light_lvl)){
        displayLightSensor( light_lvl>=50 ? 0:1 );
    }
    return 0;
}


int send_lamp_signal(int activate){
	char request[MSG_LEN+1];
	char answer[MSG_LEN+1];

	//--------------------------------
	//  request action
	//--------------------------------

	//clear request and answer
	memset(request,'\0',MSG_LEN+1);
	memset(answer,'\0',MSG_LEN+1);

	if(activate){
		strcpy(request, "LAM: SET\n");
	}else{
		strcpy(request, "LAM: CLR\n");
	}

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    //Use the simulator
    simulator(request, answer);
#endif

    if (0 == strcmp(answer, "LAM:  OK\n")) return 0;
    if (0 == strcmp(answer, "MSG: ERR\n")) return -1;
    return -2;
}


int task_lamp(){
	if(light_lvl>=50 && lamp_state==1){
		if(0==send_lamp_signal(0)) lamp_state=0;
	}else if(light_lvl<50 && lamp_state==0){
		if(0==send_lamp_signal(1)) lamp_state=1;
	}
	displayLamps(lamp_state);
	return 0;
}
int task_lamp_on(){
	if(0==send_lamp_signal(1)) lamp_state=1;
	displayLamps(lamp_state);
	return 0;
}
int task_distance()
{
    char request[MSG_LEN+1];
    char answer[MSG_LEN+1];

    //--------------------------------
    //  request distance and display it
    //--------------------------------

    //clear request and answer
    memset(request, '\0', MSG_LEN+1);
    memset(answer, '\0', MSG_LEN+1);

    // request speed
    strcpy(request, "DS:  REQ\n");

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    //Use the simulator
    simulator(request, answer);
#endif

    // display speed
    if (1 == sscanf (answer, "DS:%5d\n", &distance)){
    	if(distance<=0 && ex_mode==1){
    		ex_mode=2;
	    	sc=-1;
    	}else if(distance<=STOP_DISTANCE && ex_mode==0){
    		ex_mode=1;
	    	sc=-1;
    	}
        displayDistance(distance);
    }
    return 0;
}

int task_load_sensor(){
    char request[MSG_LEN+1];
    char answer[MSG_LEN+1];

    //--------------------------------
    //  request slope and display it
    //--------------------------------

    //clear request and answer
    memset(request,'\0',MSG_LEN+1);
    memset(answer,'\0',MSG_LEN+1);

    // request slope
    strcpy(request, "STP: REQ\n");

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    //Use the simulator
    simulator(request, answer);
#endif

    // display slope
    if (0 == strcmp(answer, "STP:  GO\n")) stp_sensor = 0;
    if (0 == strcmp(answer, "STP:STOP\n")) stp_sensor = 1;

    if(stp_sensor==0 && ex_mode==2){
    	ex_mode=0;
    	sc= -1;
    }

    displayStop(stp_sensor);

    return 0;
}





#define MAX_MILLIS 0xFFFFFFFF

#define NS_PER_S  1000000000


void normal_scheduler(){

	num_sc = 2;
	sc_time.tv_sec=5;
	switch (sc) {
	case 0:
		task_slope();
		task_speed();
		task_accelerator();
		task_light();
		task_lamp();
		break;
	case 1:
		task_distance();
		task_mixer();
		task_brake();
		task_light();
		task_lamp();
		break;
	}
}

void braking_scheduler(){
	num_sc = 6;
	sc_time.tv_sec=5;
	switch (sc) {

	case 0:
		task_speed();
		task_accelerator();
		task_brake();
		task_distance();
		task_mixer();
		break;
	case 1:
		task_speed();
		task_accelerator();
		task_brake();
		task_slope();
		task_lamp_on();
		break;
	case 2:
		task_speed();
		task_accelerator();
		task_brake();
		task_distance();
		break;
	case 3:
		task_speed();
		task_accelerator();
		task_brake();
		task_slope();
		task_mixer();
		break;
	case 4:
		task_speed();
		task_accelerator();
		task_brake();
		task_distance();
		break;
	case 5:
		task_speed();
		task_accelerator();
		task_brake();
		task_slope();
		break;

	}
}

void stop_scheduler(){
	num_sc = 3;
	sc_time.tv_sec=5;
	switch (sc) {
	case 0:
		task_load_sensor();
		task_lamp_on();
		task_mixer();
		break;
	case 1:
		task_load_sensor();
		task_lamp_on();
		break;
	case 2:
		task_load_sensor();
		task_lamp_on();
		break;

	}
}


//-------------------------------------
//-  Function: controller
//-------------------------------------
void *controller(void *arg)
{


	struct timespec start;
	struct timespec end;


	clock_gettime(CLOCK_REALTIME, &start);


	// Endless loop
	while(1) {

		switch (ex_mode){

		case 0:
			normal_scheduler();
			break;
		case 1:
			braking_scheduler();
			break;
		case 2:
			stop_scheduler();
			break;


		}


		sc = (sc+1) % num_sc; // make sure sc is < than num_sc.

		clock_gettime(CLOCK_REALTIME, &end);

		struct timespec elapsed;
		diffTime(end, start, &elapsed);
		int diff = compTime(sc_time, elapsed);
		if(diff<0){
			printf("TIME WAS BIGGER; ERROR\n\n");
			exit(-1);
		}else if(diff>0){
			struct timespec wait_time;
			diffTime(sc_time, elapsed, &wait_time);
			clock_nanosleep(CLOCK_REALTIME, 0, &wait_time, NULL);
		}

		clock_gettime(CLOCK_REALTIME, &start);
	}
}

//-------------------------------------
//-  Function: Init
//-------------------------------------
rtems_task Init (rtems_task_argument ignored)
{
    pthread_t thread_ctrl;
    sigset_t alarm_sig;
    int i;

    /* Block all real time signals so they can be used for the timers.
       Note: this has to be done in main() before any threads are created
       so they all inherit the same mask. Doing it later is subject to
       race conditions */
    sigemptyset (&alarm_sig);
    for (i = SIGRTMIN; i <= SIGRTMAX; i++) {
        sigaddset (&alarm_sig, i);
    }
    sigprocmask (SIG_BLOCK, &alarm_sig, NULL);

    // init display
    displayInit(SIGRTMAX);

#if defined(ARDUINO)
    /* Open serial port */
    char serial_dev[]="/dev/com1";
    fd_serie = open (serial_dev, O_RDWR);
    if (fd_serie < 0) {
        printf("open: error opening serial %s\n", serial_dev);
        exit(-1);
    }

    struct termios portSettings;
    speed_t speed=B9600;

    tcgetattr(fd_serie, &portSettings);
    cfsetispeed(&portSettings, speed);
    cfsetospeed(&portSettings, speed);
    cfmakeraw(&portSettings);
    tcsetattr(fd_serie, TCSANOW, &portSettings);
#endif

    /* Create first thread */
    pthread_create(&thread_ctrl, NULL, controller, NULL);
    pthread_join (thread_ctrl, NULL);
    exit(0);
}

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_MAXIMUM_TASKS 1
#define CONFIGURE_MAXIMUM_SEMAPHORES 10
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 30
#define CONFIGURE_MAXIMUM_DIRVER 10
#define CONFIGURE_MAXIMUM_POSIX_THREADS 2
#define CONFIGURE_MAXIMUM_POSIX_TIMERS 1

#define CONFIGURE_INIT
#include <rtems/confdefs.h>
