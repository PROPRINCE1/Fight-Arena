#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#define width 1200
#define height 640
#define hover height/2
#define RECT_WIDTH 30
#define RECT_HEIGHT 30
#define RECT_SPEED 4.5
#define GRAVITY 1
#define JUMP_FORCE -20
#define MAX_JUMP_HEIGHT 350
#define GROUND_LEVEL (height-100) //X-axis
#define MAX_HEALTH 250
#define ATTACK_DURATION 35 // Number of frames an attack lasts
#define PROJECTILE_SPEED 10
#define PROJECTILE_WIDTH 50
#define PROJECTILE_HEIGHT 40
#define FRAME_DELAY 120
#define WALKING 0
#define JUMPING 1
#define PUNCHNG 2
#define KICKING 3
#define STANCE 4
#define SPECIAL 5

typedef struct {
    SDL_Rect rect;
    int velocityY;
    int onGround;
    int originalY;
    int isPunching;
    int isKicking;
    int attackTimer; // Timer to persist attacks
} Player;

// Sprite structure
typedef struct {
    SDL_Texture *spriteSheet;  // Texture for the sprite
    int frameCounts[6];        // Number of frames for each animation
    int animationHeights[6];   // Heights for each animation
    int animationOffsets[6];   // Y offsets for each animation
    int frameWidths[6];        // Widths for each frame
    int spriteWidth;           // Total width of the sprite sheet
    int currentAnimation;      // Current animation (walking, jumping, punching,kicking,special,stance)
    int currentFrame;          // Current frame index
    Uint32 lastFrameTime;      // Time of the last frame update
    int x, y;                  // Position on the screen
} Sprite;

typedef struct {
    SDL_Rect rect;
    int velocityX;
    bool active;
} Projectile;

const int FRAME_COUNT = 119; // Number of frames (adjust to your total frame count)

//Fonts
const char *buttonfont = "rsrc/font/OLDENGL.TTF";
const char *textfont = "rsrc/font/TIMES.TTF";

//images
const char *menuimage = "rsrc/images/Menu.jpeg";
const char *arenabackground = "rsrc/images/FightArena.JPG";
const char *Button="rsrc/images/Button.jpeg"; 
const char *optionimage="rsrc/images/Optionpage.JPG";
const char *helpbg="rsrc/images/helppage.JPG";
const char *health= "rsrc/images/healthbar.png";
const char *loader="rsrc/images/LoadingVS.JPEG";
const char *p1wins="rsrc/animation/P1WIN.PNG";
const char *p2wins="rsrc/animation/P2WIN.PNG";
const char *creditmenu="rsrc/animation/Credits/78.jpg";


//music in fight
// const char *Powertrap = "rsrc/audio/Powertrap.mp3";

//menu music
const char *music(int *num){
    if(*num==1){
        return "rsrc/sounds/Bane.wav";
    } else if(*num==2){
        return "rsrc/sounds/War.wav";
    } else if(*num==3){
        return "rsrc/sounds/Intense.wav";
    }
    return NULL;
}
//menu sounds
const char *selection="rsrc/sounds/Selection.wav";
const char *navigation="rsrc/sounds/Navigate.wav";
const char *punching="rsrc/sounds/punch.wav";
const char *kicking="rsrc/sounds/Kicking.wav";


// Function to handle movement with collision detection
void handle_movement(Player *player, const Uint8 *keystate, int leftKey, int rightKey, SDL_Rect *otherRect) {
    SDL_Rect newPosition = player->rect; // Temporary position to test movement

    if (keystate[rightKey]) {
        newPosition.x += RECT_SPEED;
    }
    if (keystate[leftKey]) {
        newPosition.x -= RECT_SPEED;
    }

    // Check boundaries
    bool withinBounds = (newPosition.x >= 0) && (newPosition.x + RECT_WIDTH <= width);

    // Check collision with the other player
    bool colliding = SDL_HasIntersection(&newPosition, otherRect);

    if (withinBounds && !colliding) {
        player->rect = newPosition;
    }
}

// Function to handle projectile movement and collision
void update_projectile(Projectile *proj, Player *opponent) {
    if (!proj->active) return;

    proj->rect.x += proj->velocityX;

    if (proj->rect.x < 0 || proj->rect.x > width) {
        proj->active = false;
        return;
    }
}

void renderProjectile(SDL_Renderer *renderer, SDL_Texture *texture, Projectile *proj, bool flipHorizontal) {
    if (proj->active) {
        SDL_RendererFlip flip = flipHorizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        SDL_RenderCopyEx(renderer, texture, NULL, &proj->rect, 0, NULL, flip);
    }
}




// Function to compute attack rectangle based on attack type and direction
SDL_Rect compute_attack_rect(Player *attacker, Player *opponent) {
    SDL_Rect attackRect = attacker->rect;

    // Expand horizontally towards the opponent
    if (attacker->rect.x < opponent->rect.x) {
        // Expand to the right
        attackRect.w += 20;
    } else {
        // Expand to the left
        attackRect.x -= 20;
        attackRect.w += 20;
    }

    return attackRect;
}




// Function to handle jump mechanism
void handle_jump(Player *player) {
    if (!player->onGround) {
        player->velocityY += GRAVITY; // Apply gravity
    }
    player->rect.y += player->velocityY;

    // Limit the jump height
    if (player->rect.y <= player->originalY - MAX_JUMP_HEIGHT) {
        player->rect.y = player->originalY - MAX_JUMP_HEIGHT;
        player->velocityY = 0;
    }

    // Reset to ground
    if (player->rect.y >= player->originalY) {
        player->rect.y = player->originalY;
        player->velocityY = 0;
        player->onGround = true;
    }
}

// Function to handle punch and kick
void handle_attack(Player *attacker, Player *opponent, int isPunch) {
    if (attacker->attackTimer > 0) {
        SDL_Rect attackRect = attacker->rect;

        // Expand attack rectangle for punch/kick
        if (isPunch) {
            attackRect.w += 30; // Punch extends width
        } else {
            attackRect.h += 10; // Kick extends height
            attackRect.y -= 10; // Adjust position upwards for kick
        }

        // Decrease attack timer
        attacker->attackTimer--;
    } else {
        // Reset attack states
        attacker->isPunching = 0;
        attacker->isKicking = 0;
    }
}

// Function to load a texture from a file
SDL_Texture *characterTexture(const char *path, SDL_Renderer *renderer) {
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) {
        SDL_Log("Unable to load image %s! SDL_Error: %s", path, SDL_GetError());
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    return texture;
}

// Function to update a sprite's animation
void updateSprite(Sprite *sprite, Uint32 currentTime) {
    if (currentTime > sprite->lastFrameTime + FRAME_DELAY) {
        sprite->lastFrameTime = currentTime;
        sprite->currentFrame = (sprite->currentFrame + 1) % sprite->frameCounts[sprite->currentAnimation];
    }
}

// Function to render a sprite
void renderSprite(Sprite *sprite, SDL_Renderer *renderer, bool flipHorizontal) {
    int FRAME_WIDTH = sprite->spriteWidth / sprite->frameCounts[sprite->currentAnimation];
    int FRAME_HEIGHT = sprite->animationHeights[sprite->currentAnimation];
    int Y_OFFSET = sprite->animationOffsets[sprite->currentAnimation];

    SDL_Rect srcRect = {
        sprite->currentFrame * FRAME_WIDTH, // X-coordinate for the frame
        Y_OFFSET,                           // Y-offset for the animation row
        FRAME_WIDTH,                        // Width of the frame
        FRAME_HEIGHT                        // Height of the frame
    };


    // Dynamic offset adjustment based on flipHorizontal
    SDL_Rect dstRect = {
        sprite->x - FRAME_WIDTH / 2 + (flipHorizontal ? -50 :0), // Offset based on orientation
        sprite->y - FRAME_HEIGHT / 2 - 20,                        // Center vertically
        FRAME_WIDTH*2,                                              // Width to draw
        FRAME_HEIGHT*2                                              // Height to draw
    };

    // Flip the sprite horizontally if needed
    SDL_RendererFlip flip = flipHorizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    SDL_RenderCopyEx(renderer, sprite->spriteSheet, &srcRect, &dstRect, 0, NULL, flip);
}




void errors(const char *errorMessage) {
    printf("%s\n", errorMessage);
    SDL_Quit();
    Mix_Quit();
    IMG_Quit();
    TTF_Quit();
    exit(1);  // Exit the program after logging the error
}

int main(int argc, char *argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        errors("SDL_Init Error: Unable to initialize SDL.");
    }

    // Create Window
    SDL_Window *window = SDL_CreateWindow("Fight Arena", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    if (!window) {
        errors("SDL_CreateWindow Error: Unable to create window.");
    }

    // Create Renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        errors("SDL_CreateRenderer Error: Unable to create renderer.");
    }

    // Initialize SDL_mixer (for audio)
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        errors("SDL_mixer Error: Unable to initialize audio.");
    }

    // Initialize SDL_image (for textures)
    if (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG != IMG_INIT_PNG) {
        errors("SDL_image Error: Unable to initialize PNG support.");
    }

        if (!(IMG_Init(IMG_INIT_JPG) & IMG_INIT_JPG)) {
        printf("IMG_Init Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Initialize SDL_ttf (for text)
    if (TTF_Init() == -1) {
        errors("SDL_ttf Error: Unable to initialize SDL_ttf.");
    }


    // Game state flags
    bool run = true, menu = true, play = false, pause = false, select = false, option = false,voice=true,help=false,credit=false,creditsvid=true,loading;
    int borderpos = height / 2 + 90;
    int borderposY = height / 2;
    int opacity,voicecount=0,helpcount=0,creditcount=0,musiccount=1,prevmusic=1;
    int menuSelect = 0;
    int player1_health,player2_health;
    
    // Variables for file path and event handling
    char frame_path[256];

    // Load textures for menu background and options
    SDL_Color textWhite = {255, 255, 255};
    SDL_Texture *menuTexture = NULL;
    SDL_Texture *ButtonTexture = NULL;
    SDL_Texture *optionTexture = NULL;
    SDL_Texture *helpTexture = NULL;
    SDL_Texture *arenaTexture = NULL;
    SDL_Texture *healthTexture=NULL;
    SDL_Texture *loadTexture=NULL;
    SDL_Texture *winner1=NULL;
    SDL_Texture *winner2=NULL;
    SDL_Texture *creditsmenu=NULL;
    SDL_Texture *Haduoken=NULL;







    // Initialize players
    Player player1 = {
        .rect = {100, GROUND_LEVEL - RECT_HEIGHT, RECT_WIDTH, RECT_HEIGHT},
        .velocityY = 0,
        .onGround = true,
        .originalY = GROUND_LEVEL - RECT_HEIGHT,
        .isPunching = false,
        .isKicking = false,
        .attackTimer = 0
    };

    Player player2 = {
        .rect = {width - 150, GROUND_LEVEL - RECT_HEIGHT, RECT_WIDTH, RECT_HEIGHT},
        .velocityY = 0,
        .onGround = true,
        .originalY = GROUND_LEVEL - RECT_HEIGHT,
        .isPunching = false,
        .isKicking = false,
        .attackTimer = 0
    };

    Projectile player1Projectile = { .rect = {0, 0, PROJECTILE_WIDTH, PROJECTILE_HEIGHT}, .velocityX = 0, .active = false };
    Projectile player2Projectile = { .rect = {0, 0, PROJECTILE_WIDTH, PROJECTILE_HEIGHT}, .velocityX = 0, .active = false };


    // Load the main menu background texture
    menuTexture = IMG_LoadTexture(renderer, menuimage);
    if (!menuTexture) {
        errors("SDL_image Error: Unable to load menu image.");
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);  // Enable transparency blend mode

    healthTexture =IMG_LoadTexture(renderer,health);
    if(!healthTexture){
        errors("SDL_image Error: Unable to load menu image.");
    }

    winner1 =IMG_LoadTexture(renderer,p1wins);
    if(!winner1){
        errors("SDL_image Error: Unable to load menu image.");
    }
    winner2 =IMG_LoadTexture(renderer,p2wins);
    if(!winner2){
        errors("SDL_image Error: Unable to load menu image.");
    }
    creditsmenu =IMG_LoadTexture(renderer,creditmenu);
    if(!creditsmenu){
        errors("SDL_image Error: Unable to load menu image.");
    }
    // Load the button texture
    ButtonTexture = IMG_LoadTexture(renderer, Button);  
    if (!ButtonTexture) {
        errors("SDL_image Error: Unable to load button image.");
    }

    // Load the option texture
    optionTexture = IMG_LoadTexture(renderer, optionimage);
    if (!optionTexture) {
        errors("SDL_image Error: Unable to load option image.");
    }
    
    
    arenaTexture = IMG_LoadTexture(renderer, arenabackground);
    if (!arenaTexture) {
        errors("SDL_image Error: Unable to load option image.");
    }

    // Load the help texture
    helpTexture = IMG_LoadTexture(renderer, helpbg);
    if (!helpTexture) {
        errors("SDL_image Error: Unable to load help image.");
    }

    //oldengl font   
    TTF_Font *menuFont = TTF_OpenFont(buttonfont, 64);
    if (!menuFont) {
        errors("TTF_OpenFont Error: Unable to open font.");
    }

    //MTCORSVA font
    TTF_Font *normalfont = TTF_OpenFont(textfont, 20);
    if (!normalfont) {
        errors("TTF_OpenFont Error: Unable to open font.");
    }

    // Load Haduoken texture
    Haduoken = IMG_LoadTexture(renderer,"rsrc/animation/haduoken.bmp");
    if (!Haduoken) {
        errors("SDL_image Error: Unable to load Haduoken image.");
    }



        // Load sprite sheets
    SDL_Texture *ryu1 = characterTexture("rsrc/animation/ryubasic.bmp", renderer);
    SDL_Texture *ken1 = characterTexture("rsrc/animation/kenbasic.bmp", renderer);
    if (!ryu1 || !ken1) {
        SDL_Log("Could not load sprite sheets.");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
        // Initialize sprites with unique properties
    Sprite sprite1 = {
        ryu1,
        {1,1,1,1,1,1},  // Frame counts for walking, jumping, punching, kicking, special, stance (number of frames)
        {60, 60, 60,60, 60,60},  // Heights for each animation
        {1, 160, 50, 110, 208,265},  // Y offsets for each animation
        {55,55,55,55,55,55},  //width for each frame
        86,  // Sprite width
        STANCE,
        0,
        0,
        width / 2,
        height / 2
    };
        Sprite sprite2 = {
        ken1,
        {1,1,1,1,1,1},  // Frame counts for walking, jumping, punching, kicking, special, stance (number of frames)
        {65, 60, 65,65, 65,65},  // Heights for each animation
        {1, 175, 60, 120, 240,300},  // Y offsets for each animation
        {55,55,55,55,55,55},  //width for each frame
        85,  // Sprite width
        STANCE,
        0,
        0.1,
        width / 2,
        height / 2
    };
    Uint32 currentTime;




    // **Audio Setup**
    // Load background music (wav file)
    Mix_Music *bgMusic = Mix_LoadMUS(music(&musiccount)); // Use your audio file path here
    if (!bgMusic) {
        errors("Mix_LoadMUS Error: Unable to load music.");
    }
    // Load sound effects
    Mix_Chunk *sfxselect = Mix_LoadWAV(selection);
    Mix_Chunk *sfxnavigate = Mix_LoadWAV(navigation);
    Mix_Chunk *punch = Mix_LoadWAV(punching);
    Mix_Chunk *kick = Mix_LoadWAV(kicking);
    
    // Play the background music in a loop (-1 means infinite loop)
    if(voice){
    Mix_PlayMusic(bgMusic, -1);  // Start the music immediately and loop indefinitely
    }
    // Main Game Loop
    while (run) {
        if(creditsvid){
            // Loop through frames
            for (int i = 1; i <= FRAME_COUNT && run && creditsvid; i++) {
                // Generate the file path dynamically
                snprintf(frame_path, sizeof(frame_path), "rsrc/animation/Credits/%d.jpg", i);

                // Load the image
                SDL_Surface *image = IMG_Load(frame_path);
                if (!image) {
                    printf("IMG_Load Error: %s\n", IMG_GetError());
                    continue; // Skip to the next frame if there's an error
                }

                // Create a texture from the surface
                SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image);
                SDL_FreeSurface(image); // Free the surface after creating the texture
                if (!texture) {
                    printf("SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
                    continue; // Skip to the next frame if there's an error
                }

                // Render the texture (example rendering)
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);

                // Free the texture after rendering
                SDL_DestroyTexture(texture);
                // Add a delay for smooth animation (adjust as needed)
                SDL_Delay(25); // ~30 FPS
            }
        }
        creditsvid=false;



        if(play){
            SDL_Event mech;
            const Uint8 *keystate =SDL_GetKeyboardState(NULL);
            while (SDL_PollEvent(&mech)) {
                if (mech.type == SDL_QUIT) {
                    run = 0;
                } else if(mech.type == SDL_KEYDOWN) {
                    if (keystate[SDL_SCANCODE_SPACE]) {
                        menu=true;
                        play=false;
                        pause=false;
                    }
                } else if (mech.type == SDL_KEYUP) { 
                    sprite1.currentAnimation = STANCE; 
                    sprite2.currentAnimation = STANCE;
                
                }
            // Handle jumping
            if (keystate[SDL_SCANCODE_W] && player1.onGround) {
                player1.velocityY = JUMP_FORCE;
                player1.onGround = false;
                sprite1.currentAnimation = JUMPING;
            }

            if (keystate[SDL_SCANCODE_I] && player2.onGround) {
                player2.velocityY = JUMP_FORCE;
                player2.onGround = false;
                sprite2.currentAnimation = JUMPING;
            }

            // Handle attack inputs for Player 1
            if (keystate[SDL_SCANCODE_E] && player1.attackTimer == 0) {
                player1.isPunching = true;
                player1.attackTimer = ATTACK_DURATION;
                sprite1.currentAnimation = PUNCHNG;
            }
            if (keystate[SDL_SCANCODE_Q] && player1.attackTimer == 0) {
                player1.isKicking = true;
                player1.attackTimer = ATTACK_DURATION;
                sprite1.currentAnimation = KICKING;
            }
            // Handle attack inputs for Player 2
            if (keystate[SDL_SCANCODE_O] && player2.attackTimer == 0) {
                player2.isPunching = true;
                player2.attackTimer = ATTACK_DURATION;
                sprite2.currentAnimation = KICKING;
            }
            if (keystate[SDL_SCANCODE_U] && player2.attackTimer == 0) {
                player2.isKicking = true;
                player2.attackTimer = ATTACK_DURATION;
                sprite2.currentAnimation = PUNCHNG;
            }
            if(keystate[SDL_SCANCODE_J] || keystate[SDL_SCANCODE_L]){
                sprite2.currentAnimation = WALKING;
            }
            if(keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_D]){
                sprite1.currentAnimation = WALKING;
            }
            // Handle special moves
            if (keystate[SDL_SCANCODE_S] && !player1Projectile.active) {
                player1Projectile.rect.x = player1.rect.x + (player1.rect.w / 2);
                player1Projectile.rect.y = player1.rect.y + (player1.rect.h / 2);
                player1Projectile.velocityX = (player1.rect.x < player2.rect.x) ? PROJECTILE_SPEED : -PROJECTILE_SPEED;
                player1Projectile.active = true;
                sprite1.currentAnimation = SPECIAL;
            }

            if (keystate[SDL_SCANCODE_K] && !player2Projectile.active) {
                player2Projectile.rect.x = player2.rect.x + (player2.rect.w / 2);
                player2Projectile.rect.y = player2.rect.y + (player2.rect.h / 2);
                player2Projectile.velocityX = (player2.rect.x < player1.rect.x) ? PROJECTILE_SPEED : -PROJECTILE_SPEED;
                player2Projectile.active = true;
                sprite2.currentAnimation = SPECIAL;
            }
        }
            
                
        }


        SDL_Event event;
        // Event handling
        if(!play){
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                run = false;  // Exit the game
            }

            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    if (option && !help && !credit) {
                        option = false;
                        voicecount = 0;
                        borderpos = height / 2 + 150;
                    } else if (help && helpcount!=0) {  
                        helpcount = 0; 
                        help = false;
                    } else if (credit) {
                        credit = false;
                    }
                }

                if (event.key.keysym.sym == SDLK_RETURN) {
                    Mix_PlayChannel(-1, sfxselect, 0);
                    // Handle menu selection on Enter key press
                    if (menu) {
                        
                        if (borderpos == height / 2 + 90) {
                            menu = false;
                            play = true;
                        } else if (borderpos == height / 2 + 150) {
                            option = true;
                            
                        } else if (borderpos == height / 2 + 210) {
                            menu = false;
                            run = false;
                        }
                    }
                    if(option){
                        voicecount++;
                        helpcount++;
                        creditcount++;
                        if(borderposY==height /2-90){
                            if (voice == true && voicecount > 1) {
                            Mix_PauseMusic();
                            voice = false;
                        } else if (voicecount > 1 && voice == false) {
                            // If music was off, resume or play the new music
                            if (musiccount == prevmusic) {
                                Mix_ResumeMusic();
                            } else {
                                // Play new music
                                Mix_FreeMusic(bgMusic);  // Free previous music
                                bgMusic = Mix_LoadMUS(music(&musiccount));  // Load new music
                                if (!bgMusic) {
                                    errors("Mix_LoadMUS Error: Unable to load new music.");
                                }
                                Mix_PlayMusic(bgMusic, -1);  // Play new music
                            }
                            voice = true;
                        }
                        }
                        //help button toggle
                        if(borderposY==height/2+90){
                            if(helpcount>1){
                                help=true;
                            }
                        }
                        
                        
                        //credit button toggle
                        else if(borderposY==height/2+180){
                            if(creditcount>1){
                                credit=true;
                            }
                        }





                    }
                }

                // Selection movement
                if (menu == true && option == false) {
                    if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
                    Mix_PlayChannel(-1, sfxnavigate, 0);                    
                        if (borderpos > height / 2 + 90) {
                            borderpos -= 60;
                        }
                    } else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s){
                    Mix_PlayChannel(-1, sfxnavigate, 0);                    
                        if (borderpos < height / 2 + 210) {
                            borderpos += 60;
                            
                        }
                    }

                //option page
                } else if(!(menu == true && option == false)){
                    if(event.key.keysym.sym == SDLK_UP){
                        Mix_PlayChannel(-1, sfxnavigate, 0);
                        if (borderposY > height / 2-90) {
                            borderposY -= 90;
                        }



                    } else if(event.key.keysym.sym == SDLK_DOWN){
                        Mix_PlayChannel(-1, sfxnavigate, 0);
                        if (borderposY < height / 2+180) {
                            borderposY += 90;
                        }

                    }
                    if(borderposY==height/2){
                        if(event.key.keysym.sym == SDLK_LEFT){
                        Mix_PlayChannel(-1, sfxnavigate, 0);
                            if(musiccount>1){
                                prevmusic=musiccount;
                                musiccount--;
                            }
                            


                        } else if(event.key.keysym.sym == SDLK_RIGHT){
                        Mix_PlayChannel(-1,sfxnavigate, 0);
                            if(musiccount<3){
                                prevmusic=musiccount;
                                musiccount++;
                            }
                            


                        }
                    }
                    if(musiccount!=prevmusic){
                        prevmusic=musiccount;
                        // Change the music dynamically
                        Mix_FreeMusic(bgMusic);  // Free the previous music
                        bgMusic = Mix_LoadMUS(music(&musiccount));  // Load new music based on musiccount
                        if (!bgMusic) {
                            errors("Mix_LoadMUS Error: Unable to load new music.");
                        }
                        if(voice) {
                            Mix_PlayMusic(bgMusic, -1);
                        }
                        
                    }
                }
            }
        }
        }
        // Rendering Logic
        if (menu) {
            loading=true;
            player1_health=375;
            player2_health=375;
            player1.rect.x=100;
            player2.rect.x=width - 150;

            // Render the menu screen
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, menuTexture, NULL, NULL);

            // Render menu buttons with text

            // Set opacity based on selection
            if (borderpos == height / 2 + 90) {
                opacity = 0;
            } else {
                opacity = 100;
            }

            SDL_Rect playRect = {width / 2 - 100, height / 2 + 90, 200, 50};
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, opacity);
            SDL_RenderCopy(renderer, ButtonTexture, NULL, &playRect);
            SDL_RenderFillRect(renderer, &playRect);

            if (borderpos == height / 2 + 150) {
                opacity = 0;
            } else {
                opacity = 100;
            }
            SDL_Rect optionRect = {width / 2 - 100, height / 2 + 150, 200, 50};
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, opacity);
            SDL_RenderCopy(renderer, ButtonTexture, NULL, &optionRect);
            SDL_RenderFillRect(renderer, &optionRect);

            if (borderpos == height / 2 + 210) {
                opacity = 0;
            } else {
                opacity = 100;
            }
            SDL_Rect quitRect = {width / 2 - 100, height / 2 + 210, 200, 50}; 
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, opacity);
            SDL_RenderCopy(renderer, ButtonTexture, NULL, &quitRect);
            SDL_RenderFillRect(renderer, &quitRect);

            //Menu Controls
            SDL_Surface *navigateText = TTF_RenderText_Solid(normalfont, "UP/DOWN = NAVIGATE", textWhite);
            SDL_Texture *navigateTextTexture = SDL_CreateTextureFromSurface(renderer, navigateText);
            SDL_Rect navigateTextRect = {0, 0, navigateText->w-20, navigateText->h};
            SDL_RenderCopy(renderer, navigateTextTexture, NULL, &navigateTextRect);
            SDL_FreeSurface(navigateText);
            SDL_DestroyTexture(navigateTextTexture);

            SDL_Surface *selectText = TTF_RenderText_Solid(normalfont, "ENTER = SELECT", textWhite);
            SDL_Texture *selectTextTexture = SDL_CreateTextureFromSurface(renderer, selectText);
            SDL_Rect selectTextRect = {0,20, selectText->w-20, selectText->h};
            SDL_RenderCopy(renderer, selectTextTexture, NULL, &selectTextRect);
            SDL_FreeSurface(selectText);
            SDL_DestroyTexture(selectTextTexture);

            // Render the text for the buttons (the same code you had previously)
            SDL_Surface *playText = TTF_RenderText_Solid(menuFont, "Play", textWhite);
            SDL_Texture *playTextTexture = SDL_CreateTextureFromSurface(renderer, playText);
            SDL_Rect playTextRect = {width / 2 - 55, height / 2 + 90, playText->w-20, playText->h-20};
            SDL_RenderCopy(renderer, playTextTexture, NULL, &playTextRect);
            SDL_FreeSurface(playText);
            SDL_DestroyTexture(playTextTexture);

            SDL_Surface *optionText = TTF_RenderText_Solid(menuFont, "Option", textWhite);
            SDL_Texture *optionTextTexture = SDL_CreateTextureFromSurface(renderer, optionText);
            SDL_Rect optionTextRect = {width / 2 - 80, height / 2 + 150, optionText->w-20, optionText->h-20};
            SDL_RenderCopy(renderer, optionTextTexture, NULL, &optionTextRect);
            SDL_FreeSurface(optionText);
            SDL_DestroyTexture(optionTextTexture);

            SDL_Surface *quitText = TTF_RenderText_Solid(menuFont, "Quit", textWhite);
            SDL_Texture *quitTextTexture = SDL_CreateTextureFromSurface(renderer, quitText);
            SDL_Rect quitTextRect = {width / 2 - 55, height / 2 + 210, quitText->w-20, quitText->h-20};
            SDL_RenderCopy(renderer, quitTextTexture, NULL, &quitTextRect);
            SDL_FreeSurface(quitText);
            SDL_DestroyTexture(quitTextTexture);

            // Draw the border around the selected menu item
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
            SDL_Rect borderRect = {width / 2 - 100, borderpos, 200, 50};
            SDL_RenderDrawRect(renderer, &borderRect);



            //option
            if(option){
                opacity=255;
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, optionTexture, NULL, NULL);
                
                 // Set opacity based on selection
                if (borderposY==height/2-90) {
                    opacity = 0;
                } else {
                    opacity = 100;
                }

                SDL_Rect soundRect = {width / 2-130, height / 2-90 , 250, 50};
                SDL_SetRenderDrawColor(renderer, 128, 128, 128, opacity);
                SDL_RenderCopy(renderer, ButtonTexture, NULL, &soundRect);
                SDL_RenderFillRect(renderer, &soundRect);

                if (borderposY==height/2) {
                    opacity = 0;
                } else {
                    opacity = 100;
                }
                SDL_Rect musicRect = {width / 2-205, height / 2, 400, 50};
                SDL_SetRenderDrawColor(renderer, 128, 128, 128, opacity);
                SDL_RenderCopy(renderer, ButtonTexture, NULL, &musicRect);
                SDL_RenderFillRect(renderer, &musicRect);

                if (borderposY==height/2+90) {
                    opacity = 0;
                } else {
                    opacity = 100;
                }
                SDL_Rect helpRect = {width / 2-85, height / 2+90, 150, 50}; 
                SDL_SetRenderDrawColor(renderer, 128, 128, 128, opacity);
                SDL_RenderCopy(renderer, ButtonTexture, NULL, &helpRect);
                SDL_RenderFillRect(renderer, &helpRect);
        
                if (borderposY==height/2+180) {
                    opacity = 0;
                } else {
                    opacity = 100;
                }
                SDL_Rect creditRect = {width /2-105, height / 2+180, 190, 50}; 
                SDL_SetRenderDrawColor(renderer, 128, 128, 128, opacity);
                SDL_RenderCopy(renderer, ButtonTexture, NULL, &creditRect);
                SDL_RenderFillRect(renderer, &creditRect);


                SDL_Surface *navigateText = TTF_RenderText_Solid(normalfont, "UP/DOWN = NAVIGATE", textWhite);
                SDL_Texture *navigateTextTexture = SDL_CreateTextureFromSurface(renderer, navigateText);
                SDL_Rect navigateTextRect = {0, 0, navigateText->w-20, navigateText->h};
                SDL_RenderCopy(renderer, navigateTextTexture, NULL, &navigateTextRect);
                SDL_FreeSurface(navigateText);
                SDL_DestroyTexture(navigateTextTexture);

                SDL_Surface *soundchangeText = TTF_RenderText_Solid(normalfont, "LEFT/RIGHT = CHANGE MUSIC", textWhite);
                SDL_Texture *soundchangeTextTexture = SDL_CreateTextureFromSurface(renderer, soundchangeText);
                SDL_Rect soundchangeTextRect = {0, 20, soundchangeText->w-18, soundchangeText->h};
                SDL_RenderCopy(renderer, soundchangeTextTexture, NULL, &soundchangeTextRect);
                SDL_FreeSurface(soundchangeText);
                SDL_DestroyTexture(soundchangeTextTexture);


                SDL_Surface *selectText = TTF_RenderText_Solid(normalfont, "ENTER = SELECT", textWhite);
                SDL_Texture *selectTextTexture = SDL_CreateTextureFromSurface(renderer, selectText);
                SDL_Rect selectTextRect = {0,40, selectText->w-20, selectText->h};
                SDL_RenderCopy(renderer, selectTextTexture, NULL, &selectTextRect);
                SDL_FreeSurface(selectText);
                SDL_DestroyTexture(selectTextTexture);

                SDL_Surface *backText = TTF_RenderText_Solid(normalfont, "ESC = BACK", textWhite);
                SDL_Texture *backTextTexture = SDL_CreateTextureFromSurface(renderer, backText);
                SDL_Rect backTextRect = {0,60, backText->w-20, backText->h};
                SDL_RenderCopy(renderer, backTextTexture, NULL, &backTextRect);
                SDL_FreeSurface(backText);
                SDL_DestroyTexture(backTextTexture);



                SDL_Surface *soundText = TTF_RenderText_Solid(menuFont, "Music:", textWhite);
                SDL_Texture *soundTextTexture = SDL_CreateTextureFromSurface(renderer, soundText);
                SDL_Rect soundTextRect = {width / 2-120, height /2-85, soundText->w-20, soundText->h-35};
                SDL_RenderCopy(renderer, soundTextTexture, NULL, &soundTextRect);
                SDL_FreeSurface(soundText);
                SDL_DestroyTexture(soundTextTexture);
                if(voice){
                //sound on text
                    SDL_Surface *soundText = TTF_RenderText_Solid(menuFont, "ON", textWhite);
                    SDL_Texture *soundTextTexture = SDL_CreateTextureFromSurface(renderer, soundText);
                    SDL_Rect soundTextRect = {width / 2+55, height /2-85, soundText->w-70, soundText->h-35};
                    SDL_RenderCopy(renderer, soundTextTexture, NULL, &soundTextRect);
                    SDL_FreeSurface(soundText);
                    SDL_DestroyTexture(soundTextTexture);
                } else {
                //sound off text
                    SDL_Surface *soundText = TTF_RenderText_Solid(menuFont, "OFF", textWhite);
                    SDL_Texture *soundTextTexture = SDL_CreateTextureFromSurface(renderer, soundText);
                    SDL_Rect soundTextRect = {width / 2+53, height /2-85, soundText->w-100, soundText->h-35};
                    SDL_RenderCopy(renderer, soundTextTexture, NULL, &soundTextRect);
                    SDL_FreeSurface(soundText);
                    SDL_DestroyTexture(soundTextTexture);
                }
                if(musiccount==1){
                    SDL_Surface *changeText = TTF_RenderText_Solid(menuFont, "Music:Bane", textWhite);
                    SDL_Texture *changeTextTexture = SDL_CreateTextureFromSurface(renderer, changeText);
                    SDL_Rect changeTextRect = {width / 2-160, height /2+5, changeText->w-20, changeText->h-35};
                    SDL_RenderCopy(renderer,changeTextTexture, NULL, &changeTextRect);
                    SDL_FreeSurface(changeText);
                    SDL_DestroyTexture(changeTextTexture);
                } else if(musiccount==2){
                    SDL_Surface *changeText = TTF_RenderText_Solid(menuFont, "Music:War", textWhite);
                    SDL_Texture *changeTextTexture = SDL_CreateTextureFromSurface(renderer, changeText);
                    SDL_Rect changeTextRect = {width / 2-145, height /2+5, changeText->w-20, changeText->h-35};
                    SDL_RenderCopy(renderer,changeTextTexture, NULL, &changeTextRect);
                    SDL_FreeSurface(changeText);
                    SDL_DestroyTexture(changeTextTexture);  
                } else if(musiccount==3){
                    SDL_Surface *changeText = TTF_RenderText_Solid(menuFont, "Music:Intense", textWhite);
                    SDL_Texture *changeTextTexture = SDL_CreateTextureFromSurface(renderer, changeText);
                    SDL_Rect changeTextRect = {width / 2-185, height /2+5, changeText->w-20, changeText->h-35};
                    SDL_RenderCopy(renderer,changeTextTexture, NULL, &changeTextRect);
                    SDL_FreeSurface(changeText);
                    SDL_DestroyTexture(changeTextTexture);  
                    
                }
                
                SDL_Surface *helpText = TTF_RenderText_Solid(menuFont, "Help", textWhite);
                SDL_Texture *helpTextTexture = SDL_CreateTextureFromSurface(renderer, helpText);
                SDL_Rect helpTextRect = {width / 2-75, height /2+95, helpText->w, helpText->h-35};
                SDL_RenderCopy(renderer,helpTextTexture, NULL, &helpTextRect);
                SDL_FreeSurface(helpText);
                SDL_DestroyTexture(helpTextTexture);
                
                
                
                SDL_Surface *creditText = TTF_RenderText_Solid(menuFont, "Credits", textWhite);
                SDL_Texture *creditTextTexture = SDL_CreateTextureFromSurface(renderer, creditText);
                SDL_Rect creditTextRect = {width / 2-95, height /2+185, creditText->w-20, creditText->h-35};
                SDL_RenderCopy(renderer,creditTextTexture, NULL, &creditTextRect);
                SDL_FreeSurface(creditText);
                SDL_DestroyTexture(creditTextTexture);
                
                // Border Around Selected Buttons
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
                SDL_Rect borderRect = {width/2-200, borderposY, 300, 50};
                SDL_RenderDrawRect(renderer, &borderRect);
                
                
                if(help){
                    SDL_RenderClear(renderer); 
                    SDL_RenderCopy(renderer, helpTexture, NULL, NULL);
                    SDL_Surface *backText = TTF_RenderText_Solid(normalfont, "ESC = BACK", textWhite);
                    SDL_Texture *backTextTexture = SDL_CreateTextureFromSurface(renderer, backText);
                    SDL_Rect backTextRect = {0,0, backText->w-20, backText->h};
                    SDL_RenderCopy(renderer, backTextTexture, NULL, &backTextRect);
                    SDL_FreeSurface(backText);
                    SDL_DestroyTexture(backTextTexture);



                    SDL_RenderPresent(renderer); // Present the rendered frame to the screen
                   

                }
                if(credit){
                    //credit on text
                    SDL_RenderClear(renderer);  // Clear screen to fill with credit content_
                    SDL_RenderCopy(renderer,creditsmenu, NULL, NULL);  // Full-screen credit image
                    SDL_Surface *creditstext = TTF_RenderText_Solid(normalfont,"CREDITS",textWhite);
                    SDL_Rect creditsTextRect = {width / 2-95, height /2-100, creditstext->w-20, creditstext->h-35};
                    SDL_Texture *creditsTextTexture= SDL_CreateTextureFromSurface(renderer,creditstext);
                    SDL_RenderCopy(renderer,creditsTextTexture,NULL,&creditsTextRect);
                    SDL_FreeSurface(creditstext);
                    SDL_DestroyTexture(creditsTextTexture);
                    SDL_Surface *backText = TTF_RenderText_Solid(normalfont, "ESC = BACK", textWhite);
                    SDL_Texture *backTextTexture = SDL_CreateTextureFromSurface(renderer, backText);
                    SDL_Rect backTextRect = {0,0, backText->w-20, backText->h};
                    SDL_RenderCopy(renderer, backTextTexture, NULL, &backTextRect);
                    SDL_FreeSurface(backText);
                    SDL_DestroyTexture(backTextTexture);
                    SDL_RenderPresent(renderer);
                }





            }
        SDL_RenderPresent(renderer);

        bool face=true;

        } else if (play) {

            if(loading){
                
                loadTexture =IMG_LoadTexture(renderer,loader);
                if(!loadTexture){
                    errors("SDL_image Error: Unable to load menu image.");
                }
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, loadTexture, NULL, NULL);
                SDL_RenderPresent(renderer);
                SDL_Delay(3000);
                loading=false;
                SDL_DestroyTexture(loadTexture);
            }

            // Render the arena (gameplay)
            SDL_RenderClear(renderer);
            SDL_Rect arenaRect = {0, 0, width, height};
            SDL_RenderCopy(renderer, arenaTexture, NULL, &arenaRect);



 


            if(!pause){
                const Uint8 *keystate = SDL_GetKeyboardState(NULL);
                // Handle movement and collision for both players
                handle_movement(&player2, keystate, SDL_SCANCODE_J, SDL_SCANCODE_L, &player1.rect);
                handle_movement(&player1, keystate, SDL_SCANCODE_A, SDL_SCANCODE_D, &player2.rect);
                
                // Handle jump for both players
                handle_jump(&player1);
                handle_jump(&player2);

                // Handle attacks and collision detection
                if (player1.isPunching || player1.isKicking) {
                    SDL_Rect attackRect1 = compute_attack_rect(&player1, &player2);
                    if (SDL_HasIntersection(&attackRect1, &player2.rect)) {
                        if (player1.isPunching) {
                            Mix_PlayChannel(1, punch, 0);
                            player2_health -= 1;
                        } else if (player1.isKicking) {
                            Mix_PlayChannel(1, kick, 0);
                            player2_health -= 0.5;
                        }

                        // Clamp health to a minimum of 0
                        if (player2_health < 0) {
                            player2_health = 0;
                        }
                    }
                }
                if(player2_health==0){
                    play=false;
                    menu=true;
                    SDL_RenderCopy(renderer,winner1,NULL,NULL);
                    SDL_RenderPresent(renderer);
                    SDL_Delay(5000);
                }
                if (player2.isPunching || player2.isKicking) {
                    SDL_Rect attackRect2 = compute_attack_rect(&player2, &player1);
                    if (SDL_HasIntersection(&attackRect2, &player1.rect)) {
                        if (player2.isPunching) {
                            player1_health -= 1;
                        } else if (player2.isKicking) {
                            player1_health -= 0.5;
                        }

                        // Clamp health to a minimum of 0
                        if (player1_health < 0) {
                            player1_health = 0;
                        }
                    }
                } 
                if (player1Projectile.active && SDL_HasIntersection(&player1Projectile.rect, &player2.rect)) {
                    player2_health -= 5; // Only hits the opponent
                    player1Projectile.active = false;
                }

                if (player2Projectile.active && SDL_HasIntersection(&player2Projectile.rect, &player1.rect)) {
                    player1_health -= 5; // Only hits the opponent
                    player2Projectile.active = false;
                }

                if(player1_health==0){
                    play=false;
                    menu=true;
                    SDL_RenderCopy(renderer,winner2,NULL,NULL);
                    SDL_RenderPresent(renderer);
                    SDL_Delay(5000);
                }


       }

            // Decrease attack timers
        if (player1.attackTimer > 0) {
            player1.attackTimer--;
            if (player1.attackTimer == 0) {
                player1.isPunching = false;
                player1.isKicking = false;
            }
        }

        if (player2.attackTimer > 0) {
            player2.attackTimer--;
            if (player2.attackTimer == 0) {
                player2.isPunching = false;
                player2.isKicking = false;
            }
        }

        player1Projectile.rect.w = PROJECTILE_WIDTH; // Set width
        player1Projectile.rect.h = PROJECTILE_HEIGHT; // Set height


        //Update sprite positions based on player rects
        sprite1.x = player1.rect.x + player1.rect.w / 2;
        sprite1.y = player1.rect.y + player1.rect.h / 2;

        sprite2.x = player2.rect.x + player2.rect.w / 2;
        sprite2.y = player2.rect.y + player2.rect.h / 2;

        // Render player rectangles
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 0); // Red for Player 1
        SDL_RenderFillRect(renderer, &player1.rect);

        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 0); // Blue for Player 2
        SDL_RenderFillRect(renderer, &player2.rect);

        if(player1.rect.x<player2.rect.x){
        renderSprite(&sprite1, renderer, false); // Render sprite1
        renderSprite(&sprite2, renderer, true); // Render sprite2
        }
        else{
        renderSprite(&sprite1, renderer, true); // Render sprite1
        renderSprite(&sprite2, renderer, false); // Render sprite2
            
        }


        // Render Player 1's attack
        if (player1.isPunching || player1.isKicking) {
            SDL_Rect attackRect1 = compute_attack_rect(&player1, &player2);
            SDL_SetRenderDrawColor(renderer, 255, 165, 0, 0); // Orange for attack
            SDL_RenderFillRect(renderer, &attackRect1);
        }

        // Render Player 2's attack
        if (player2.isPunching || player2.isKicking) {
            SDL_Rect attackRect2 = compute_attack_rect(&player2, &player1);
            SDL_SetRenderDrawColor(renderer, 0, 255, 255, 0); // Cyan for attack
            SDL_RenderFillRect(renderer, &attackRect2);
        }
        // Render projectiles
        if (player1Projectile.active) {
            bool flip = (player1Projectile.velocityX < 0); // Flip if moving left
            renderProjectile(renderer, Haduoken, &player1Projectile, flip);
        }
        if (player2Projectile.active) {
            bool flip = (player2Projectile.velocityX < 0); // Flip if moving left
            renderProjectile(renderer, Haduoken, &player2Projectile, flip);
        }


           



            // Update and render sprites
            currentTime = SDL_GetTicks();
            updateSprite(&sprite1, currentTime);
            updateSprite(&sprite2, currentTime);


            // Update projectiles
            update_projectile(&player1Projectile, &player2);
            update_projectile(&player2Projectile, &player1);


            
            SDL_Rect health1={140,80,player1_health,20};
            SDL_SetRenderDrawColor(renderer,255,0,0,255);
            SDL_RenderFillRect(renderer,&health1);
            SDL_RenderDrawRect(renderer,&health1);
            
            int health2_center = 800;
            int health2_width = player2_health; // Health bar width based on current health
            SDL_Rect health2 = {1070 - player2_health, 80, player2_health, 20};
            SDL_SetRenderDrawColor(renderer,255,0,0,255);
            SDL_RenderFillRect(renderer,&health2);
            SDL_RenderDrawRect(renderer,&health2);

            SDL_Rect healthRect_p1={100,0,450,175};
            SDL_RenderCopy(renderer,healthTexture,NULL,&healthRect_p1);
            SDL_Rect healthRect_p2={650,0,450,175};
            SDL_RenderCopy(renderer,healthTexture,NULL,&healthRect_p2);

            SDL_Surface *player1Text = TTF_RenderText_Solid(normalfont, "PLAYER 1", textWhite);
            SDL_Texture *player1TextTexture = SDL_CreateTextureFromSurface(renderer, player1Text);
            SDL_Rect player1TextRect = {137,0, player1Text->w+100, player1Text->h+50};
            SDL_RenderCopy(renderer, player1TextTexture, NULL, &player1TextRect);
            SDL_FreeSurface(player1Text);
            SDL_DestroyTexture(player1TextTexture);

            SDL_Surface *player2Text = TTF_RenderText_Solid(normalfont, "PLAYER 2", textWhite);
            SDL_Texture *player2TextTexture = SDL_CreateTextureFromSurface(renderer, player2Text);
            SDL_Rect player2TextRect = {687,0, player2Text->w+100, player2Text->h+50};
            SDL_RenderCopy(renderer, player2TextTexture, NULL, &player2TextRect);
            SDL_FreeSurface(player2Text);
            SDL_DestroyTexture(player2TextTexture);

        }

        SDL_RenderPresent(renderer); //render everything 

        SDL_Delay(16); //60 FPS 
    }

    // Cleanup
    Mix_FreeMusic(bgMusic);
    Mix_FreeChunk(sfxnavigate);
    Mix_FreeChunk(sfxselect);
    SDL_DestroyTexture(loadTexture);
    SDL_DestroyTexture(menuTexture);
    SDL_DestroyTexture(optionTexture);
    SDL_DestroyTexture(helpTexture);
    SDL_DestroyTexture(ryu1);
    SDL_DestroyTexture(ken1);
    SDL_DestroyTexture(healthTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(menuFont); 
    TTF_Quit();
    Mix_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
