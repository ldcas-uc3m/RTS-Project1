// --------------------------------------
// Include files
// --------------------------------------
#include <string.h>
#include <stdio.h>
#include <Wire.h>

// --------------------------------------
// Global Constants
// --------------------------------------
#define SLAVE_ADDR 0x8
#define MESSAGE_SIZE 8
#define MAX_MILIS 0xFFFFFFFF  // max number for the milis() clock function

// --------------------------------------
// Global Variables
// --------------------------------------
double speed = 55.5;
bool request_received = false;
bool requested_answered = false;
char request[MESSAGE_SIZE+1];
char answer[MESSAGE_SIZE+1];

// --------------------------------------
// Function: comm_server
// --------------------------------------
int comm_server()
{
   static int count = 0;
   char car_aux;

   // If there were a received msg, send the processed answer or ERROR if none.
   // then reset for the next request.
   // NOTE: this requires that between two calls of com_server all possible 
   //       answers have been processed.
   if (request_received) {
      // if there is an answer send it, else error
      if (requested_answered) {
          Serial.print(answer);
      } else {
          Serial.print("MSG: ERR\n");
      }  
      // reset flags and buffers
      request_received = false;
      requested_answered = false;
      memset(request,'\0', MESSAGE_SIZE+1);
      memset(answer,'\0', MESSAGE_SIZE+1);
   }

   while (Serial.available()) {
      // read one character
      car_aux =Serial.read();
        
      //skip if it is not a valid character
      if  ( ( (car_aux < 'A') || (car_aux > 'Z') ) &&
           (car_aux != ':') && (car_aux != ' ') && (car_aux != '\n') ) {
         continue;
      }
      
      //Store the character
      request[count] = car_aux;
      
      // If the last character is an enter or
      // There are 9th characters set an enter and finish.
      if ( (request[count] == '\n') || (count == 8) ) {
         request[count] = '\n';
         count = 0;
         request_received = true;
         break;
      }

      // Increment the count
      count++;
   }
}

// --------------------------------------
// Function: speed_req
// --------------------------------------
int speed_req()
{
   // If there is a request not answered, check if this is the one
   if ( (request_received) && (!requested_answered) &&
        (0 == strcmp("SPD: REQ\n",request)) ) {
          
      // send the answer for speed request
      char num_str[5];
      dtostrf(speed,4,1,num_str);
      sprintf(answer,"SPD:%s\n",num_str);

      // set request as answered
      requested_answered = true;
   }
   return 0;
}



// ---------
// Function: Scheduler
// ------
void scheduler() {
   int sc = 0;
   int sc_time = 4;
   int n = 4;  // number of sec. cycles

   start = milis();

   while(1) {

      switch (sc) {
      case 0:
         /* functions */
         break;
      }

      sc = (sc + 1) % n;

      end = milis();

      if (start > end) {  // overflow
         elapsed = MAX_MILIS - start + end;
      } else {
         elapsed = end - start;
      }

      // sleep for the rest of the cycle
      if (sc_time < elapsed) {  // sth went wrong
         exit();
      } else if (sc_time - elapsed) < 10 {  // more accurate to use miliseconds
            delayMicroseconds((sc_time - elapsed) * 1000);
      } else {
         delay(sc_time - elapsed);
      }

      start += sc_time;  // reset timer
   }
}


// --------------------------------------
// Function: setup
// --------------------------------------
void setup()
{
   // Setup Serial Monitor
   Serial.begin(9600);
}


// --------------------------------------
// Function: loop
// --------------------------------------
void loop()
{
   comm_server();
   speed_req();
}
