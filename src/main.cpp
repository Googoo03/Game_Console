#include "timerISR.h"
#include "helper.h"
#include "periph.h"
#include "serialATmega-1.h"



#define NUM_TASKS 8 //TODO: Change to the number of tasks being used


//Task struct for concurrent synchSMs implmentations
typedef struct _task{
	signed 	 char state; 		//Task's current state
	unsigned long period; 		//Task period
	unsigned long elapsedTime; 	//Time elapsed since last task tick
	int (*TickFct)(int); 		//Task tick function
} task;


//TODO: Define Periods for each task
// e.g. const unsined long TASK1_PERIOD = <PERIOD>
const unsigned long GCD_PERIOD = 1;//TODO:Set the GCD Period

task tasks[NUM_TASKS]; // declared task array with 5 tasks

enum L_BUTTON_SM_STATES {LBUTTON_INIT,LBOTTON_OFF,LBUTTON_ON};
enum R_BUTTON_SM_STATES {RBUTTON_INIT,RBOTTON_OFF,RBUTTON_ON};
enum LED_SM_STATES {LED_INIT, LED_OFF,LED_ON};
enum JOYSTICK_SM_STATES {JOYSTICK_INIT,JOYSTICK_IDLE};
enum BUZZER_SM_STATES {BUZZER_INIT,BUZZER_OFF,BUZZER_ON};
enum STEPMOTOR_SM_STATES {STEPMOTOR_INIT, STEPMOTOR_OFF, STEPMOTOR_FORWARD, STEPMOTOR_BACKWARD};
enum SERVO_SM_STATES {SERVO_INIT,SERVO_ADJUST};

int lerp(int a, int b, double t){
  return (int)((double)a * t + (double)b*(1-t));
}

void TimerISR() {
	for ( unsigned int i = 0; i < NUM_TASKS; i++ ) {                   // Iterate through each task in the task array
		if ( tasks[i].elapsedTime >= tasks[i].period ) {           // Check if the task is ready to tick
			tasks[i].state = tasks[i].TickFct(tasks[i].state); // Tick and set the next state for this task
			tasks[i].elapsedTime = 0;                          // Reset the elapsed time for the next tick
		}
		tasks[i].elapsedTime += GCD_PERIOD;                        // Increment the elapsed time by GCD_PERIOD
	}
}



/*void driveMotor(bool reverse,int steps){
  //& first to reset pins 2-5 but not 0-1 then | with phase shifted left 2 to assign the right value to pins 2-5
  if(!reverse){
    for(int i = 0; i < 8*steps;){
      PORTB = (PORTB & 0x03) | stages[i%8] << 2;
      //serial_println(PORTB);
      i++;
      //if(i>7)i=0;
    }
  }else{
    for(int i = 8*steps; i >= 0;){
    PORTB = (PORTB & 0x03) | stages[i%8] << 2;
    //serial_println(PORTB);
    i--;
    //if(i < 0)i=7;
    }
  }
  //PORTB = 0X00;
}*/


int stages[8] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001};//Stepper motor phases

//////////////////GLOBALS////////////////////////
int joystickThreshold = 400;
bool left_LED = false;
bool right_LED = false;
bool buzzer = false;
bool forward = false;
bool backward  =false;

int motor_period = 30;
int servo_rotation = 3000;
/////////////////////////////////////////////////

int checkMove(){
  //int joystickThreshold = 256;
  int yInput = ADC_read(0);
  int xInput = ADC_read(1);

  int yDist = ADC_read(0)-512; //CHANGE ADC READ
      yDist = yDist < 0 ? -yDist : yDist;
  int xDist = ADC_read(1)-512; //CHANGE ADC READ
      xDist = xDist < 0 ? -xDist : xDist;
  
  if(yInput > 1023-joystickThreshold) return 1; //up
  if(yInput < joystickThreshold) return 0; //down
  //if(xInput > 1023-joystickThreshold && yDist < joystickThreshold) return 2; //right
  //if(xInput < joystickThreshold && yDist < joystickThreshold) return 3; //left MIGHT BE BACKWARDS
  return -1;
}

//TODO: Create your tick functions for each task

int Tick_LButton(int state){


  switch (state) //State transitions
  {
  case LBUTTON_INIT:
    state = LBOTTON_OFF;
    break;
  case LBOTTON_OFF:
    //serial_println(ADC_read(3));
    if(ADC_read(3) > 512) state = LBUTTON_ON;
    break;
  case LBUTTON_ON:
    if(ADC_read(3) < 512) state = LBOTTON_OFF;
    break;
  default:
    break;
  }

  switch (state) //State actions
  {
  case LBUTTON_INIT:
    break;
  case LBOTTON_OFF:
    left_LED = false;
    break;
  case LBUTTON_ON:
    if(!right_LED) left_LED = true;
    break;
  default:
    break;
  }

  return state;
}

int Tick_RButton(int state){

  switch (state) //State transitions
  {
  case RBUTTON_INIT:
    state = RBOTTON_OFF;
    break;
  case RBOTTON_OFF:
    //serial_println(ADC_read(3));
    if(ADC_read(4) > 512) state = RBUTTON_ON;
    break;
  case RBUTTON_ON:
    if(ADC_read(4) < 512) state = RBOTTON_OFF;
    break;
  default:
    break;
  }

  switch (state) //State actions
  {
  case RBUTTON_INIT:
    break;
  case RBOTTON_OFF:
    right_LED = false;
    break;
  case RBUTTON_ON:
    if(!left_LED) right_LED = true;
    break;
  default:
    break;
  }

  return state;
}

int Tick_LLED(int state){
  static int count;
  //static unsigned char output;
  switch (state) //state transitions
  {
  case LED_INIT:
    state = LED_OFF;
    break;
  case LED_OFF:
    if(left_LED){
      state = LED_ON;
      serial_println("changed to on!");
    }
    count = 0;
    break;
  case LED_ON:
    if(!left_LED) state = LED_OFF;
    break;
  
  default:
    break;
  }

  switch (state) //state actions
  {
  case LED_INIT:
    break;
  case LED_OFF:
    PORTB &= (0xFE); //shut off led 3
    PORTD &= ~(0xA0); //shut off led 1 and 2
    break;
  case LED_ON:
    //serial_println(count);
    count++;
    count %= 4;


    if(count == 1) PORTB |= (0x01);

    if(count == 2) PORTD |= (0x80); 
    if(count == 3) PORTD |= (0x20); 
    if(count == 0){
      PORTB &= (0xFE); //shut off led 3
      PORTD &= ~(0xA0); //shut off led 1 and 2
    }

    /*if(count == 2){ //there must be a better way to do this
      PORTB |= (0x01);
      PORTD &= ~(0xA0);
      serial_println(PORTD);
    }else if(count == 1){
      PORTD = (PORTD & 0x5F) | 0x80;
      PORTB &= ~(0x01);
      //serial_println(PORTB);
      serial_println(PORTD);
    }else if(count == 0){
      PORTD = (PORTD & 0x5F) | 0x20;
      PORTB &= ~(0x01);
      //serial_println(PORTB);
      serial_println(PORTD);
    }*/
    //PORTB |= ~(0x80);
    break;
  
  default:
    break;
  }

  return state;
}

int Tick_RLED(int state){
  static int count;

  switch (state) //state transitions
  {
  case LED_INIT:
    state = LED_OFF;
    break;
  case LED_OFF:
    if(right_LED){
      state = LED_ON;
    }
    count = 0;
    break;
  case LED_ON:
    if(!right_LED) state = LED_OFF;
    break;
  
  default:
    break;
  }

  switch (state) //state actions
  {
  case LED_INIT:
    break;
  case LED_OFF:
    PORTD &= ~(0x1C); //shut off led 3
    break;
  case LED_ON:
    //serial_println(count);
    count++;
    count %= 4;
    //PORTB |= ~(0x80);
    if(count == 1) PORTD |= (0x10); 
    if(count == 2) PORTD |= (0x08); 
    if(count == 3) PORTD |= (0x04); 
    if(count == 0) PORTD &= ~(0x1C); //reset
    break;
  
  default:
    break;
  }

  return state;
}

int Tick_Joystick(int state){

  static bool centered_y;
  static int move;
  static double t;

  switch(state){ //state transitions
    case JOYSTICK_INIT:
      state = JOYSTICK_IDLE;
      centered_y = false;
      break;
    case JOYSTICK_IDLE:
      break;
    default:
      break;
  }

  switch(state){ //state actions
    case JOYSTICK_INIT:
      break;
    case JOYSTICK_IDLE:

      move = checkMove();
      //serial_println(ADC_read(2));
      int yDist = ADC_read(0)-512; //CHANGE ADC READ
      yDist = yDist < 0 ? -yDist : yDist;

      if(yDist<256){
        centered_y = true;
        backward = false;
        forward = false;
      }

      if(move != -1 && centered_y) centered_y = false;

      if(move == 0){
        backward = true;
      }else if(move == 1){
        forward = true;
      }

      servo_rotation = lerp(999,4999,(double)ADC_read(1)/1024.0);

      if(forward || backward){
        t=((double)yDist-512.0)/(512.0);
        //t=0.5;
        //serial_println(t);
        motor_period = lerp(2,30,(double)yDist/512.0);
        serial_println(motor_period);
        motor_period = motor_period < 2 ? 2 : motor_period;
        motor_period = motor_period > 30 ? 30 : motor_period;
        tasks[6].period = motor_period;
      }else{
        motor_period = 30;
      }

      //serial_println(backward);
      //serial_println(forward);
      buzzer = (ADC_read(2) < 50);
      break;
    default:
      break;
  }

  return state;
}

int Tick_Buzzer(int state){
  static int count;

  switch(state){ //state transitions
    case BUZZER_INIT:
      state = BUZZER_OFF;
      count = 0;
      break;
    case BUZZER_OFF:

      if(backward){
        count ++;
      }else{count=0;}

      if(buzzer || (backward && count >= 100)){
        if(backward){
          TCCR0B = (TCCR0B & 0xF8) | 0x04;//set prescaler to 256
        }else{
          TCCR0B = (TCCR0B & 0xF8) | 0x05;//set prescaler to 1024
        }
        state = BUZZER_ON;
        count = 0;
      }
      break;
    case BUZZER_ON:
      if(backward){
        count++;
      }

      if( (!buzzer && !backward) || (backward && count >= 50)){
        state = BUZZER_OFF;
        count = 0;
      }
      break;
    default:
      break;
  }

  switch(state){ //state transitions
    case BUZZER_INIT:
      break;
    case BUZZER_OFF:
      OCR0A = 255;
      
      //PORTD &= ~(0x40);
      break;
    case BUZZER_ON:
      
      //serial_println("buzzer on!");
      OCR0A = 128;
      //PORTD |= 0x40;
      break;
    default:
      break;
  }

  return state;
}

int Tick_StepMotor(int state){
  static int index;
  switch (state)
  {
  case STEPMOTOR_INIT:
    index = 0;
    state = STEPMOTOR_OFF;
    break;
  case STEPMOTOR_OFF:
    if(forward) state = STEPMOTOR_FORWARD;
    else if(backward) state = STEPMOTOR_BACKWARD;
    break;
  case STEPMOTOR_FORWARD:
    if(!forward) state = STEPMOTOR_OFF;
    break;
  case STEPMOTOR_BACKWARD:
    if(!backward) state = STEPMOTOR_OFF;
    break;
  
  default:
    break;
  }

  switch (state)
  {
  case STEPMOTOR_INIT:
    break;
  case STEPMOTOR_OFF:
    break;
  case STEPMOTOR_FORWARD:

    PORTB = (PORTB & 0x03) | stages[index%8] << 2;

    index++;
    index %= 7;

    break;
  case STEPMOTOR_BACKWARD:
    PORTB = (PORTB & 0x03) | stages[index%8] << 2;
    if(index > 0){
      index--;
    }else if(index == 0){
      index = 7;
    }
    
    break;
  
  default:
    break;
  }

  return state;
}

int Tick_Servo(int state){

  switch (state) // state transitions
  {
  case SERVO_INIT:
    state = SERVO_ADJUST;
    break;
  case SERVO_ADJUST:
    break;
  
  default:
    break;
  }

  switch (state) // state actions
  {
  case SERVO_INIT:
    break;
  case SERVO_ADJUST:
    OCR1A = servo_rotation;
    //servo_rotation += 5;
    break;
  
  default:
    break;
  }

  return state;
}

int main(void) {
  //TODO: initialize all your inputs and ouputs

  DDRC = 0x00; PORTC = 0xFF;
  DDRD = 0xFF; PORTD = 0x00;
  DDRB = 0xFF; PORTB = 0x00;

  ADC_init();   // initializes ADC
  serial_init(9600);

  //TODO: Initialize the buzzer timer/pwm(timer0)
  TCCR0A |= (1 << COM0A1);// use Channel A
  TCCR0A |= (1 << WGM01) | (1 << WGM00);// set fast PWM Mode
  //TCCR0B = (TCCR0B & 0xF8) | 0x02; //set prescaler to 8
  //TCCR0B = (TCCR0B & 0xF8) | 0x03;//set prescaler to 64
  TCCR0B = (TCCR0B & 0xF8) | 0x04;//set prescaler to 256
  TCCR0B = (TCCR0B & 0xF8) | 0x05;//set prescaler to 1024

  //TODO: Initialize the servo timer/pwm(timer1)
  TCCR1A |= (1 << WGM11) | (1 << COM1A1); //COM1A1 sets it to channel A
  TCCR1B |= (1 << WGM12) | (1 << WGM13) | (1 << CS11); //CS11 sets the prescaler to be 8
  //WGM11, WGM12, WGM13 set timer to fast pwm mode

  ICR1 = 39999; //20ms pwm period

  OCR1A =  servo_rotation;

  //TODO: Initialize tasks here
  // e.g. 
  // tasks[0].period = ;
  // tasks[0].state = ;
  // tasks[0].elapsedTime = ;
  // tasks[0].TickFct = ;

  //L_Button Task
  tasks[0].period = 20;
  tasks[0].state = LBUTTON_INIT;
  tasks[0].elapsedTime = 0;
  tasks[0].TickFct = &Tick_LButton;

  //R_Button Task
  tasks[1].period = 20;
  tasks[1].state = RBUTTON_INIT;
  tasks[1].elapsedTime = 0;
  tasks[1].TickFct = &Tick_RButton;

  //Left LED Sequence
  tasks[2].period = 500;
  tasks[2].state = RBUTTON_INIT;
  tasks[2].elapsedTime = 0;
  tasks[2].TickFct = &Tick_LLED;

  //Right LED Sequence
  tasks[3].period = 500;
  tasks[3].state = RBUTTON_INIT;
  tasks[3].elapsedTime = 0;
  tasks[3].TickFct = &Tick_RLED;

  //Joystick
  tasks[4].period = 20;
  tasks[4].state = JOYSTICK_INIT;
  tasks[4].elapsedTime = 0;
  tasks[4].TickFct = &Tick_Joystick;

  //Buzzer
  tasks[5].period = 20;
  tasks[5].state = BUZZER_INIT;
  tasks[5].elapsedTime = 0;
  tasks[5].TickFct = &Tick_Buzzer;

  //Step Motor
  tasks[6].period = motor_period;
  tasks[6].state = STEPMOTOR_INIT;
  tasks[6].elapsedTime = 0;
  tasks[6].TickFct = &Tick_StepMotor;

  //Servo Motor
  tasks[7].period = 5;
  tasks[7].state = SERVO_INIT;
  tasks[7].elapsedTime = 0;
  tasks[7].TickFct = &Tick_Servo;


  TimerSet(GCD_PERIOD);
  TimerOn();

  while (1) {
    /*
    serial_println("left:");
    serial_println(left_LED);
    serial_println("right:");
    serial_println(right_LED);
    */
  }

  return 0;
}