// BRO DASH DS
// Juego run & gun original inspirado en Broforce, para Nintendo DS (libnds)
// v2: intro tipo comic, menu tactil, HUD en pantalla inferior, 2 bros seleccionables

#include <nds.h>
#include <stdio.h>

#define SCREEN_W     256
#define SCREEN_H     192
#define GROUND_Y     160
#define MAX_BULLETS  16
#define MAX_ENEMIES  4
#define GRAVITY      1
#define JUMP_FORCE   -10
#define MOVE_SPEED   2
#define BULLET_SPEED 4

typedef enum { STATE_INTRO, STATE_MENU, STATE_BROS, STATE_OPTIONS, STATE_PLAY } GameState;

typedef struct { int x, y; int vx; bool alive; } Enemy;
typedef struct { int x, y; int vx, vy; bool alive; } Bullet;

// --- Graficos (main = pantalla de arriba, sub = pantalla de abajo) ---
u16 *playerGfx, *groundGfx, *broGfx;
u16 *bulletGfx[MAX_BULLETS];
u16 *enemyGfx[MAX_ENEMIES];
u16 *healthSegGfx, *broforceSegGfx, *mapDotPlayerGfx, *mapDotEnemyGfx;
u16 *iconAGfx, *iconBGfx, *cursorGfx;

GameState state = STATE_INTRO;
int introPanel = 0;

// --- Jugador / gameplay ---
int playerX, playerY, playerVY;
bool onGround, facingRight;
int playerHealth, invulnTimer;
int broforce, rampageTimer, fireCooldown;
int selectedBro; // 0 = Rifle, 1 = Escopeta
int score;

Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];

// Dibuja una silueta humanoide simple 16x16: cabeza, torso, piernas separadas y brazo+arma
void buildHumanoid(u8* out, u8 skin, u8 body, u8 legs, u8 gun) {
    for (int i = 0; i < 256; i++) out[i] = 0;
    for (int y = 1; y <= 4; y++)
        for (int x = 6; x <= 9; x++) out[y * 16 + x] = skin;
    for (int y = 5; y <= 10; y++)
        for (int x = 4; x <= 11; x++) out[y * 16 + x] = body;
    for (int y = 11; y <= 14; y++) {
        for (int x = 5; x <= 7; x++) out[y * 16 + x] = legs;
        for (int x = 9; x <= 11; x++) out[y * 16 + x] = legs;
    }
    for (int y = 6; y <= 7; y++)
        for (int x = 11; x <= 15; x++) out[y * 16 + x] = gun;
}

// Duplica cada pixel 2x2 para pasar de 16x16 a 32x32 (mismo dibujo, mas grande)
void upscale2x(const u8* src16, u8* dst32) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            u8 v = src16[y * 16 + x];
            int dx = x * 2, dy = y * 2;
            dst32[dy * 32 + dx] = v;
            dst32[dy * 32 + dx + 1] = v;
            dst32[(dy + 1) * 32 + dx] = v;
            dst32[(dy + 1) * 32 + dx + 1] = v;
        }
    }
}

// Bloque de suelo con textura: franja de pasto arriba + tierra con motas oscuras
void buildGroundTile(u8* out, u8 grass, u8 dirt, u8 darkDirt) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (y < 3) out[y * 16 + x] = grass;
            else if ((x + y) % 4 == 0) out[y * 16 + x] = darkDirt;
            else out[y * 16 + x] = dirt;
        }
    }
}

// Bala pequena en forma de rombo en vez de cuadrado
void buildBulletShape(u8* out, u8 color) {
    static const char shape[8][8] = {
        {0,0,0,1,1,0,0,0},
        {0,0,1,1,1,1,0,0},
        {0,1,1,1,1,1,1,0},
        {1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1},
        {0,1,1,1,1,1,1,0},
        {0,0,1,1,1,1,0,0},
        {0,0,0,1,1,0,0,0},
    };
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            out[y * 8 + x] = shape[y][x] ? color : 0;
}

void setupSprites(void) {
    // --- Sprites pantalla principal ---
    vramSetBankA(VRAM_A_MAIN_SPRITE);
    oamInit(&oamMain, SpriteMapping_1D_32, false);

    SPRITE_PALETTE[1] = RGB15(31, 22, 15);  // piel
    SPRITE_PALETTE[2] = RGB15(6, 18, 4);    // uniforme bro (verde militar)
    SPRITE_PALETTE[3] = RGB15(10, 10, 10);  // pantalon/bota oscura
    SPRITE_PALETTE[4] = RGB15(3, 3, 3);     // arma (negro)
    SPRITE_PALETTE[5] = RGB15(6, 22, 4);    // pasto
    SPRITE_PALETTE[6] = RGB15(14, 9, 4);    // tierra
    SPRITE_PALETTE[7] = RGB15(8, 5, 2);     // tierra oscura
    SPRITE_PALETTE[8] = RGB15(31, 31, 4);   // bala normal
    SPRITE_PALETTE[9] = RGB15(31, 14, 2);   // bala rampage
    SPRITE_PALETTE[21] = RGB15(20, 3, 3);   // uniforme enemigo (rojo)

    playerGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    groundGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    broGfx    = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);

    u8 tmp16[256];
    u8 tmp32[1024];
    u8 tmp8[64];

    buildHumanoid(tmp16, 1, 2, 3, 4);
    dmaCopy(tmp16, playerGfx, 256);

    upscale2x(tmp16, tmp32);
    dmaCopy(tmp32, broGfx, 1024);

    buildGroundTile(tmp16, 5, 6, 7);
    dmaCopy(tmp16, groundGfx, 256);

    buildBulletShape(tmp8, 8);
    for (int i = 0; i < MAX_BULLETS; i++) {
        bulletGfx[i] = oamAllocateGfx(&oamMain, SpriteSize_8x8, SpriteColorFormat_256Color);
        dmaCopy(tmp8, bulletGfx[i], 64);
    }

    buildHumanoid(tmp16, 1, 21, 3, 4);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemyGfx[i] = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
        dmaCopy(tmp16, enemyGfx[i], 256);
    }

    // --- Sprites pantalla tactil (HUD) ---
    vramSetBankD(VRAM_D_SUB_SPRITE);
    oamInit(&oamSub, SpriteMapping_1D_32, false);

    SPRITE_PALETTE_SUB[5] = RGB15(0, 31, 0);   // vida
    SPRITE_PALETTE_SUB[6] = RGB15(31, 31, 0);  // bro-fuerza
    SPRITE_PALETTE_SUB[7] = RGB15(0, 20, 31);  // punto jugador en mapa
    SPRITE_PALETTE_SUB[8] = RGB15(31, 0, 0);   // punto enemigo en mapa
    SPRITE_PALETTE_SUB[9] = RGB15(31, 20, 0);  // icono bro A (rifle)
    SPRITE_PALETTE_SUB[10] = RGB15(0, 31, 20); // icono bro B (escopeta)
    SPRITE_PALETTE_SUB[11] = RGB15(31, 31, 31);// cursor menu

    healthSegGfx    = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_256Color);
    broforceSegGfx  = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_256Color);
    mapDotPlayerGfx = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_256Color);
    mapDotEnemyGfx  = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_256Color);
    iconAGfx        = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_256Color);
    iconBGfx        = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_256Color);
    cursorGfx       = oamAllocateGfx(&oamSub, SpriteSize_32x8, SpriteColorFormat_256Color);

    dmaFillHalfWords(0x0505, healthSegGfx, 8 * 8);
    dmaFillHalfWords(0x0606, broforceSegGfx, 8 * 8);
    dmaFillHalfWords(0x0707, mapDotPlayerGfx, 8 * 8);
    dmaFillHalfWords(0x0808, mapDotEnemyGfx, 8 * 8);
    dmaFillHalfWords(0x0909, iconAGfx, 16 * 16);
    dmaFillHalfWords(0x0A0A, iconBGfx, 16 * 16);
    dmaFillHalfWords(0x0B0B, cursorGfx, 32 * 8);
}

void changeState(GameState s) {
    state = s;
    oamClear(&oamMain, 0, 128);
    oamClear(&oamSub, 0, 128);
    consoleClear();
}

void startGame(void) {
    playerX = 40; playerY = GROUND_Y - 16; playerVY = 0;
    onGround = true; facingRight = true;
    playerHealth = 5; invulnTimer = 0;
    broforce = 0; rampageTimer = 0; fireCooldown = 0;
    score = 0;
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].alive = false;
    enemies[0] = (Enemy){ 180, GROUND_Y - 16, -1, true };
    enemies[1] = (Enemy){ 230, GROUND_Y - 16,  1, true };
    enemies[2] = (Enemy){ 100, GROUND_Y - 16, 1, true };
    enemies[3] = (Enemy){ 0, 0, 0, false };
    changeState(STATE_PLAY);
}

void fireBullet(int vx, int vy) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].alive) {
            bullets[i].alive = true;
            bullets[i].x = playerX + (facingRight ? 16 : -8);
            bullets[i].y = playerY + 4;
            bullets[i].vx = vx;
            bullets[i].vy = vy;
            return;
        }
    }
}

int main(void) {
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    consoleDemoInit();
    setupSprites();

    touchPosition touch;

    while (1) {
        scanKeys();
        int down = keysDown();
        int held = keysHeld();
        touchRead(&touch);

        // ================= INTRO (comic) =================
        if (state == STATE_INTRO) {
            BG_PALETTE[0] = RGB15(20, 4, 2) ; // fondo rojizo/incendio
            oamSet(&oamMain, 0, 96, 80, 0, 0, SpriteSize_32x32, SpriteColorFormat_256Color,
                   broGfx, -1, false, false, false, false, false);
            oamUpdate(&oamMain);

            consoleClear();
            if (introPanel == 0) {
                iprintf("\n Un ejercito enemigo\n invadio la selva...\n");
            } else if (introPanel == 1) {
                iprintf("\n Solo un grupo de\n Bros puede detenerlos.\n");
            } else {
                iprintf("\n Es hora de la\n BRO DASH.\n");
            }
            iprintf("\n\n Toca la pantalla\n para continuar");

            if (down & KEY_TOUCH) {
                introPanel++;
                if (introPanel > 2) changeState(STATE_MENU);
            }
        }

        // ================= MENU =================
        else if (state == STATE_MENU) {
            oamSet(&oamMain, 0, 96, 80, 0, 0, SpriteSize_32x32, SpriteColorFormat_256Color,
                   broGfx, -1, false, false, false, false, false);
            oamUpdate(&oamMain);

            consoleClear();
            iprintf("\n   BRO DASH DS\n\n");
            iprintf("   > JUGAR\n\n");
            iprintf("   > BROS\n\n");
            iprintf("   > OPCIONES\n");

            if (down & KEY_TOUCH) {
                if (touch.py >= 30 && touch.py < 46) startGame();
                else if (touch.py >= 46 && touch.py < 62) changeState(STATE_BROS);
                else if (touch.py >= 62 && touch.py < 78) changeState(STATE_OPTIONS);
            }
        }

        // ================= SELECCION DE BRO =================
        else if (state == STATE_BROS) {
            consoleClear();
            iprintf("\n  Elige tu Bro\n\n");
            iprintf(" [Izq] Rifle    [Der] Escopeta\n\n");
            iprintf(" Actual: %s\n\n", selectedBro == 0 ? "Rifle" : "Escopeta");
            iprintf(" Toca arriba para volver");

            if (down & KEY_TOUCH) {
                if (touch.py < 20) changeState(STATE_MENU);
                else if (touch.px < 128) selectedBro = 0;
                else selectedBro = 1;
            }
        }

        // ================= OPCIONES =================
        else if (state == STATE_OPTIONS) {
            consoleClear();
            iprintf("\n  OPCIONES\n\n");
            iprintf(" (proximamente)\n\n");
            iprintf(" Toca para volver");
            if (down & KEY_TOUCH) changeState(STATE_MENU);
        }

        // ================= JUEGO =================
        else if (state == STATE_PLAY) {
            // --- movimiento ---
            if (held & KEY_LEFT)  { playerX -= MOVE_SPEED; facingRight = false; }
            if (held & KEY_RIGHT) { playerX += MOVE_SPEED; facingRight = true; }
            if (playerX < 0) playerX = 0;
            if (playerX > SCREEN_W - 16) playerX = SCREEN_W - 16;

            if ((down & KEY_A) && onGround) { playerVY = JUMP_FORCE; onGround = false; }
            playerVY += GRAVITY;
            playerY += playerVY;
            if (playerY >= GROUND_Y - 16) { playerY = GROUND_Y - 16; playerVY = 0; onGround = true; }

            // --- disparo ---
            if (fireCooldown > 0) fireCooldown--;
            bool wantsFire = (rampageTimer > 0) ? (held & KEY_B) : (down & KEY_B);
            if (wantsFire && fireCooldown <= 0) {
                int vx = facingRight ? BULLET_SPEED : -BULLET_SPEED;
                if (selectedBro == 0) {
                    fireBullet(vx, 0);
                } else {
                    fireBullet(vx, 0);
                    fireBullet(vx, -2);
                    fireBullet(vx, 2);
                }
                fireCooldown = (rampageTimer > 0) ? 6 : 16;
            }

            // --- selector de bro tactil (esquinas inferiores) ---
            if (down & KEY_TOUCH) {
                if (touch.px < 24 && touch.py > 168) selectedBro = 0;
                else if (touch.px > 232 && touch.py > 168) selectedBro = 1;
            }

            // --- balas ---
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (!bullets[i].alive) continue;
                bullets[i].x += bullets[i].vx;
                bullets[i].y += bullets[i].vy;
                if (bullets[i].x < -8 || bullets[i].x > SCREEN_W) bullets[i].alive = false;

                for (int e = 0; e < MAX_ENEMIES; e++) {
                    if (!enemies[e].alive) continue;
                    if (bullets[i].x + 8 > enemies[e].x && bullets[i].x < enemies[e].x + 16 &&
                        bullets[i].y + 8 > enemies[e].y && bullets[i].y < enemies[e].y + 16) {
                        enemies[e].alive = false;
                        bullets[i].alive = false;
                        score += 10;
                        broforce += 20;
                        if (broforce >= 100) { broforce = 0; rampageTimer = 180; }
                    }
                }
            }

            // --- enemigos: patrulla + dano al jugador ---
            if (invulnTimer > 0) invulnTimer--;
            if (rampageTimer > 0) rampageTimer--;
            for (int e = 0; e < MAX_ENEMIES; e++) {
                if (!enemies[e].alive) continue;
                enemies[e].x += enemies[e].vx;
                if (enemies[e].x <= 0 || enemies[e].x >= SCREEN_W - 16) enemies[e].vx = -enemies[e].vx;

                if (invulnTimer == 0 &&
                    playerX + 16 > enemies[e].x && playerX < enemies[e].x + 16 &&
                    playerY + 16 > enemies[e].y && playerY < enemies[e].y + 16) {
                    playerHealth--;
                    invulnTimer = 60;
                    playerX += (playerX < enemies[e].x) ? -10 : 10;
                    if (playerHealth <= 0) startGame(); // reinicio simple
                }
            }

            // --- dibujar pantalla principal ---
            int oamId = 0;
            for (int gx = 0; gx < SCREEN_W; gx += 16) {
                oamSet(&oamMain, oamId++, gx, GROUND_Y, 3, 0, SpriteSize_16x16,
                       SpriteColorFormat_256Color, groundGfx, -1, false, false, false, false, false);
            }
            bool blink = invulnTimer > 0 && (invulnTimer / 4) % 2 == 0;
            oamSet(&oamMain, oamId++, playerX, playerY, 0, 0, SpriteSize_16x16,
                   SpriteColorFormat_256Color, playerGfx, -1, false, blink, !facingRight, false, false);

            for (int i = 0; i < MAX_BULLETS; i++) {
                bool alive = bullets[i].alive;
                oamSet(&oamMain, oamId++, alive ? bullets[i].x : 0, alive ? bullets[i].y : 0,
                       0, 0, SpriteSize_8x8, SpriteColorFormat_256Color, bulletGfx[i],
                       -1, false, !alive, false, false, false);
            }
            for (int e = 0; e < MAX_ENEMIES; e++) {
                bool alive = enemies[e].alive;
                oamSet(&oamMain, oamId++, alive ? enemies[e].x : 0, alive ? enemies[e].y : 0,
                       1, 0, SpriteSize_16x16, SpriteColorFormat_256Color, enemyGfx[e],
                       -1, false, !alive, false, false, false);
            }
            oamUpdate(&oamMain);

            // --- dibujar HUD (pantalla inferior) ---
            int subId = 0;
            for (int h = 0; h < 5; h++) {
                oamSet(&oamSub, subId++, 4 + h * 10, 4, 0, 0, SpriteSize_8x8,
                       SpriteColorFormat_256Color, healthSegGfx, -1, false, h >= playerHealth, false, false, false);
            }
            int broSegs = broforce / 20;
            for (int b = 0; b < 5; b++) {
                oamSet(&oamSub, subId++, 4 + b * 10, 16, 0, 0, SpriteSize_8x8,
                       SpriteColorFormat_256Color, broforceSegGfx, -1, false, b >= broSegs, false, false, false);
            }
            // minimapa (franja de 80px)
            int mapX = 4 + (playerX * 80) / SCREEN_W;
            oamSet(&oamSub, subId++, mapX, 30, 0, 0, SpriteSize_8x8, SpriteColorFormat_256Color,
                   mapDotPlayerGfx, -1, false, false, false, false, false);
            for (int e = 0; e < MAX_ENEMIES; e++) {
                bool alive = enemies[e].alive;
                int ex = 4 + (enemies[e].x * 80) / SCREEN_W;
                oamSet(&oamSub, subId++, alive ? ex : 0, 30, 0, 0, SpriteSize_8x8,
                       SpriteColorFormat_256Color, mapDotEnemyGfx, -1, false, !alive, false, false, false);
            }
            // selector de bro (esquinas inferiores)
            oamSet(&oamSub, subId++, 4, 172, 0, 0, SpriteSize_16x16, SpriteColorFormat_256Color,
                   iconAGfx, -1, false, false, false, false, false);
            oamSet(&oamSub, subId++, 236, 172, 0, 0, SpriteSize_16x16, SpriteColorFormat_256Color,
                   iconBGfx, -1, false, false, false, false, false);
            oamSet(&oamSub, subId++, selectedBro == 0 ? 0 : 232, 168, 1, 0, SpriteSize_32x8,
                   SpriteColorFormat_256Color, cursorGfx, -1, false, false, false, false, false);
            oamUpdate(&oamSub);

            consoleClear();
            iprintf("\n\n\n\n\n  Puntos: %d\n", score);
            if (rampageTimer > 0) iprintf("  RAMPAGE!\n");
        }

        swiWaitForVBlank();
    }
    return 0;
}
