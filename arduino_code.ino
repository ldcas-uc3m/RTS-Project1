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
// PINOUT
// --------------------------------------
#define GAS_LED 13
#define BRK_LED 12
#define MIX_LED 11
#define SPD_LED 10

#define DOWN_SLP_SWITCH 9
#define UP_SLP_SWITCH 8


// --------------------------------------
// Global Variables
// --------------------------------------
double speed = 55.5;
bool request_received = false;
bool requested_answered = false;
char request[MESSAGE_SIZE + 1];
char answer[MESSAGE_SIZE + 1];


// --------------------------------------
// TASKS
// --------------------------------------

int comm_server() {
   /*
   Read messages from Serial port
   */

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
      memset(request,'\0', MESSAGE_SIZE + 1);
      memset(answer,'\0', MESSAGE_SIZE + 1);
   }

   while (Serial.available()) {
      // read one character
      car_aux =Serial.read();
        
      //skip if it is not a valid character
      if  (((car_aux < 'A') || (car_aux > 'Z')) &&
           (car_aux != ':') && (car_aux != ' ') && (car_aux != '\n') ) {
         continue;
      }
      
      //Store the character
      request[count] = car_aux;
      
      // If the last character is an enter or
      // There are 9th characters set an enter and finish.
      if ((request[count] == '\n') || (count == 8)) {
         request[count] = '\n';
         count = 0;
         request_received = true;
         break;
      }

      // Increment the count
      count++;
   }
}


int speed() {
   /*
   Answer speed request from Serial & update speed
   */

   // If there is a request not answered, check if this is the one
   if (request_received && !requested_answered &&
      (0 == strcmp("SPD: REQ\n", request))) {
      
      // TODO: Compute speed

      // send the answer for speed request
      char num_str[5];
      dtostrf(speed, 4, 1, num_str);
      sprintf(answer, "SPD:%s\n", num_str);

      // set request as answered
      requested_answered = true;
   }
   // TODO: Errors?
   return 0;
}


void accelerator() {

   // If there is a request not answered, check if this is the one
   if (request_received && !requested_answered) {
      if (0 == strcmp("GAS: SET\n", request))) {  // activate accelerator
            
         // display LEDs
         digitalWrite(GAS_LED, HIGH);

         // answer request
         sprintf(answer, "GAS:  OK\n", num_str);
         
         // set request as answered
         requested_answered = true;

      } else if (0 == strcmp("GAS: CLR\n", request))) {  // deactivate accelerator
            
         // display LEDs
         digitalWrite(GAS_LED, LOW);

         // answer request
         sprintf(answer, "GAS:  OK\n", num_str);
         
         // set request as answered
         requested_answered = true;

      }
      // TODO: Errors?
   }
}


void brake() {
   // If there is a request not answered, check if this is the one
   if (request_received && !requested_answered) {
      if (0 == strcmp("BRK: SET\n", request))) {  // activate accelerator
            
         // display LEDs
         digitalWrite(BRK_LED, HIGH);

         // answer request
         sprintf(answer, "BRK:  OK\n", num_str);
         
         // set request as answered
         requested_answered = true;

      } else if (0 == strcmp("BRK: CLR\n", request))) {  // deactivate accelerator
            
         // display LEDs
         digitalWrite(BRK_LED, LOW);

         // answer request
         sprintf(answer, "BRK:  OK\n", num_str);
         
         // set request as answered
         requested_answered = true;


      }
      // TODO: Errors?
   }
}


void mixer() {
   // If there is a request not answered, check if this is the one
   if (request_received && !requested_answered) {
      if (0 == strcmp("MIX: SET\n", request))) {  // activate accelerator
            
         // display LEDs
         digitalWrite(MIX_LED, HIGH);

         // answer request
         sprintf(answer, "MIX:  OK\n", num_str);
         
         // set request as answered
         requested_answered = true;

      } else if (0 == strcmp("MIX: CLR\n", request))) {  // deactivate accelerator
            
         // display LEDs
         digitalWrite(MIX_LED, LOW);

         // answer request
         sprintf(answer, "MIX:  OK\n", num_str);
         
         // set request as answered
         requested_answered = true;

      }
      // TODO: Errors?
   }
}


void slope() {
   // If there is a request not answered, check if this is the one
   if (request_received && !requested_answered &&
      (0 == strcmp("SLP: REQ\n", request))) {
      
      // read slope
      bool isDown = digitalRead(DOWN_SLP_SWITCH);
      bool isUp = digitalRead(UP_SLP_SWITCH);

      // answer request
      if (!isDown && !isUp) {
         sprintf(answer, "SLP:  FLAT\n", num_str);
      } else if (isDown && isUp) { // error
         sprintf(answer, "MSG: ERR\n", num_str);
      } else if (isUp) {
         sprintf(answer, "SLP:  UP\n", num_str);
      } else if (isDown) {
         sprintf(answer, "SLP:  DOWN\n", num_str);
      }

      // set request as answered
      requested_answered = true;
   }
}


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
// ARDUINO FUNCTIONS
// --------------------------------------
void setup() {
   // Setup Serial Monitor
   Serial.begin(9600);

   // Setup pins
   pinMode(GAS_LED, OUTPUT);
   pinMode(BRK_LED, OUTPUT);
   pinMode(MIX_LED, OUTPUT);
   pinMode(SPD_LED, OUTPUT);
   
   pinMode(DOWN_SLP_SWITCH, INPUT);
   pinMode(UP_SLP_SWITCH, INPUT);
}


void loop() {
   comm_server();
   speed_req();
}
