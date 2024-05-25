#include "timerISR.h"
#include "helper.h"
#include "periph.h"
#include "serialATmega-1.h"
#include "spiAVR.h"

#define NUM_TASKS 4
#define MAX_Q     100

#define SWRESET 0x01
#define SLPOUT  0x11
#define COLMOD  0x3A
#define DISPON  0x29
#define INVON   0X21
#define CASET   0x2A
#define RASET   0x2B
#define RAMWR   0x2C

typedef void (*func_ptr)(void); 

typedef struct _queue{
  signed char front;
  signed char back;
  //int items[MAX_Q];
  func_ptr DrawFct[MAX_Q];
} queue;

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
queue q;

enum SCREEN_SM_STATES {SCREEN_INIT,SCREEN_PRINT};
enum BACKGROUND_SM_STATES {BACKGROUND_INIT,BACKGROUND_DRAW};
enum PLAYER_SM_STATES {PLAYER_INIT,PLAYER_IDLE};
enum JOYSTICK_SM_STATES {JOYSTICK_INIT,JOYSTICK_IDLE};

//////////////GLOBALS/////////////////

//SPRITES
uint16_t background_color = 0x001F;
uint8_t playerX = 64;
uint8_t playerY = 64;
uint8_t playerSpeed = 5;

//JOYSTICK
int joystickThreshold = 100;


//////////////////////////////////////


bool qFull(){ return !(q.back < MAX_Q);}

bool qEmpty(){ return q.back <= -1;}

void push (void(*draw)()){
  
  if(qFull()) return;
  
  q.back ++;
  q.DrawFct[q.back] = draw; //push draw function to queue
  
}


void pop (){
  if(qEmpty()) return; //make sure the queue is not empty already
  
  //int(*returnfct)() = q.DrawFct[0];

  for(int i = 0; i < q.back; ++i){ //no reason to return front
    q.DrawFct[i] = q.DrawFct[i+1];
  }
  q.back --; //decrement back
  
}

void qInit(){
  q.front = 0;
  q.back = -1;
}

void HardwareReset(){
  serial_println("hardware reset START");
  PORTD &= ~(0x40); //reset pin
  _delay_ms(200);
  PORTD |= 0x40;
  _delay_ms(200);
  serial_println("hardware reset END");
}

void Send_Command(int command){
  PORTD &= ~(0x20); //Set A0 pin to 0
  PORTB &= ~(0x04); //Set CS pin to 0 to select display
  SPI_SEND(command);
  PORTB |= (0x04); //Set CS pin to 1 to deselect the display
}

void Send_Data(int data){
  PORTD |= 0x20; //Set A0 pin to 1
  PORTB &= ~(0x04); //Set CS pin to 0 to select display
  SPI_SEND(data);
  PORTB |= (0x04); //Set CS pin to 1 to deselect the display
}

void set_address_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
	Send_Command(CASET);
	Send_Data(0x00);
	Send_Data(x0);
	Send_Data(0x00);
	Send_Data(x1);

	Send_Command(RASET);
	Send_Data(0x00);
	Send_Data(y0);
	Send_Data(0x00);
	Send_Data(y1);
}

void ST7735_init(){
  serial_println("run");
  HardwareReset();

  Send_Command(SWRESET);

  _delay_ms(150);
  Send_Command(SLPOUT);

  _delay_ms(200);
  Send_Command(COLMOD);
  Send_Data(0x05); //for 16 bit color mode. You can pick any color mode you want

  _delay_ms(10);
  Send_Command(DISPON);
  serial_println("finished");
}

void drawScreen() {
  uint16_t color = background_color;
  serial_println("set background");
	set_address_window(0, 0, 130, 130); // Set full screen as the address window
	Send_Command(RAMWR);

	uint8_t high = color >> 8;
	uint8_t low = color & 0xFF;
	for (uint16_t i = 0; i < 128 * 160; i++) {
		Send_Data(high);
		Send_Data(low);
	}
}

void drawPlayer() {
  uint16_t color = 0xF800;

  serial_println(playerX);
  serial_println(playerY);
	set_address_window(playerX, playerY, playerX+8, playerY+8);
	Send_Command(RAMWR);

	uint8_t high = color >> 8;
	uint8_t low = color & 0xFF;
	for (uint16_t i = 0; i < 64; i++) {
    //serial_println("draw player!");
		Send_Data(high);
		Send_Data(low);
	}
}

void dummy(){}


int checkMove(){
  //int joystickThreshold = 256;
  uint8_t output = 0x00;

  int yInput = ADC_read(0);
  int xInput = ADC_read(1);

  int yDist = ADC_read(0)-512; //CHANGE ADC READ
      yDist = yDist < 0 ? -yDist : yDist;
  int xDist = ADC_read(1)-512; //CHANGE ADC READ
      xDist = xDist < 0 ? -xDist : xDist;
  
  if(yInput > 1023-joystickThreshold) output += 1; //up
  if(yInput < joystickThreshold) output += 2; //down
  if(xInput > 1023-joystickThreshold) output += 4; //right
  if(xInput < joystickThreshold) output += 8; //left MIGHT BE BACKWARDS
  return output;
}

/*void drawScreen(uint16_t color){

  Send_Command(CASET);
  Send_Data(0);
  Send_Data(5); //start column 0
  Send_Data(0);
  Send_Data(50); //end column 5

  Send_Command(RASET);
  Send_Data(0);
  Send_Data(5); //start row 0
  Send_Data(0);
  Send_Data(50); //end row 5

  Send_Command(RAMWR);

  Send_Data(color >> 8);
  Send_Data(color & 0xFF);

}*/


void TimerISR() {
	for ( unsigned int i = 0; i < NUM_TASKS; i++ ) {                   // Iterate through each task in the task array
		if ( tasks[i].elapsedTime == tasks[i].period ) {           // Check if the task is ready to tick
			tasks[i].state = tasks[i].TickFct(tasks[i].state); // Tick and set the next state for this task
			tasks[i].elapsedTime = 0;                          // Reset the elapsed time for the next tick
		}
		tasks[i].elapsedTime += GCD_PERIOD;                        // Increment the elapsed time by GCD_PERIOD
	}
}

int Tick_Screen(int state){

  switch (state) //state transitions
  {
  case SCREEN_INIT:
    state = SCREEN_PRINT;
    break;
  
  case SCREEN_PRINT:
    break;  
  default:
    break;
  }

  switch (state) //state actions
  {
  case SCREEN_INIT:
    break;
  
  case SCREEN_PRINT:
    while(!qEmpty()){
      serial_println("FUNCTION");
      (*q.DrawFct)();
      pop();
      if(q.back == -1) break;
    }
    break;  
  default:
    break;
  }

  return state;
}

int Tick_Background(int state){
  static void(*draw)() = &drawScreen;
  switch (state) //state transitions
  {
  case BACKGROUND_INIT: 
    state = BACKGROUND_DRAW;
    break;
  
  case BACKGROUND_DRAW:
    break;
  default:
    break;
  }

  switch (state) //state actions
  {
  case BACKGROUND_INIT:
    break;
  
  case BACKGROUND_DRAW:
    serial_println("push bg");
    push(drawScreen);
    break;
  default:
    break;
  }

  return state;
}

int Tick_Player(int state){

  static void(*drawP)() = drawPlayer;

  switch (state) //state transitions
  {
  case PLAYER_INIT:
    state = PLAYER_IDLE;
    break;
  case PLAYER_IDLE:
    break;
  
  default:
    break;
  }

  switch (state) //state actions
  {
  case PLAYER_INIT:
    break;
  case PLAYER_IDLE:
    serial_println("push player");
    push(drawPlayer);
    break;
  
  default:
    break;
  }

  return state;
}

int Tick_Joystick(int state){

  static int move;

  switch (state) //state transitions
  {
  case JOYSTICK_INIT:
    state = JOYSTICK_IDLE;
    break;
  case JOYSTICK_IDLE:
    break;
  default:
    break;
  }

  switch (state) // state actions
  {
  case JOYSTICK_INIT:
    break;
  case JOYSTICK_IDLE:
    move = checkMove();
    serial_println(move);
    if((move & 0x01) == 1){ //up
      serial_println("UP");
      playerY += playerSpeed;
    }
    if((move & 0x02) == 2){ //down
      serial_println("DOWN");
      playerY -= playerSpeed;
    }
    if((move & 0x04) == 4){ //right
      serial_println("RIGHT");
      playerX -= playerSpeed;
    }
    if((move & 0x08) == 8){ //left
      serial_println("LEFT");
      playerX += playerSpeed;
    }
    playerX %= 130;
    playerY %= 130;
    break;
  default:
    break;
  }

  return state;
}

int main(void) {
    //TODO: initialize all your inputs and ouputs
    DDRB = 0xFF; PORTB = 0x00;
    
    DDRD = 0xFF; PORTD = 0x00;

    DDRC = 0x00; PORTC = 0xFF; //Set PORTC to all inputs

    ADC_init();   // initializes ADC
    serial_init(9600);
    SPI_INIT();

    ST7735_init();
    qInit();

    background_color = 0x5555;
    drawScreen();
    background_color = 0x001F;
    //TODO: Initialize tasks here
    // e.g. tasks[0].period = TASK1_PERIOD
    // tasks[0].state = ...
    // tasks[0].timeElapsed = ...
    // tasks[0].TickFct = &task1_tick_function;

    

    //background
    tasks[0].period = 100;
    tasks[0].state = BACKGROUND_INIT;
    tasks[0].elapsedTime = 0;
    tasks[0].TickFct = &Tick_Background;

    //player
    tasks[1].period = 100;
    tasks[1].state = PLAYER_INIT;
    tasks[1].elapsedTime = 0;
    tasks[1].TickFct = &Tick_Player;

    //screen manager
    tasks[2].period = 100;
    tasks[2].state = SCREEN_INIT;
    tasks[2].elapsedTime = 0;
    tasks[2].TickFct = &Tick_Screen;

    //joystick
    tasks[3].period = 100;
    tasks[3].state = JOYSTICK_INIT;
    tasks[3].elapsedTime = 0;
    tasks[3].TickFct = &Tick_Joystick;

    TimerSet(GCD_PERIOD);
    TimerOn();

    while (1) {}

    return 0;
}