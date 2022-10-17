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
// #define MESSAGE_SIZE 7
#define MESSAGE_SIZE 8
#define MAX_MILLIS 0xFFFFFFFF  // max number for the millis() clock function
#define MIN_SPEED 40
#define MAX_SPEED 70

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
double curr_speed = 55.5;
bool isAcc = false;
bool isBrk = false;
bool isMix = false;
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

   // int count = 0;
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
         // request[count + 1] = '\n';
         request[count] = '\n';
         count = 0;
         request_received = true;
         break;
      }

      // increment the count
      count++;
   }
}


int speed() {
   /*
   Answer speed request from Serial & update speed
   */

   // read slope
   bool isDown = digitalRead(DOWN_SLP_SWITCH);
   bool isUp = digitalRead(UP_SLP_SWITCH);

   // compute speed due to slope
    if (isDown && isUp) { // error
      return -1;
   } else if (isUp) {  // decelerate
      curr_speed -= 0.25 * 0.2;  // 0.25 m/s^2 * 0.2 s
   } else if (isDown) {  // accelerate
      curr_speed += 0.25 * 0.2;
   }

   // compute speed due to machine
   if (isBrk) { curr_speed -= 0.5 * 0.2; }
   if (isAcc) { curr_speed += 0.5 * 0.2; }

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
   return 0;
}


void accelerator() {

   // answer request
   if (request_received && !requested_answered) {
      if (0 == strcmp("GAS: SET\n", request)) {  // activate accelerator

         if (isAcc) {  // already accelerating
            Serial.print("MSG: ERR\n");
            requested_answered = true;
            return;
         }

         isAcc = true;
            
         // display LEDs
         digitalWrite(GAS_LED, HIGH);

         // answer request
         sprintf(answer, "GAS:  OK\n");
         
         // set request as answered
         requested_answered = true;

      } else if (0 == strcmp("GAS: CLR\n", request)) {  // deactivate accelerator
         
         if (!isAcc) {  // already deactivated
            Serial.print("MSG: ERR\n");
            requested_answered = true;
            return;
         }

         isAcc = false;

         // display LEDs
         digitalWrite(GAS_LED, LOW);

         // answer request
         sprintf(answer, "GAS:  OK\n");
         
         // set request as answered
         requested_answered = true;

      }
   }
}


void brake() {
   // answer request
   if (request_received && !requested_answered) {
      if (0 == strcmp("BRK: SET\n", request)) {  // activate accelerator

         if (isBrk) {  // already breaking
            Serial.print("MSG: ERR\n");
            requested_answered = true;
            return;
         }

         isBrk = true;
            
         // display LEDs
         digitalWrite(BRK_LED, HIGH);

         // answer request
         sprintf(answer, "BRK:  OK\n");
         
         // set request as answered
         requested_answered = true;

      } else if (0 == strcmp("BRK: CLR\n", request)) {  // deactivate accelerator
         
         if (!isBrk) {  // already deactivated
            Serial.print("MSG: ERR\n");
            requested_answered = true;
            return;
         }

         isBrk = false;
            
         // display LEDs
         digitalWrite(BRK_LED, LOW);

         // answer request
         sprintf(answer, "BRK:  OK\n");
         
         // set request as answered
         requested_answered = true;


      }
   }
}


void mixer() {
   // answer request
   if (request_received && !requested_answered) {
      if (0 == strcmp("MIX: SET\n", request)) {  // activate mixer
         
         if (isMix) {  // already mixing
            Serial.print("MSG: ERR\n");
            requested_answered = true;
            return;
         }

         isMix = true;

         // display LEDs
         digitalWrite(MIX_LED, HIGH);

         // answer request
         sprintf(answer, "MIX:  OK\n");
         
         // set request as answered
         requested_answered = true;

      } else if (0 == strcmp("MIX: CLR\n", request)) {  // deactivate mixer
         
         if (!isMix) {  // already deactivated
            Serial.print("MSG: ERR\n");
            requested_answered = true;
            return;
         }

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


void scheduler() {
   int sc = 0;
   int sc_time = 4;
   int n = 4;  // number of sec. cycles
   int elapsed;

   int start = millis();

   while(1) {

      switch (sc) {
      case 0:
         /* functions */
         break;
      }

      sc = (sc + 1) % n;

      int end = millis();

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
   
   pinMode(DOWN_SLP_SWITCH, INPUT);
   pinMode(UP_SLP_SWITCH, INPUT);
}


void loop() {
   serial_test();
   delay(1000);
}


// --------------------------------------
// TEST FUNCTIONS
// --------------------------------------

void serial_test() {
   /*
   Test messages through serial
   */

   comm_server();
   
   Serial.print(Serial.available());
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
  
  
   Serial.print(request_received);
   Serial.print(requested_answered);
   Serial.print("\n");   
}

void task_test() {
   /*
   Test compute time of each task 
   NOTE: For worst-case, perform in down slope
   */
   
   int testResults[6][10];

   // speed
   Serial.print("Speed:\n");

   for (int i = 0; i <= 10; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "SPD: REQ\n";

      // run test
      int start_time = micros();
      speed();
      int stop_time = micros();

      // save results
      testResults[0][i] = stop_time - start_time;
      
      // print results
      Serial.println(stop_time - start_time);
   }
   
   // accelerator - SET
   Serial.print("Accelerator:\n");

   for (int i = 0; i <= 5; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "GAS: SET\n";

      // run test
      int start_time = micros();
      accelerator();
      int stop_time = micros();

      // save results
      testResults[1][i] = stop_time - start_time;

      // print results
      Serial.println(stop_time - start_time);
   }
   
   // accelerator - CLR
   for (int i = 0; i <= 5; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "GAS: CLR\n";

      // run test
      int start_time = micros();
      accelerator();
      int stop_time = micros();

      // save results
      testResults[1][5 + i] = stop_time - start_time;

      // print results
      Serial.println(stop_time - start_time);
   }
   
   // brake - SET
   Serial.print("Brake:\n");

   for (int i = 0; i <= 5; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "BRK: SET\n";

      // run test
      int start_time = micros();
      brake();
      int stop_time = micros();

      // save results
      testResults[2][i] = stop_time - start_time;

      // print results
      Serial.println(stop_time - start_time);
   }
   
   // brake - CLR
   for (int i = 0; i <= 5; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "BRK: CLR\n";

      // run test
      int start_time = micros();
      brake();
      int stop_time = micros();

      // save results
      testResults[2][5 + i] = stop_time - start_time;

      // print results
      Serial.println(stop_time - start_time);
   }
   
   // mixer - SET
   Serial.print("Mixer:\n");

   for (int i = 0; i <= 5; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "MIX: SET\n";

      // run test
      int start_time = micros();
      brake();
      int stop_time = micros();

      // save results
      testResults[3][i] = stop_time - start_time;

      // print results
      Serial.println(stop_time - start_time);
   }
   
   // mixer - CLR
   for (int i = 0; i <= 5; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "MIX: CLR\n";

      // run test
      int start_time = micros();
      brake();
      int stop_time = micros();

      // save results
      testResults[3][5 + i] = stop_time - start_time;

      // print results
      Serial.println(stop_time - start_time);
   }
   
   // slope
   Serial.print("Slope:\n");

   for (int i = 0; i <= 10; i++) {
      // setup test
      request_received = true;
      requested_answered = false;
      char request[] = "SLP: REQ\n";

      // run test
      int start_time = micros();
      slope();
      int stop_time = micros();

      // save results
      testResults[4][i] = stop_time - start_time;

      // print results
      Serial.println(stop_time - start_time);
   }

   // TODO: comm_server

}