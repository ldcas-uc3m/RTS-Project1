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
#define MESSAGE_SIZE 9
// #define MESSAGE_SIZE 8
#define MAX_MILLIS 0xFFFFFFFF  // max number for the millis() clock function

// minumum and maximum speeds
#define MIN_SPEED 0
#define MAX_SPEED 70

// minumum and maximum distances
#define MIN_DISTANCE 10000
#define MAX_DISTANCE 90000

#define COMP_TEST_ITERATIONS 10  // note: make it an even number
#define NUM_TASKS 8

// --------------------------------------
// PINOUT
// --------------------------------------

/* Digital */
#define GAS_LED 13
#define BRK_LED 12
#define MIX_LED 11
#define SPD_LED 10
#define LAM_LED  7

#define DOWN_SLP_SWITCH 9
#define UP_SLP_SWITCH 8

#define STP_BUTTON 6

#define DISP_A 2
#define DISP_B 3
#define DISP_C 4
#define DISP_D 5

/* Analog */
#define PHOTORESISTOR 0
#define POTENTIOMETER 1

// minimum and maximum light levels for the photoresistor
#define MIN_LIT 54
#define MAX_LIT 974

// minumum and maximum values for the potentiometer
#define MIN_POT 511
#define MAX_POT 1023


// --------------------------------------
// Global Variables
// --------------------------------------
double curr_speed = 55.5;
long curr_distance = 0;
bool isAcc = false;
bool isBrk = false;
bool isMix = false;
bool request_received = false;
bool requested_answered = false;

char request[MESSAGE_SIZE + 1];
char answer[MESSAGE_SIZE + 1];

const byte BCD[10][4] = {  // translate numbers into (reversed) bit string for 7-segment display 
   {0,0,0,0},  // 0
   {1,0,0,0},  // 1
   {0,1,0,0},  // 2
   {1,1,0,0},  // 3
   {0,0,1,0},  // 4
   {1,0,1,0},  // 5
   {0,1,1,0},  // 6
   {1,1,1,0},  // 7
   {0,0,0,1},  // 8
   {1,0,0,1}   // 9
};

const int FULL_DISPLAY[4] = {DISP_A, DISP_B, DISP_C, DISP_D}; 

// --------------------------------------
// TASKS
// --------------------------------------

void comm_server() {
   /*
   Read messages from Serial port
   */

   int count = 0;
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
         requested_answered = true;
      }  
      // reset flags and buffers
      request_received = false;
      requested_answered = false;
      memset(request, '\0', MESSAGE_SIZE + 1);
      memset(answer, '\0', MESSAGE_SIZE + 1);
   }

   while (Serial.available()) {
      // read one character
      car_aux = Serial.read();
        
      // skip if it is not a valid character
      if  (((car_aux < 'A') || (car_aux > 'Z')) &&
           (car_aux != ':') && (car_aux != ' ') && (car_aux != '\n')) {
         continue;
      }
      
      // store the character
      request[count] = car_aux;
      
      // if the last character is an enter or
      // there are 9th characters set an enter and finish.
      if ((request[count] == '\n') || (count == MESSAGE_SIZE)) {
      // if ((request[count] == '\n') || (count == MESSAGE_SIZE - 1)) {
         request[count] = '\n';
         // request[count + 1] = '\n';
         count = 0;
         request_received = true;
         break;
      }

      // increment the count
      count++;
   }
}


void speed(int mode = 0) {
   /*
   Answer speed request from Serial & update speed
   Two modes: 
   - 0: normal mode
   - 1: stop/emergency mode (off)
   */

   if (mode == 0) {
      // read slope
      bool isDown = digitalRead(DOWN_SLP_SWITCH);
      bool isUp = digitalRead(UP_SLP_SWITCH);

      // compute speed due to slope
      if (isDown && isUp) { // error
         return;
      } else if (isUp) {  // decelerate
         curr_speed -= 0.25 * 0.2;  // 0.25 m/s^2 * 0.2 s
      } else if (isDown) {  // accelerate
         curr_speed += 0.25 * 0.2;
      }

      // compute speed due to machine
      if (isBrk) { curr_speed -= 0.5 * 0.2; }
      if (isAcc) { curr_speed += 0.5 * 0.2; }

      // prevent negative speed
      if (curr_speed <= 0) { curr_speed = 0; }

   } else if (mode == 1) {
      curr_speed = 0;
   }
   
   // LED
   analogWrite(SPD_LED, map(curr_speed, MIN_SPEED, MAX_SPEED, 0, 255));

   // answer request
   if (request_received && !requested_answered &&
      (0 == strcmp("SPD: REQ\n", request))) {
      
      // send the answer for speed request
      char num_str[5];
      dtostrf(curr_speed, 4, 1, num_str);
      sprintf(answer, "SPD:%s\n", num_str);

      // set request as answered
      requested_answered = true;
   }
}


void accelerator(int mode = 0) {
   /*
   Two modes: 
   - 0: normal mode
   - 1: emergency mode (off)
   */

   if (mode == 0) {
      // answer request
      if (request_received && !requested_answered) {
         if (0 == strcmp("GAS: SET\n", request)) {  // activate accelerator

            isAcc = true;
               
            // display LEDs
            digitalWrite(GAS_LED, HIGH);

            // answer request
            sprintf(answer, "GAS:  OK\n");
            
            // set request as answered
            requested_answered = true;

         } else if (0 == strcmp("GAS: CLR\n", request)) {  // deactivate accelerator
         
            isAcc = false;

            // display LEDs
            digitalWrite(GAS_LED, LOW);

            // answer request
            sprintf(answer, "GAS:  OK\n");
            
            // set request as answered
            requested_answered = true;

         }
      }
   } else if (mode == 1) {
      isAcc = false;
      digitalWrite(GAS_LED, HIGH);

   }
}


void brake(int mode = 0) {
   /*
   Two modes: 
   - 0: normal mode
   - 1: emergency mode (off)
   */

   if (mode == 0) {
      // answer request
      if (request_received && !requested_answered) {
         if (0 == strcmp("BRK: SET\n", request)) {  // activate brake

            isBrk = true;
               
            // display LEDs
            digitalWrite(BRK_LED, HIGH);

            // answer request
            sprintf(answer, "BRK:  OK\n");
            
            // set request as answered
            requested_answered = true;

         } else if (0 == strcmp("BRK: CLR\n", request)) {  // deactivate brake
            
            isBrk = false;
               
            // display LEDs
            digitalWrite(BRK_LED, LOW);

            // answer request
            sprintf(answer, "BRK:  OK\n");
            
            // set request as answered
            requested_answered = true;


         }
      }
   } else if (mode == 1) {
      isBrk = false;
      digitalWrite(BRK_LED, HIGH);
   }
}


void mixer() {
   // answer request
   if (request_received && !requested_answered) {
      if (0 == strcmp("MIX: SET\n", request)) {  // activate mixer
         
         isMix = true;

         // display LEDs
         digitalWrite(MIX_LED, HIGH);

         // answer request
         sprintf(answer, "MIX:  OK\n");
         
         // set request as answered
         requested_answered = true;

      } else if (0 == strcmp("MIX: CLR\n", request)) {  // deactivate mixer

         isMix = false;

         // display LEDs
         digitalWrite(MIX_LED, LOW);

         // answer request
         sprintf(answer, "MIX:  OK\n");
         
         // set request as answered
         requested_answered = true;

      }
   }
}


void slope() {
   // answer request
   if (request_received && !requested_answered &&
      (0 == strcmp("SLP: REQ\n", request))) {
      
      // read slope
      bool isDown = digitalRead(DOWN_SLP_SWITCH);
      bool isUp = digitalRead(UP_SLP_SWITCH);

      // answer request
      if (!isDown && !isUp) {
         sprintf(answer, "SLP:  FLAT\n");
      } else if (isDown && isUp) {  // error
         sprintf(answer, "MSG: ERR\n");
      } else if (isUp) {
         sprintf(answer, "SLP:  UP\n");
      } else if (isDown) {
         sprintf(answer, "SLP:DOWN\n");
      }

      // set request as answered
      requested_answered = true;
   }
}


void light() {
   // read light sensor
   if (request_received && !requested_answered && 
      (0 == strcmp("LIT: REQ\n", request))) {

      int level = map(analogRead(PHOTORESISTOR), MIN_LIT, MAX_LIT, 0, 99);
      // send the answer for light request
      sprintf(answer, "LIT:%02d%%\n", level);

      // light_test();

      requested_answered = true;
   }
}


void lamp(int mode = 0) {
   /*
   Two modes: 
   - 0: normal mode
   - 1: emergency mode (on)
   */
   if (mode == 0) {
      if (request_received && !requested_answered) {
         // light lamps
         if (0 == strcmp("LAM: SET\n", request)) {
            digitalWrite(LAM_LED, HIGH);
            sprintf(answer, "LAM:  OK\n");

            requested_answered = true;
         }
         // clear lamps
         else if (0 == strcmp("LAM: CLR\n", request)) {
            digitalWrite(LAM_LED, LOW);
            sprintf(answer, "LAM:  OK\n");

            requested_answered = true;
         }

      }
   } else if (mode == 1) {
      digitalWrite(LAM_LED, HIGH);
      
   }
}


void distance_select() {
   curr_distance = map(analogRead(POTENTIOMETER), MIN_POT, MAX_POT, MIN_DISTANCE, MAX_DISTANCE);
}


void distance_display(int mode = 0) {
   /* 
   This function has two modes:
   - mode 0: distance selection mode
   - mode 1: approaching mode
   */

   // thanks to https://www.tinkercad.com/things/3bH6sxdJBnt-bcd-to-seven-segment-decoder
   // and https://www.tinkercad.com/things/3bH6sxdJBnt-bcd-to-seven-segment-decoder

   if (mode == 1) {
      // compute new distance

      // check if it's near the stop (if going to stop in 2 cycles)
      if ((curr_distance - (curr_speed * 2 * 0.2) <= 0) || (curr_distance == 0)) {
         curr_distance = 0;
         curr_speed = 0;
      } else { // update the distance
         curr_distance -= curr_speed * 0.2;  // m/s * s
      }
   }

   // translate the current velocity into one digit
   int n = curr_distance / 10000;
   if (curr_distance % 10000 >= 5000) {n++;}

   // display number
   for(int i = 0; i < 4; i++){
      digitalWrite(FULL_DISPLAY[i], BCD[n][i]);
   }

   // answer request
   if (request_received && !requested_answered && 
      (0 == strcmp("DS:  REQ\n", request))) {
      
      sprintf(answer, "DS:%05d\n", curr_distance);
      
      requested_answered = true;
   }
}


int distance_validate() {
   // returns 1 if the button was pressed, else 0

   return (digitalRead(STP_BUTTON));
}


void movement_go() {
   // answers the request for STP in case it's moving
   if (request_received && !requested_answered && 
      (0 == strcmp("STP: REQ\n", request))) {

      sprintf(answer, "STP:  GO\n");

      requested_answered = true;
   }
}


void movement_stop() {
   // answers the request for STP in case it's stopped
   if (request_received && !requested_answered && 
      (0 == strcmp("STP: REQ\n", request))) {

      sprintf(answer, "STP:STOP\n");

      requested_answered = true;
   }
}


int emergency() {
   // checks for an emergency. returns 1 if emergency, else 0

   // answers the request
   if (request_received && !requested_answered && 
      (0 == strcmp("ERR: SET\n", request))) {

      sprintf(answer, "ERR:  OK\n");

      requested_answered = true;
      return 1;
   }

   return 0;
}



// --------------------------------------
// EXECUTION MODES SCHEDULERS
// --------------------------------------


int distance_scheduler() {
   // returns 0 if it's time to change to next mode, 1 in case of emergency

   int sc = 0;  // current sec. cycle
   int sc_time = 200;  // ms
   int n = 1;  // number of sec. cycles
   int elapsed;

   unsigned long start = millis();

   while(1) {

      switch (sc) {
      case 0:
         comm_server();
         accelerator();
         brake();
         mixer();
         speed();
         slope();
         light();
         lamp();
         distance_select();
         distance_display();
         if (distance_validate() == 1){
            unsigned long end = millis();

            // sleep for the rest of the cycle
            if (sc_time < elapsed) {  // sth went wrong
               sprintf(answer, "MSG: ERR\n");
               break;
            } else if ((sc_time - elapsed) < 10) {  // more accurate to use miliseconds
                  delayMicroseconds((sc_time - elapsed) * 1000);
            } else {
               delay(sc_time - elapsed);
            }

            // change mode
            return 0;
         }
         movement_stop();

         if (emergency() == 1) { return 1; }

         break;
      }

      sc = (sc + 1) % n;

      unsigned long end = millis();

      if (start > end) {  // overflow
         elapsed = MAX_MILLIS - start + end;
      } else {
         elapsed = end - start;
      }

      // sleep for the rest of the cycle
      if (sc_time < elapsed) {  // sth went wrong
         sprintf(answer, "MSG: ERR\n");
         break;
      } else if ((sc_time - elapsed) < 10) {  // more accurate to use miliseconds
            delayMicroseconds((sc_time - elapsed) * 1000);
      } else {
         delay(sc_time - elapsed);
      }

      start += sc_time;  // reset timer
   }
}


int approaching_scheduler() {
   // returns if it's time to change to next mode

   int sc = 0;  // current sec. cycle
   int sc_time = 200;  // ms
   int n = 1;  // number of sec. cycles
   int elapsed;

   unsigned long start = millis();

   while(1) {

      switch (sc) {
      case 0:
         comm_server();
         accelerator();
         brake();
         mixer();
         speed();
         slope();
         light();
         lamp();
         distance_display(1);
         movement_go();

         // check if stopped
         if ((curr_distance == 0) && (curr_speed <= 10)) {
            // change mode
            return 0;
         }

         if (emergency() == 1) { return 1; }


         break;
      }

      sc = (sc + 1) % n;

      unsigned long end = millis();

      if (start > end) {  // overflow
         elapsed = MAX_MILLIS - start + end;
      } else {
         elapsed = end - start;
      }

      // sleep for the rest of the cycle
      if (sc_time < elapsed) {  // sth went wrong
         sprintf(answer, "MSG: ERR\n");
         break;
      } else if ((sc_time - elapsed) < 10) {  // more accurate to use miliseconds
            delayMicroseconds((sc_time - elapsed) * 1000);
      } else {
         delay(sc_time - elapsed);
      }

      start += sc_time;  // reset timer
   }
}


int stop_scheduler() {
   // returns if it's time to change to next mode

   int sc = 0;  // current sec. cycle
   int sc_time = 200;  // ms
   int n = 1;  // number of sec. cycles
   int elapsed;

   unsigned long start = millis();

   while(1) {

      switch (sc) {
      case 0:
         comm_server();
         accelerator();
         brake();
         mixer();
         speed(1);
         slope();
         light();
         lamp();
         if (distance_validate() == 1){
            unsigned long end = millis();

            // sleep for the rest of the cycle
            if (sc_time < elapsed) {  // sth went wrong
               sprintf(answer, "MSG: ERR\n");
               break;
            } else if ((sc_time - elapsed) < 10) {  // more accurate to use miliseconds
                  delayMicroseconds((sc_time - elapsed) * 1000);
            } else {
               delay(sc_time - elapsed);
            }

            // change mode
            return 0;
         }

         if (emergency() == 1) { return 1; }

         movement_stop();

         break;
      }

      sc = (sc + 1) % n;

      unsigned long end = millis();

      if (start > end) {  // overflow
         elapsed = MAX_MILLIS - start + end;
      } else {
         elapsed = end - start;
      }

      // sleep for the rest of the cycle
      if (sc_time < elapsed) {  // sth went wrong
         sprintf(answer, "MSG: ERR\n");
         break;
      } else if ((sc_time - elapsed) < 10) {  // more accurate to use miliseconds
            delayMicroseconds((sc_time - elapsed) * 1000);
      } else {
         delay(sc_time - elapsed);
      }

      start += sc_time;  // reset timer
   }
}


void emergency_scheduler() {
   // returns 0 if it's time to change to next mode, 1 in case of emergency

   int sc = 0;  // current sec. cycle
   int sc_time = 200;  // ms
   int n = 1;  // number of sec. cycles
   int elapsed;
   

   unsigned long start = millis();

   while(1) {

      switch (sc) {
      case 0:
         comm_server();
         accelerator(1);
         brake(1);
         mixer();
         speed(1);
         slope();
         light();
         lamp(1);

         break;
      }

      sc = (sc + 1) % n;

      unsigned long end = millis();

      if (start > end) {  // overflow
         elapsed = MAX_MILLIS - start + end;
      } else {
         elapsed = end - start;
      }

      // sleep for the rest of the cycle
      if (sc_time < elapsed) {  // sth went wrong
         sprintf(answer, "MSG: ERR\n");
         break;
      } else if ((sc_time - elapsed) < 10) {  // more accurate to use miliseconds
            delayMicroseconds((sc_time - elapsed) * 1000);
      } else {
         delay(sc_time - elapsed);
      }

      start += sc_time;  // reset timer
   }
}


// --------------------------------------
// TEST FUNCTIONS
// --------------------------------------

void serial_test() {
   /*
   Test messages through serial
   */

   comm_server();
   
   Serial.print(Serial.available());  // message length
   Serial.print(" ");
   Serial.print(request_received);
   Serial.print(requested_answered);
   Serial.print(" ");
   Serial.print(request);
   Serial.print("  ");
  
   speed();
   accelerator();
   slope();
   brake();
   mixer();
   light();
   lamp();
   distance_select();
   distance_display();
   distance_validate();
   movement_go();
  
  
   Serial.print(request_received);
   Serial.print(requested_answered);
   Serial.print("\n");

   delay(1000);

}


void task_test() {
   /*
   Test compute time of each task 
   NOTE: For worst-case, perform in down slope
   */
   
   /*-- speed --*/
   Serial.print("Speed:\n");

   for (int i = 0; i < COMP_TEST_ITERATIONS; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "SPD: REQ\n";

      // run test
      unsigned long  start_time = micros();
      speed(0);  // worst-case is mode 0
      unsigned long  stop_time = micros();

      // print results
      Serial.println(stop_time - start_time);
   }
   
   /*-- accelerator - SET --*/ 
   Serial.print("Accelerator:\n");

   for (int i = 0; i < COMP_TEST_ITERATIONS / 2; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "GAS: SET\n";

      // run test
      unsigned long start_time = micros();
      accelerator(0);  // worst-case is mode 0
      unsigned long stop_time = micros();

      // print results
      Serial.println(stop_time - start_time);
   }
   
   /*-- accelerator - CLR --*/
   for (int i = 0; i < COMP_TEST_ITERATIONS / 2; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "GAS: CLR\n";

      // run test
      unsigned long start_time = micros();
      accelerator(0);  // worst-case is mode 0
      unsigned long stop_time = micros();

      // print results
      Serial.println(stop_time - start_time);
   }
   
   /*-- brake - SET --*/
   Serial.print("Brake:\n");

   for (int i = 0; i < 5; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "BRK: SET\n";

      // run test
      unsigned long start_time = micros();
      brake(0);  // worst-case is mode 0
      unsigned long stop_time = micros();

      // print results
      Serial.println(stop_time - start_time);
   }
   
   /*-- brake - CLR --*/ 
   for (int i = 0; i < COMP_TEST_ITERATIONS / 2; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "BRK: CLR\n";

      // run test
      unsigned long start_time = micros();
      brake();
      unsigned long stop_time = micros();

      // print results
      Serial.println(stop_time - start_time);
   }
   
   /*-- mixer - SET --*/ 
   Serial.print("Mixer:\n");

   for (int i = 0; i < COMP_TEST_ITERATIONS / 2; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "MIX: SET\n";

      // run test
      unsigned long start_time = micros();
      mixer();
      unsigned long stop_time = micros();

      // print results
      Serial.println(stop_time - start_time);
   }
   
   /*-- mixer - CLR --*/ 
   for (int i = 0; i < COMP_TEST_ITERATIONS / 2; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "MIX: CLR\n";

      // run test
      unsigned long start_time = micros();
      mixer();
      unsigned long stop_time = micros();

      // print results
      Serial.println(stop_time - start_time);
   }
   
   /*-- slope --*/ 
   Serial.print("Slope:\n");

   for (int i = 0; i < COMP_TEST_ITERATIONS; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "SLP: REQ\n";

      // run test
      unsigned long start_time = micros();
      slope();
      unsigned long stop_time = micros();

      // print results
      Serial.println(stop_time - start_time);
   }

   /*-- comm_server --*/
   Serial.print("Communication server:\n");

   for(int i = 0; i < COMP_TEST_ITERATIONS; i++) {
      /*---
      SETUP
      ---*/
      request_received = true;
      requested_answered = true;  // for worst-case
      char answer[] = "--TEST--\n";
      // char answer[] = "--TEST--";

      unsigned long start_time = micros();

      /*---
      KERNEL
      ---*/

      static int count = 0;
      // int count = 0;
      char car_aux;

      if (request_received) {
         if (requested_answered) {
            Serial.print(answer);
         } else {
            Serial.print("MSG: ERR\n");
         }  
         request_received = false;
         requested_answered = false;
         memset(request, '\0', MESSAGE_SIZE + 1);
         memset(answer, '\0', MESSAGE_SIZE + 1);
      }

      while (Serial.available()) {
         car_aux = Serial.read();
         
         if  (((car_aux < 'A') || (car_aux > 'Z')) &&
            (car_aux != ':') && (car_aux != ' ') && (car_aux != '\n')) {
            continue;
         }
         
         request[count] = car_aux;
         
         if ((request[count] == '\n') || (count == MESSAGE_SIZE)) {
            request[count] = '\n';
            // request[count + 1] = '\n';
            count = 0;
            request_received = true;
            break;
         }

         count++;
      }

      unsigned long stop_time = micros();

      // print results
      Serial.println(stop_time - start_time);

      /*-- light --*/
      Serial.print("Light:\n");

      for (int i = 0; i < COMP_TEST_ITERATIONS; i++) {
         // setup test
         request_received = true;
         requested_answered = false;
         char request[] = "LIT: REQ\n";

         // run test
         unsigned long start_time = micros();
         light();
         unsigned long stop_time = micros();

         // print results
         Serial.println(stop_time - start_time);
      }

      /*-- lamp - SET --*/ 
      Serial.print("Lamp:\n");

      for (int i = 0; i < COMP_TEST_ITERATIONS / 2; i++) {
         // setup test
         request_received = true;
         requested_answered = false;
         char request[] = "LAM: SET\n";

         // run test
         unsigned long start_time = micros();
         lamp(0);  // worst-case is mode 0
         unsigned long stop_time = micros();

         // print results
         Serial.println(stop_time - start_time);
      }
      
      /*-- lamp - CLR --*/ 
      for (int i = 0; i < COMP_TEST_ITERATIONS / 2; i++) {
         // setup test
         request_received = true;
         requested_answered = false;
         char request[] = "LAM: CLR\n";

         // run test
         unsigned long start_time = micros();
         lamp(0);  // worst-case is mode 0
         unsigned long stop_time = micros();

         // print results
         Serial.println(stop_time - start_time);
      }

      /*-- distance_select --*/
      Serial.print("distance_select:\n");

      for (int i = 0; i < COMP_TEST_ITERATIONS; i++) {

         // run test
         unsigned long start_time = micros();
         distance_select();
         unsigned long stop_time = micros();

         // print results
         Serial.println(stop_time - start_time);
      }
      
      /*-- distance_display --*/
      Serial.print("distance_display:\n");

      for (int i = 0; i < COMP_TEST_ITERATIONS; i++) {
         // setup test
         request_received = true;
         requested_answered = false;
         char request[] = "DS:  REQ\n";

         // run test
         unsigned long start_time = micros();
         distance_display(1);  // worst-case is mode 1
         unsigned long stop_time = micros();

         // print results
         Serial.println(stop_time - start_time);
      }
      
      /*-- distance_validate --*/
      Serial.print("distance_validate:\n");

      for (int i = 0; i < COMP_TEST_ITERATIONS; i++) {

         // run test
         unsigned long start_time = micros();
         distance_validate();
         unsigned long stop_time = micros();

         // print results
         Serial.println(stop_time - start_time);
      }

      /*-- movement_go --*/
      Serial.print("movement_go:\n");

      for (int i = 0; i < COMP_TEST_ITERATIONS; i++) {
         // setup test
         request_received = true;
         requested_answered = false;
         char request[] = "STP: REQ\n";

         // run test
         unsigned long start_time = micros();
         movement_go();  // worst-case is mode 1
         unsigned long stop_time = micros();

         // print results
         Serial.println(stop_time - start_time);
      }
      
      /*-- movement_stop --*/
      Serial.print("movement_stop:\n");

      for (int i = 0; i < COMP_TEST_ITERATIONS; i++) {
         // setup test
         request_received = true;
         requested_answered = false;
         char request[] = "STP: REQ\n";

         // run test
         unsigned long start_time = micros();
         movement_stop();  // worst-case is mode 1
         unsigned long stop_time = micros();

         // print results
         Serial.println(stop_time - start_time);
      }
      
      /*-- emergency --*/
      Serial.print("emergency:\n");

      for (int i = 0; i < COMP_TEST_ITERATIONS; i++) {
         // setup test
         request_received = true;
         requested_answered = false;
         char request[] = "ERR: SET\n";

         // run test
         unsigned long start_time = micros();
         emergency();
         unsigned long stop_time = micros();

         // print results
         Serial.println(stop_time - start_time);
      }
   }

   delay(10000);
}


void light_test() {
   // tests the current light level for fine-tuning MIN_LIT & MAX_LIT
   Serial.print("\nLight level: ");
   Serial.print(analogRead(PHOTORESISTOR));
   Serial.print("\n");
   
   delay(100);
}


void potentiometer_test() {
   // tests the current potentiometer level for fine-tuning MIN_POT & MAX_POT
   Serial.print("\nPotentiometer level: ");
   Serial.print(analogRead(POTENTIOMETER));
   Serial.print("\n");

   delay(100);
}


// --------------------------------------
// ARDUINO FUNCTIONS
// --------------------------------------

void setup() {
   // setup Serial Monitor
   Serial.begin(9600);

   // setup pins
   pinMode(GAS_LED, OUTPUT);
   pinMode(BRK_LED, OUTPUT);
   pinMode(MIX_LED, OUTPUT);
   pinMode(SPD_LED, OUTPUT);
   pinMode(LAM_LED, OUTPUT);
   
   pinMode(DOWN_SLP_SWITCH, INPUT);
   pinMode(UP_SLP_SWITCH, INPUT);
   pinMode(PHOTORESISTOR, INPUT);
   pinMode(POTENTIOMETER, INPUT);
   pinMode(STP_BUTTON, INPUT);
}


void loop() {
   /* 
   the schedulers are executed sequentially. when one finishes, it returns 0 to change to 
   the next mode  if it returns 1, it enters into emergency mode.
   */

   // Serial.print("Distance selection mode\n");
   if (distance_scheduler() == 1) { 
      // Serial.print("Emergency mode\n");
      emergency_scheduler();
   }
   // Serial.print("Approach mode\n");
   if (approaching_scheduler() == 1) { 
      // Serial.print("Emergency mode\n");
      emergency_scheduler();
   }
   // Serial.print("Stop mode\n");
   if (stop_scheduler() == 1) { 
      // Serial.print("Emergency mode\n");
      emergency_scheduler();
   }

   // light_test();
   // potentiometer_test();
   
}