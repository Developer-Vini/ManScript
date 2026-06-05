# jsScript

<image src="logo/logo.png">

## Visao Geral

jsScript e uma linguagem dinamica, interpretada, pequena e pensada para scripts, automacao leve, apps 2D e jogos simples. O nucleo de console fica em `bin/mz.exe`. O backend grafico SDL fica em `bin/mz_sdl.exe`.

Exemplo minimo:

```js
print("jsScript");

let nome = "Zero";
let pontos = 10;

if (pontos > 0) {
    print(nome + ":", pontos);
}
```
# Instalação

```powershell
git clone https://github.com/Developer-Vini/jsScript.git
```

## Como Executar

Console:

```powershell
.\bin\mz.exe examples\basico.js
```

SDL:

```powershell
.\bin\mz_sdl.exe examples\games\snake_zero.js
```

## Comentarios

```js
# comentario de linha
// comentario de linha

/*
comentario
em bloco
*/
```

## Identificadores

Nomes podem conter letras, numeros e `_`, mas devem comecar com letra ou `_`.

```js
let vida = 100;
let player_x = 32;
let _interno = true;
```

## Palavras Reservadas

`let`, `var`, `fun`, `func`, `return`, `if`, `else`, `while`, `for`, `switch`, `case`, `default`, `break`, `continue`, `true`, `false`, `null`, `and`, `or`, `not`.

`var` e sinonimo de `let`. `func` e sinonimo de `fun`.

## Tipos

Tipos nativos:

- `number`: numeros de ponto flutuante.
- `bool`: `true` ou `false`.
- `string`: texto entre aspas duplas.
- `array`: lista indexada.
- `object`: mapa de campos.
- `function`: funcao escrita na linguagem.
- `native`: funcao implementada em C.
- `null`: ausencia de valor.

Use `type(valor)` para inspecionar:

```js
print(type(10));        # number
print(type("oi"));      # string
print(type([1, 2]));    # array
print(type({x: 1}));    # object
```

## Valores Falsos e Verdadeiros

Sao falsos:

- `null`
- `false`
- numero `0`
- string vazia `""`

Todo array, objeto, funcao e native e verdadeiro. Numeros diferentes de zero e strings nao vazias tambem sao verdadeiros.

## Numeros

```js
let a = 10;
let b = 3.5;
let c = .25;
```

Numeros sao usados tambem para coordenadas, tajshos, indices e handles de imagem.

## Strings

Strings usam aspas duplas:

```js
let nome = "Ana";
print("Ola, " + nome);
```

O operador `+` concatena quando pelo menos um dos lados e string. Outros tipos sao convertidos para texto.

Indexacao de string retorna um caractere como string:

```js
let s = "abc";
print(s[0]); # a
```

Indice fora do texto retorna `null`.

## Variaveis

```js
let x = 10;
var y = 20;
let vazio;
```

Variavel sem valor inicial recebe `null`.

## Arrays

```js
let lista = [10, 20, 30];
print(lista[0]);

lista[1] = 99;
push(lista, 40);
print(len(lista));
```

Ao atribuir em um indice alem do fim, o array cresce e preenche lacunas com `null`:

```js
let a = [];
a[2] = "fim";
print(len(a)); # 3
```

## Objetos

```js
let player = {x: 10, y: 20, nome: "Heroi"};
print(player.x);

player.vida = 100;
player["jsa"] = 50;
```

Chaves literais podem ser identificadores ou strings:

```js
let obj = {
    nome: "Loja",
    "preco-base": 25
};
```

Acesso inexistente retorna `null`.

## Funcoes

Declaracao nomeada:

```js
fun soma(a, b) {
    return a + b;
}

print(soma(2, 3));
```

Funcao anonima:

```js
let dobro = fun(n) {
    return n * 2;
};
```

`return` sem expressao retorna `null`.

Funcoes capturam o escopo onde foram criadas:

```js
fun contador() {
    let n = 0;
    return fun() {
        n++;
        return n;
    };
}

let prox = contador();
print(prox()); # 1
print(prox()); # 2
```

Limites atuais:

- Ate 64 parametros.
- Ate 64 argumentos por chamada.
- Se passar argumentos a mais, eles sao ignorados.
- Se passar argumentos de menos, ocorre erro.

## Metodos e `this`

Quando uma funcao guardada em objeto e chamada com ponto, o objeto entra como `this`.

```js
let player = {
    x: 10,
    mover: fun(dx) {
        this.x += dx;
    }
};

player.mover(5);
print(player.x); # 15
```

## Blocos e Escopo

Blocos criam escopo:

```js
let x = 1;
{
    let x = 2;
    print(x); # 2
}
print(x); # 1
```

`while` cria um escopo novo para o corpo a cada iteracao. `for` usa um escopo de loop compartilhado.

## Condicionais

```js
if (vida <= 0) {
    print("fim");
} else {
    print("jogando");
}
```

O corpo pode ser um bloco ou uma unica instrucao, mas em jogos prefira blocos.

`switch` compara o valor principal com cada `case` usando `==`. Ele executa somente o primeiro `case` compatível, ou `default` se nenhum combinar. Nao existe fallthrough automatico: o proximo `case` nao roda sozinho.

```js
switch (tile) {
    case 1:
        textura = parede;
    case 2:
        textura = porta;
    default:
        textura = vazio;
}
```

Voce pode usar `break;` dentro do `switch` para sair antes do fim do bloco do `case`, especialmente quando ha varios cojsdos.

## Loops

`while`:

```js
let i = 0;
while (i < 5) {
    print(i);
    i++;
}
```

`for` C-style:

```js
for (let i = 0; i < 5; i++) {
    print(i);
}
```

`break` sai do loop. `continue` pula para a proxima iteracao.

Nao existe `for in` no jsScript atual. Para percorrer arrays, use indice:

```js
let itens = ["a", "b", "c"];
let i = 0;
while (i < len(itens)) {
    print(itens[i]);
    i++;
}
```

## Operadores

Aritmeticos:

```js
a + b
a - b
a * b
a / b
a % b
-a
```

Comparacao:

```js
a == b
a != b
a < b
a <= b
a > b
a >= b
```

Logicos:

```js
not vivo
!vivo
a and b
a or b
```

Atribuicao:

```js
x = 10;
x += 2;
x -= 2;
x *= 2;
x /= 2;
x %= 2;
x++;
x--;
```

Atribuicao funciona em variaveis, campos e indices:

```js
player.x += 4;
lista[0]++;
obj["chave"] = 123;
```

## Precedencia

Da maior para a menor:

1. Chamada, indice e campo: `f()`, `a[i]`, `obj.campo`
2. Unary: `-x`, `!x`, `not x`
3. Multiplicacao: `*`, `/`, `%`
4. Soma: `+`, `-`
5. Comparacao: `<`, `<=`, `>`, `>=`
6. Igualdade: `==`, `!=`
7. `and`
8. `or`

Use parenteses quando quiser deixar a intencao obvia.

## Conversoes

Conversao numerica interna:

- `number` vira ele mesmo.
- `bool` vira `1` ou `0`.
- `string` usa conversao numerica estilo C.
- outros tipos viram `0`.

Conversao para string:

- `null` vira `"null"`.
- bool vira `"true"` ou `"false"`.
- number vira texto numerico.
- array vira `"[array]"`.
- object vira `"{object}"`.
- function vira `"<function>"`.
- native vira `"<native>"`.

## Biblioteca Padrao do Nucleo

### Console e Conversao

`print(...)`

Imprime os argumentos separados por espaco e quebra linha.

```js
print("vida", 100);
```

`str(valor)`

Converte para string.

`num(valor)`

Converte para number.

`type(valor)`

Retorna `"null"`, `"number"`, `"bool"`, `"string"`, `"array"`, `"object"`, `"function"` ou `"native"`.

### Tajsho e Colecoes

`len(valor)`

Retorna:

- tajsho de string.
- quantidade de itens de array.
- quantidade de campos de object.
- `0` para outros tipos.

`push(array, valor)`

Adiciona no fim e retorna o novo tajsho.

`pop(array)`

Remove e retorna o ultimo item. Se vazio, retorna `null`.

`remove(array, indice)`

Remove e retorna o item no indice. Se invalido, retorna `null`.

`has(objeto, chave)`

Para objeto, testa se a chave existe. Para array, testa se o indice existe.

`del(objeto, chave)`

Remove campo de objeto. Retorna bool.

`range(end)`

Gera `[0, 1, ..., end - 1]`.

`range(start, end)`

Gera de `start` ate antes de `end`.

`range(start, end, step)`

Usa passo customizado. `step` zero vira `1`.

`join(array, sep)`

Converte itens para texto e junta com separador.

### Matematica

Constante:

```js
PI
```

Funcoes:

```js
abs(x)
floor(x)
ceil(x)
sqrt(x)
sin(x)
cos(x)
pow(a, b)
atan2(y, x)
min(a, b, ...)
max(a, b, ...)
clamp(x, min, max)
rand(max)
rand_int(max)
```

`rand(max)` retorna numero de `0` ate antes de `max`, aproximadamente. Se omitido, usa `1`.

`rand_int(max)` retorna inteiro de `0` ate `max - 1`. Se `max <= 0`, usa `1`.

## Backend SDL 2D

As funcoes abaixo existem somente no executavel `mz_sdl.exe`.

### Janela e Loop

`window(titulo, largura, altura)`

Cria janela SDL. Tambem aceita `window(largura, altura)` e `window(largura, altura, titulo)`.

```js
window("Meu Jogo", 800, 600);
```

`logical_size(largura, altura, esticar)`

Define uma resolucao logica para desenho. A janela pode ser maior, mas todos os cojsdos de desenho usam essa resolucao e o SDL escala o resultado. Use para jogos pixel art ou raycasting ficarem mais leves.

`running()`

Retorna se a janela continua aberta.

`poll()`

Processa eventos. Chame uma vez por frame.

`quit()`

Fecha o loop.

`delay(ms)`

Pausa por milissegundos.

`ticks()`

Retorna milissegundos desde inicializacao SDL.

### Teclado e Mouse

`key(nome)`

Retorna bool se a tecla esta pressionada.

Exemplos de nomes: `"Up"`, `"Down"`, `"Left"`, `"Right"`, `"Escape"`, `"Space"`, `"A"`, `"D"`, `"1"`.

`mouse_x()`, `mouse_y()`

Coordenadas atuais do mouse.

`mouse_rel_x()`, `mouse_rel_y()`

Movimento relativo acumulado do mouse desde o ultimo `poll()`.

`mouse_relative(ativo)`

Ativa ou desativa modo relativo do mouse na janela SDL. Quando ativo, o mouse fica preso/capturado pela janela e e ideal para camera de FPS.

`mouse_left()`, `mouse_right()`

Estado dos botoes.

### Desenho Basico

`clear(r, g, b)`

Limpa a tela.

`color(r, g, b)`

Define cor atual. Tambem aceita alpha: `color(r, g, b, a)`.

`rect(x, y, w, h)`

Retangulo preenchido.

`rect_line(x, y, w, h)`

Contorno de retangulo.

`line(x1, y1, x2, y2)`

Linha.

`point(x, y)`

Ponto.

`circle(x, y, raio)`

Circulo preenchido.

`circle(x, y, raio, preenchido)`

Se `preenchido` for `0`, desenha contorno.

`text(texto, x, y)`

Desenha texto com uma fonte bitmap embutida usando a cor atual. Tambem aceita escala:

```js
color(255, 240, 200);
text("READY?", 320, 180, 4);
```

Suporta letras ASCII, numeros, espaco, quebra de linha e pontuacao comum. Use imagens se precisar de uma fonte artistica especifica.

`present()`

Mostra o frame. Chame depois de desenhar tudo.

### Imagens

`image_load(caminho)`

Carrega BMP ou PNG. Retorna id numerico ou `0` se falhar.

`image_load_key(caminho, r, g, b, tolerancia)`

Carrega BMP ou PNG e transforma pixels proximos da cor indicada em transparentes. Use para folhas com fundo verde/magenta.

```js
let sprite = image_load_key("assets/sprite.png", 71, 112, 76, 8);
```

`image(id, x, y)`

Desenha no tajsho original.

`image(id, x, y, w, h)`

Desenha escalado.

`image_part(id, sx, sy, sw, sh, x, y)`

Desenha um recorte da imagem.

`image_part(id, sx, sy, sw, sh, x, y, w, h)`

Desenha um recorte escalado.

`image_part_flip(id, sx, sy, sw, sh, x, y, w, h, flip_x, flip_y)`

Desenha um recorte escalado com espelhamento horizontal e/ou vertical. Use para personagens virarem de lado sem duplicar sprites.

`image_w(id)`, `image_h(id)`

Retornam tajsho da imagem.

`image_free(id)`

Libera a textura.

### Raycasting Acelerado

`raycast_walls(mapa, mapa_w, mapa_h, px, py, angulo, fov, distancia_tela, escala, texturas, textura_tajsho, tela_w, tela_h)`

Desenha colunas de parede por raycasting no backend C/SDL. `mapa` e um array linear de tiles, `texturas` e um array onde o indice do tile aponta para a imagem carregada. Foi criado para jogos estilo Wolfenstein, evitando centenas de chamadas interpretadas por frame.

`ray_wall_distance_fast(mapa, mapa_w, mapa_h, px, py, ex, ey)`

Retorna a distancia ate a primeira parede entre o ponto do player e o ponto alvo. Use para testes de visibilidade/oclusao sem fazer esse raycast em jsScript a cada sprite ou NPC.

`next_path_cell_fast(mapa, mapa_w, mapa_h, sx, sy, gx, gy)`

Calcula em C a proxima celula de um caminho em grade e retorna o indice linear da celula (`y * mapa_w + x`). Use para IA de inimigos em jogos de labirinto sem rodar BFS interpretado a cada frame.

### Voxels 3D Acelerados

`voxel3d_render(blocos, largura, altura, profundidade, cam_x, cam_y, cam_z, yaw, pitch, fov, atlas, tile_tajsho, tela_w, tela_h, distancia, max_faces)`

Renderiza um mundo de cubos 3D usando o renderer SDL. `blocos` e um array linear em ordem `(y * profundidade + z) * largura + x`, onde `0` e ar e valores maiores apontam para celulas do atlas. A funcao faz a projecao e desenho das faces em C para jster jogos voxel leves na linguagem.


## Padroes Recomendados

Use ponto e virgula por clareza, embora muitas instrucoes aceitem fim sem `;`.

```js
let x = 10;
print(x);
```

Em jogos, jstenha o loop neste formato:

```js
window("Jogo", 640, 480);

while (running()) {
    poll();
    if (key("Escape")) { quit(); }

    clear(10, 12, 18);
    # atualizar e desenhar
    present();
    delay(16);
}
```

Para listas grandes, prefira `while` com indice. Para entidades, use objetos dentro de arrays:

```js
let inimigos = [
    {x: 100, y: 80, vida: 3},
    {x: 180, y: 90, vida: 2}
];
```
