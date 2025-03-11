# Embedded Systems Game Console

This project is a collection of the skills and tools used in the **Embedded Systems** course at University of California, Riverside. As much as this was apart of our curriculum, it was a pleasure to work on this project for the 3 weeks allocated to it. I actively enjoyed implementing features, incorporating old and new skills to build the best product possible. This game mimics the original Zelda game on the NES console.

## Why Game Development?

It is likely many poeple's dream to be able to make the games that built their childhood, myself included. I was introduced to game development in 4th grade when I began to write programs using MS-DOS on the command line. Since then, I've always loved to grow my skills and experiences. It's ultimately fascinating the different levels of engineering, mathematics, algorithms, and optimizations that are at play in the games that I've enjoyed all my life.

So, when given the opportunity to build a game using embedded systems hardware, I immediately took it.

## The Final Product

Before I delve into the design process of this project, let me showcase the final product.

![Zoida_Off](https://github.com/user-attachments/assets/c531448d-b6f0-4d78-b2d2-9df4d20e0a24)


## Design Process

When designing this project, I wanted to mimic the original NES controller as much as possible. This resulted in a joystick on the left side (instead of the traditional Directional-Pad) and 2 buttons on the right of the screen. The joystick controlled player movement, and the buttons controlled the player abilites such as attacking.

### Procedural Generation

To create the varied landscapes, and make the player not feel as though they are walking in an infinite void, procedural generation techniques were used to create scattered rocks on the ground. I used a previously created Perlin Noise algorithm used in one of my other projects, Fantasma, and tweaked the parameters to make the rocks few and far between. Each perlin noise value was simply clipped, if it passed a certain threshold, then the background tile at that location would be a rock, and grass otherwise.

![Zoida_Procedural_Generation](https://github.com/user-attachments/assets/022bd7a6-82f0-4d21-b462-aac0475d3492)


If time and memory space allowed, additional techniques would've been implemented, namely dungeon generation and more varied land generation, including lakes, rivers, towns, shops, etc. 

### Enemy Scripts

Enemy behavior was determined pseudorandomly. Each enemy had its own seed value that was passed into a Linear Congruential Generator, a type of Pseudo Random Number Generator. This value that was created each frame determined the direction of the enemy movement.

Enemies spawn opposite of the player whenever the player enters a new background space, and move in a random walk as described previously. When an enemy is touching a player, is activates the player-hurt state and causes the player to lose health over a desired time interval.

### Drawing Manager

To maintain cleanliness and readability of code, all drawing that is done on screen was handed to the drawing manager. The drawing manager referenced a stack of drawing operations, which each object can push to when needed, and allowed for all sprites to be drawn in sequence. The stack was composed of function pointers, which allowed for incredible modularity in the code.

![Drawing_Manager](https://github.com/user-attachments/assets/1347946b-cc5b-48a7-9978-cbfc4a57bac1)


### Limitations & Compromises

One of the greatest compromises that I faced during this project was the storage capacity. We are only given 32KB of memory to be used for all code, graphics, environments, and features. This brought whatever ideas I had for a great exploration game with intense enemy combat back to reality. 

I overcame the graphical limitation by only storing the sprites at half resolution, then adding a scale modifier in my code to draw sprites at any scale, though a scale modifier of 2 is used mostly throughout the program. Any sprite that is used multiple times with different hues is stored with a monotone hue so that it can be modified to fit any need. Additionally, adding procedural generation into the background generation allowed for varied landscapes with little overhead.

At the end, the project meet the 32KB limit, and had no impedence on the quality of the project.

### Demo Video

[![Watch the video]](https://youtu.be/CkpDbP_w0i8)


