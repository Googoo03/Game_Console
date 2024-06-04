#include "timerISR.h"
#include "helper.h"
#include "periph.h"
#include "serialATmega-1.h"
#include "spiAVR.h"
#include "EEPROM.h"

//PLAYER SPRITES
#include "playerSprite.h"

//ENEMY SPRITES
#include "enemy.h"

//TILESET
#include "tileset.h"

//PROCEDURAL NOISE
#include "perlin.h"

#define NUM_TASKS 6
#define MAX_Q     100
#define MAX_ENEMIES 4

#define SWRESET 0x01
#define SLPOUT  0x11
#define COLMOD  0x3A
#define DISPON  0x29
#define INVON   0X21
#define CASET   0x2A
#define RASET   0x2B
#define RAMWR   0x2C
#define IDMON  0x39
#define GAMSET 0x26

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

typedef struct _player{
  uint8_t X = 64;
  uint8_t Y = 64;

  uint8_t lastX = X; //this is the previous position the sprite was at
  uint8_t lastY = Y;


  uint8_t speed = 5;

  uint8_t health = 10;
  bool dead = false;

  bool attack = false;
  uint8_t attackCoolDown = 2; //amount of ticks before attacking again
} player_S;


//TODO: Define Periods for each task
// e.g. const unsined long TASK1_PERIOD = <PERIOD>
const unsigned long GCD_PERIOD = 100;//TODO:Set the GCD Period

task tasks[NUM_TASKS]; // declared task array with 5 tasks
queue q;

enum SCREEN_SM_STATES {SCREEN_INIT,SCREEN_PRINT};
enum BACKGROUND_SM_STATES {BACKGROUND_INIT,BACKGROUND_DRAW};
enum PLAYER_SM_STATES {PLAYER_INIT,PLAYER_IDLE};
enum JOYSTICK_SM_STATES {JOYSTICK_INIT,JOYSTICK_IDLE};
enum BACKLIGHT_SM_STATES {BACKLIGHT_INIT,BACKLIGHT_OFF,BACKLIGHT_ON};
enum A_BUTTON_SM_STATES {A_BUTTON_INIT,A_BUTTON_OFF,A_BUTTON_ON};
enum B_BUTTON_SM_STATES {B_BUTTON_INIT,B_BUTTON_OFF,B_BUTTON_ON};
enum ENEMY_MANAGER_SM_STATES {ENEMY_INIT, ENEMY_UPDATE};

//////////////GLOBALS/////////////////

//PLAYER STRUCT
player_S player; 

//ENEMY STRUCT AND QUEUE
_player enemies[MAX_ENEMIES];
uint8_t num_enemies;
//number of enemies per background is determined by map_perlin[SPRITE_SIZE]

//SPRITES
uint16_t background_color = 0x001F;

//JOYSTICK
int joystickThreshold = 100;
int move;

//////////////////////////////////////

void generateNewEnemies(uint8_t dir){
  int perlinVal;//s = (int)(map_perlin[SPRITE_SIZE]*100000*MAX_ENEMIES); //might have to change later
  
  perlinVal = 4/(world_x/8);
  perlinVal = 4/(world_y/8) > perlinVal ?  4/(world_y/8) : perlinVal; 

  //assume all previous enemies are dead, no need to check dead flag

  /*
   world_y += ((dir & 0x08) == 0x08)*8;    //up
  world_y += ((dir & 0x04) == 0x04)*-8; //down

  world_x += ((dir & 0x02) == 0x02)*8;      //right
  world_x += ((dir & 0x01) == 0x01) * -8; //left
  */

  for(int i = 0; i < perlinVal; ++i){
    enemies[i].dead = false;
    if((dir & 0x08) == 0x08){
      enemies[i].Y = 100;
      enemies[i].X = 64;
    }else if((dir & 0x04) == 0x04){
      enemies[i].Y = 10;
      enemies[i].X = 64;

    }else if((dir & 0x02) == 0x02){
      enemies[i].Y = 64;
      enemies[i].X = 100;
    }else{
      enemies[i].Y = 64;
      enemies[i].X = 10;
    }
    //enemies[i].X = (dir & 0x02) == 0x02 ? 125 : 10;
    //enemies[i].Y = (dir & 0x04) == 0x04 ? 10 : 125;
  }
  return;
}


void killAllEnemies(){
  for(int i = 0; i < MAX_ENEMIES;++i){
    enemies[i].dead = true;
  }
  return;
}

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
  PORTD &= ~(0x40); //reset pin
  _delay_ms(200);
  PORTD |= 0x40;
  _delay_ms(200);
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

  HardwareReset();

  Send_Command(SWRESET);

  _delay_ms(150);
  Send_Command(SLPOUT);

  _delay_ms(200);
  Send_Command(COLMOD);
  Send_Data(0x05); //for 16 bit color mode. You can pick any color mode you want

  _delay_ms(10);
  Send_Command(DISPON);
}

void drawScreen() {
  uint16_t color;// = 0xF800;
  uint16_t scale = 2;
  uint16_t res = 128/(8*scale);

	uint8_t high;
	uint8_t low;
  uint16_t color_change = (world_x == 0 && world_y == 0) ? 0xFFFF :0xFFE0;
  for(uint16_t k = 0; k < res*res; ++k){

    double perlinValue = map_perlin[k];
      
      //serial_println(perlinValue*100);
      

    for (uint16_t i = 0; i < 64; i++) { //per sprite
      
      //color = tileset[0][i];

      if(perlinValue*100 > 0){
        color = tileset[1][i];
        //color_change = 0x08FF;
      }else{
        color = tileset[0][i];
        //color_change = 0xFFE0;
      }

      if(color == WHITE) color &= color_change;

      int x = ((i%8)*scale)+((k%8)*8*scale);
      int y = ((i/8)*scale)+((k/8)*8*scale);
      set_address_window(x, y, x+scale-1, y+scale-1);
      Send_Command(RAMWR);

      high = color >> 8;
      low = color & 0xFF;
      for(uint16_t j = 0; j < scale*scale; ++j){
        Send_Data(high);
        Send_Data(low);
      }
      //if(color == 0xFFFF) continue; //WE SKIP PURE WHITE. IT IS DESIGNATED AS ALPHA
    }
  }
  return;
}

void drawPlayer() {
  uint16_t color;// = 0xF800;
  uint16_t scale = 2;

	uint8_t high;
	uint8_t low;
	for (uint16_t i = 0; i < 64; i++) {

    color = player_Sprite[0][i];
    if(player.attack) color = 0xFFFF - color;
    int x = player.X+((i%8)*scale);
    int y = player.Y+((i/8)*scale);
    set_address_window(x, y, x+scale-1, y+scale-1);
	  Send_Command(RAMWR);

    high = color >> 8;
	  low = color & 0xFF;
    for(uint16_t j = 0; j < scale*scale; ++j){
      Send_Data(high);
		  Send_Data(low);
    }
	}
  return;
}

void drawEnemy() {
  uint16_t color;// = 0xF800;
  uint16_t scale = 2;

	uint8_t high;
	uint8_t low;
  _player enemy;

  for(uint16_t k = 0; k < MAX_ENEMIES;++k){

    if(enemies[k].dead) continue; //only draw the alive enemies
    enemy = enemies[k];

    for (uint16_t i = 0; i < 64; i++) {

      color = enemy_sprite[0][i];
      if(enemy.attack) color = 0xFFFF - color;
      int x = enemy.X+((i%8)*scale);
      int y = enemy.Y+((i/8)*scale);
      set_address_window(x, y, x+scale-1, y+scale-1);
      Send_Command(RAMWR);

      high = color >> 8;
      low = color & 0xFF;
      for(uint16_t j = 0; j < scale*scale; ++j){
        Send_Data(high);
        Send_Data(low);
      }
    }
  }
  return;
}

/*void clearSprite() { //replaces the sprite with the background
  uint16_t color = background_color;

	set_address_window(player.lastX, player.lastY, player.lastX+7, player.lastX+7);
	Send_Command(RAMWR);

	uint8_t high;
	uint8_t low;

  bool x_inbounds;
  bool y_inbounds;
	for (uint16_t i = 0; i < 64; i++) {
    //if(color == 0xFFFF) continue; //WE SKIP PURE WHITE. IT IS DESIGNATED AS ALPHA
    x_inbounds = ((int)(player.lastX + (i%8)) - (int)(player.X)) < 7;
    y_inbounds = ((int)(player.lastY + (i%8)) - (int)(player.y)) < 7;
    if(x_inbounds && y_inbounds){
      color = 
    }
    high = color >> 8;
	  low = color & 0xFF;
		Send_Data(high);
		Send_Data(low);
	}
}*/

void changeBackground(uint8_t dir){

  world_y += ((dir & 0x08) == 0x08)*8;    //up
  world_y += ((dir & 0x04) == 0x04)*-8; //down

  world_x += ((dir & 0x02) == 0x02)*8;      //right
  world_x += ((dir & 0x01) == 0x01) * -8; //left

  generate = true;
  generateMap();
  generateNewEnemies(dir);
  //might need to add generate function
  return;
}

void dummy(){return;}


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
    if(move != 0x00) push(&drawScreen); //temporary -- draw the brackground only when the player is moving
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
    push(&drawPlayer);
    break;
  
  default:
    break;
  }

  return state;
}

int Tick_Joystick(int state){
  static uint8_t bounds;
  static uint16_t xMOD;
  static uint16_t yMOD;
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
    player.lastX = player.X;
    player.lastY = player.Y;

    ////////CHECK PLAYER MOVE DIRECTION
    move = checkMove();
    if((move & 0x01) == 1){ //up
      player.Y += player.speed;
    }
    if((move & 0x02) == 2){ //down
      player.Y -= player.speed;
    }
    if((move & 0x04) == 4){ //right
      player.X -= player.speed;
    }
    if((move & 0x08) == 8){ //left
      player.X += player.speed;
    }
    ///////////////////////////////////

    ////////CHECK IF PLAYER IS OUT OF BOUNDS///////////
    bounds = 0x00;
    xMOD = player.X % 130;
    yMOD = player.Y % 130;

    if(xMOD > 60 && xMOD != player.X){
      bounds += 0x01; //left go to right
    }else if(xMOD < 60 && xMOD != player.X){
      bounds += 0x02; //right go to left
    }

    if(yMOD > 60 && yMOD != player.Y){
      bounds += 0x04; //down go to up
    }else if(yMOD < 60 && yMOD != player.Y){
      bounds += 0x08; //up go to down
    }
    if(bounds != 0x00){
      killAllEnemies();
      changeBackground(bounds);
    }

    player.X %= 130;
    player.Y %= 130;
    ///////////////////////////////////////////////////

    
    break;
  default:
    break;
  }

  return state;
}

int Tick_A_Button(int state){
  static uint8_t count;
  switch (state) //state transitions
  {
  case A_BUTTON_INIT:
    count = 0;
    state = A_BUTTON_OFF;
    break;
  case A_BUTTON_OFF:
    if(ADC_read(3) > 512) state = A_BUTTON_ON;
    break;
  case A_BUTTON_ON:
    if(ADC_read(3) < 512) state = A_BUTTON_OFF;
    break;
  
  default:
    break;
  }

  switch (state) //state actions
  {
  case A_BUTTON_INIT:
    break;
  case A_BUTTON_OFF:
    player.attack = false;
    count = 0;
    break;
  case A_BUTTON_ON:
    player.attack = false;
    if(count < player.attackCoolDown){
      count ++;
    }else{
      player.attack = true;
      count = 0;
    }
    break;
  
  default:
    break;
  }

  return state;
}

//ENEMY MANAGER
int Tick_Enemy_Manager(int state){

  switch (state) //state transitions
  {
  case ENEMY_INIT:
    state = ENEMY_UPDATE;
    break;
  
  case ENEMY_UPDATE:
    break;  
  default:
    break;
  }

  switch (state) //state actions
  {
  case ENEMY_INIT:
    break;
  
  case ENEMY_UPDATE:
    for(int i = 0; i < MAX_ENEMIES; ++i){
      
      if(enemies[i].dead) continue;
      //do enemy routine check

      //add to draw

    }
    push(&drawEnemy);
    //how to remove enemies once they die?
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
    generateMap();
    background_color = 0x0000;// 0x001F;
    Send_Command(GAMSET);
    Send_Data(0x01);
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

    //A button
    tasks[4].period = 100;
    tasks[4].state = A_BUTTON_INIT;
    tasks[4].elapsedTime = 0;
    tasks[4].TickFct = &Tick_A_Button;

    //Enemy Manager
    tasks[5].period = 100;
    tasks[5].state = ENEMY_INIT;
    tasks[5].elapsedTime = 0;
    tasks[5].TickFct = &Tick_Enemy_Manager;

    TimerSet(GCD_PERIOD);
    TimerOn();

    while (1) {}

    return 0;
}