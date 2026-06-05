# Desenvolvendo Jogos em ManScript Zero


Este guia mostra um caminho passo a passo para criar jogos 2D no ManScript Zero usando `mz_sdl.exe`.

## 1. Crie a Janela

Todo jogo SDL comeca com `window`.

```man
window("Meu Jogo", 800, 512);
```

Defina constantes de tela no inicio, porque o Zero atual nao tem `window_w()` ou `window_h()`:

```man
let W = 800;
let H = 512;
```

## 2. Monte o Loop Principal

O loop padrao tem quatro partes: eventos, update, draw e apresentacao.

```man
window("Loop", 640, 480);

while (running()) {
    poll();
    if (key("Escape")) { quit(); }

    # update

    clear(12, 14, 20);
    # draw
    present();

    delay(16);
}
```

`delay(16)` mira aproximadamente 60 FPS.

## 3. Leia Entrada do Teclado

`key(nome)` fica verdadeiro enquanto a tecla esta pressionada.

```man
let x = 100;
let y = 100;
let speed = 3;

if (key("Left") or key("A")) { x -= speed; }
if (key("Right") or key("D")) { x += speed; }
if (key("Up") or key("W")) { y -= speed; }
if (key("Down") or key("S")) { y += speed; }
```

Para detectar aperto unico, guarde uma trava:

```man
let pause = false;
let lock_p = false;

if (key("P") and !lock_p) {
    pause = !pause;
    lock_p = true;
}
if (!key("P")) { lock_p = false; }
```

## 4. Desenhe Formas Basicas

Antes de usar imagens, teste jogo com formas:

```man
clear(8, 10, 18);

color(80, 180, 255);
rect(x, y, 32, 32);

color(255, 220, 80);
circle(400, 240, 12);

present();
```

Isso ajuda a validar movimento, colisao e camera.

## 5. Estruture o Estado do Jogo

Use variaveis para estado global simples:

```man
let score = 0;
let alive = true;
```

Use objetos para entidades:

```man
let player = {x: 100, y: 100, w: 32, h: 32, speed: 3};
```

Use arrays de objetos para inimigos, tiros e itens:

```man
let enemies = [
    {x: 300, y: 100, w: 28, h: 28, vx: -1},
    {x: 450, y: 180, w: 28, h: 28, vx: 1}
];
```

Percorra com `while`:

```man
let i = 0;
while (i < len(enemies)) {
    enemies[i].x += enemies[i].vx;
    i++;
}
```

## 6. Faca Colisao Retangular

Funcao classica AABB:

```man
fun hit(a, b) {
    return a.x < b.x + b.w and
           a.x + a.w > b.x and
           a.y < b.y + b.h and
           a.y + a.h > b.y;
}
```

Uso:

```man
if (hit(player, enemy)) {
    alive = false;
}
```

## 7. Crie um Mapa de Tiles

Um mapa pode ser um array de numeros.

```man
let TILE = 32;
let MAP_W = 10;
let MAP_H = 8;

let map = [
    1,1,1,1,1,1,1,1,1,1,
    1,0,0,0,0,0,0,0,0,1,
    1,0,2,0,0,0,2,0,0,1,
    1,0,0,0,0,0,0,0,0,1,
    1,1,1,1,1,1,1,1,1,1
];

fun tile_at(tx, ty) {
    if (tx < 0 or ty < 0 or tx >= MAP_W or ty >= MAP_H) { return 1; }
    return map[ty * MAP_W + tx];
}
```

Desenho:

```man
let y = 0;
while (y < MAP_H) {
    let x = 0;
    while (x < MAP_W) {
        let t = tile_at(x, y);
        if (t == 0) { color(40, 160, 70); }
        else if (t == 1) { color(80, 80, 90); }
        else { color(40, 120, 220); }
        rect(x * TILE, y * TILE, TILE, TILE);
        x++;
    }
    y++;
}
```

## 8. Colisao com Tiles Solidos

Marque alguns tiles como solidos.

```man
fun solid(tx, ty) {
    let t = tile_at(tx, ty);
    return t == 1 or t == 2;
}

fun blocked(px, py) {
    return solid(floor(px / TILE), floor(py / TILE));
}
```

Movimento com colisao por eixo:

```man
fun try_move(dx, dy) {
    let nx = player.x + dx;
    let ny = player.y + dy;

    if (!blocked(nx + player.w / 2, player.y + player.h / 2)) {
        player.x = nx;
    }
    if (!blocked(player.x + player.w / 2, ny + player.h / 2)) {
        player.y = ny;
    }
}
```

Mover por eixo evita prender em cantos.

## 9. Carregue Imagens

O SDL carrega BMP e PNG:

```man
let atlas = image_load("assets/tiles.png");
let hero = image_load("assets/hero.png");
```

Se a imagem tem fundo verde ou magenta que deve sumir:

```man
let hero = image_load_key("assets/hero.png", 71, 112, 76, 8);
```

O ultimo numero e a tolerancia. Use `0` para cor exata. Use valores maiores, como `20` ou `40`, para fundos com variacao.

Sempre confira se carregou:

```man
if (hero == 0) {
    print("nao carregou hero.png");
}
```

## 10. Desenhe Sprites

Imagem inteira:

```man
image(hero, player.x, player.y);
```

Imagem escalada:

```man
image(hero, player.x, player.y, 64, 64);
```

Recorte de spritesheet:

```man
image_part(hero, 0, 0, 32, 32, player.x, player.y, 64, 64);
```

Parametros:

```man
image_part(imagem, origem_x, origem_y, origem_w, origem_h, destino_x, destino_y, destino_w, destino_h);
```

## 11. Anime um Personagem 32x32

Para spritesheets em grade:

```man
let frame_w = 32;
let frame_h = 32;
let frame = floor(ticks() / 120) % 4;
let row = 0;

image_part(hero,
    frame * frame_w, row * frame_h, frame_w, frame_h,
    player.x, player.y, 64, 64);
```

Para quatro direcoes:

```man
let dir = 0; # 0 baixo, 1 esquerda, 2 direita, 3 cima

if (key("Down")) { dir = 0; }
if (key("Left")) { dir = 1; }
if (key("Right")) { dir = 2; }
if (key("Up")) { dir = 3; }

let frame = 0;
if (moving) { frame = floor(ticks() / 120) % 4; }

image_part(hero,
    frame * 32, dir * 32, 32, 32,
    player.x, player.y, 64, 64);
```

Se sua folha tem espacos ou rotulos, ajuste `sx` e `sy` manualmente. O jogo `teste jogo com imagem/vila_aventura.man` faz isso.

## 12. Desenhe Tiles com Tileset

Defina recortes:

```man
let tile_grass = {x: 0, y: 0, w: 32, h: 32};
let tile_wall = {x: 32, y: 0, w: 32, h: 32};

fun draw_tile(id, x, y) {
    if (id == 0) {
        image_part(tiles, tile_grass.x, tile_grass.y, tile_grass.w, tile_grass.h, x, y, TILE, TILE);
    } else {
        image_part(tiles, tile_wall.x, tile_wall.y, tile_wall.w, tile_wall.h, x, y, TILE, TILE);
    }
}
```

Depois use o mapa para escolher cada tile.

## 13. Adicione Camera

Camera segue o jogador:

```man
let world_w = MAP_W * TILE;
let world_h = MAP_H * TILE;
let cam_x = clamp(player.x - W / 2, 0, world_w - W);
let cam_y = clamp(player.y - H / 2, 0, world_h - H);
```

Desenhe tudo subtraindo camera:

```man
rect(obj.x - cam_x, obj.y - cam_y, obj.w, obj.h);
image_part(hero, sx, sy, 32, 32, player.x - cam_x, player.y - cam_y, 64, 64);
```

## 14. Ordene Desenho por Camadas

Ordem comum em top-down:

1. fundo
2. tiles
3. itens no chao
4. personagens e inimigos
5. objetos altos/casas/arvores
6. HUD

Se uma casa deve aparecer atras ou na frente do jogador, desenhe antes/depois conforme o `y`.

Forma simples:

```man
draw_world();
draw_items();
draw_buildings_back();
draw_player();
draw_buildings_front();
draw_hud();
```

## 15. Crie Coletaveis

```man
let coins = [
    {x: 120, y: 100, got: false},
    {x: 260, y: 140, got: false}
];

fun update_coins() {
    let i = 0;
    while (i < len(coins)) {
        if (!coins[i].got) {
            let dx = coins[i].x - player.x;
            let dy = coins[i].y - player.y;
            if (sqrt(dx * dx + dy * dy) < 28) {
                coins[i].got = true;
            }
        }
        i++;
    }
}
```

Desenhe apenas os nao coletados.

## 16. Crie Inimigos

```man
let enemies = [
    {x: 300, y: 140, vx: 1, min_x: 260, max_x: 380}
];

fun update_enemies() {
    let i = 0;
    while (i < len(enemies)) {
        enemies[i].x += enemies[i].vx;
        if (enemies[i].x < enemies[i].min_x or enemies[i].x > enemies[i].max_x) {
            enemies[i].vx *= -1;
        }
        i++;
    }
}
```

## 17. Crie Telas de Estado

Use flags:

```man
let state = "play";

if (state == "gameover") {
    # desenha tela de fim
}
```

Use `text("mensagem", x, y, escala)` para HUD/debug e imagens quando precisar de uma fonte artistica especifica.

## 18. Raycasting 2.5D

O Zero tem `sin`, `cos`, `atan2`, `sqrt`, `floor`, `clamp`, entao suporta raycasting.

Ideia basica:

1. mapa em grade, com `0` vazio e numeros maiores como paredes.
2. jogador tem `px`, `py`, `ang`.
3. para cada coluna da tela, lance um raio.
4. avance no grid ate bater em parede.
5. distancia define altura da parede.

Veja [raycast_zero.man](../examples/games/raycast_zero.man) para um exemplo completo.

## 19. Organizacao de Arquivos

Sugestao:

```text
meu_jogo/
  jogo.man
  assets/
    tiles.png
    hero.png
    casas.png
```

Rode a partir da pasta `manscript-zero`:

```powershell
.\bin\mz_sdl.exe .\meu_jogo\jogo.man
```

Use caminhos relativos ao diretorio de onde voce executa o comando.

## 20. Checklist de Um Jogo Completo

- Janela criada com `window`.
- Loop com `poll`, `clear`, desenho, `present`, `delay`.
- Tecla `Escape` chama `quit`.
- Estado separado em variaveis/objetos.
- Movimento testado com formas antes de sprites.
- Colisao por eixo.
- Assets carregados e checados contra `0`.
- Spritesheet desenhado com `image_part`.
- Camera subtraida em tudo que esta no mundo.
- HUD desenhado por ultimo.
- Reset com tecla `R`.
- Objetivo claro: coletar, sobreviver, chegar em ponto, pontuar.

## Mini Exemplo Completo

```man
window("Mini Jogo", 640, 360);

let W = 640;
let H = 360;
let player = {x: 300, y: 160, w: 32, h: 32, speed: 3};
let coin = {x: 120, y: 100, got: false};
let score = 0;

fun hit(a, b) {
    return a.x < b.x + b.w and a.x + a.w > b.x and a.y < b.y + b.h and a.y + a.h > b.y;
}

while (running()) {
    poll();
    if (key("Escape")) { quit(); }

    if (key("Left") or key("A")) { player.x -= player.speed; }
    if (key("Right") or key("D")) { player.x += player.speed; }
    if (key("Up") or key("W")) { player.y -= player.speed; }
    if (key("Down") or key("S")) { player.y += player.speed; }

    player.x = clamp(player.x, 0, W - player.w);
    player.y = clamp(player.y, 0, H - player.h);

    if (!coin.got and hit(player, {x: coin.x, y: coin.y, w: 20, h: 20})) {
        coin.got = true;
        score++;
    }

    clear(12, 14, 20);

    if (!coin.got) {
        color(255, 220, 60);
        circle(coin.x + 10, coin.y + 10, 10);
    }

    color(80, 180, 255);
    rect(player.x, player.y, player.w, player.h);

    color(255, 255, 255);
    rect(16, 16, score * 24, 8);

    present();
    delay(16);
}
```

## Exemplos no Projeto

- `examples/games/snake_zero.man`: grid, input e loop.
- `examples/games/dodge_zero.man`: spawn, colisao e dificuldade.
- `examples/apps/paint_zero.man`: mouse e circulos.
- `examples/apps/image_zero.man`: imagens e spritesheet.
- `examples/games/raycast_zero.man`: raycasting 2.5D.
- `teste jogo com imagem/vila_aventura.man`: jogo top-down usando tiles, sprites e construcoes.
