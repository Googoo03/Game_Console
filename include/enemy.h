#include "colors.h"

#ifndef ENEMY_H

#define ENEMY_NUM_SPRITES 2
#define SPRITE_SIZE 64

#define ENEMY_H

uint16_t enemy_sprite[ENEMY_NUM_SPRITES][SPRITE_SIZE]= {
    
    {WHITE,WHITE,PINK,PINK,BLUE,RED,WHITE,WHITE,
    WHITE,BROWN,ORANGE,ORANGE,YELLOW,ORANGE,WHITE,WHITE,
    YELLOW,WHITE,BROWN,PINK,PINK,RED,BROWN,WHITE,
    YELLOW,TAN,PINK,PINK,RED,RED,WHITE,WHITE,
    WHITE,YELLOW,YELLOW,TAN,TAN,TAN,TAN,WHITE,
    WHITE,YELLOW,TAN,YELLOW,BLACK,TAN,BLACK,WHITE,
    WHITE,YELLOW,YELLOW,YELLOW,YELLOW,TAN,YELLOW,WHITE,
    WHITE,WHITE,YELLOW,YELLOW,YELLOW,RED,YELLOW,WHITE},

    {
        WHITE,WHITE,ORANGE,WHITE,WHITE,WHITE,RED,WHITE,
        GRAY,GRAY,ORANGE,ORANGE,ORANGE,ORANGE,RED,BLACK,
        ORANGE,RED,YELLOW,YELLOW,YELLOW,YELLOW,ORANGE,RED,
        GRAY,GRAY,YELLOW,GRAY,GRAY,GRAY,GRAY,WHITE,
        WHITE,GRAY,GRAY,GRAY,GRAY,GRAY,GRAY,GRAY,
        WHITE,RED,GRAY,GRAY,BLACK,GRAY,TAN,TAN,
        WHITE,WHITE,GRAY,GRAY,GRAY,GRAY,TAN,TAN,
        WHITE,WHITE,GRAY,WHITE,WHITE,BLACK,WHITE,TAN
    }
};

#endif