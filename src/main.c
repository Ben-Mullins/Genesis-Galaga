#include <genesis.h>
#include <resources.h>

#define LEFT_EDGE 0
#define RIGHT_EDGE 320
#define ANIM_STRAIGHT 0
#define ANIM_MOVE 1
#define BOTTOM_EDGE 224
#define MAX_BULLETS 6
#define SHOT_INTERVAL 120
#define MAX_PLAYER_BULLETS 3
#define SFX_LASER 64 // 0-63 is reserved for music, 64+ is for sound effects

u16 shotByPlayer = 0;

typedef struct {
    int x;
    int y;
    int w; // Width of sprite
    int h; // Height of sprite
    int velx;
    int vely;
    int health;
    Sprite* sprite;
    char name[6]; // Not actually used, but good for debug
} Entity;

#define MAX_ENEMIES 6
Entity enemies[MAX_ENEMIES];
u16 enemiesLeft = 0;

Entity bullets[MAX_BULLETS];
u16 bulletsOnScreen = 0;

Entity player = {0, 0, 16, 16, 0, 0, 0, NULL, "PLAYER" };

u16 shotTicker = 0;

// HUD
int score = 0;
char hud_string[40] = "";

void updateScoreDisplay(){
    sprintf(hud_string,"SCORE: %d - LEFT: %d",score,enemiesLeft);
    VDP_clearText(0,0,40);
    VDP_drawText(hud_string,0,0);
}

void killEntity(Entity* e){
	e->health = 0;
	SPR_setVisibility(e->sprite,HIDDEN);
}

void reviveEntity(Entity* e){
	e->health = 1;
	SPR_setVisibility(e->sprite,VISIBLE);
}

void positionEnemies(){
    shotTicker++;
    u16 i = 0;
    for(i = 0; i < MAX_ENEMIES; i++){
        Entity* e = &enemies[i];
        if(e->health > 0){
            if( (e->x+e->w) > RIGHT_EDGE){
                e->velx = -1;
            }
            else if(e->x < LEFT_EDGE){
                e->velx = 1;
            }
            e->x += e->velx;
            SPR_setPosition(e->sprite,e->x,e->y);

            /*Shooting*/
            if(shotTicker >= SHOT_INTERVAL){
                if( (random() % (10-1+1)+1) > 4 ){
                    shootBullet(*e);
                    shotTicker = 0;
                }
            }
        }
    }
}

void positionPlayer(){
    /*Add the player's velocity to its position*/
    player.x += player.velx;

    /*Keep the player within the bounds of the screen*/
    if(player.x < LEFT_EDGE) player.x = LEFT_EDGE;
    if(player.x + player.w > RIGHT_EDGE) player.x = RIGHT_EDGE - player.w;

    /*Let the Sprite engine position the sprite*/
    SPR_setPosition(player.sprite,player.x,player.y);
}

void shootBullet(Entity Shooter){
    bool fromPlayer = (Shooter.y > 100);

    
    if( bulletsOnScreen < MAX_BULLETS ){
        if(fromPlayer == TRUE){
            if(shotByPlayer >= MAX_PLAYER_BULLETS){
                return;
            }
        }
        Entity* b;
        u16 i = 0;
        for(i=0; i<MAX_BULLETS; i++){
            b = &bullets[i];
            if(b->health == 0){

                b->x = Shooter.x+4;
                b->y = Shooter.y;

                reviveEntity(b);
                if(fromPlayer == TRUE){
                    shotByPlayer++;
                    b->vely = -3;
                } else{
                    b->vely = 3;
                }

                SPR_setPosition(b->sprite,b->x,b->y);
                XGM_startPlayPCM(SFX_LASER,1,SOUND_PCM_CH2);
                bulletsOnScreen++;
                break;
            }
        }	
    }
}

void positionBullets(){
    u16 i = 0;
    Entity *b;
    for(i = 0; i < MAX_BULLETS; i++){
        b = &bullets[i];
        if(b->health > 0){
            b->y += b->vely;
            if (b->y + b->h < 0)
            {
                killEntity(b);
                bulletsOnScreen--;
                shotByPlayer--;
            } else if(b->y > BOTTOM_EDGE){
                killEntity(b);
                bulletsOnScreen--;
            } else{
                SPR_setPosition(b->sprite,b->x,b->y);
            }
        }
    }
}

void myJoyHandler( u16 joy, u16 changed, u16 state)
{
    if (state & BUTTON_RIGHT)
    {
        player.velx = 2;
        SPR_setAnim(player.sprite,ANIM_MOVE);
        SPR_setHFlip(player.sprite,TRUE);
    }
    else if (state & BUTTON_LEFT)
    {
        player.velx = -2;
        SPR_setAnim(player.sprite,ANIM_MOVE);
        SPR_setHFlip(player.sprite,FALSE);
    }
    else{
        if( (changed & BUTTON_RIGHT) | (changed & BUTTON_LEFT) ){
            player.velx = 0;
            SPR_setAnim(player.sprite,ANIM_STRAIGHT);
        }
    }

    // Needs to be separate because they could be moving and shooting
    if (state & BUTTON_B & changed)
    {
        shootBullet(player);
    }
}

int collideEntities(Entity* a, Entity* b)
{
    return (a->x < b->x + b->w && a->x + a->w > b->x && a->y < b->y + b->h && a->y + a->h >= b->y); // AABB algorithm for collisions
}

void handleCollisions() {
    Entity* b; // Bullet
    Entity* e; // Enemy
    int i = 0;
    int j = 0;
    for(i = 0; i < MAX_BULLETS; i++){
        b = &bullets[i];
        if(b->health > 0){
            if(b->vely < 0){
                for(j = 0; j < MAX_ENEMIES; j++){
                    e = &enemies[j];
                    if (e->health > 0) {
                        if(collideEntities( b, e )){
                            killEntity(e);
                            killEntity(b);
                            enemiesLeft--;
                            bulletsOnScreen--;
                            shotByPlayer--;
                            score += 10;
                            updateScoreDisplay();
                            break;
                        }
                    }
                }
            } else{ 
                if(collideEntities(b,&player)){
                    killEntity(&player);
                }
            }
        }
    }
}

int main()
{
    VDP_loadTileSet(background.tileset,1,DMA);
    VDP_setPalette(PAL1, background.palette->data);
    VDP_setPalette(PAL2, background.palette->data);

    int i = 0;
    int thex = 0;
    int they = 0;
    int val = 1;

    XGM_setPCM(SFX_LASER, sfx_laser, sizeof(sfx_laser)); // set up sound effect

    JOY_init();
    JOY_setEventHandler( &myJoyHandler );

    SYS_disableInts();
    for( i=0; i < 1280; i++){
        thex = i % 40;
        they = i / 40;
        val = (random() %  (10-1+1))+1; // random() % (max - min + 1) + min
        if(val > 3) val = 1;
        VDP_setTileMapXY(BG_B,TILE_ATTR_FULL(PAL1,0,0,0,val), thex, they );
    }
    SYS_enableInts();

    int offset = 0;
    VDP_setScrollingMode(HSCROLL_PLANE,VSCROLL_PLANE);

    SPR_init();

    /*Add the player*/
    player.x = 152;
    player.y = 192;
    player.health = 1;
    player.sprite = SPR_addSprite(&ship,player.x,player.y,TILE_ATTR(PAL1,0,FALSE,FALSE));
    SPR_update();

    /*Create all enemy sprites*/
    Entity* e = enemies; // points to first item in array by default
    for (i = 0; i < MAX_ENEMIES; i++){
        e->x = i*32;
        e->y = 32;
        e->w = 16;
        e->h = 16;
        e->velx = 1;
        e->health = 1;
        e->sprite = SPR_addSprite(&ship,e->x,e->y,TILE_ATTR(PAL2,0,TRUE,FALSE)); // Note that the third argument of TILE_ATTR is TRUE. This will flip the sprite so that the enemy is pointing downwards
        sprintf(e->name, "En%d",i);
        enemiesLeft++;
        e++;
    }
    VDP_setPaletteColor(34,RGB24_TO_VDPCOLOR(0x0078f8));

    Entity* b = bullets;
    for(i = 0; i < MAX_BULLETS; i++){
        b->x = 0;
        b->y = -10;
        b->w = 8;
        b->h = 8;
        b->sprite = SPR_addSprite(&bullet,bullets[0].x,bullets[0].y,TILE_ATTR(PAL1,0,FALSE,FALSE));
        sprintf(b->name, "Bu%d",i);
        b++;
    }

    updateScoreDisplay();

    while(1)
    {
        // background scroll
        VDP_setVerticalScroll(BG_B,offset -= 2);
        if(offset >= 256) offset = 0;

        positionPlayer();
        positionBullets();
        positionEnemies();
        handleCollisions();
        SPR_update(); // update sprites

        SYS_doVBlankProcess();
    }

    return (0);
}
