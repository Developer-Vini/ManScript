# MAN 

**MAN** é uma linguagem de script simples e interpretada, escrita em C, com suporte a variáveis, arrays, condicionais, laços, funções embutidas e entrada/saída. Ideal para aprender os fundamentos de linguagens de programação ou criar scripts.

## Índice

- [Instalação](#instalação)
- [Uso](#uso)
- [Sintaxe básica](#sintaxe-básica)
  - [Comentários](#comentários)
  - [Variáveis](#variáveis)
  - [Saída](#saída)
  - [Entrada](#entrada)
  - [Condicionais](#condicionais)
  - [Laços](#laços)
  - [Arrays](#arrays)
  - [Funções embutidas](#funções-embutidas)
  - [Aritmética](#aritmética)
- [Modo interativo](#modo-interativo)
- [Referência rápida](#referência-rápida)

---

## Instalação

Clone o repositório e compile com GCC:

```bash
git clone https://github.com/seu-usuario/ManScript
cd ManScript
gcc -o man main.c
```

No Windows, você pode usar o MinGW ou compilar com MSVC normalmente.

---

## Uso

**Executar um arquivo `.man`:**
```bash
./man meu_script.man
```

**Modo silencioso** (sem logs internos, só a saída do programa):
```bash
./man -s meu_script.man
```

**Modo interativo** (REPL):
```bash
./man
```

**Ajuda:**
```bash
./man -h
```

---

## Sintaxe básica

### Comentários

Use `#` para comentários de linha. Tudo após o `#` é ignorado.

```
# isso é um comentário
let x = 10  # comentário inline também funciona
```

---

### Variáveis

Variáveis são criadas com `let`. Não há tipos declarados — MAN trata tudo como string internamente, convertendo para número quando necessário.

```
let nome = "Maria"
let idade = 25
let resultado = idade + 5
```

Atribuição direta (sem `let`) também funciona:

```
x = 42
mensagem = "olá mundo"
```

---

### Saída

| Comando | Comportamento |
|---|---|
| `write(expr)` | Imprime e pula linha |
| `writeln(expr)` | Imprime **sem** pular linha |

```
write("Olá, mundo!")
write("Valor: " + x)
writeln("Digite algo: ")
```

Concatenação é feita com `+` dentro das strings:

```
let nome = "Ana"
write("Bem-vinda, " + nome + "!")
```

---

### Entrada

```
input(variavel)
input("Qual o seu nome?", nome)
```

O primeiro argumento (opcional) é o texto do prompt. O segundo é a variável onde o valor digitado será armazenado.

```
input("Digite sua idade: ", idade)
write("Você tem " + idade + " anos.")
```

---

### Condicionais

A estrutura `if/else/end` usa `begin` para abrir o bloco:

```
if (x > 10)
begin
    write("x é maior que 10")
else
    write("x é 10 ou menor")
end
```

Também é aceito `if (cond) begin` em uma única linha:

```
if (nome == "admin") begin
    write("Acesso liberado")
end
```

**Operadores de comparação:**

| Operador | Significado |
|---|---|
| `==` | igual |
| `!=` | diferente |
| `>` | maior |
| `<` | menor |
| `>=` | maior ou igual |
| `<=` | menor ou igual |

**Operadores lógicos:** `and`, `or`

```
if (idade >= 18 and pais == "Brasil") begin
    write("Pode votar!")
end
```

---

### Laços

`while` repete um bloco enquanto a condição for verdadeira. Use `break` para sair antecipadamente.

```
let i = 0
while (i < 5)
begin
    write(i)
    i = i + 1
end
```

Com `break`:

```
let i = 0
while (1 == 1)
begin
    if (i >= 3) begin
        break
    end
    write(i)
    i = i + 1
end
```

---

### Arrays

Crie um array com tamanho fixo usando `array nome[N]`. Os índices começam em `0`.

```
array numeros[5]

numeros[0] = 10
numeros[1] = 20
numeros[2] = 30

write(numeros[1])   # imprime 20
```

Lendo com `let`:

```
let val = numeros[2]
write(val)   # 30
```

---

### Funções embutidas

#### `rand(min, max)`
Retorna um número inteiro aleatório entre `min` e `max` (inclusive).

```
let sorteio = rand(1, 100)
write("Número sorteado: " + sorteio)
```

#### `len(var)`
Retorna o comprimento de uma string.

```
let nome = "Carlos"
write(len(nome))   # 6
```

#### `upper(var)` / `lower(var)`
Converte uma string para maiúsculas ou minúsculas.

```
let s = "hello"
write(upper(s))   # HELLO
write(lower(s))   # hello
```

#### `contains(var, "texto")`
Retorna `1` se a string contém o texto, `0` caso contrário.

```
let frase = "bom dia mundo"
write(contains(frase, "dia"))   # 1
```

#### `str(expr)` / `int(var)`
Converte entre número e string.

```
let n = 42
let texto = str(n)
let de_volta = int(texto)
```

#### `sleep(ms)`
Pausa a execução por `ms` milissegundos.

```
write("Aguarde...")
sleep(2000)
write("Pronto!")
```

---

### Aritmética

Expressões aritméticas são avaliadas em qualquer contexto numérico.

| Operador | Operação |
|---|---|
| `+` | adição |
| `-` | subtração |
| `*` | multiplicação |
| `/` | divisão inteira |
| `%` | módulo (resto) |

```
let a = 10
let b = 3
write(a * b)       # 30
write(a / b)       # 3
write(a % b)       # 1
```

Expressões podem ser usadas diretamente em `let`, `write`, condicionais e índices de array:

```
let x = 2 * 3 + 1
array dados[x]
```

---

## Modo interativo

Execute `./man` sem argumentos para entrar no REPL:

```
>> let x = 5
'x' = "5"
>> write(x * 2)
10
>> list
=== VARIAVEIS ===
  x                    = "5"
=================
>> exit
```

Comandos úteis no modo interativo:

| Comando | Ação |
|---|---|
| `list` | Lista todas as variáveis e arrays |
| `clear` / `cls` | Limpa a tela |
| `version` | Exibe a versão |
| `help` ou `?` | Exibe ajuda |
| `exit` / `quit` | Sai |

---

## Referência rápida

```
# Variáveis
let x = valor
x = valor

# Saída
write(expr)          # com newline
writeln(expr)        # sem newline

# Entrada
input(var)
input("Mensagem", var)

# Condicional
if (cond) begin
    ...
else
    ...
end

# Laço
while (cond) begin
    ...
    break
end

# Arrays
array nome[N]
nome[i] = valor
let x = nome[i]

# Funções
rand(min, max)
len(var)
upper(var)  /  lower(var)
contains(var, "texto")
str(expr)  /  int(var)
sleep(ms)

# Aritmética
+ - * / %

# Comparações
== != > < >= <=

# Lógicos
and  or

# Comentário
# texto
```

---

## Licença

fique à vontade para usar, modificar ❤
