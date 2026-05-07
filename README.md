# MAN — Linguagem de Script

**MAN** é uma linguagem de script interpretada, escrita em C, com suporte a variáveis, arrays, condicionais, laços, funções, classes, gráficos 2D, motor 3D com z-buffer e raycaster pseudo-3D estilo Wolfenstein. Roda no Windows via Win32 API.

---

## Índice

- [Compilação](#compilação)
- [Uso](#uso)
- [Estrutura do Projeto](#estrutura-do-projeto)
- [Sintaxe](#sintaxe)
  - [Comentários](#comentários)
  - [Variáveis](#variáveis)
  - [Saída e Entrada](#saída-e-entrada)
  - [Condicionais](#condicionais)
  - [Laços](#laços)
  - [Funções](#funções)
  - [Arrays](#arrays)
  - [Classes e Objetos](#classes-e-objetos)
  - [Funções Embutidas](#funções-embutidas)
- [Gráficos 2D](#gráficos-2d)
- [Motor 3D](#motor-3d)
- [Raycaster](#raycaster)
- [Mouse e Teclado](#mouse-e-teclado)
- [Referência Rápida](#referência-rápida)

---

## Compilação

```bash
gcc main.c -o man.exe -lgdi32 -luser32 -lkernel32 -lgdiplus -mwindows
```

---

## Uso

```bash
man.exe programa.man
man.exe --console programa.man
man.exe -c programa.man
man.exe
```

A flag `--console` (ou `-c`) executa no terminal sem abrir janela gráfica.

---

## Estrutura do Projeto

```
.
├── main.c
├── Sintaxe/
│   ├── Array/
│   ├── Avancado/
│   ├── Condicionais/
│   ├── Entrada e Saida/
│   ├── Matematica/
│   ├── String/
│   ├── Variaveis/
│   └── While/
├── Exemplos/
│   ├── Apps/
│   ├── Jogos/
│   └── Graficos/
│       ├── 2D/
│       ├── 3D/
│       └── Raycaster/
└── Pixel Art/
```

---

## Sintaxe

### Comentários

```
# linha inteira ignorada
let x = 10  # comentário inline
```

---

### Variáveis

```
let nome = "Maria"
let idade = 25
let resultado = idade + 5
x = 42
```

Variáveis não têm tipo declarado. Internamente são strings, convertidas para número quando necessário.

---

### Saída e Entrada

```
write("Olá, mundo!")
write("Valor: " + x)
writeln("Sem quebra de linha: ")
write(x + 1)

input(variavel)
input("Digite seu nome: ", nome)
```

`write` imprime com quebra de linha. `writeln` imprime sem.

---

### Condicionais

```
if (x > 10)
begin
    write("maior")
else
    write("menor ou igual")
end
```

Operadores de comparação: `==` `!=` `>` `<` `>=` `<=`

Operadores lógicos: `and` `or`

```
if (idade >= 18 and pais == "Brasil") begin
    write("Pode votar!")
end
```

---

### Laços

```
let i = 0
while (i < 5)
begin
    write(i)
    i = i + 1
end
```

Use `break` para sair antecipadamente e `continue` para pular para a próxima iteração.

---

### Funções

```
function saudacao(nome)
begin
    write("Olá, " + nome + "!")
end

function soma(a, b)
begin
    return a + b
end

saudacao("Ana")
let resultado = soma(3, 4)
```

Funções podem receber parâmetros e retornar valores com `return`.

---

### Arrays

```
array numeros[5]

numeros[0] = 10
numeros[1] = 20
numeros[2] = 30

write(numeros[1])

let val = numeros[2]
```

Arrays de registros (com campos nomeados):

```
array jogadores[10]

jogadores[0].nome = "Ana"
jogadores[0].pontos = 100

write(jogadores[0].nome)
```

---

### Classes e Objetos

```
class Animal
begin
    campo nome
    campo som

    function falar()
    begin
        write(this.nome + " faz: " + this.som)
    end
end

let gato = new Animal
gato.nome = "Felix"
gato.som = "Miau"
gato.falar()
delete gato
```

Herança:

```
class Cachorro extends Animal
begin
    function buscar()
    begin
        write(this.nome + " foi buscar a bolinha!")
    end
end
```

---

### Funções Embutidas

```
rand(min, max)
len(var)
upper(var)
lower(var)
contains(var, "texto")
substring(var, inicio, tamanho)
str(expr)
int(var)
sleep(ms)
```

---

## Gráficos 2D

Toda janela gráfica opera num ciclo `clear` → desenho → `render`. Sem `render`, nada aparece na tela.

```
bgcolor(r, g, b)
clear

color(r, g, b)
rect(x, y, w, h)
circle(x, y, raio)
line(x1, y1, x2, y2)
triangle(x1, y1, x2, y2, x3, y3)
text(x, y, tamanho, "mensagem")
pixel(x, y)

render
sleep(16)
```

Exemplo — bouncing ball:

```
let bx = 400
let by = 300
let vx = 4
let vy = 3
let rodando = 1

while (rodando == 1)
begin
    bgcolor(10, 10, 30)
    clear

    color(100, 200, 255)
    circle(bx, by, 20)

    let bx = bx + vx
    let by = by + vy

    if (bx < 20 or bx > 780)
    begin
        let vx = 0 - vx
    end
    if (by < 20 or by > 580)
    begin
        let vy = 0 - vy
    end

    render
    sleep(16)

    key(tecla)
    if (tecla == "escape")
    begin
        let rodando = 0
    end
end
```

---

## Motor 3D

Motor com z-buffer, câmera livre, suporte a sólidos com cor sólida ou wireframe.

### Inicialização e câmera

```
3dinit
3dcam(x, y, z, yaw, pitch)
3dfov(valor)
```

`yaw` e `pitch` são em graus. `fov` padrão é 520.

### Sólidos

```
3dcube("nome", tamanho)
3dsphere("nome", raio, aneis, fatias)
3dpyramid("nome", base, altura)
3dcylinder("nome", raio, altura, fatias)
3dplane("nome", largura, profundidade)
```

### Transformações

```
3dpos("nome", x, y, z)
3drot("nome", rx, ry, rz)
3dscale("nome", sx, sy, sz)
3dcolor("nome", r, g, b)
3dwireframe("nome", 0_ou_1)
```

### Renderização e controle

```
3drender
3dclear("nome")
3dremove("nome")
```

### Exemplo — cubo girando

```
3dinit
3dcam(0, 0, -8, 0, 0)
3dfov(520)

3dcube("cubo", 2)
3dcolor("cubo", 100, 180, 255)
3dwireframe("cubo", 0)
3dpos("cubo", 0, 0, 4)

let ang = 0

while (1 == 1)
begin
    bgcolor(8, 8, 18)
    clear
    3drot("cubo", ang, ang, 0)
    3drender
    render
    sleep(16)
    let ang = ang + 1
end
```

---

## Raycaster

Motor pseudo-3D estilo Wolfenstein com mapa em grade, câmera direcional, minimapa e suporte a texturas PNG.

### Mapa

```
rcsetmap(largura, altura)
rcwall(x, y, textura)
```

`textura` é um índice base-1 (1, 2, 3...). O valor `0` significa célula vazia (corredor).

### Câmera

```
rcsetpos(px, py, dx, dy)
rcmoveto(x, y)
rcmove(distancia)
rcstrafe(distancia)
rcturn(graus)
rcgetpx(variavel)
rcgetpy(variavel)
```

### Ambiente

```
rcsetceil(r, g, b)
rcsetfloor(r, g, b)
rcminimap(0_ou_1)
rcloadtex("arquivo.png")
rcrender
```

### Exemplo — labirinto básico

```
rcsetmap(8, 8)

rcwall(0,0,1) rcwall(1,0,1) rcwall(2,0,1) rcwall(3,0,1)
rcwall(4,0,1) rcwall(5,0,1) rcwall(6,0,1) rcwall(7,0,1)
rcwall(0,7,1) rcwall(1,7,1) rcwall(2,7,1) rcwall(3,7,1)
rcwall(4,7,1) rcwall(5,7,1) rcwall(6,7,1) rcwall(7,7,1)
rcwall(0,1,1) rcwall(0,2,1) rcwall(0,3,1) rcwall(0,4,1)
rcwall(0,5,1) rcwall(0,6,1)
rcwall(7,1,1) rcwall(7,2,1) rcwall(7,3,1) rcwall(7,4,1)
rcwall(7,5,1) rcwall(7,6,1)
rcwall(3,2,1) rcwall(4,2,1) rcwall(3,3,1)
rcwall(5,4,1) rcwall(5,5,1) rcwall(4,5,1)

rcsetceil(30, 30, 60)
rcsetfloor(80, 60, 40)
rcminimap(1)
rcsetpos(1, 1, 1, 0)

let vel = 0.07
let rodando = 1

while (rodando == 1)
begin
    key(tecla)
    if (tecla == "up")    begin rcmove(vel)         end
    if (tecla == "down")  begin rcmove(0 - vel)     end
    if (tecla == "left")  begin rcturn(0 - 3)       end
    if (tecla == "right") begin rcturn(3)            end
    if (tecla == "a")     begin rcstrafe(0 - vel)   end
    if (tecla == "d")     begin rcstrafe(vel)        end
    if (tecla == "escape") begin let rodando = 0    end
    rcrender
    render
    sleep(16)
end
```

---

## Mouse e Teclado

### Teclado

```
key(tecla)

if (tecla == "left")   begin ... end
if (tecla == "right")  begin ... end
if (tecla == "up")     begin ... end
if (tecla == "down")   begin ... end
if (tecla == "space")  begin ... end
if (tecla == "escape") begin ... end
if (tecla == "w")      begin ... end
if (tecla == "a")      begin ... end
if (tecla == "s")      begin ... end
if (tecla == "d")      begin ... end
```

### Mouse

```
mouse_update

let mx = mouse_x
let my = mouse_y
let clicou = mouse_left
let direito = mouse_right
let clique_unico = mouse_click
```

---

## Referência Rápida

```
let x = valor
x = valor

write(expr)
writeln(expr)
input(var)
input("Mensagem", var)

if (cond) begin
    ...
else
    ...
end

while (cond) begin
    ...
    break
    continue
end

function nome(a, b)
begin
    return a + b
end

array nome[N]
nome[i] = valor
nome[i].campo = valor

class Nome
begin
    campo x
    function metodo() begin ... end
end
let obj = new Nome
delete obj

rand(min, max)
len(var)
upper(var)  /  lower(var)
contains(var, "texto")
substring(var, inicio, tam)
str(expr)  /  int(var)
sleep(ms)

bgcolor(r, g, b)
color(r, g, b)
clear
render
rect(x, y, w, h)
circle(x, y, r)
line(x1, y1, x2, y2)
triangle(x1,y1, x2,y2, x3,y3)
text(x, y, tam, expr)
pixel(x, y)

3dinit
3dcam(x, y, z, yaw, pitch)
3dfov(valor)
3dcube("nome", tam)
3dsphere("nome", r, aneis, fatias)
3dpyramid("nome", base, altura)
3dcylinder("nome", r, h, fatias)
3dplane("nome", w, d)
3dpos("nome", x, y, z)
3drot("nome", rx, ry, rz)
3dscale("nome", sx, sy, sz)
3dcolor("nome", r, g, b)
3dwireframe("nome", 0_ou_1)
3drender
3dclear("nome")
3dremove("nome")

rcsetmap(w, h)
rcwall(x, y, tex)
rcsetpos(px, py, dx, dy)
rcmoveto(x, y)
rcmove(dist)
rcstrafe(dist)
rcturn(graus)
rcgetpx(var)
rcgetpy(var)
rcsetceil(r, g, b)
rcsetfloor(r, g, b)
rcminimap(0_ou_1)
rcloadtex("arquivo.png")
rcrender

key(tecla)
mouse_update
mouse_x  /  mouse_y
mouse_left  /  mouse_right  /  mouse_click

+ - * / %
== != > < >= <=
and  or
# comentário
```

---

## Licença

Fique à vontade para usar e modificar ❤
