#include "colors.h"

#ifndef ENEMY_H

#define ENEMY_NUM_SPRITES 1
#define SPRITE_SIZE 64

#define ENEMY_H

unsigned int enemy_sprite[ENEMY_NUM_SPRITES][SPRITE_SIZE]= {
    
    WHITE,WHITE,PINK,PINK,BLUE,RED,WHITE,WHITE,
    WHITE,BROWN,ORANGE,ORANGE,YELLOW,ORANGE,WHITE,WHITE,
    YELLOW,WHITE,BROWN,PINK,PINK,RED,BROWN,WHITE,
    YELLOW,TAN,PINK,PINK,RED,RED,WHITE,WHITE,
    WHITE,YELLOW,YELLOW,TAN,TAN,TAN,TAN,WHITE,
    WHITE,YELLOW,TAN,YELLOW,BLACK,TAN,BLACK,WHITE,
    WHITE,YELLOW,YELLOW,YELLOW,YELLOW,TAN,YELLOW,WHITE,
    WHITE,WHITE,YELLOW,YELLOW,YELLOW,RED,YELLOW,WHITE
};

#endif