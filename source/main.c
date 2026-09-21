// BRO DASH DS
// Juego run & gun original inspirado en Broforce, para Nintendo DS (libnds)

#include <nds.h>
#include <stdio.h>

#define SCREEN_W     256
#define GROUND_Y     160
#define MAX_BULLETS  8
#define MAX_ENEMIES  4
#define GRAVITY      1
#define JUMP_FORCE   -10
#define MOVE_SPEED   2
#define BULLET_SPEED 4

typedef struct {
    int x, y;
    int vx;
    bool alive;
} Entity;

int main(void) {
    // --- Pantalla principal: solo sprites (jugador, balas, enemigos, suelo) ---
    videoSetMode(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_SPRITE);
    oamInit(&oamMain, SpriteMapping_1D_32, false);

    // --- Pantalla inferior: consola de texto (score / controles) ---
    videoSetModeSub(MODE_0_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    consoleDemoInit();

    // Color de fondo (cielo)
    BG_PALETTE[0] = RGB15(12, 18, 31);

    // Paleta de sprites (color plano por tipo)
    SPRITE_PALETTE[1] = RGB15(31, 20, 0);  // jugador - naranja
    SPRITE_PALETTE[2] = RGB15(31, 31, 0);  // bala - amarillo
    SPRITE_PALETTE[3] = RGB15(31, 0, 0);   // enemigo - rojo
    SPRITE_PALETTE[4] = RGB15(10, 25, 5);  // suelo - verde oscuro

    // Reservar memoria de gráficos para cada sprite
    u16* playerGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    u16* groundGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    u16* bulletGfx[MAX_BULLETS];
    u16* enemyGfx[MAX_ENEMIES];

    dmaFillHalfWords(0x0101, playerGfx, 16 * 16);
    dmaFillHalfWords(0x0404, groundGfx, 16 * 16);

    for (int i = 0; i < MAX_BULLETS; i++) {
        bulletGfx[i] = oamAllocateGfx(&oamMain, SpriteSize_8x8, SpriteColorFormat_256Color);
        dmaFillHalfWords(0x0202, bulletGfx[i], 8 * 8);
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemyGfx[i] = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
        dmaFillHalfWords(0x0303, enemyGfx[i], 16 * 16);
    }

    // Estado del jugador
    int playerX = 40, playerY = GROUND_Y - 16;
    int playerVY = 0;
    bool onGround = true;
    bool facingRight = true;

    // Balas y enemigos
    Entity bullets[MAX_BULLETS];
    Entity enemies[MAX_ENEMIES];
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].alive = false;

    enemies[0] = (Entity){ 180, GROUND_Y - 16, -1, true };
    enemies[1] = (Entity){ 230, GROUND_Y - 16,  1, true };
    enemies[2] = (Entity){ 0, 0, 0, false };
    enemies[3] = (Entity){ 0, 0, 0, false };

    int score = 0;

    while (1) {
        scanKeys();
        int held = keysHeld();
        int down = keysDown();

        // Movimiento horizontal
        if (held & KEY_LEFT)  { playerX -= MOVE_SPEED; facingRight = false; }
        if (held & KEY_RIGHT) { playerX += MOVE_SPEED; facingRight = true; }
        if (playerX < 0) playerX = 0;
        if (playerX > SCREEN_W - 16) playerX = SCREEN_W - 16;

        // Salto
        if ((down & KEY_A) && onGround) {
            playerVY = JUMP_FORCE;
            onGround = false;
        }

        // Gravedad
        playerVY += GRAVITY;
        playerY += playerVY;
        if (playerY >= GROUND_Y - 16) {
            playerY = GROUND_Y - 16;
            playerVY = 0;
            onGround = true;
        }

        // Disparo
        if (down & KEY_B) {
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (!bullets[i].alive) {
                    bullets[i].alive = true;
                    bullets[i].x = playerX + (facingRight ? 16 : -8);
                    bullets[i].y = playerY + 4;
                    bullets[i].vx = facingRight ? BULLET_SPEED : -BULLET_SPEED;
                    break;
                }
            }
        }

        // Actualizar balas + colisiones con enemigos
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (!bullets[i].alive) continue;
            bullets[i].x += bullets[i].vx;
            if (bullets[i].x < -8 || bullets[i].x > SCREEN_W) bullets[i].alive = false;

            for (int e = 0; e < MAX_ENEMIES; e++) {
                if (!enemies[e].alive) continue;
                if (bullets[i].x + 8 > enemies[e].x && bullets[i].x < enemies[e].x + 16 &&
                    bullets[i].y + 8 > enemies[e].y && bullets[i].y < enemies[e].y + 16) {
                    enemies[e].alive = false;
                    bullets[i].alive = false;
                    score += 10;
                }
            }
        }

        // Patrulla simple de enemigos
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!enemies[e].alive) continue;
            enemies[e].x += enemies[e].vx;
            if (enemies[e].x <= 0 || enemies[e].x >= SCREEN_W - 16) enemies[e].vx = -enemies[e].vx;
        }

        // --- Dibujar todo con OAM ---
        int oamId = 0;

        // Suelo
        for (int gx = 0; gx < SCREEN_W; gx += 16) {
            oamSet(&oamMain, oamId++, gx, GROUND_Y, 3, 0, SpriteSize_16x16,
                   SpriteColorFormat_256Color, groundGfx, -1, false, false, false, false, false);
        }

        // Jugador
        oamSet(&oamMain, oamId++, playerX, playerY, 0, 0, SpriteSize_16x16,
               SpriteColorFormat_256Color, playerGfx, -1, false, false, !facingRight, false, false);

        // Balas
        for (int i = 0; i < MAX_BULLETS; i++) {
            bool alive = bullets[i].alive;
            oamSet(&oamMain, oamId++, alive ? bullets[i].x : 0, alive ? bullets[i].y : 0,
                   0, 0, SpriteSize_8x8, SpriteColorFormat_256Color, bulletGfx[i],
                   -1, false, !alive, false, false, false);
        }

        // Enemigos
        for (int e = 0; e < MAX_ENEMIES; e++) {
            bool alive = enemies[e].alive;
            oamSet(&oamMain, oamId++, alive ? enemies[e].x : 0, alive ? enemies[e].y : 0,
                   1, 0, SpriteSize_16x16, SpriteColorFormat_256Color, enemyGfx[e],
                   -1, false, !alive, false, false, false);
        }

        oamUpdate(&oamMain);

        // Texto en pantalla inferior
        consoleClear();
        iprintf("BRO DASH DS\n\n");
        iprintf("Puntos: %d\n\n", score);
        iprintf("Flechas: moverse\n");
        iprintf("A: saltar\n");
        iprintf("B: disparar\n");

        swiWaitForVBlank();
    }

    return 0;
}
