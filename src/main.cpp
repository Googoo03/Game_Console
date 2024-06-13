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

#define NUM_TASKS 9
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
  uint8_t scale = 2;

  uint8_t move; //movement direction
  uint8_t moveCooldown = 5; //for enemies. ticks until change direction

  bool isBoss; //for enemies.Denotes the boss

  uint8_t moveTicks = 0;

  uint8_t lastX = X; //this is the previous position the sprite was at
  uint8_t lastY = Y;


  uint8_t speed = 5;

  uint8_t health = 10;
  bool dead = false;

  bool hurt;
  uint8_t hurtCooldown = 5; //amount of ticks before getting hurt again
  uint8_t hurtTick = 0;

  bool attack = false;
  uint8_t attackCoolDown = 2; //amount of ticks before attacking again
} player_S;

typedef struct _crosshair {
  uint8_t X = 64;
  uint8_t Y = 64;

  uint8_t lastX = X;
  uint8_t lastY = Y;
} crosshair_S; 


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
enum HEALTH_MANAGER_SM_STATES {HEALTH_INIT,HEALTH_UPDATE};
enum CROSSHAIR_SM_STATES {CROSSHAIR_INIT,CROSSHAIR_UPDATE};
enum KILLSCREEN_SM_STATES {KILLSCREEN_INIT,KILLSCREEN_CHECK};

//////////////GLOBALS/////////////////

//PLAYER STRUCT
player_S player; 

//CROSSHAIR STRUCT
player_S crosshair;

//HEALTH ICON
bool blink;

//KILLSCREEN
bool endGame = false;

//ENEMY STRUCT AND QUEUE
_player enemies[MAX_ENEMIES];
uint8_t num_enemies;
//number of enemies per background is determined by map_perlin[SPRITE_SIZE]

//SPRITES
uint16_t background_color = 0x001F;
uint8_t sprite_scale = 2;

//JOYSTICK
int joystickThreshold = 100;

//RANDOM NUMBER GENERATOR
int _state = 22;

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
  if(world_x == 0 && world_y == 0){
    enemies[0].isBoss = true;
    enemies[0].dead = false;
    enemies[0].X = 64;
    enemies[0].Y = 64;

    enemies[0].health = 10;
  }

  for(int i = 0; i < perlinVal; ++i){
    enemies[i].dead = false;
    enemies[i].moveTicks = 0;
    enemies[i].health = 1;
    enemies[i].isBoss = false;

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

int rand(){
  int c = 74;
  int a = 75;
  int32_t m = 65537;
  serial_println(_state);
  _state = (_state*a + c) % m;
  return _state;
}

void enemyRoutine(_player* enemy){
  int dist;
  //if enemy is in range, hurt the player
  dist = sqrt( (player.X - enemy->X)*(player.X - enemy->X) + (player.Y - enemy->Y)*(player.Y - enemy->Y));
  if(dist < (8*sprite_scale)){
    //hurt the player if possible
    enemy->attack = true;
    player.hurt = true;
    //the player will only take damage if the cooldown finished.
  }else{
    enemy->attack = false;
  }

  enemy->dead = (enemy->health <= 0);
  

  if(enemy->hurt){
    enemy->health --;
  }

  if(player.move == 0x00) return; //DO NOT MOVE IF THE PLAYER IS NOT MOVING

  if(enemy->moveTicks < enemy->moveCooldown){
    enemy->moveTicks++;
  }else{
    enemy->move = (rand() / 1000) % 4;
    enemy->moveTicks = 0;
  }
  enemy->lastX = enemy->X;
  enemy->lastY = enemy->Y;
  if(enemy->move == 0x00) enemy->X += enemy->speed;
  if(enemy->move == 0x01) enemy->X -= enemy->speed;
  if(enemy->move == 0x02) enemy->Y += enemy->speed;
  if(enemy->move == 0x03) enemy->Y -= enemy->speed;


  enemy->X %= 130;
  enemy->Y %= 130;
  
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
  uint16_t color_change = (world_x == 0 && world_y == 0) ? 0x2222 :0x0FE0;
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

void drawKillScreen(){ //called in 2 scenarios, either the player dies, or wins.
  uint16_t color;// = 0xF800;

	uint8_t high;
	uint8_t low;

  int x = 0;
  int y = 0;

  color = player.health <= 0 ? RED : GREEN;
  set_address_window(x, y, 128, 128);
  Send_Command(RAMWR);

  high = color >> 8;
  low = color & 0xFF;
  for(uint16_t j = 0; j < 128*128; ++j){
    Send_Data(high);
    Send_Data(low);
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
    
    if(player.hurt && player.hurtTick == 0){
      color = color & 0x008F;
    }
    

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

void drawCrosshair() {
  uint16_t color;// = 0xF800;
  uint16_t scale = 2;

	uint8_t high;
	uint8_t low;
	for (uint16_t i = 0; i < 4; i++) {

    color = WHITE;

    int x = crosshair.X+((i%2)*scale);
    int y = crosshair.Y+((i/2)*scale);
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
      int index = (enemy.isBoss == true);
      color = enemy_sprite[index][i];
      if(enemy.attack && player.hurtTick == 0) color = 0xFFFF - color;
      if(enemy.hurt) color &= RED;
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

void clearSprite(_player sprite){
    uint16_t scale = 2;
  uint16_t res = 8;
  uint16_t color = background_color;

	uint8_t high;
	uint8_t low;

  int tile_x = (sprite.lastX/(8*scale));
  int tile_y = (sprite.lastY/(8*scale));

  int tileIndex = tile_y*res + tile_x;
  uint16_t color_change = (world_x == 0 && world_y == 0) ? 0x2222 :0x0FE0;

  for(uint16_t k = 0; k < 4;++k){
    
    int finalIndex = tileIndex + (res*(k/2)+(k%2));
    double perlinValue = map_perlin[finalIndex];

    for (uint16_t i = 0; i < 64; i++) {
      //if(color == 0xFFFF) continue; //WE SKIP PURE WHITE. IT IS DESIGNATED AS ALPHA
      
      if(perlinValue*100 > 0){
        color = tileset[1][i];
        //color_change = 0x08FF;
      }else{
        color = tileset[0][i];
        //color_change = 0xFFE0;
      }

      if(color == WHITE) color &= color_change;
      int tile_x = (sprite.lastX/(8*scale));
      int tile_y = (sprite.lastY/(8*scale));
      int x = ((i%8)*scale)+(tile_x*res*scale)+((k%2)*8*scale);
      int y = ((i/8)*scale)+(tile_y*res*scale)+((k/2)*8*scale);
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

void drawHealth(){
  uint16_t color;// = 0xF800;
  uint16_t scale = 2;

	uint8_t high;
	uint8_t low;
	for (uint16_t i = 0; i < 64; i++) {

    color = healthbar_Sprite[0][i];
    
    if(color == RED && (i/8) > player.health) color |= 0xFFFF;
    if(color == WHITE && blink) color = RED;

    int x = 128-(8*scale)+((i%8)*scale);
    int y = 128-(8*scale)+((i/8)*scale);
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

void resetGame(){
  player.X = 64;
  player.Y = 64;
  player.health = 10;
  world_x = 8;
  world_y = 0;

  generateMap();
  generateNewEnemies(0x01);
  drawScreen();
  return;
}

void clearPlayer() { //replaces the sprite with the background
  clearSprite(player);
  return;
}

void clearCrosshair(){
  clearSprite(crosshair);
  return;
}

void clearEnemy(){
  for(int i = 0; i < MAX_ENEMIES; ++i){
    if(!enemies[i].dead) clearSprite(enemies[i]); //make sure last X and last Y of the enemies is good
  }
  return;
}

void damageEnemies(){
  int dist;
  for(int i = 0; i < MAX_ENEMIES; ++i){

    if(enemies[i].dead) continue;
    dist = sqrt( (crosshair.X - enemies[i].X)*(crosshair.X - enemies[i].X) + (crosshair.Y - enemies[i].Y)*(crosshair.Y - enemies[i].Y));
    if(dist < (8*sprite_scale)) enemies[i].hurt = true;
  }
  return;
}

void resetEnemyHurt(){
  for(int i = 0; i < MAX_ENEMIES; ++i){

    if(enemies[i].dead) continue;
    enemies[i].hurt = false;
  }
  return;
}

void changeBackground(uint8_t dir){

  world_y += ((dir & 0x08) == 0x08)*8;    //up
  world_y += ((dir & 0x04) == 0x04)*-8; //down

  world_x += ((dir & 0x02) == 0x02)*8;      //right
  world_x += ((dir & 0x01) == 0x01) * -8; //left

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
  //static void(*draw)() = &drawScreen;
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
    if(generate) push(&drawScreen);
    if(player.move != 0x00){
      push(&clearPlayer);//push(&drawScreen); //temporary -- draw the brackground only when the player is moving
      //clear enemies
      push(&clearEnemy);
      push(&clearCrosshair);
    }
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
    player.speed = 5;
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
    if(endGame) break; //dont do anything if game ends

    resetEnemyHurt(); //in order to abide by single task modifies globals at a time
    player.hurtTick += player.hurt;
    player.hurtTick %= player.hurtCooldown;
    if(!player.hurt) player.hurtTick = 0;

    //remove health if player is hurt and cooldown finished, health bottoms out at 0
    if(player.hurt && player.hurtTick == 0) player.health = player.health > 0 ? player.health-1 : 0;

    if(player.attack){ //check the distance of all enemies and take damage accordingly
      damageEnemies();
    }

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

    if(endGame) break; //dont do anything if game ends
    player.lastX = player.X;
    player.lastY = player.Y;

    ////////CHECK PLAYER MOVE DIRECTION
    player.move = checkMove();
    if((player.move & 0x01) == 1){ //up
      player.Y += player.speed;
    }
    if((player.move & 0x02) == 2){ //down
      player.Y -= player.speed;
    }
    if((player.move & 0x04) == 4){ //right
      player.X -= player.speed;
    }
    if((player.move & 0x08) == 8){ //left
      player.X += player.speed;
    }
    ///////////////////////////////////

    ////////CHECK IF PLAYER IS OUT OF BOUNDS///////////
    bounds = 0x00;
    xMOD = player.X % 130;
    yMOD = player.Y % 130;
    generate = false;

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
      generate = true;
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
  if(endGame) break; //dont do anything if game ends
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
    

    player.hurt = false;
    if(endGame) break; //dont do anything if game ends
    
    for(int i = 0; i < MAX_ENEMIES; ++i){
      
      if(enemies[i].dead) continue;
      //do enemy routine check
      enemyRoutine(enemies+i);
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


int Tick_Health_Manager (int state) {
  static unsigned char count;
  switch (state) //state transitions
  {
  case HEALTH_INIT:
    count = 0;
    blink = false;
    state = HEALTH_UPDATE;
    break;
  case HEALTH_UPDATE:
    break;
  
  default:
    break;
  }

  switch (state) //state actions
  {
  case HEALTH_INIT:
    break;
  case HEALTH_UPDATE:
    if(endGame) break; //dont do anything if game ends
    blink = false;

    if(player.health < 5){
      blink = (count == 0);
      count ++;
      count %= 5;
    }
    push(&drawHealth);
    break;
  
  default:
    break;
  }
  return state;
}


int Tick_Crosshair(int state) {
  
  switch (state) //state transitions
  {
  case CROSSHAIR_INIT:
    state = CROSSHAIR_UPDATE;
    break;
  
  case CROSSHAIR_UPDATE:
    break;
  default:
    break;
  }
  
  switch (state) //state actions
  {
  case CROSSHAIR_INIT:
    break;
  
  case CROSSHAIR_UPDATE:
    if(endGame) break; //dont do anything if game ends
    crosshair.lastX = crosshair.X;
    crosshair.lastY = crosshair.Y;
    //CROSS HAIR STUFF HERE. IT SHOULD ALWAYS FOLLOW THE PLAYER DIRECTION, IE "player->move"
    if((player.move & 0x08) == 8){ //left
      crosshair.X = player.X + (12*player.scale);
      crosshair.Y = player.Y + (4*player.scale);
    }else if((player.move & 0x04) == 4){ //right
      crosshair.X = player.X - (4*player.scale);
      crosshair.Y = player.Y + (4*player.scale);

    }else if((player.move & 0x02) == 2){ //down
      crosshair.X = player.X + (4*player.scale);
      crosshair.Y = player.Y - (4*player.scale);
      
    }else if((player.move & 0x01) == 1){ //up
      crosshair.X = player.X + (4*player.scale);
      crosshair.Y = player.Y + (12*player.scale);
    }
    push(&drawCrosshair);
    break;
  default:
    break;
  }
  return state;
}

int Tick_Killscreen(int state){
  static unsigned char count;
  static bool draw_KillScreen;
  switch (state) //state transitions
  {
  case KILLSCREEN_INIT:
    state = KILLSCREEN_CHECK;
    draw_KillScreen = false;
    break;
  case KILLSCREEN_CHECK:
    break;
  
  default:
    break;
  }

  switch (state) //state actions
  {
  case KILLSCREEN_INIT:
    break;
  case KILLSCREEN_CHECK:

    if((enemies[0].dead && enemies[0].isBoss) || player.health <= 0) endGame = true;
    if(endGame){
      
      if(!draw_KillScreen){
        push(&drawKillScreen);
        draw_KillScreen = true;
      }
      count++;
    }else{
      count = 0;
    }
    if(count == 10){
      //reset game
      endGame =false;
      draw_KillScreen = false;
      resetGame();
    }
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
    
    generateMap();
    generateNewEnemies(0x01);
    drawScreen();
    
    background_color = 0x0000;// 0x001F;
    Send_Command(GAMSET);
    Send_Data(0x01);
    //TODO: Initialize tasks here
    // e.g. tasks[0].period = TASK1_PERIOD
    // tasks[0].state = ...
    // tasks[0].timeElapsed = ...
    // tasks[0].TickFct = &task1_tick_function;

    //joystick
    tasks[0].period = 100;
    tasks[0].state = JOYSTICK_INIT;
    tasks[0].elapsedTime = 0;
    tasks[0].TickFct = &Tick_Joystick;

    //A button
    tasks[1].period = 100;
    tasks[1].state = A_BUTTON_INIT;
    tasks[1].elapsedTime = 0;
    tasks[1].TickFct = &Tick_A_Button;

    //background
    tasks[2].period = 100;
    tasks[2].state = BACKGROUND_INIT;
    tasks[2].elapsedTime = 0;
    tasks[2].TickFct = &Tick_Background;

    //player
    tasks[3].period = 100;
    tasks[3].state = PLAYER_INIT;
    tasks[3].elapsedTime = 0;
    tasks[3].TickFct = &Tick_Player;

    //Enemy Manager
    tasks[4].period = 100;
    tasks[4].state = ENEMY_INIT;
    tasks[4].elapsedTime = 0;
    tasks[4].TickFct = &Tick_Enemy_Manager;

    //Health Manager
    tasks[5].period = 100;
    tasks[5].state = HEALTH_INIT;
    tasks[5].elapsedTime = 0;
    tasks[5].TickFct = &Tick_Health_Manager;

    //Crosshair
    tasks[6].period = 100;
    tasks[6].state = CROSSHAIR_INIT;
    tasks[6].elapsedTime = 0;
    tasks[6].TickFct = &Tick_Crosshair;

    //Killscreen
    tasks[7].period = 100;
    tasks[7].state = KILLSCREEN_INIT;
    tasks[7].elapsedTime = 0;
    tasks[7].TickFct = &Tick_Killscreen;

    //screen manager
    tasks[8].period = 100;
    tasks[8].state = SCREEN_INIT;
    tasks[8].elapsedTime = 0;
    tasks[8].TickFct = &Tick_Screen;

    TimerSet(GCD_PERIOD);
    TimerOn();

    while (1) {}

    return 0;
}