#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#include <objbase.h>
#include <gdiplus.h>

#define RC_MAP_MAX 64
#define RC_TEX_MAX 16
#define RC_TEX_W 64
#define RC_TEX_H 64

#define WIN_W 800
#define WIN_H 600

#define MAX_SHAPES 500
#define MAX_VARS 200
#define MAX_LINES 4000
#define MAX_LINE 512
#define MAX_TOKENS 64
#define MAX_FUNCS 100
#define MAX_PARAMS 16
#define MAX_STACK 200
#define MAX_SCOPE 64
#define MAX_SPRITES 100
#define MAX_CLASSES 64
#define MAX_METHODS 32
#define MAX_FIELDS 32
#define MAX_OBJECTS 256
#define MAX_OBJ_VARS 64
#define MAX_ARRAYS 50
#define MAX_ARRAY_SIZE 100
#define MAX_ARRAY_FIELDS 16

#define VERSAO "2.0"
// #define MZX 10

typedef enum
{
    SH_RECT,
    SH_CIRCLE,
    SH_LINE,
    SH_TRIANGLE,
    SH_TEXT,
    SH_PIXEL
} ShapeType;

typedef struct
{
    ShapeType tipo;
    int x, y, w, h, x2, y2, x3, y3, r;
    COLORREF cor;
    int preenchido;
    char texto[256];
} Shape;

typedef struct
{
    char nome[50];
    int x, y, w, h;
} Sprite;

typedef struct
{
    char nome[64];
    char valor[256];
} Var;

typedef struct
{
    char campos[MAX_ARRAY_FIELDS][64];
    char valores[MAX_ARRAY_FIELDS][256];
    int nCampos;
    int ativo;
} ArrayElem;

typedef struct
{
    char nome[64];
    ArrayElem elems[MAX_ARRAY_SIZE];
    int tamanho;
} Array;

typedef struct
{
    char nome[64];
    int linhaInicio;
    char params[MAX_PARAMS][64];
    int nParams;
    char classeOwner[64];
} Func;

typedef struct
{
    char nome[64];
    char pai[64];
    int metodos[MAX_METHODS];
    int nMetodos;
    char campos[MAX_FIELDS][64];
    char valoresCampos[MAX_FIELDS][256];
    int nCampos;
    int construtorIdx;
    int destrutorIdx;
} Classe;

typedef struct
{
    int id;
    char classeNome[64];
    Var campos[MAX_OBJ_VARS];
    int nCampos;
    int ativo;
} Objeto;

typedef enum
{
    NIV_IF,
    NIV_WHILE,
    NIV_FUNC,
    NIV_METHOD
} TipoNivel;

typedef struct
{
    TipoNivel tipo;
    int exec, ifRes, elseUsado, linhaWhile;
    char condWhile[300];
} NivelPilha;

typedef struct
{
    Var vars[MAX_VARS];
    int n;
} Escopo;

static Shape shapes[MAX_SHAPES];
static int nShapes = 0;
static COLORREF corAtual = RGB(255, 255, 255);
static int preenchido = 1;
static HWND hwnd = NULL;
static HDC hdcBuffer = NULL;
static HBITMAP hBitmap = NULL;
static int bgR = 0, bgG = 0, bgB = 0;

static int mouse_x = 400, mouse_y = 300;
static int mouse_left = 0, mouse_right = 0, mouse_click = 0;

static Sprite sprites[MAX_SPRITES];
static int nSprites = 0;
static DWORD startTick = 0;

static Var gVars[MAX_VARS];
static int nVars = 0;
static NivelPilha pilha[MAX_STACK];
static int topo = -1;
static Func funcs[MAX_FUNCS];
static int nFuncs = 0;
static int retPilha[MAX_STACK];
static int topoRet = -1;
static Escopo escopos[MAX_SCOPE];
static int topoEsc = -1;
static char retval[256] = "";
static int flagReturn = 0, flagBreak = 0;

static Classe classes[MAX_CLASSES];
static int nClasses = 0;
static Objeto objetos[MAX_OBJECTS];
static int nObjetos = 0;
static int objNextId = 1;
static int thisObjId = -1;
static int thisPilha[MAX_STACK];
static int topoThis = -1;

static char progLines[MAX_LINES][MAX_LINE];
static int nLines = 0, cursor = 0;
static int ultimoRes = 1, ultimoEhWhile = 0, ultimaLinhaWhile = 0;
static char ultimaCondWhile[300] = "";

static Array arrays[MAX_ARRAYS];
static int nArrays = 0;

static int modoConsole = 0;

typedef enum
{
    ERR_GERAL,
    ERR_VAR,
    ERR_FUNC,
    ERR_CLASSE,
    ERR_OBJETO,
    ERR_ARRAY,
    ERR_SINTAXE,
    ERR_DIVISAO,
    ERR_LIMITE,
    ERR_ARQUIVO
} TipoErro;

static int erroCount = 0;

static void erroMAN(TipoErro tipo, const char *msg)
{
    erroCount++;

    int linhaAtual = (cursor > 0) ? cursor : 1;

    const char *prefixo = "";
    switch (tipo)
    {
    case ERR_VAR:
        prefixo = "Variavel";
        break;
    case ERR_FUNC:
        prefixo = "Funcao";
        break;
    case ERR_CLASSE:
        prefixo = "Classe";
        break;
    case ERR_OBJETO:
        prefixo = "Objeto";
        break;
    case ERR_ARRAY:
        prefixo = "Array";
        break;
    case ERR_SINTAXE:
        prefixo = "Sintaxe";
        break;
    case ERR_DIVISAO:
        prefixo = "Matematica";
        break;
    case ERR_LIMITE:
        prefixo = "Limite";
        break;
    case ERR_ARQUIVO:
        prefixo = "Arquivo";
        break;
    default:
        prefixo = "Erro";
        break;
    }

    fprintf(stderr, "\n[ERRO %s – linha %d] %s\n", prefixo, linhaAtual, msg);

    if (linhaAtual > 0 && linhaAtual <= nLines)
    {
        fprintf(stderr, "  -> %s\n", progLines[linhaAtual - 1]);
    }
    fprintf(stderr, "\n");
}

#define erroF(tipo, fmt, ...)                               \
    do                                                      \
    {                                                       \
        char _emsg[512];                                    \
        snprintf(_emsg, sizeof(_emsg), fmt, ##__VA_ARGS__); \
        erroMAN(tipo, _emsg);                               \
    } while (0)

typedef struct
{
    unsigned char data[RC_TEX_W * RC_TEX_H * 4];
    int w, h;
    int ativo;
} RcTextura;

static RcTextura rcTex[RC_TEX_MAX];
static int rcNTex = 0;

static int rcMapa[RC_MAP_MAX][RC_MAP_MAX];
static int rcMapW = 0, rcMapH = 0;

static double rcPx = 2.0, rcPy = 2.0;
static double rcDx = 1.0, rcDy = 0.0;
static double rcCx = 0.0, rcCy = 0.66;

static int rcAtivo = 0;
static int rcCeilR = 50, rcCeilG = 50, rcCeilB = 80;
static int rcFloorR = 100, rcFloorG = 80, rcFloorB = 60;
static int rcMinimap = 1;

static double rcZBuffer[800];

static ULONG_PTR rcGdiplusToken = 0;

static char *getVar(char *nome);
static void chamarFuncao(int idx, char *argsStr);

static void trim(char *s)
{
    if (!s)
        return;
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == '\t'))
        s[--n] = '\0';
    int i = 0;
    while (s[i] == ' ' || s[i] == '\t')
        i++;
    if (i > 0)
        memmove(s, s + i, n - i + 1);
}

static void toLow(char *s)
{
    for (int i = 0; s[i]; i++)
        s[i] = (char)tolower((unsigned char)s[i]);
}

static void toUp(char *s)
{
    for (int i = 0; s[i]; i++)
        s[i] = (char)toupper((unsigned char)s[i]);
}

static int isNumStr(const char *s)
{
    if (!s || !*s)
        return 0;
    int i = 0;
    if (s[i] == '-')
        i++;
    if (!s[i])
        return 0;
    for (; s[i]; i++)
        if (!isdigit((unsigned char)s[i]))
            return 0;
    return 1;
}

static Array *getArray(const char *nome)
{
    for (int i = 0; i < nArrays; i++)
        if (!strcmp(arrays[i].nome, nome))
            return &arrays[i];
    return NULL;
}

static Array *criarArray(const char *nome)
{
    Array *a = getArray(nome);
    if (a)
        return a;
    if (nArrays >= MAX_ARRAYS)
    {
        erroMAN(ERR_LIMITE, "numero maximo de arrays atingido (limite: 50)");
        return NULL;
    }
    a = &arrays[nArrays++];
    memset(a, 0, sizeof(Array));
    strncpy(a->nome, nome, 63);
    return a;
}

static char *getArrayElemCampo(ArrayElem *e, const char *campo)
{
    for (int i = 0; i < e->nCampos; i++)
        if (!strcmp(e->campos[i], campo))
            return e->valores[i];
    return NULL;
}

static void setArrayElemCampo(ArrayElem *e, const char *campo, const char *val)
{
    for (int i = 0; i < e->nCampos; i++)
    {
        if (!strcmp(e->campos[i], campo))
        {
            strncpy(e->valores[i], val, 255);
            return;
        }
    }
    if (e->nCampos < MAX_ARRAY_FIELDS)
    {
        strncpy(e->campos[e->nCampos], campo, 63);
        strncpy(e->valores[e->nCampos], val, 255);
        e->campos[e->nCampos][63] = '\0';
        e->valores[e->nCampos][255] = '\0';
        e->nCampos++;
    }
}

static void setArrayElem(const char *nomeArray, int idx,
                         const char *campo, const char *val)
{
    Array *a = getArray(nomeArray);
    if (!a)
    {
        erroF(ERR_ARRAY, "array '%s' nao foi criado (use mkarray(%s) antes)", nomeArray, nomeArray);
        return;
    }
    if (idx < 0 || idx >= MAX_ARRAY_SIZE)
    {
        erroF(ERR_ARRAY, "indice %d fora do limite no array '%s' (maximo: %d)", idx, nomeArray, MAX_ARRAY_SIZE - 1);
        return;
    }
    ArrayElem *e = &a->elems[idx];
    e->ativo = 1;
    if (idx >= a->tamanho)
        a->tamanho = idx + 1;
    setArrayElemCampo(e, campo, val);
}

static char *getArrayElem(const char *nomeArray, int idx, const char *campo)
{
    Array *a = getArray(nomeArray);
    if (!a || idx < 0 || idx >= a->tamanho)
        return NULL;
    ArrayElem *e = &a->elems[idx];
    return e->ativo ? getArrayElemCampo(e, campo) : NULL;
}

static int resolveArrayExpr(const char *expr, char *out, int outMax)
{
    const char *lb = strchr(expr, '[');
    if (!lb)
        return 0;
    const char *rb = strchr(lb, ']');
    if (!rb)
        return 0;
    const char *dot = strchr(rb, '.');
    if (!dot)
        return 0;

    char nomeArr[64] = "";
    int nl = (int)(lb - expr);
    strncpy(nomeArr, expr, nl < 63 ? nl : 63);
    trim(nomeArr);

    char idxStr[128] = "";
    int il = (int)(rb - lb - 1);
    strncpy(idxStr, lb + 1, il < 127 ? il : 127);
    trim(idxStr);

    char campo[64] = "";
    strncpy(campo, dot + 1, 63);
    trim(campo);

    int idx = 0;
    char *v = NULL;
    if (topoEsc >= 0)
    {
        Escopo *e = &escopos[topoEsc];
        for (int i = 0; i < e->n; i++)
            if (!strcmp(e->vars[i].nome, idxStr))
            {
                v = e->vars[i].valor;
                break;
            }
    }
    if (!v)
        for (int i = 0; i < nVars; i++)
            if (!strcmp(gVars[i].nome, idxStr))
            {
                v = gVars[i].valor;
                break;
            }
    idx = v ? atoi(v) : atoi(idxStr);

    char *val = getArrayElem(nomeArr, idx, campo);
    if (val)
    {
        strncpy(out, val, outMax - 1);
        out[outMax - 1] = '\0';
    }
    else
    {
        out[0] = '\0';
    }
    return 1;
}

static int isArraySetExpr(const char *expr)
{
    const char *lb = strchr(expr, '[');
    if (!lb)
        return 0;
    const char *rb = strchr(lb, ']');
    if (!rb)
        return 0;
    return strchr(rb, '.') ? 1 : 0;
}

static Objeto *getObjById(int id)
{
    for (int i = 0; i < nObjetos; i++)
        if (objetos[i].ativo && objetos[i].id == id)
            return &objetos[i];
    return NULL;
}

static Classe *getClasse(const char *nome)
{
    for (int i = 0; i < nClasses; i++)
        if (!strcmp(classes[i].nome, nome))
            return &classes[i];
    return NULL;
}

static char *getObjCampo(Objeto *o, const char *campo)
{
    for (int i = 0; i < o->nCampos; i++)
        if (!strcmp(o->campos[i].nome, campo))
            return o->campos[i].valor;
    return NULL;
}

static void setObjCampo(Objeto *o, const char *campo, const char *val)
{
    for (int i = 0; i < o->nCampos; i++)
    {
        if (!strcmp(o->campos[i].nome, campo))
        {
            strncpy(o->campos[i].valor, val, 255);
            return;
        }
    }
    if (o->nCampos < MAX_OBJ_VARS)
    {
        strncpy(o->campos[o->nCampos].nome, campo, 63);
        strncpy(o->campos[o->nCampos].valor, val, 255);
        o->campos[o->nCampos].nome[63] = '\0';
        o->campos[o->nCampos].valor[255] = '\0';
        o->nCampos++;
    }
}

static int resolveObjField(const char *nome,
                           char *objOut, int objMax,
                           char *fieldOut, int fieldMax)
{
    const char *dot = strchr(nome, '.');
    if (!dot)
        return 0;
    int nl = (int)(dot - nome);
    strncpy(objOut, nome, nl < objMax - 1 ? nl : objMax - 1);
    objOut[nl < objMax - 1 ? nl : objMax - 1] = '\0';
    strncpy(fieldOut, dot + 1, fieldMax - 1);
    fieldOut[fieldMax - 1] = '\0';
    trim(objOut);
    trim(fieldOut);
    return 1;
}

static Objeto *getObjByVarName(const char *varName)
{
    char *idStr = NULL;
    if (topoEsc >= 0)
    {
        Escopo *e = &escopos[topoEsc];
        for (int i = 0; i < e->n; i++)
            if (!strcmp(e->vars[i].nome, varName))
            {
                idStr = e->vars[i].valor;
                break;
            }
    }
    if (!idStr)
        for (int i = 0; i < nVars; i++)
            if (!strcmp(gVars[i].nome, varName))
            {
                idStr = gVars[i].valor;
                break;
            }
    return idStr ? getObjById(atoi(idStr)) : NULL;
}

static void setVar(char *nome, char *val)
{
    trim(nome);
    trim(val);

    if (isArraySetExpr(nome))
    {
        const char *lb = strchr(nome, '[');
        const char *rb = strchr(lb, ']');
        const char *dot = strchr(rb, '.');

        char nomeArr[64] = "";
        int nl = (int)(lb - nome);
        strncpy(nomeArr, nome, nl < 63 ? nl : 63);
        trim(nomeArr);

        char idxStr[128] = "";
        int il = (int)(rb - lb - 1);
        strncpy(idxStr, lb + 1, il < 127 ? il : 127);
        trim(idxStr);

        char campo[64] = "";
        strncpy(campo, dot + 1, 63);
        trim(campo);

        int idx = 0;
        char *v = NULL;
        if (topoEsc >= 0)
        {
            Escopo *e = &escopos[topoEsc];
            for (int i = 0; i < e->n; i++)
                if (!strcmp(e->vars[i].nome, idxStr))
                {
                    v = e->vars[i].valor;
                    break;
                }
        }
        if (!v)
            for (int i = 0; i < nVars; i++)
                if (!strcmp(gVars[i].nome, idxStr))
                {
                    v = gVars[i].valor;
                    break;
                }
        idx = v ? atoi(v) : atoi(idxStr);
        setArrayElem(nomeArr, idx, campo, val);
        return;
    }

    char objName[64], fieldName[64];
    if (resolveObjField(nome, objName, 64, fieldName, 64))
    {
        Objeto *o = (!strcmp(objName, "this") && thisObjId != -1)
                        ? getObjById(thisObjId)
                        : getObjByVarName(objName);
        if (o)
            setObjCampo(o, fieldName, val);
        return;
    }

    if (topoEsc >= 0)
    {
        Escopo *e = &escopos[topoEsc];
        for (int i = 0; i < e->n; i++)
        {
            if (!strcmp(e->vars[i].nome, nome))
            {
                strncpy(e->vars[i].valor, val, 255);
                return;
            }
        }
        if (e->n < MAX_VARS)
        {
            strncpy(e->vars[e->n].nome, nome, 63);
            strncpy(e->vars[e->n].valor, val, 255);
            e->vars[e->n].nome[63] = '\0';
            e->vars[e->n].valor[255] = '\0';
            e->n++;
            return;
        }
    }

    for (int i = 0; i < nVars; i++)
    {
        if (!strcmp(gVars[i].nome, nome))
        {
            strncpy(gVars[i].valor, val, 255);
            return;
        }
    }
    if (nVars < MAX_VARS)
    {
        strncpy(gVars[nVars].nome, nome, 63);
        strncpy(gVars[nVars].valor, val, 255);
        gVars[nVars].nome[63] = '\0';
        gVars[nVars].valor[255] = '\0';
        nVars++;
    }
}

static char *getVar(char *nome)
{
    char t[80];
    strncpy(t, nome, 79);
    t[79] = '\0';
    trim(t);

    if (!strcmp(t, "__retval"))
        return retval;

    char objName[64], fieldName[64];
    if (resolveObjField(t, objName, 64, fieldName, 64))
    {
        Objeto *o = (!strcmp(objName, "this") && thisObjId != -1)
                        ? getObjById(thisObjId)
                        : getObjByVarName(objName);
        return o ? getObjCampo(o, fieldName) : NULL;
    }

    if (topoEsc >= 0)
    {
        Escopo *e = &escopos[topoEsc];
        for (int i = 0; i < e->n; i++)
            if (!strcmp(e->vars[i].nome, t))
                return e->vars[i].valor;
    }
    for (int i = 0; i < nVars; i++)
        if (!strcmp(gVars[i].nome, t))
            return gVars[i].valor;
    return NULL;
}

static int splitArgs(const char *src, char args[][300], int maxArgs)
{
    int n = 0, dep = 0, inQ = 0, tl = 0;
    char tok[300] = "";
    for (int i = 0;; i++)
    {
        char c = src[i];
        if (c == '"')
            inQ = !inQ;
        if (!inQ && (c == '(' || c == '['))
            dep++;
        if (!inQ && (c == ')' || c == ']'))
            dep--;
        int sep = (!inQ && dep == 0 && c == ',') || c == '\0';
        if (sep)
        {
            tok[tl] = '\0';
            trim(tok);
            if (n < maxArgs)
            {
                strncpy(args[n], tok, 299);
                args[n][299] = '\0';
                n++;
            }
            tl = 0;
            if (c == '\0')
                break;
        }
        else
        {
            if (tl < 299)
                tok[tl++] = c;
        }
    }
    return n;
}

static int extractParens(const char *src, char *out, int outMax)
{
    const char *lp = strchr(src, '(');
    if (!lp)
        return 0;
    const char *rp = strrchr(src, ')');
    if (!rp || rp <= lp)
        return 0;
    int len = (int)(rp - lp - 1);
    if (len < 0)
        len = 0;
    if (len >= outMax)
        len = outMax - 1;
    strncpy(out, lp + 1, len);
    out[len] = '\0';
    return 1;
}

static int evalExpr(char *expr);
static void evalFunc(char *expr, char *out);

static int resolveAtom(char *tok)
{
    char t[300];
    strncpy(t, tok, 299);
    t[299] = '\0';
    trim(t);

    if (isArraySetExpr(t))
    {
        char tmp[256] = "";
        resolveArrayExpr(t, tmp, 256);
        return atoi(tmp);
    }
    if (!strncmp(t, "len(", 4))
    {
        char inner[100] = "";
        extractParens(t, inner, 100);
        trim(inner);
        Array *a = getArray(inner);
        if (a)
            return a->tamanho;
        char *v = getVar(inner);
        return v ? (int)strlen(v) : 0;
    }
    if (!strncmp(t, "rand(", 5))
    {
        char inner[100] = "";
        extractParens(t, inner, 100);
        char a2[60] = "", b2[60] = "";
        sscanf(inner, "%59[^,],%59s", a2, b2);
        trim(a2);
        trim(b2);
        int a = evalExpr(a2), b = evalExpr(b2);
        if (b < a)
        {
            int tmp = a;
            a = b;
            b = tmp;
        }
        return a + rand() % (b - a + 1);
    }
    if (!strncmp(t, "abs(", 4))
    {
        char inner[200] = "";
        extractParens(t, inner, 200);
        int v = evalExpr(inner);
        return v < 0 ? -v : v;
    }
    if (!strncmp(t, "min(", 4))
    {
        char inner[200] = "";
        extractParens(t, inner, 200);
        char a2[60] = "", b2[60] = "";
        sscanf(inner, "%59[^,],%59s", a2, b2);
        trim(a2);
        trim(b2);
        int a = evalExpr(a2), b = evalExpr(b2);
        return a < b ? a : b;
    }
    if (!strncmp(t, "max(", 4))
    {
        char inner[200] = "";
        extractParens(t, inner, 200);
        char a2[60] = "", b2[60] = "";
        sscanf(inner, "%59[^,],%59s", a2, b2);
        trim(a2);
        trim(b2);
        int a = evalExpr(a2), b = evalExpr(b2);
        return a > b ? a : b;
    }
    if (!strncmp(t, "timer(", 6))
        return (int)(GetTickCount() - startTick);
    if (!strcmp(t, "mouse_x"))
        return mouse_x;
    if (!strcmp(t, "mouse_y"))
        return mouse_y;
    if (!strcmp(t, "mouse_left"))
        return mouse_left;
    if (!strcmp(t, "mouse_right"))
        return mouse_right;
    if (!strcmp(t, "mouse_click"))
        return mouse_click;

    if (!strncmp(t, "instanceof(", 11))
    {
        char inner[200] = "";
        extractParens(t, inner, 200);
        char ta[80] = "", tb[80] = "";
        sscanf(inner, "%79[^,],%79s", ta, tb);
        trim(ta);
        trim(tb);
        char *idStr = getVar(ta);
        if (!idStr)
            return 0;
        Objeto *o = getObjById(atoi(idStr));
        if (!o)
            return 0;
        char cur[64];
        strncpy(cur, o->classeNome, 63);
        while (strlen(cur) > 0)
        {
            if (!strcmp(cur, tb))
                return 1;
            Classe *c = getClasse(cur);
            if (!c || !strlen(c->pai))
                break;
            strncpy(cur, c->pai, 63);
        }
        return 0;
    }

    if (strchr(t, '(') && t[strlen(t) - 1] == ')')
    {
        char *lp = strchr(t, '(');
        char fnome[64] = "";
        int fl = (int)(lp - t);
        strncpy(fnome, t, fl < 63 ? fl : 63);
        trim(fnome);
        for (int i = 0; i < nFuncs; i++)
        {
            if (!strcmp(funcs[i].nome, fnome) && !strlen(funcs[i].classeOwner))
            {
                char argsStr[256] = "";
                extractParens(t, argsStr, 256);
                chamarFuncao(i, argsStr);
                return atoi(retval);
            }
        }
    }

    char *v = getVar(t);
    return v ? atoi(v) : atoi(t);
}

static int evalExpr(char *expr)
{
    char buf[400];
    strncpy(buf, expr, 399);
    buf[399] = '\0';
    trim(buf);

    if (buf[0] == '(' && buf[strlen(buf) - 1] == ')')
    {
        int depth = 0, allWrap = 1;
        for (int i = 0; buf[i]; i++)
        {
            if (buf[i] == '(')
                depth++;
            if (buf[i] == ')')
                depth--;
            if (depth == 0 && buf[i + 1] != '\0')
            {
                allWrap = 0;
                break;
            }
        }
        if (allWrap)
        {
            char inner[398];
            int len = (int)strlen(buf);
            strncpy(inner, buf + 1, len - 2);
            inner[len - 2] = '\0';
            return evalExpr(inner);
        }
    }

    char toks[MAX_TOKENS][160];
    char ops[MAX_TOKENS];
    int nT = 0, nO = 0, dep = 0;
    char cur[160];
    int cL = 0;

    for (int i = 0;; i++)
    {
        char c = buf[i];
        if (c == '(' || c == '[')
            dep++;
        else if (c == ')' || c == ']')
            dep--;
        int isMul = (c == '*' || c == '/' || c == '%') && dep == 0;
        int isAdd = (c == '+' || c == '-') && dep == 0;
        int isSig = (c == '-') && cL == 0 && dep == 0;
        int isOp = (isMul || isAdd) && !isSig;

        if (c == '\0' || isOp)
        {
            cur[cL] = '\0';
            trim(cur);
            if (cL > 0 && nT < MAX_TOKENS)
            {
                strncpy(toks[nT], cur, 159);
                toks[nT][159] = '\0';
                nT++;
                cL = 0;
            }
            if (c != '\0' && isOp && nO < MAX_TOKENS)
                ops[nO++] = c;
            if (c == '\0')
                break;
        }
        else
        {
            if (cL < 159)
                cur[cL++] = c;
        }
    }
    if (nT == 0)
        return 0;

    int v[MAX_TOKENS];
    for (int i = 0; i < nT; i++)
        v[i] = resolveAtom(toks[i]);

    for (int i = 0; i < nO; i++)
    {
        if (ops[i] == '*' || ops[i] == '/' || ops[i] == '%')
        {
            int r;
            if (ops[i] == '*')
                r = v[i] * v[i + 1];
            else if (ops[i] == '/')
            {
                if (!v[i + 1])
                {
                    erroF(ERR_DIVISAO, "divisao por zero: %d / 0", v[i]);
                    r = 0;
                }
                else
                    r = v[i] / v[i + 1];
            }
            else
            {
                if (!v[i + 1])
                {
                    erroF(ERR_DIVISAO, "modulo por zero: %d %% 0", v[i]);
                    r = 0;
                }
                else
                    r = v[i] % v[i + 1];
            }
            v[i] = r;
            for (int j = i + 1; j < nT - 1; j++)
                v[j] = v[j + 1];
            for (int j = i; j < nO - 1; j++)
                ops[j] = ops[j + 1];
            nT--;
            nO--;
            i--;
        }
    }

    int res = v[0];
    for (int i = 0; i < nO; i++)
        res += (ops[i] == '+') ? v[i + 1] : -v[i + 1];
    return res;
}

static void evalFunc(char *expr, char *out)
{
    char t[512];
    strncpy(t, expr, 511);
    t[511] = '\0';
    trim(t);
    out[0] = '\0';

    if (t[0] == '"')
    {
        int hasPlusOut = 0, inQ2 = 0, dep3 = 0;
        for (int i = 0; t[i]; i++)
        {
            if (t[i] == '"')
                inQ2 = !inQ2;
            if (!inQ2 && (t[i] == '(' || t[i] == '['))
                dep3++;
            if (!inQ2 && (t[i] == ')' || t[i] == ']'))
                dep3--;
            if (!inQ2 && dep3 == 0 && t[i] == '+')
            {
                hasPlusOut = 1;
                break;
            }
        }
        if (!hasPlusOut)
        {
            char *end = strrchr(t + 1, '"');
            if (end)
                *end = '\0';
            strncpy(out, t + 1, 255);
            out[255] = '\0';
            return;
        }
    }

    if (!strcmp(t, "__retval"))
    {
        strncpy(out, retval, 255);
        return;
    }

    if (isArraySetExpr(t))
    {
        resolveArrayExpr(t, out, 256);
        return;
    }

    if (!strncmp(t, "upper(", 6))
    {
        char inner[200] = "";
        extractParens(t, inner, 200);
        trim(inner);
        char *v = getVar(inner);
        if (!v)
            v = inner;
        strncpy(out, v, 255);
        toUp(out);
        return;
    }
    if (!strncmp(t, "lower(", 6))
    {
        char inner[200] = "";
        extractParens(t, inner, 200);
        trim(inner);
        char *v = getVar(inner);
        if (!v)
            v = inner;
        strncpy(out, v, 255);
        toLow(out);
        return;
    }
    if (!strncmp(t, "str(", 4) || !strncmp(t, "int(", 4))
    {
        char inner[200] = "";
        extractParens(t, inner, 200);
        sprintf(out, "%d", evalExpr(inner));
        return;
    }
    if (!strncmp(t, "rand(", 5))
    {
        sprintf(out, "%d", resolveAtom(t));
        return;
    }
    if (!strncmp(t, "abs(", 4))
    {
        sprintf(out, "%d", resolveAtom(t));
        return;
    }
    if (!strncmp(t, "min(", 4))
    {
        sprintf(out, "%d", resolveAtom(t));
        return;
    }
    if (!strncmp(t, "max(", 4))
    {
        sprintf(out, "%d", resolveAtom(t));
        return;
    }
    if (!strncmp(t, "instanceof(", 11))
    {
        sprintf(out, "%d", resolveAtom(t));
        return;
    }
    if (!strncmp(t, "timer(", 6))
    {
        sprintf(out, "%d", (int)(GetTickCount() - startTick));
        return;
    }

    if (!strncmp(t, "len(", 4))
    {
        char inner[200] = "";
        extractParens(t, inner, 200);
        trim(inner);
        Array *a = getArray(inner);
        if (a)
        {
            sprintf(out, "%d", a->tamanho);
            return;
        }
        char *v = getVar(inner);
        sprintf(out, "%d", v ? (int)strlen(v) : 0);
        return;
    }
    if (!strncmp(t, "typeof(", 7))
    {
        char inner[100] = "";
        extractParens(t, inner, 100);
        trim(inner);
        char *v = getVar(inner);
        if (!v)
        {
            strcpy(out, "null");
            return;
        }
        Objeto *o = getObjById(atoi(v));
        if (o)
        {
            strncpy(out, o->classeNome, 255);
            return;
        }
        strcpy(out, isNumStr(v) ? "int" : "string");
        return;
    }
    if (!strncmp(t, "concat(", 7))
    {
        char inner[400] = "";
        extractParens(t, inner, 400);
        char a2[2][300] = {{""}, {""}};
        splitArgs(inner, a2, 2);
        char va[256] = "", vb[256] = "";
        evalFunc(a2[0], va);
        evalFunc(a2[1], vb);
        snprintf(out, 255, "%s%s", va, vb);
        return;
    }
    if (!strncmp(t, "substring(", 10))
    {
        char inner[400] = "";
        extractParens(t, inner, 400);
        char a2[3][300] = {{""}, {""}, {""}};
        splitArgs(inner, a2, 3);
        char src[256] = "";
        evalFunc(a2[0], src);
        int ini = evalExpr(a2[1]), len2 = evalExpr(a2[2]);
        int slen = (int)strlen(src);
        if (ini < 0)
            ini = 0;
        if (ini >= slen)
        {
            out[0] = '\0';
            return;
        }
        if (len2 > slen - ini)
            len2 = slen - ini;
        strncpy(out, src + ini, len2);
        out[len2] = '\0';
        return;
    }
    if (!strncmp(t, "collide(", 8))
    {
        char inner[200] = "";
        extractParens(t, inner, 200);
        char na[60] = "", nb[60] = "";
        sscanf(inner, "%59[^,],%59s", na, nb);
        trim(na);
        trim(nb);
        Sprite *sa = NULL, *sb = NULL;
        for (int i = 0; i < nSprites; i++)
        {
            if (!strcmp(sprites[i].nome, na))
                sa = &sprites[i];
            if (!strcmp(sprites[i].nome, nb))
                sb = &sprites[i];
        }
        if (!sa || !sb)
        {
            strcpy(out, "0");
            return;
        }
        int hit = !(sa->x + sa->w <= sb->x || sb->x + sb->w <= sa->x ||
                    sa->y + sa->h <= sb->y || sb->y + sb->h <= sa->y);
        sprintf(out, "%d", hit);
        return;
    }
    if (!strcmp(t, "mouse_x"))
    {
        sprintf(out, "%d", mouse_x);
        return;
    }
    if (!strcmp(t, "mouse_y"))
    {
        sprintf(out, "%d", mouse_y);
        return;
    }
    if (!strcmp(t, "mouse_left"))
    {
        sprintf(out, "%d", mouse_left);
        return;
    }
    if (!strcmp(t, "mouse_right"))
    {
        sprintf(out, "%d", mouse_right);
        return;
    }
    if (!strcmp(t, "mouse_click"))
    {
        sprintf(out, "%d", mouse_click);
        return;
    }

    {
        char *lp = strchr(t, '(');
        if (lp && t[strlen(t) - 1] == ')')
        {
            int dep2 = 0, isFunc = 1;
            for (int i = 0; t[i]; i++)
            {
                if (t[i] == '(' || t[i] == '[')
                    dep2++;
                else if (t[i] == ')' || t[i] == ']')
                    dep2--;
                else if (dep2 == 0 && (t[i] == '+' || t[i] == '-' || t[i] == '*' || t[i] == '/') && i > 0)
                {
                    isFunc = 0;
                    break;
                }
            }
            if (isFunc)
            {
                char fnome[64] = "";
                int fl = (int)(lp - t);
                strncpy(fnome, t, fl < 63 ? fl : 63);
                trim(fnome);
                for (int i = 0; i < nFuncs; i++)
                {
                    if (!strcmp(funcs[i].nome, fnome) && !strlen(funcs[i].classeOwner))
                    {
                        char argsStr[256] = "";
                        extractParens(t, argsStr, 256);
                        chamarFuncao(i, argsStr);
                        strncpy(out, retval, 255);
                        return;
                    }
                }

                int ehBuiltin = (!strcmp(fnome, "len") || !strcmp(fnome, "str") || !strcmp(fnome, "int") || !strcmp(fnome, "rand") || !strcmp(fnome, "abs") || !strcmp(fnome, "sqrt") || !strcmp(fnome, "upper") || !strcmp(fnome, "lower") || !strcmp(fnome, "substr") || !strcmp(fnome, "contains") || !strcmp(fnome, "indexof") || !strcmp(fnome, "replace") || !strcmp(fnome, "trim") || !strcmp(fnome, "split") || !strcmp(fnome, "time") || !strcmp(fnome, "color") || !strcmp(fnome, "rect") || !strcmp(fnome, "circle") || !strcmp(fnome, "text") || !strcmp(fnome, "line") || !strcmp(fnome, "pixel") || !strcmp(fnome, "write") || !strcmp(fnome, "print") || !strcmp(fnome, "input"));
                if (!ehBuiltin && strlen(fnome) > 0)
                    erroF(ERR_FUNC, "funcao '%s' nao foi definida", fnome);
            }
        }
    }

    {
        int dep2 = 0, inQ = 0, hasPlus = 0, hasOtherOp = 0, hasStr = 0;
        for (int i = 0; t[i]; i++)
        {
            if (t[i] == '"')
                inQ = !inQ;
            if (!inQ && (t[i] == '(' || t[i] == '['))
                dep2++;
            if (!inQ && (t[i] == ')' || t[i] == ']'))
                dep2--;
            if (!inQ && dep2 == 0)
            {
                if (t[i] == '+')
                    hasPlus = 1;
                if (t[i] == '*' || t[i] == '/' || t[i] == '%')
                    hasOtherOp = 1;
                if (t[i] == '-' && i > 0)
                    hasOtherOp = 1;
            }
            if (t[i] == '"')
                hasStr = 1;
        }

        if (hasPlus || hasOtherOp)
        {
            int doCat = 0;
            if (hasPlus)
            {
                if (hasStr)
                {
                    doCat = 1;
                }
                else
                {
                    dep2 = 0;
                    inQ = 0;
                    char tmp[512] = "";
                    int tl2 = 0;
                    for (int i = 0;; i++)
                    {
                        char c = t[i];
                        if (c == '"')
                            inQ = !inQ;
                        if (!inQ && (c == '(' || c == '['))
                            dep2++;
                        if (!inQ && (c == ')' || c == ']'))
                            dep2--;
                        int boundary = (c == '\0') ||
                                       (!inQ && dep2 == 0 && i > 0 && (c == '+' || c == '-' || c == '*' || c == '/' || c == '%'));
                        if (boundary)
                        {
                            tmp[tl2] = '\0';
                            trim(tmp);
                            if (strlen(tmp) > 0)
                            {
                                if (strchr(tmp, '.') || isArraySetExpr(tmp))
                                {
                                    doCat = 1;
                                    break;
                                }
                                if (!tmp[0] || (!isdigit((unsigned char)tmp[0]) && tmp[0] != '-'))
                                {
                                    char *vv = getVar(tmp);
                                    if (vv && !isNumStr(vv))
                                    {
                                        doCat = 1;
                                        break;
                                    }
                                    if (!vv && !isNumStr(tmp) && tmp[0] != '(')
                                    {
                                        doCat = 1;
                                        break;
                                    }
                                }
                            }
                            tl2 = 0;
                            if (c == '\0')
                                break;
                        }
                        else
                        {
                            if (tl2 < 511)
                                tmp[tl2++] = c;
                        }
                    }
                }
            }

            if (doCat)
            {

                char result[512] = "";
                dep2 = 0;
                inQ = 0;
                char tok2[512] = "";
                int tl2 = 0;
                for (int i = 0;; i++)
                {
                    char c = t[i];
                    if (c == '"')
                        inQ = !inQ;
                    if (!inQ && (c == '(' || c == '['))
                        dep2++;
                    if (!inQ && (c == ')' || c == ']'))
                        dep2--;
                    int isSep = (!inQ && dep2 == 0 && c == '+') || c == '\0';
                    if (isSep)
                    {
                        tok2[tl2] = '\0';
                        trim(tok2);
                        char part[256] = "";
                        evalFunc(tok2, part);
                        strncat(result, part, sizeof(result) - strlen(result) - 1);
                        tl2 = 0;
                        if (c == '\0')
                            break;
                    }
                    else
                    {
                        if (tl2 < 511)
                            tok2[tl2++] = c;
                    }
                }
                strncpy(out, result, 255);
                out[255] = '\0';
                return;
            }

            sprintf(out, "%d", evalExpr(t));
            return;
        }
    }

    char *v = getVar(t);
    if (v)
    {
        strncpy(out, v, 255);
        return;
    }
    strncpy(out, t, 255);
}

static int evalCondSimp(char *cond)
{
    char buf[400];
    strncpy(buf, cond, 399);
    buf[399] = '\0';
    trim(buf);

    if (!strncmp(buf, "not(", 4))
    {
        char inner[300] = "";
        extractParens(buf, inner, 300);
        return !evalCondSimp(inner);
    }

    static const struct
    {
        const char *s;
        int len;
    } ops[] = {
        {"!=", 2}, {">=", 2}, {"<=", 2}, {"==", 2}, {">", 1}, {"<", 1}, {NULL, 0}};
    char opS[3] = "";
    int opL = 0;
    char *opPtr = NULL;
    for (int i = 0; ops[i].s; i++)
    {
        char *p = strstr(buf, ops[i].s);
        if (p)
        {
            opPtr = p;
            strcpy(opS, ops[i].s);
            opL = ops[i].len;
            break;
        }
    }
    if (!opPtr)
    {
        char r[256] = "";
        evalFunc(buf, r);
        return atoi(r) != 0;
    }

    char esq[200] = "", dir[200] = "";
    int el = (int)(opPtr - buf);
    strncpy(esq, buf, el < 199 ? el : 199);
    trim(esq);
    strncpy(dir, opPtr + opL, 199);
    trim(dir);

    int dStr = 0;
    if (dir[0] == '"')
    {
        dStr = 1;
        memmove(dir, dir + 1, strlen(dir));
        int dl = (int)strlen(dir);
        if (dl > 0 && dir[dl - 1] == '"')
            dir[dl - 1] = '\0';
    }

    char ve[256] = "", vd[256] = "";
    evalFunc(esq, ve);
    if (!dStr)
        evalFunc(dir, vd);
    else
        strncpy(vd, dir, 255);

    int bothNum = !dStr && isNumStr(ve) && isNumStr(vd);
    if (bothNum)
    {
        int a = atoi(ve), b = atoi(vd);
        if (!strcmp(opS, "=="))
            return a == b;
        if (!strcmp(opS, "!="))
            return a != b;
        if (!strcmp(opS, ">"))
            return a > b;
        if (!strcmp(opS, "<"))
            return a < b;
        if (!strcmp(opS, ">="))
            return a >= b;
        if (!strcmp(opS, "<="))
            return a <= b;
    }
    int c = strcmp(ve, vd);
    if (!strcmp(opS, "=="))
        return c == 0;
    if (!strcmp(opS, "!="))
        return c != 0;
    if (!strcmp(opS, ">"))
        return c > 0;
    if (!strcmp(opS, "<"))
        return c < 0;
    if (!strcmp(opS, ">="))
        return c >= 0;
    if (!strcmp(opS, "<="))
        return c <= 0;
    return 0;
}

static int evalCond(char *cond)
{
    char buf[400];
    strncpy(buf, cond, 399);
    buf[399] = '\0';
    trim(buf);
    char *orP = strstr(buf, " or ");
    if (orP)
    {
        char e[200] = "", d[200] = "";
        int el = (int)(orP - buf);
        strncpy(e, buf, el < 199 ? el : 199);
        strncpy(d, orP + 4, 199);
        return evalCond(e) || evalCond(d);
    }
    char *anP = strstr(buf, " and ");
    if (anP)
    {
        char e[200] = "", d[200] = "";
        int el = (int)(anP - buf);
        strncpy(e, buf, el < 199 ? el : 199);
        strncpy(d, anP + 5, 199);
        return evalCond(e) && evalCond(d);
    }
    return evalCondSimp(buf);
}

static void push(TipoNivel tipo, int exec, int res, int lw, char *cw)
{
    if (topo >= MAX_STACK - 1)
        return;
    topo++;
    pilha[topo].tipo = tipo;
    pilha[topo].exec = exec;
    pilha[topo].ifRes = res;
    pilha[topo].elseUsado = 0;
    pilha[topo].linhaWhile = lw;
    pilha[topo].condWhile[0] = '\0';
    if (cw)
        strncpy(pilha[topo].condWhile, cw, 299);
}

static void popStack()
{
    if (topo >= 0)
        topo--;
}

static int canExec()
{
    for (int i = 0; i <= topo; i++)
        if (!pilha[i].exec)
            return 0;
    return 1;
}

static void gfxInit()
{
    HDC hdc = GetDC(hwnd);
    hdcBuffer = CreateCompatibleDC(hdc);
    hBitmap = CreateCompatibleBitmap(hdc, WIN_W, WIN_H);
    SelectObject(hdcBuffer, hBitmap);
    ReleaseDC(hwnd, hdc);
    RECT r = {0, 0, WIN_W, WIN_H};
    HBRUSH br = CreateSolidBrush(RGB(bgR, bgG, bgB));
    FillRect(hdcBuffer, &r, br);
    DeleteObject(br);
}

static void gfxClear()
{
    if (!hdcBuffer)
        return;
    RECT r = {0, 0, WIN_W, WIN_H};
    HBRUSH br = CreateSolidBrush(RGB(bgR, bgG, bgB));
    FillRect(hdcBuffer, &r, br);
    DeleteObject(br);
    nShapes = 0;
}

static void drawShape(Shape *s)
{
    if (!hdcBuffer)
        return;
    HPEN pen = CreatePen(PS_SOLID, 2, s->cor);
    HBRUSH brush = s->preenchido
                       ? CreateSolidBrush(s->cor)
                       : (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdcBuffer, pen);
    SelectObject(hdcBuffer, brush);

    switch (s->tipo)
    {
    case SH_RECT:
        Rectangle(hdcBuffer, s->x, s->y, s->x + s->w, s->y + s->h);
        break;
    case SH_CIRCLE:
        Ellipse(hdcBuffer, s->x - s->r, s->y - s->r, s->x + s->r, s->y + s->r);
        break;
    case SH_LINE:
        MoveToEx(hdcBuffer, s->x, s->y, NULL);
        LineTo(hdcBuffer, s->x2, s->y2);
        break;
    case SH_TRIANGLE:
    {
        POINT pts[3] = {{s->x, s->y}, {s->x2, s->y2}, {s->x3, s->y3}};
        Polygon(hdcBuffer, pts, 3);
        break;
    }
    case SH_TEXT:
    {
        SetTextColor(hdcBuffer, s->cor);
        SetBkMode(hdcBuffer, TRANSPARENT);
        HFONT font = CreateFont(
            s->h ? s->h : 20, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH, "Arial");
        HFONT oldFont = (HFONT)SelectObject(hdcBuffer, font);
        TextOut(hdcBuffer, s->x, s->y, s->texto, (int)strlen(s->texto));
        SelectObject(hdcBuffer, oldFont);
        DeleteObject(font);
        break;
    }
    case SH_PIXEL:
        SetPixel(hdcBuffer, s->x, s->y, s->cor);
        break;
    }
    DeleteObject(pen);
    if (s->preenchido)
        DeleteObject(brush);
}

static void gfxRender()
{
    if (!hdcBuffer || !hwnd)
        return;
    HDC hdc = GetDC(hwnd);
    BitBlt(hdc, 0, 0, WIN_W, WIN_H, hdcBuffer, 0, 0, SRCCOPY);
    ReleaseDC(hwnd, hdc);
}

static void addShape(Shape s)
{
    if (nShapes < MAX_SHAPES)
    {
        shapes[nShapes++] = s;
        drawShape(&shapes[nShapes - 1]);
    }
}

static void processMessages()
{
    mouse_click = 0;
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            exit(0);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static void procWrite(const char *conteudo, int nl)
{
    char buf[512];
    strncpy(buf, conteudo, 511);
    buf[511] = '\0';
    trim(buf);
    char out[512] = "";
    evalFunc(buf, out);
    if (nl)
        printf("%s\n", out);
    else
    {
        printf("%s", out);
        fflush(stdout);
    }
}

static int buscarMetodo(const char *classeNome, const char *metodo)
{
    char cur[64];
    strncpy(cur, classeNome, 63);
    while (strlen(cur) > 0)
    {
        Classe *c = getClasse(cur);
        if (!c)
            break;
        for (int i = 0; i < c->nMetodos; i++)
        {
            int fi = c->metodos[i];
            if (!strcmp(funcs[fi].nome, metodo))
                return fi;
        }
        strncpy(cur, c->pai, 63);
    }
    return -1;
}

static int criarObjeto(const char *classeNome, char *argsStr)
{
    Classe *c = getClasse(classeNome);
    if (!c)
    {
        erroF(ERR_CLASSE, "classe '%s' nao foi definida", classeNome);
        return -1;
    }
    if (nObjetos >= MAX_OBJECTS)
    {
        erroMAN(ERR_LIMITE, "numero maximo de objetos atingido (limite: 256) — delete objetos nao usados");
        return -1;
    }

    Objeto *o = &objetos[nObjetos++];
    memset(o, 0, sizeof(Objeto));
    o->id = objNextId++;
    o->ativo = 1;
    strncpy(o->classeNome, classeNome, 63);

    char hier[16][64];
    int nh = 0;
    char cur[64];
    strncpy(cur, classeNome, 63);
    while (strlen(cur) > 0 && nh < 16)
    {
        strncpy(hier[nh++], cur, 63);
        Classe *cc = getClasse(cur);
        if (!cc || !strlen(cc->pai))
            break;
        strncpy(cur, cc->pai, 63);
    }
    for (int h = nh - 1; h >= 0; h--)
    {
        Classe *cc = getClasse(hier[h]);
        if (!cc)
            continue;
        for (int i = 0; i < cc->nCampos; i++)
            setObjCampo(o, cc->campos[i], cc->valoresCampos[i]);
    }

    int ctorIdx = c->construtorIdx;
    if (ctorIdx == -1)
        ctorIdx = buscarMetodo(classeNome, "init");
    if (ctorIdx != -1)
    {
        if (topoThis < MAX_STACK - 1)
            thisPilha[++topoThis] = thisObjId;
        thisObjId = o->id;
        chamarFuncao(ctorIdx, argsStr);

        if (topoThis >= 0)
            thisObjId = thisPilha[topoThis--];
    }
    return o->id;
}

static void execLinha(char *cmd);

static void chamarFuncao(int idx, char *argsStr)
{
    if (topoEsc >= MAX_SCOPE - 1)
    {
        erroMAN(ERR_LIMITE, "estouro de escopo: recursao muito profunda (limite: 64 niveis) — verifique se ha chamada infinita");
        return;
    }

    Func *f = &funcs[idx];

    char resolvedArgs[MAX_PARAMS][256];
    int nResolved = 0;
    if (f->nParams > 0 && argsStr && argsStr[0])
    {
        char argBuf[1024];
        strncpy(argBuf, argsStr, 1023);
        argBuf[1023] = '\0';
        char raw[MAX_PARAMS][300];
        nResolved = splitArgs(argBuf, raw, f->nParams);
        for (int i = 0; i < nResolved; i++)
        {
            char res[256] = "";
            evalFunc(raw[i], res);
            strncpy(resolvedArgs[i], res, 255);
            resolvedArgs[i][255] = '\0';
        }
    }

    int savedCursor = cursor;
    int savedTopo = topo;
    int savedTopoEsc = topoEsc;
    int savedFlagReturn = flagReturn;
    char savedRetval[256];
    strncpy(savedRetval, retval, 255);

    topoEsc++;
    escopos[topoEsc].n = 0;

    for (int i = 0; i < nResolved && i < f->nParams; i++)
    {
        char pname[64];
        strncpy(pname, f->params[i], 63);
        setVar(pname, resolvedArgs[i]);
    }

    retval[0] = '\0';
    flagReturn = 0;
    cursor = f->linhaInicio;

    int funcDepth = 0;
    while (cursor < nLines && !flagReturn)
    {
        char linha[MAX_LINE];
        strncpy(linha, progLines[cursor], MAX_LINE - 1);
        cursor++;

        char tmpLo[MAX_LINE];
        strncpy(tmpLo, linha, MAX_LINE - 1);
        toLow(tmpLo);
        trim(tmpLo);
        char *com = strchr(tmpLo, '#');
        if (com)
            *com = '\0';
        trim(tmpLo);

        if (!strcmp(tmpLo, "begin"))
        {
            funcDepth++;
        }
        else if (!strcmp(tmpLo, "end"))
        {
            if (funcDepth == 0)
                break;
            funcDepth--;
        }
        execLinha(linha);
    }

    char funcRetval[256];
    strncpy(funcRetval, retval, 255);

    cursor = savedCursor;
    topo = savedTopo;
    topoEsc = savedTopoEsc;
    flagReturn = savedFlagReturn;
    strncpy(retval, funcRetval, 255);
}

static void execLinhaSimples(char *cmd);

static void execLinha(char *cmd)
{
    cmd[strcspn(cmd, "\n")] = '\0';

    {
        int inQ = 0;
        for (int i = 0; cmd[i]; i++)
        {
            if (cmd[i] == '"')
                inQ = !inQ;
            if (!inQ && cmd[i] == '#')
            {
                cmd[i] = '\0';
                break;
            }
        }
    }
    trim(cmd);
    if (!strlen(cmd))
        return;

    int inQ = 0, dep = 0, temSemicolon = 0;
    for (int i = 0; cmd[i]; i++)
    {
        if (cmd[i] == '"')
            inQ = !inQ;
        if (!inQ && (cmd[i] == '(' || cmd[i] == '['))
            dep++;
        if (!inQ && (cmd[i] == ')' || cmd[i] == ']'))
            dep--;
        if (!inQ && dep == 0 && cmd[i] == ';')
        {
            temSemicolon = 1;
            break;
        }
    }

    if (temSemicolon)
    {
        char buf[MAX_LINE];
        strncpy(buf, cmd, MAX_LINE - 1);
        char parte[MAX_LINE];
        int pL = 0;
        inQ = 0;
        dep = 0;
        for (int i = 0;; i++)
        {
            char c = buf[i];
            if (c == '"')
                inQ = !inQ;
            if (!inQ && (c == '(' || c == '['))
                dep++;
            if (!inQ && (c == ')' || c == ']'))
                dep--;
            int isSep = (!inQ && dep == 0 && c == ';') || c == '\0';
            if (isSep)
            {
                parte[pL] = '\0';
                trim(parte);
                if (strlen(parte) > 0)
                    execLinhaSimples(parte);
                pL = 0;
                if (c == '\0')
                    break;
            }
            else
            {
                if (pL < MAX_LINE - 1)
                    parte[pL++] = c;
            }
        }
        return;
    }
    execLinhaSimples(cmd);
}

static int rcCarregarTextura(const char *arquivo)
{
    if (rcNTex >= RC_TEX_MAX)
    {
        erroF(ERR_LIMITE, "limite de texturas do raycaster atingido (maximo: %d)", RC_TEX_MAX);
        return -1;
    }

    wchar_t wpath[512];
    MultiByteToWideChar(CP_UTF8, 0, arquivo, -1, wpath, 512);

    GpBitmap *bmp = NULL;
    GpStatus st = GdipCreateBitmapFromFile(wpath, &bmp);
    if (st != Ok || !bmp)
    {
        erroF(ERR_ARQUIVO, "nao foi possivel carregar a textura '%s' — verifique se o arquivo PNG existe (codigo GDI+: %d)", arquivo, (int)st);
        return -1;
    }

    UINT origW = 0, origH = 0;
    GdipGetImageWidth((GpImage *)bmp, &origW);
    GdipGetImageHeight((GpImage *)bmp, &origH);

    RcTextura *tex = &rcTex[rcNTex];
    memset(tex->data, 0, sizeof(tex->data));
    tex->w = RC_TEX_W;
    tex->h = RC_TEX_H;
    tex->ativo = 1;

    for (int ty = 0; ty < RC_TEX_H; ty++)
    {
        for (int tx = 0; tx < RC_TEX_W; tx++)
        {

            UINT sx = (UINT)((tx * origW) / RC_TEX_W);
            UINT sy = (UINT)((ty * origH) / RC_TEX_H);
            if (sx >= origW)
                sx = origW - 1;
            if (sy >= origH)
                sy = origH - 1;

            ARGB argb = 0;
            GdipBitmapGetPixel(bmp, sx, sy, &argb);

            int idx = (ty * RC_TEX_W + tx) * 4;
            tex->data[idx + 0] = (unsigned char)((argb >> 16) & 0xFF);
            tex->data[idx + 1] = (unsigned char)((argb >> 8) & 0xFF);
            tex->data[idx + 2] = (unsigned char)(argb & 0xFF);
            tex->data[idx + 3] = (unsigned char)((argb >> 24) & 0xFF);
        }
    }

    GdipDisposeImage((GpImage *)bmp);
    printf("rcloadtex: textura %d carregada de '%s' (%dx%d -> %dx%d)\n",
           rcNTex, arquivo, origW, origH, RC_TEX_W, RC_TEX_H);
    return rcNTex++;
}

static COLORREF rcTexPixel(int texIdx, int tx, int ty, int lado, double dist)
{
    unsigned char r, g, b;
    if (texIdx < 0 || texIdx >= rcNTex || !rcTex[texIdx].ativo)
    {
        int c = ((tx >> 3) ^ (ty >> 3)) & 1;
        int base = 80 + texIdx * 40;
        if (base > 220)
            base = 220;
        return c ? RGB(base, base / 2, base / 3) : RGB(base / 2, base / 3, base);
    }
    if (tx < 0)
        tx = 0;
    if (tx >= RC_TEX_W)
        tx = RC_TEX_W - 1;
    if (ty < 0)
        ty = 0;
    if (ty >= RC_TEX_H)
        ty = RC_TEX_H - 1;
    int idx = (ty * RC_TEX_W + tx) * 4;
    r = rcTex[texIdx].data[idx + 0];
    g = rcTex[texIdx].data[idx + 1];
    b = rcTex[texIdx].data[idx + 2];
    if (lado == 1)
    {
        r = (unsigned char)(r * 2 / 3);
        g = (unsigned char)(g * 2 / 3);
        b = (unsigned char)(b * 2 / 3);
    }
    double fog = 1.0 - dist * 0.04;
    if (fog < 0.1)
        fog = 0.1;
    return RGB((unsigned char)(r * fog), (unsigned char)(g * fog), (unsigned char)(b * fog));
}

static DWORD rcFB[WIN_W * WIN_H];

static inline void fbSet(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{

    rcFB[y * WIN_W + x] = (DWORD)((r << 16) | (g << 8) | b);
}

static void fbFlush()
{
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = WIN_W;
    bmi.bmiHeader.biHeight = -WIN_H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(hdcBuffer,
                      0, 0, WIN_W, WIN_H,
                      0, 0, 0, WIN_H,
                      rcFB, &bmi, DIB_RGB_COLORS);
}

#define M3D_MAX_VERTS 512
#define M3D_MAX_EDGES 1024
#define M3D_MAX_FACES 256
#define M3D_MAX_MESHES 64

typedef struct
{
    double x, y, z;
} V3;
typedef struct
{
    int a, b;
} Edge3;
typedef struct
{
    int v[4];
    int nv;
} Face3;

typedef struct
{
    V3 verts[M3D_MAX_VERTS];
    int nVerts;
    Edge3 edges[M3D_MAX_EDGES];
    int nEdges;
    Face3 faces[M3D_MAX_FACES];
    int nFaces;
    double tx, ty, tz;
    double rx, ry, rz;
    double sx, sy, sz;
    unsigned char cr, cg, cb;
    int wireframe;
    int ativo;
    char nome[64];
} Mesh3;

static Mesh3 m3Meshes[M3D_MAX_MESHES];
static int m3NMeshes = 0;

static double m3CamX = 0.0, m3CamY = 0.0, m3CamZ = -8.0;
static double m3CamYaw = 0.0, m3CamPitch = 0.0;
static double m3Fov = 500.0;
static int m3Active = 0;

static double m3ZBuf[WIN_W * WIN_H];

static void m3Reset()
{
    m3NMeshes = 0;
    m3CamX = 0.0;
    m3CamY = 0.0;
    m3CamZ = -8.0;
    m3CamYaw = 0.0;
    m3CamPitch = 0.0;
    m3Fov = 500.0;
    m3Active = 1;
}

static Mesh3 *m3GetMesh(const char *nome)
{
    for (int i = 0; i < m3NMeshes; i++)
        if (m3Meshes[i].ativo && !strcmp(m3Meshes[i].nome, nome))
            return &m3Meshes[i];
    return NULL;
}

static Mesh3 *m3NewMesh(const char *nome)
{
    Mesh3 *m = m3GetMesh(nome);
    if (m)
        return m;
    if (m3NMeshes >= M3D_MAX_MESHES)
        return NULL;
    m = &m3Meshes[m3NMeshes++];
    memset(m, 0, sizeof(Mesh3));
    strncpy(m->nome, nome, 63);
    m->sx = m->sy = m->sz = 1.0;
    m->cr = 255;
    m->cg = 255;
    m->cb = 255;
    m->wireframe = 1;
    m->ativo = 1;
    m3Active = 1;
    return m;
}

static void m3ClearGeometry(Mesh3 *m)
{
    m->nVerts = 0;
    m->nEdges = 0;
    m->nFaces = 0;
}

static V3 m3TransformVert(Mesh3 *m, int vi)
{
    double x = m->verts[vi].x * m->sx;
    double y = m->verts[vi].y * m->sy;
    double z = m->verts[vi].z * m->sz;

    double cx = cos(m->rx), sx2 = sin(m->rx);
    double y2 = cx * y - sx2 * z;
    double z2 = sx2 * y + cx * z;
    y = y2;
    z = z2;

    double cy = cos(m->ry), sy2 = sin(m->ry);
    double x2 = cy * x + sy2 * z;
    double z3 = -sy2 * x + cy * z;
    x = x2;
    z = z3;

    double cz = cos(m->rz), sz2 = sin(m->rz);
    double xa = cz * x - sz2 * y;
    double ya = sz2 * x + cz * y;
    x = xa;
    y = ya;

    x += m->tx;
    y += m->ty;
    z += m->tz;

    double cpitch = cos(m3CamPitch), spitch = sin(m3CamPitch);
    double cyaw = cos(m3CamYaw), syaw = sin(m3CamYaw);

    double rx2 = x - m3CamX;
    double ry2 = y - m3CamY;
    double rz2 = z - m3CamZ;

    double rx3 = cyaw * rx2 + syaw * rz2;
    double rz3 = -syaw * rx2 + cyaw * rz2;
    double ry3 = cpitch * ry2 - spitch * rz3;
    double rz4 = spitch * ry2 + cpitch * rz3;

    V3 r;
    r.x = rx3;
    r.y = ry3;
    r.z = rz4;
    return r;
}

static int m3Project(V3 v, int *sx2, int *sy2)
{
    if (v.z < 0.1)
        return 0;
    *sx2 = (int)(WIN_W / 2 + v.x * m3Fov / v.z);
    *sy2 = (int)(WIN_H / 2 - v.y * m3Fov / v.z);
    return 1;
}

static void m3DrawLine3D(int x0, int y0, double z0, int x1, int y1, double z1,
                         unsigned char r, unsigned char g, unsigned char b)
{
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx2 = x0 < x1 ? 1 : -1, sy2 = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int steps = dx > dy ? dx : dy;
    if (steps == 0)
        steps = 1;
    int cx = x0, cy = y0, s = 0;
    while (1)
    {
        if (cx >= 0 && cx < WIN_W && cy >= 0 && cy < WIN_H)
        {
            double zt = z0 + (z1 - z0) * (double)s / steps;
            int idx2 = cy * WIN_W + cx;
            if (zt < m3ZBuf[idx2])
            {
                m3ZBuf[idx2] = zt;
                rcFB[idx2] = (DWORD)((r << 16) | (g << 8) | b);
            }
        }
        if (cx == x1 && cy == y1)
            break;
        int e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            cx += sx2;
        }
        if (e2 < dx)
        {
            err += dx;
            cy += sy2;
        }
        s++;
    }
}

static void m3FillTriangle(int x0, int y0, double z0,
                           int x1, int y1, double z1,
                           int x2, int y2, double z2,
                           unsigned char r, unsigned char g, unsigned char b)
{
    if (y0 > y1)
    {
        int t = y0;
        y0 = y1;
        y1 = t;
        t = x0;
        x0 = x1;
        x1 = t;
        double d = z0;
        z0 = z1;
        z1 = d;
    }
    if (y0 > y2)
    {
        int t = y0;
        y0 = y2;
        y2 = t;
        t = x0;
        x0 = x2;
        x2 = t;
        double d = z0;
        z0 = z2;
        z2 = d;
    }
    if (y1 > y2)
    {
        int t = y1;
        y1 = y2;
        y2 = t;
        t = x1;
        x1 = x2;
        x2 = t;
        double d = z1;
        z1 = z2;
        z2 = d;
    }
    int totalH = y2 - y0;
    if (totalH == 0)
        return;
    for (int i = 0; i < totalH; i++)
    {
        int second = (i > y1 - y0) || (y1 == y0);
        int segH = second ? (y2 - y1) : (y1 - y0);
        if (segH == 0)
            continue;
        double alpha = (double)i / totalH;
        double beta = (double)(i - (second ? (y1 - y0) : 0)) / segH;
        double ax = x0 + (x2 - x0) * alpha, bx = second ? x1 + (x2 - x1) * beta : x0 + (x1 - x0) * beta;
        double az = z0 + (z2 - z0) * alpha, bz = second ? z1 + (z2 - z1) * beta : z0 + (z1 - z0) * beta;
        if (ax > bx)
        {
            double t = ax;
            ax = bx;
            bx = t;
            t = az;
            az = bz;
            bz = t;
        }
        int cy = y0 + i;
        if (cy < 0 || cy >= WIN_H)
            continue;
        for (int cx = (int)ax; cx <= (int)bx; cx++)
        {
            if (cx < 0 || cx >= WIN_W)
                continue;
            double t2 = (bx - ax) > 0.0001 ? (cx - ax) / (bx - ax) : 0;
            double zt = az + (bz - az) * t2;
            int idx2 = cy * WIN_W + cx;
            if (zt < m3ZBuf[idx2])
            {
                m3ZBuf[idx2] = zt;
                rcFB[idx2] = (DWORD)((r << 16) | (g << 8) | b);
            }
        }
    }
}

static void m3RenderAll()
{

    DWORD bgColor = (DWORD)(((unsigned char)bgR << 16) | ((unsigned char)bgG << 8) | (unsigned char)bgB);
    for (int i = 0; i < WIN_W * WIN_H; i++)
    {
        rcFB[i] = bgColor;
        m3ZBuf[i] = 1e18;
    }

    for (int mi = 0; mi < m3NMeshes; mi++)
    {
        Mesh3 *m = &m3Meshes[mi];
        if (!m->ativo)
            continue;

        V3 tv[M3D_MAX_VERTS];
        int px[M3D_MAX_VERTS], py[M3D_MAX_VERTS];
        int vis[M3D_MAX_VERTS];
        for (int vi = 0; vi < m->nVerts; vi++)
        {
            tv[vi] = m3TransformVert(m, vi);
            vis[vi] = m3Project(tv[vi], &px[vi], &py[vi]);
        }

        unsigned char r = m->cr, g = m->cg, b = m->cb;

        if (!m->wireframe && m->nFaces > 0)
        {
            for (int fi = 0; fi < m->nFaces; fi++)
            {
                Face3 *f = &m->faces[fi];
                if (f->nv < 3)
                    continue;
                int allvis = 1;
                for (int k = 0; k < f->nv; k++)
                    if (!vis[f->v[k]])
                    {
                        allvis = 0;
                        break;
                    }
                if (!allvis)
                    continue;
                double lit = 1.0;
                if (f->nv >= 3)
                {
                    int ia = f->v[0], ib = f->v[1], ic = f->v[2];
                    double ax2 = tv[ib].x - tv[ia].x, ay2 = tv[ib].y - tv[ia].y, az2 = tv[ib].z - tv[ia].z;
                    double bx2 = tv[ic].x - tv[ia].x, by2 = tv[ic].y - tv[ia].y, bz2 = tv[ic].z - tv[ia].z;
                    double nx2 = ay2 * bz2 - az2 * by2, ny2 = az2 * bx2 - ax2 * bz2, nz2 = ax2 * by2 - ay2 * bx2;
                    double nl = sqrt(nx2 * nx2 + ny2 * ny2 + nz2 * nz2);
                    if (nl > 0.0001)
                    {
                        double dot = -(nx2 * 0 + ny2 * 0 + nz2 * 1.0) / nl;

                        if (dot < 0.0)
                            continue;
                        lit = 0.25 + 0.75 * dot;
                    }
                }
                unsigned char fr = (unsigned char)(r * lit);
                unsigned char fg2 = (unsigned char)(g * lit);
                unsigned char fb = (unsigned char)(b * lit);
                int ia = f->v[0];
                for (int k = 1; k < f->nv - 1; k++)
                {
                    int ib2 = f->v[k], ic2 = f->v[k + 1];
                    m3FillTriangle(px[ia], py[ia], tv[ia].z,
                                   px[ib2], py[ib2], tv[ib2].z,
                                   px[ic2], py[ic2], tv[ic2].z,
                                   fr, fg2, fb);
                }
            }
            for (int ei = 0; ei < m->nEdges; ei++)
            {
                int a2 = m->edges[ei].a, b2 = m->edges[ei].b;
                if (!vis[a2] || !vis[b2])
                    continue;
                unsigned char dr = (unsigned char)(r > 40 ? r - 40 : 0);
                unsigned char dg = (unsigned char)(g > 40 ? g - 40 : 0);
                unsigned char db = (unsigned char)(b > 40 ? b - 40 : 0);
                m3DrawLine3D(px[a2], py[a2], tv[a2].z - 0.001,
                             px[b2], py[b2], tv[b2].z - 0.001, dr, dg, db);
            }
        }
        else
        {
            for (int ei = 0; ei < m->nEdges; ei++)
            {
                int a2 = m->edges[ei].a, b2 = m->edges[ei].b;
                if (!vis[a2] || !vis[b2])
                    continue;
                m3DrawLine3D(px[a2], py[a2], tv[a2].z,
                             px[b2], py[b2], tv[b2].z, r, g, b);
            }
        }
    }
    fbFlush();
}

static void m3AddCube(Mesh3 *m, double size)
{
    double h = size / 2.0;
    double vs[8][3] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}};
    int base = m->nVerts;
    for (int i = 0; i < 8; i++)
    {
        if (m->nVerts >= M3D_MAX_VERTS)
            break;
        m->verts[m->nVerts].x = vs[i][0];
        m->verts[m->nVerts].y = vs[i][1];
        m->verts[m->nVerts].z = vs[i][2];
        m->nVerts++;
    }
    int edges[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (int i = 0; i < 12; i++)
    {
        if (m->nEdges >= M3D_MAX_EDGES)
            break;
        m->edges[m->nEdges].a = base + edges[i][0];
        m->edges[m->nEdges].b = base + edges[i][1];
        m->nEdges++;
    }
    int faceIdx[][4] = {{0, 1, 2, 3}, {4, 7, 6, 5}, {0, 4, 5, 1}, {2, 6, 7, 3}, {1, 5, 6, 2}, {0, 3, 7, 4}};
    for (int i = 0; i < 6; i++)
    {
        if (m->nFaces >= M3D_MAX_FACES)
            break;
        Face3 *f = &m->faces[m->nFaces++];
        f->nv = 4;
        for (int j = 0; j < 4; j++)
            f->v[j] = base + faceIdx[i][j];
    }
}

static void m3AddSphere(Mesh3 *m, double r2, int stacks, int slices)
{
    if (stacks < 2)
        stacks = 2;
    if (slices < 3)
        slices = 3;
    int base = m->nVerts;
    if (m->nVerts >= M3D_MAX_VERTS)
        return;
    m->verts[m->nVerts].x = 0;
    m->verts[m->nVerts].y = r2;
    m->verts[m->nVerts].z = 0;
    m->nVerts++;
    for (int i = 1; i < stacks; i++)
    {
        double phi = M_PI * i / stacks;
        for (int j = 0; j < slices; j++)
        {
            double theta = 2 * M_PI * j / slices;
            if (m->nVerts >= M3D_MAX_VERTS)
                break;
            m->verts[m->nVerts].x = r2 * sin(phi) * cos(theta);
            m->verts[m->nVerts].y = r2 * cos(phi);
            m->verts[m->nVerts].z = r2 * sin(phi) * sin(theta);
            m->nVerts++;
        }
    }
    if (m->nVerts >= M3D_MAX_VERTS)
        return;
    m->verts[m->nVerts].x = 0;
    m->verts[m->nVerts].y = -r2;
    m->verts[m->nVerts].z = 0;
    int bot = m->nVerts++;
    for (int j = 0; j < slices; j++)
    {
        if (m->nEdges >= M3D_MAX_EDGES)
            break;
        m->edges[m->nEdges].a = base;
        m->edges[m->nEdges].b = base + 1 + j;
        m->nEdges++;
    }
    for (int i = 0; i < stacks - 2; i++)
    {
        for (int j = 0; j < slices; j++)
        {
            int cur = base + 1 + i * slices + j;
            int nxt = base + 1 + i * slices + (j + 1) % slices;
            int dn = cur + slices;
            if (m->nEdges < M3D_MAX_EDGES)
            {
                m->edges[m->nEdges].a = cur;
                m->edges[m->nEdges].b = nxt;
                m->nEdges++;
            }
            if (m->nEdges < M3D_MAX_EDGES)
            {
                m->edges[m->nEdges].a = cur;
                m->edges[m->nEdges].b = dn;
                m->nEdges++;
            }
        }
    }
    int lastRow = base + 1 + (stacks - 2) * slices;
    for (int j = 0; j < slices; j++)
    {
        int cur = lastRow + j;
        int nxt = lastRow + (j + 1) % slices;
        if (m->nEdges < M3D_MAX_EDGES)
        {
            m->edges[m->nEdges].a = cur;
            m->edges[m->nEdges].b = nxt;
            m->nEdges++;
        }
        if (m->nEdges < M3D_MAX_EDGES)
        {
            m->edges[m->nEdges].a = cur;
            m->edges[m->nEdges].b = bot;
            m->nEdges++;
        }
    }

    for (int i = 0; i < stacks - 2; i++)
    {
        for (int j = 0; j < slices; j++)
        {
            int cur = base + 1 + i * slices + j;
            int nxt = base + 1 + i * slices + (j + 1) % slices;
            int dn = cur + slices;
            int dnx = nxt + slices;
            if (m->nFaces < M3D_MAX_FACES)
            {
                Face3 *f = &m->faces[m->nFaces++];
                f->nv = 4;
                f->v[0] = cur;
                f->v[1] = nxt;
                f->v[2] = dnx;
                f->v[3] = dn;
            }
        }
    }

    for (int j = 0; j < slices; j++)
    {
        if (m->nFaces < M3D_MAX_FACES)
        {
            Face3 *f = &m->faces[m->nFaces++];
            f->nv = 3;
            f->v[0] = base;
            f->v[1] = base + 1 + (j + 1) % slices;
            f->v[2] = base + 1 + j;
        }
    }

    for (int j = 0; j < slices; j++)
    {
        if (m->nFaces < M3D_MAX_FACES)
        {
            Face3 *f = &m->faces[m->nFaces++];
            f->nv = 3;
            f->v[0] = bot;
            f->v[1] = lastRow + j;
            f->v[2] = lastRow + (j + 1) % slices;
        }
    }
}

static void m3AddPyramid(Mesh3 *m, double base2, double height)
{
    double h = base2 / 2.0;
    int b = m->nVerts;
    if (m->nVerts + 5 > M3D_MAX_VERTS)
        return;
    double bs[4][3] = {{-h, 0, -h}, {h, 0, -h}, {h, 0, h}, {-h, 0, h}};
    for (int i = 0; i < 4; i++)
    {
        m->verts[m->nVerts].x = bs[i][0];
        m->verts[m->nVerts].y = bs[i][1];
        m->verts[m->nVerts].z = bs[i][2];
        m->nVerts++;
    }
    m->verts[m->nVerts].x = 0;
    m->verts[m->nVerts].y = height;
    m->verts[m->nVerts].z = 0;
    int apex = m->nVerts++;

    int es[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    for (int i = 0; i < 4; i++)
    {
        if (m->nEdges >= M3D_MAX_EDGES)
            break;
        m->edges[m->nEdges].a = b + es[i][0];
        m->edges[m->nEdges].b = b + es[i][1];
        m->nEdges++;
    }
    for (int i = 0; i < 4; i++)
    {
        if (m->nEdges >= M3D_MAX_EDGES)
            break;
        m->edges[m->nEdges].a = b + i;
        m->edges[m->nEdges].b = apex;
        m->nEdges++;
    }

    if (m->nFaces < M3D_MAX_FACES)
    {
        Face3 *f = &m->faces[m->nFaces++];
        f->nv = 4;
        f->v[0] = b;
        f->v[1] = b + 1;
        f->v[2] = b + 2;
        f->v[3] = b + 3;
    }
    for (int i = 0; i < 4; i++)
    {
        if (m->nFaces >= M3D_MAX_FACES)
            break;
        Face3 *f = &m->faces[m->nFaces++];
        f->nv = 3;
        f->v[0] = b + i;
        f->v[1] = b + (i + 1) % 4;
        f->v[2] = apex;
    }
}

static void m3AddCylinder(Mesh3 *m, double r2, double height, int slices)
{
    if (slices < 3)
        slices = 3;
    int b = m->nVerts;
    double h = height / 2.0;
    for (int i = 0; i < slices; i++)
    {
        if (m->nVerts + 2 > M3D_MAX_VERTS)
            break;
        double a = 2 * M_PI * i / slices;
        m->verts[m->nVerts].x = r2 * cos(a);
        m->verts[m->nVerts].y = h;
        m->verts[m->nVerts].z = r2 * sin(a);
        m->nVerts++;
        m->verts[m->nVerts].x = r2 * cos(a);
        m->verts[m->nVerts].y = -h;
        m->verts[m->nVerts].z = r2 * sin(a);
        m->nVerts++;
    }
    for (int i = 0; i < slices; i++)
    {
        int n = (i + 1) % slices;
        int top0 = b + i * 2, top1 = b + n * 2;
        int bot0 = b + i * 2 + 1, bot1 = b + n * 2 + 1;
        if (m->nEdges < M3D_MAX_EDGES)
        {
            m->edges[m->nEdges].a = top0;
            m->edges[m->nEdges].b = top1;
            m->nEdges++;
        }
        if (m->nEdges < M3D_MAX_EDGES)
        {
            m->edges[m->nEdges].a = bot0;
            m->edges[m->nEdges].b = bot1;
            m->nEdges++;
        }
        if (m->nEdges < M3D_MAX_EDGES)
        {
            m->edges[m->nEdges].a = top0;
            m->edges[m->nEdges].b = bot0;
            m->nEdges++;
        }

        if (m->nFaces < M3D_MAX_FACES)
        {
            Face3 *f = &m->faces[m->nFaces++];
            f->nv = 4;
            f->v[0] = top0;
            f->v[1] = bot0;
            f->v[2] = bot1;
            f->v[3] = top1;
        }
    }

    if (m->nVerts + 2 <= M3D_MAX_VERTS)
    {
        int topC = m->nVerts;
        m->verts[m->nVerts].x = 0;
        m->verts[m->nVerts].y = h;
        m->verts[m->nVerts].z = 0;
        m->nVerts++;
        int botC = m->nVerts;
        m->verts[m->nVerts].x = 0;
        m->verts[m->nVerts].y = -h;
        m->verts[m->nVerts].z = 0;
        m->nVerts++;
        for (int i = 0; i < slices; i++)
        {
            int n = (i + 1) % slices;
            if (m->nFaces < M3D_MAX_FACES)
            {
                Face3 *f = &m->faces[m->nFaces++];
                f->nv = 3;
                f->v[0] = topC;
                f->v[1] = b + n * 2;
                f->v[2] = b + i * 2;
            }
            if (m->nFaces < M3D_MAX_FACES)
            {
                Face3 *f = &m->faces[m->nFaces++];
                f->nv = 3;
                f->v[0] = botC;
                f->v[1] = b + i * 2 + 1;
                f->v[2] = b + n * 2 + 1;
            }
        }
    }
}

static void m3AddPlane(Mesh3 *m, double w2, double d2)
{
    double hw = w2 / 2.0, hd = d2 / 2.0;
    int b = m->nVerts;
    if (m->nVerts + 4 > M3D_MAX_VERTS)
        return;
    m->verts[m->nVerts].x = -hw;
    m->verts[m->nVerts].y = 0;
    m->verts[m->nVerts].z = -hd;
    m->nVerts++;
    m->verts[m->nVerts].x = hw;
    m->verts[m->nVerts].y = 0;
    m->verts[m->nVerts].z = -hd;
    m->nVerts++;
    m->verts[m->nVerts].x = hw;
    m->verts[m->nVerts].y = 0;
    m->verts[m->nVerts].z = hd;
    m->nVerts++;
    m->verts[m->nVerts].x = -hw;
    m->verts[m->nVerts].y = 0;
    m->verts[m->nVerts].z = hd;
    m->nVerts++;
    int es[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {0, 2}, {1, 3}};
    for (int i = 0; i < 6; i++)
    {
        if (m->nEdges >= M3D_MAX_EDGES)
            break;
        m->edges[m->nEdges].a = b + es[i][0];
        m->edges[m->nEdges].b = b + es[i][1];
        m->nEdges++;
    }
    if (m->nFaces < M3D_MAX_FACES)
    {
        Face3 *f = &m->faces[m->nFaces++];
        f->nv = 4;
        f->v[0] = b;
        f->v[1] = b + 1;
        f->v[2] = b + 2;
        f->v[3] = b + 3;
    }
}

static inline void fbTexPixel(int x, int y,
                              int texIdx, int tx, int ty,
                              int lado, double dist)
{
    unsigned char r, g, b;

    if (texIdx < 0 || texIdx >= rcNTex || !rcTex[texIdx].ativo)
    {

        int c = ((tx >> 3) ^ (ty >> 3)) & 1;
        int base = 80 + (texIdx + 1) * 35;
        if (base > 220)
            base = 220;
        r = c ? (unsigned char)base : (unsigned char)(base / 2);
        g = c ? (unsigned char)(base / 2) : (unsigned char)(base / 3);
        b = c ? (unsigned char)(base / 3) : (unsigned char)base;
    }
    else
    {
        if (tx < 0)
            tx = 0;
        if (tx >= RC_TEX_W)
            tx = RC_TEX_W - 1;
        if (ty < 0)
            ty = 0;
        if (ty >= RC_TEX_H)
            ty = RC_TEX_H - 1;
        int idx = (ty * RC_TEX_W + tx) * 4;
        r = rcTex[texIdx].data[idx + 0];
        g = rcTex[texIdx].data[idx + 1];
        b = rcTex[texIdx].data[idx + 2];
    }

    if (lado == 1)
    {
        r = (unsigned char)(r * 2 / 3);
        g = (unsigned char)(g * 2 / 3);
        b = (unsigned char)(b * 2 / 3);
    }

    if (dist > 1.0)
    {
        int fog256 = (int)(256.0 / (dist * 0.18 + 1.0));
        if (fog256 < 26)
            fog256 = 26;
        if (fog256 > 256)
            fog256 = 256;
        r = (unsigned char)((r * fog256) >> 8);
        g = (unsigned char)((g * fog256) >> 8);
        b = (unsigned char)((b * fog256) >> 8);
    }

    fbSet(x, y, r, g, b);
}

static void rcRenderFrame()
{
    if (!hdcBuffer || !rcAtivo || rcMapW == 0)
        return;

    int sw = WIN_W, sh = WIN_H;
    int halfH = sh / 2;

    for (int y = 0; y < sh; y++)
    {
        int isCeil = (y < halfH);
        int fog256;
        unsigned char r, g, b;
        if (isCeil)
        {
            fog256 = 26 + (int)(230 * (halfH - y) / halfH);
            r = (unsigned char)((rcCeilR * fog256) >> 8);
            g = (unsigned char)((rcCeilG * fog256) >> 8);
            b = (unsigned char)((rcCeilB * fog256) >> 8);
        }
        else
        {
            fog256 = 26 + (int)(230 * (y - halfH) / halfH);
            r = (unsigned char)((rcFloorR * fog256) >> 8);
            g = (unsigned char)((rcFloorG * fog256) >> 8);
            b = (unsigned char)((rcFloorB * fog256) >> 8);
        }
        DWORD packed = (DWORD)((r << 16) | (g << 8) | b);
        DWORD *row = rcFB + y * sw;
        for (int x = 0; x < sw; x++)
            row[x] = packed;
    }

    for (int x = 0; x < sw; x++)
    {
        double camX = 2.0 * x / sw - 1.0;
        double rayDx = rcDx + rcCx * camX;
        double rayDy = rcDy + rcCy * camX;

        int mapX = (int)rcPx;
        int mapY = (int)rcPy;

        double deltaDistX = (fabs(rayDx) < 1e-20) ? 1e20 : fabs(1.0 / rayDx);
        double deltaDistY = (fabs(rayDy) < 1e-20) ? 1e20 : fabs(1.0 / rayDy);
        double sideDistX, sideDistY;

        int stepX, stepY, hit = 0, lado = 0;

        if (rayDx < 0)
        {
            stepX = -1;
            sideDistX = (rcPx - mapX) * deltaDistX;
        }
        else
        {
            stepX = 1;
            sideDistX = (mapX + 1.0 - rcPx) * deltaDistX;
        }
        if (rayDy < 0)
        {
            stepY = -1;
            sideDistY = (rcPy - mapY) * deltaDistY;
        }
        else
        {
            stepY = 1;
            sideDistY = (mapY + 1.0 - rcPy) * deltaDistY;
        }

        while (!hit)
        {
            if (sideDistX < sideDistY)
            {
                sideDistX += deltaDistX;
                mapX += stepX;
                lado = 0;
            }
            else
            {
                sideDistY += deltaDistY;
                mapY += stepY;
                lado = 1;
            }
            if (mapX < 0 || mapX >= rcMapW || mapY < 0 || mapY >= rcMapH)
            {
                hit = 2;
                break;
            }
            if (rcMapa[mapY][mapX] > 0)
                hit = 1;
        }

        if (hit != 1)
        {
            rcZBuffer[x] = 1e20;
            continue;
        }

        double perpWallDist;
        if (lado == 0)
            perpWallDist = (mapX - rcPx + (1 - stepX) * 0.5) / rayDx;
        else
            perpWallDist = (mapY - rcPy + (1 - stepY) * 0.5) / rayDy;
        if (perpWallDist < 0.001)
            perpWallDist = 0.001;
        rcZBuffer[x] = perpWallDist;

        int lineH = (int)(sh / perpWallDist);
        int drawStart = sh / 2 - lineH / 2;
        if (drawStart < 0)
            drawStart = 0;
        int drawEnd = sh / 2 + lineH / 2;
        if (drawEnd >= sh)
            drawEnd = sh - 1;

        int texIdx = rcMapa[mapY][mapX] - 1;

        double wallX = (lado == 0)
                           ? rcPy + perpWallDist * rayDy
                           : rcPx + perpWallDist * rayDx;
        wallX -= floor(wallX);

        int texX = (int)(wallX * RC_TEX_W);
        if ((lado == 0 && rayDx > 0) || (lado == 1 && rayDy < 0))
            texX = RC_TEX_W - texX - 1;
        if (texX < 0)
            texX = 0;
        if (texX >= RC_TEX_W)
            texX = RC_TEX_W - 1;

        int stepFP = (RC_TEX_H << 16) / (lineH ? lineH : 1);
        int texPosFP = (int)(((drawStart - sh / 2.0 + lineH / 2.0) * RC_TEX_H / (lineH ? lineH : 1)) * 65536.0);

        for (int y = drawStart; y <= drawEnd; y++)
        {
            int texY = (texPosFP >> 16) & (RC_TEX_H - 1);
            texPosFP += stepFP;
            fbTexPixel(x, y, texIdx, texX, texY, lado, perpWallDist);
        }
    }

    fbFlush();

    if (rcMinimap)
    {
        int sz = 5, ox = 8, oy = 8;

        HBRUSH brBg = CreateSolidBrush(RGB(10, 10, 10));
        RECT rcBg = {ox - 1, oy - 1,
                     ox + rcMapW * sz + 1, oy + rcMapH * sz + 1};
        FillRect(hdcBuffer, &rcBg, brBg);
        DeleteObject(brBg);

        HBRUSH brWall = CreateSolidBrush(RGB(190, 150, 100));
        HBRUSH brEmpty = CreateSolidBrush(RGB(35, 35, 35));
        for (int my = 0; my < rcMapH; my++)
        {
            for (int mx = 0; mx < rcMapW; mx++)
            {
                RECT cell = {ox + mx * sz, oy + my * sz,
                             ox + mx * sz + sz - 1, oy + my * sz + sz - 1};
                FillRect(hdcBuffer, &cell, rcMapa[my][mx] ? brWall : brEmpty);
            }
        }
        DeleteObject(brWall);
        DeleteObject(brEmpty);

        int px2 = ox + (int)(rcPx * sz);
        int py2 = oy + (int)(rcPy * sz);
        HBRUSH brP = CreateSolidBrush(RGB(255, 255, 0));
        RECT rP = {px2 - 2, py2 - 2, px2 + 2, py2 + 2};
        FillRect(hdcBuffer, &rP, brP);
        DeleteObject(brP);

        HPEN penDir = CreatePen(PS_SOLID, 1, RGB(255, 80, 80));
        HPEN oldPen = (HPEN)SelectObject(hdcBuffer, penDir);
        MoveToEx(hdcBuffer, px2, py2, NULL);
        LineTo(hdcBuffer, px2 + (int)(rcDx * sz * 1.5),
               py2 + (int)(rcDy * sz * 1.5));
        SelectObject(hdcBuffer, oldPen);
        DeleteObject(penDir);
    }
}

static void execLinhaSimples(char *cmd)
{
    trim(cmd);
    if (!strlen(cmd))
        return;

    char lo[MAX_LINE];
    strncpy(lo, cmd, MAX_LINE - 1);
    lo[MAX_LINE - 1] = '\0';
    toLow(lo);

    if (!strncmp(lo, "class ", 6))
    {
        if (nClasses >= MAX_CLASSES)
        {
            erroMAN(ERR_LIMITE, "numero maximo de classes atingido (limite: 64)");
            return;
        }
        char restC[200] = "";
        strncpy(restC, cmd + 6, 199);
        trim(restC);
        Classe *cl = &classes[nClasses];
        memset(cl, 0, sizeof(Classe));
        cl->construtorIdx = -1;
        cl->destrutorIdx = -1;

        {
            char restLo[200];
            strncpy(restLo, restC, 199);
            toLow(restLo);
            char *found = strstr(restLo, " extends ");
            if (found)
            {
                int off = (int)(found - restLo);
                strncpy(cl->nome, restC, off < 63 ? off : 63);
                trim(cl->nome);
                char paiStr[64] = "";
                strncpy(paiStr, restC + off + 9, 63);
                trim(paiStr);
                strncpy(cl->pai, paiStr, 63);
            }
            else
            {
                strncpy(cl->nome, restC, 63);
                trim(cl->nome);
            }
        }

        while (cursor < nLines)
        {
            char tmp[MAX_LINE];
            strncpy(tmp, progLines[cursor], MAX_LINE - 1);
            toLow(tmp);
            trim(tmp);
            cursor++;
            if (!strcmp(tmp, "begin"))
                break;
        }

        int dep = 1;
        while (cursor < nLines && dep > 0)
        {
            char mLine[MAX_LINE];
            strncpy(mLine, progLines[cursor], MAX_LINE - 1);
            char mLo[MAX_LINE];
            strncpy(mLo, mLine, MAX_LINE - 1);
            toLow(mLo);
            trim(mLo);
            cursor++;
            if (!strcmp(mLo, "begin"))
            {
                dep++;
                continue;
            }
            if (!strcmp(mLo, "end"))
            {
                if (--dep == 0)
                    break;
                continue;
            }
            trim(mLine);
            if (!strlen(mLine) || mLine[0] == '#')
                continue;

            if (!strncmp(mLo, "field ", 6))
            {
                char fRest[200] = "";
                strncpy(fRest, mLine + 6, 199);
                trim(fRest);
                char fNome[64] = "", fVal[200] = "";
                char *eq2 = strchr(fRest, '=');
                if (eq2)
                {
                    int nl2 = (int)(eq2 - fRest);
                    strncpy(fNome, fRest, nl2 < 63 ? nl2 : 63);
                    trim(fNome);
                    strncpy(fVal, eq2 + 1, 199);
                    trim(fVal);
                    if (fVal[0] == '"')
                    {
                        memmove(fVal, fVal + 1, strlen(fVal));
                        int fl = (int)strlen(fVal);
                        if (fl > 0 && fVal[fl - 1] == '"')
                            fVal[fl - 1] = '\0';
                    }
                }
                else
                {
                    strncpy(fNome, fRest, 63);
                    trim(fNome);
                }
                if (cl->nCampos < MAX_FIELDS)
                {
                    strncpy(cl->campos[cl->nCampos], fNome, 63);
                    strncpy(cl->valoresCampos[cl->nCampos], fVal, 255);
                    cl->nCampos++;
                }
                continue;
            }

            if (!strncmp(mLo, "method ", 7))
            {
                char mRest[200] = "";
                strncpy(mRest, mLine + 7, 199);
                trim(mRest);
                char mNome[64] = "", mParams[200] = "";
                char *lp = strchr(mRest, '('), *rp = strrchr(mRest, ')');
                if (lp && rp && rp > lp)
                {
                    int nl2 = (int)(lp - mRest);
                    strncpy(mNome, mRest, nl2 < 63 ? nl2 : 63);
                    trim(mNome);
                    int pl = (int)(rp - lp - 1);
                    strncpy(mParams, lp + 1, pl < 199 ? pl : 199);
                    trim(mParams);
                }
                else
                {
                    strncpy(mNome, mRest, 63);
                    trim(mNome);
                }

                if (nFuncs >= MAX_FUNCS)
                    continue;
                Func *f = &funcs[nFuncs];
                memset(f, 0, sizeof(Func));
                strncpy(f->nome, mNome, 63);
                strncpy(f->classeOwner, cl->nome, 63);
                if (strlen(mParams) > 0)
                {
                    char raw[MAX_PARAMS][300];
                    int np = splitArgs(mParams, raw, MAX_PARAMS);
                    for (int i = 0; i < np && f->nParams < MAX_PARAMS; i++)
                    {
                        trim(raw[i]);
                        if (strlen(raw[i]) > 0)
                            strncpy(f->params[f->nParams++], raw[i], 63);
                    }
                }
                int mdep = 0;
                while (cursor < nLines)
                {
                    char tmp2[MAX_LINE];
                    strncpy(tmp2, progLines[cursor], MAX_LINE - 1);
                    char tmpLo[MAX_LINE];
                    strncpy(tmpLo, tmp2, MAX_LINE - 1);
                    toLow(tmpLo);
                    trim(tmpLo);
                    cursor++;
                    if (!strcmp(tmpLo, "begin"))
                    {
                        if (++mdep == 1)
                            f->linhaInicio = cursor;
                    }
                    if (!strcmp(tmpLo, "end"))
                    {
                        if (--mdep <= 0)
                            break;
                    }
                }
                int fi = nFuncs++;
                if (cl->nMetodos < MAX_METHODS)
                    cl->metodos[cl->nMetodos++] = fi;
                if (!strcmp(mNome, "init"))
                    cl->construtorIdx = fi;
                if (!strcmp(mNome, "destroy"))
                    cl->destrutorIdx = fi;
                continue;
            }
        }
        nClasses++;
        return;
    }

    if (!strncmp(lo, "function ", 9))
    {
        char restF[200] = "";
        strncpy(restF, cmd + 9, 199);
        trim(restF);
        char nomeF[64] = "", paramStr[200] = "";
        char *lp = strchr(restF, '('), *rp = strrchr(restF, ')');
        if (lp && rp && rp > lp)
        {
            int nl2 = (int)(lp - restF);
            strncpy(nomeF, restF, nl2 < 63 ? nl2 : 63);
            trim(nomeF);
            int pl = (int)(rp - lp - 1);
            strncpy(paramStr, lp + 1, pl < 199 ? pl : 199);
            trim(paramStr);
        }
        else
        {
            strncpy(nomeF, restF, 63);
            trim(nomeF);
        }
        if (nFuncs >= MAX_FUNCS)
            return;
        Func *f = &funcs[nFuncs];
        memset(f, 0, sizeof(Func));
        strncpy(f->nome, nomeF, 63);
        if (strlen(paramStr) > 0)
        {
            char raw[MAX_PARAMS][300];
            int np = splitArgs(paramStr, raw, MAX_PARAMS);
            for (int i = 0; i < np && f->nParams < MAX_PARAMS; i++)
            {
                trim(raw[i]);
                if (strlen(raw[i]) > 0)
                    strncpy(f->params[f->nParams++], raw[i], 63);
            }
        }
        int dep = 0;
        int temBegin = 0;
        while (cursor < nLines)
        {
            char tmp[MAX_LINE];
            strncpy(tmp, progLines[cursor], MAX_LINE - 1);
            toLow(tmp);
            trim(tmp);
            cursor++;
            if (!strcmp(tmp, "begin"))
            {
                temBegin = 1;
                if (++dep == 1)
                    f->linhaInicio = cursor;
            }
            if (!strcmp(tmp, "end"))
            {
                if (--dep <= 0)
                    break;
            }
        }
        if (!temBegin)
            erroF(ERR_SINTAXE, "funcao '%s' definida sem 'begin' — coloque 'begin' na linha seguinte ao 'function'", f->nome);
        nFuncs++;
        return;
    }

    if (!strncmp(lo, "call ", 5))
    {
        if (!canExec())
            return;
        char restC[300] = "";
        strncpy(restC, cmd + 5, 299);
        trim(restC);
        char nomeF[64] = "", argsStr[256] = "";
        char *lp = strchr(restC, '('), *rp = strrchr(restC, ')');
        if (lp && rp && rp > lp)
        {
            int nl2 = (int)(lp - restC);
            strncpy(nomeF, restC, nl2 < 63 ? nl2 : 63);
            trim(nomeF);
            int al = (int)(rp - lp - 1);
            strncpy(argsStr, lp + 1, al < 255 ? al : 255);
            trim(argsStr);
        }
        else
        {
            strncpy(nomeF, restC, 63);
            trim(nomeF);
        }
        int idx = -1;
        for (int i = 0; i < nFuncs; i++)
            if (!strcmp(funcs[i].nome, nomeF) && !strlen(funcs[i].classeOwner))
            {
                idx = i;
                break;
            }
        if (idx != -1)
            chamarFuncao(idx, argsStr);
        else
            erroF(ERR_FUNC, "funcao '%s' nao foi definida — verifique o nome ou se a funcao esta antes do uso", nomeF);
        return;
    }

    {
        char *newPtr = strstr(lo, " new ");
        if (newPtr)
        {
            char *eqPtr = strchr(lo, '=');
            if (eqPtr && eqPtr < newPtr && canExec())
            {
                char varNome[64] = "";
                char *start = lo;
                if (!strncmp(start, "let ", 4))
                    start += 4;
                int off = (int)(start - lo), nl2 = (int)(eqPtr - lo);
                strncpy(varNome, cmd + off, nl2 - off < 63 ? nl2 - off : 63);
                trim(varNome);
                char *newCmd = cmd + (newPtr - lo) + 5;
                char classeNome[64] = "", argsStr[256] = "";
                char *lp = strchr(newCmd, '('), *rp = strrchr(newCmd, ')');
                if (lp && rp && rp > lp)
                {
                    int clen = (int)(lp - newCmd);
                    strncpy(classeNome, newCmd, clen < 63 ? clen : 63);
                    trim(classeNome);
                    int alen = (int)(rp - lp - 1);
                    strncpy(argsStr, lp + 1, alen < 255 ? alen : 255);
                }
                else
                {
                    strncpy(classeNome, newCmd, 63);
                    trim(classeNome);
                }
                int id = criarObjeto(classeNome, argsStr);
                if (id > 0)
                {
                    char idStr[20];
                    sprintf(idStr, "%d", id);
                    setVar(varNome, idStr);
                }
                return;
            }
        }
    }

    {
        char *dot = strchr(lo, '.');
        if (dot && canExec())
        {
            char before[64] = "";
            int bl = (int)(dot - lo);
            strncpy(before, lo, bl < 63 ? bl : 63);
            trim(before);
            int valid = 1;
            if (!strcmp(before, "fill") || !strcmp(before, "mouse"))
                valid = 0;
            for (int i = 0; before[i] && valid; i++)
                if (!isalnum((unsigned char)before[i]) && before[i] != '_')
                    valid = 0;
            if (valid && strlen(before) > 0)
            {
                char afterDot[256] = "";
                strncpy(afterDot, cmd + (dot - lo) + 1, 255);
                char *lp = strchr(afterDot, '(');
                if (lp)
                {
                    char metodo[64] = "";
                    int ml = (int)(lp - afterDot);
                    strncpy(metodo, afterDot, ml < 63 ? ml : 63);
                    trim(metodo);
                    char argsStr[256] = "";
                    char *rp = strrchr(afterDot, ')');
                    if (rp && rp > lp)
                    {
                        int al = (int)(rp - lp - 1);
                        strncpy(argsStr, lp + 1, al < 255 ? al : 255);
                    }
                    int objId = -1;
                    if (!strcmp(before, "this"))
                        objId = thisObjId;
                    else
                    {
                        char *idStr = getVar((char *)before);
                        if (idStr)
                            objId = atoi(idStr);
                    }
                    if (objId > 0)
                    {
                        Objeto *o = getObjById(objId);
                        if (o)
                        {
                            int fi = buscarMetodo(o->classeNome, metodo);
                            if (fi != -1)
                            {
                                if (topoThis < MAX_STACK - 1)
                                    thisPilha[++topoThis] = thisObjId;
                                thisObjId = objId;
                                chamarFuncao(fi, argsStr);
                                if (topoThis >= 0)
                                    thisObjId = thisPilha[topoThis--];
                                return;
                            }
                            else
                                erroF(ERR_OBJETO, "metodo '%s' nao existe na classe '%s'", metodo, o->classeNome);
                        }
                    }
                    else
                        erroF(ERR_OBJETO, "variavel '%s' nao contem um objeto valido (valor nao e um ID de objeto)", before);
                    return;
                }
            }
        }
    }

    {
        char *eq2 = NULL;
        int loLen = (int)strlen(lo);
        for (int i = 1; i < loLen - 1; i++)
        {
            if (lo[i] == '=' && lo[i - 1] != '!' && lo[i - 1] != '<' && lo[i - 1] != '>' && lo[i + 1] != '=')
            {
                eq2 = &lo[i];
                break;
            }
        }
        if (eq2 && canExec())
        {
            char rhs[300] = "";
            strncpy(rhs, cmd + (eq2 - lo) + 1, 299);
            trim(rhs);
            char rhsLo[300];
            strncpy(rhsLo, rhs, 299);
            toLow(rhsLo);
            char *dot2 = strchr(rhsLo, '.');
            if (dot2)
            {
                char *lp2 = strchr(rhsLo, '(');
                if (lp2)
                {
                    char varNome[64] = "";
                    int nl2 = (int)(eq2 - lo);
                    strncpy(varNome, lo, nl2 < 63 ? nl2 : 63);
                    if (!strncmp(varNome, "let ", 4))
                        memmove(varNome, varNome + 4, strlen(varNome));
                    trim(varNome);
                    int bl2 = (int)(dot2 - rhsLo);
                    char objPart[64] = "";
                    strncpy(objPart, rhsLo, bl2 < 63 ? bl2 : 63);
                    trim(objPart);
                    char metodo2[64] = "";
                    int afterOff = bl2 + 1;
                    int mlen = (int)(lp2 - rhsLo) - afterOff;
                    strncpy(metodo2, rhs + afterOff, mlen < 63 ? mlen : 63);
                    trim(metodo2);
                    char argsStr2[256] = "";
                    char *rp2 = strrchr(rhs, ')');
                    if (rp2)
                    {
                        int lp2off = (int)(lp2 - rhsLo);
                        int al2 = (int)(rp2 - (rhs + lp2off)) - 1;
                        if (al2 > 0)
                            strncpy(argsStr2, rhs + lp2off + 1, al2 < 255 ? al2 : 255);
                    }
                    int objId2 = -1;
                    if (!strcmp(objPart, "this"))
                        objId2 = thisObjId;
                    else
                    {
                        char *idStr2 = getVar(objPart);
                        if (idStr2)
                            objId2 = atoi(idStr2);
                    }
                    if (objId2 > 0)
                    {
                        Objeto *o2 = getObjById(objId2);
                        if (o2)
                        {
                            int fi2 = buscarMetodo(o2->classeNome, metodo2);
                            if (fi2 != -1)
                            {
                                if (topoThis < MAX_STACK - 1)
                                    thisPilha[++topoThis] = thisObjId;
                                thisObjId = objId2;
                                retval[0] = '\0';
                                chamarFuncao(fi2, argsStr2);
                                setVar(varNome, retval);
                                if (topoThis >= 0)
                                    thisObjId = thisPilha[topoThis--];
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    if (!strncmp(lo, "delete ", 7) && canExec())
    {
        char varNome[64] = "";
        strncpy(varNome, cmd + 7, 63);
        trim(varNome);
        char *idStr = getVar(varNome);
        if (idStr)
        {
            Objeto *o = getObjById(atoi(idStr));
            if (o)
            {
                Classe *cl = getClasse(o->classeNome);
                if (cl && cl->destrutorIdx != -1)
                {
                    if (topoThis < MAX_STACK - 1)
                        thisPilha[++topoThis] = thisObjId;
                    thisObjId = o->id;
                    chamarFuncao(cl->destrutorIdx, "");
                    if (topoThis >= 0)
                        thisObjId = thisPilha[topoThis--];
                }
                o->ativo = 0;
            }
            setVar(varNome, "0");
        }
        return;
    }

    if (!strncmp(lo, "if", 2) && (lo[2] == ' ' || lo[2] == '('))
    {
        char cond[300] = "";
        extractParens(cmd, cond, 300);
        ultimoRes = canExec() ? evalCond(cond) : 0;
        ultimoEhWhile = 0;
        return;
    }

    if (!strncmp(lo, "while", 5) && (lo[5] == ' ' || lo[5] == '('))
    {
        char cond[300] = "";
        extractParens(cmd, cond, 300);
        ultimoRes = canExec() ? evalCond(cond) : 0;
        ultimoEhWhile = 1;
        ultimaLinhaWhile = cursor - 1;
        strncpy(ultimaCondWhile, cond, 299);
        return;
    }

    if (!strcmp(lo, "begin"))
    {
        int paiExec = (topo >= 0) ? pilha[topo].exec : 1;
        int exec = paiExec && ultimoRes;
        if (ultimoEhWhile)
            push(NIV_WHILE, exec, ultimoRes, ultimaLinhaWhile, ultimaCondWhile);
        else
            push(NIV_IF, exec, ultimoRes, 0, NULL);
        ultimoEhWhile = 0;
        return;
    }

    if (!strcmp(lo, "else"))
    {
        if (topo >= 0 && !pilha[topo].elseUsado && pilha[topo].tipo == NIV_IF)
        {
            int paiExec = (topo > 0) ? pilha[topo - 1].exec : 1;
            pilha[topo].exec = (!pilha[topo].ifRes) && paiExec;
            pilha[topo].elseUsado = 1;
        }
        return;
    }

    if (!strcmp(lo, "end"))
    {
        if (topo >= 0)
        {
            if (pilha[topo].tipo == NIV_WHILE && pilha[topo].exec && !flagBreak)
            {
                if (evalCond(pilha[topo].condWhile))
                {
                    int lw = pilha[topo].linhaWhile;
                    popStack();
                    cursor = lw;
                    return;
                }
            }
            flagBreak = 0;
            popStack();
        }
        return;
    }

    if (!strncmp(lo, "return", 6) && (lo[6] == '\0' || lo[6] == ' '))
    {
        if (!canExec())
            return;
        retval[0] = '\0';
        if (lo[6] == ' ')
        {
            char rexpr[400] = "";
            strncpy(rexpr, cmd + 7, 399);
            trim(rexpr);
            evalFunc(rexpr, retval);
        }
        flagReturn = 1;
        return;
    }

    if (!strcmp(lo, "break"))
    {
        if (!canExec())
            return;

        int nIfs = 0;
        for (int i = topo; i >= 0; i--)
        {
            if (pilha[i].tipo == NIV_WHILE)
                break;
            else
                nIfs++;
        }

        int dep = nIfs + 1;
        while (cursor < nLines)
        {
            char tmp[MAX_LINE];
            strncpy(tmp, progLines[cursor], MAX_LINE - 1);
            toLow(tmp);
            trim(tmp);

            char *com2 = strchr(tmp, '#');
            if (com2)
                *com2 = '\0';
            trim(tmp);
            cursor++;
            if (!strcmp(tmp, "begin"))
                dep++;
            else if (!strcmp(tmp, "end"))
            {
                dep--;
                if (dep == 0)
                    break;
            }
        }
        while (topo >= 0)
        {
            int ehW = (pilha[topo].tipo == NIV_WHILE);
            popStack();
            if (ehW)
                break;
        }
        flagBreak = 0;
        return;
    }

    if (!canExec())
        return;

    if (!strncmp(lo, "sleep(", 6))
    {
        char inner[100] = "";
        extractParens(cmd, inner, 100);
        Sleep(evalExpr(inner));
        processMessages();
        return;
    }
    if (!strncmp(lo, "bgcolor(", 8))
    {
        if (modoConsole)
            return;
        char inner[100] = "";
        extractParens(cmd, inner, 100);
        char a[3][300] = {{""}, {""}, {""}};
        splitArgs(inner, a, 3);
        bgR = evalExpr(a[0]);
        bgG = evalExpr(a[1]);
        bgB = evalExpr(a[2]);
        return;
    }
    if (!strncmp(lo, "color(", 6))
    {
        if (modoConsole)
            return;
        char inner[200] = "";
        extractParens(cmd, inner, 200);
        char a[3][300] = {{""}, {""}, {""}};
        splitArgs(inner, a, 3);
        corAtual = RGB(evalExpr(a[0]), evalExpr(a[1]), evalExpr(a[2]));
        return;
    }
    if (!strcmp(lo, "fill on"))
    {
        if (!modoConsole)
            preenchido = 1;
        return;
    }
    if (!strcmp(lo, "fill off"))
    {
        if (!modoConsole)
            preenchido = 0;
        return;
    }

    if (!strncmp(lo, "rect(", 5))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[4][300] = {{""}, {""}, {""}, {""}};
        splitArgs(inner, a, 4);
        Shape s = {0};
        s.tipo = SH_RECT;
        s.x = evalExpr(a[0]);
        s.y = evalExpr(a[1]);
        s.w = evalExpr(a[2]);
        s.h = evalExpr(a[3]);
        s.cor = corAtual;
        s.preenchido = preenchido;
        addShape(s);
        return;
    }
    if (!strncmp(lo, "circle(", 7))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[3][300] = {{""}, {""}, {""}};
        splitArgs(inner, a, 3);
        Shape s = {0};
        s.tipo = SH_CIRCLE;
        s.x = evalExpr(a[0]);
        s.y = evalExpr(a[1]);
        s.r = evalExpr(a[2]);
        s.cor = corAtual;
        s.preenchido = preenchido;
        addShape(s);
        return;
    }
    if (!strncmp(lo, "line(", 5))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[4][300] = {{""}, {""}, {""}, {""}};
        splitArgs(inner, a, 4);
        Shape s = {0};
        s.tipo = SH_LINE;
        s.x = evalExpr(a[0]);
        s.y = evalExpr(a[1]);
        s.x2 = evalExpr(a[2]);
        s.y2 = evalExpr(a[3]);
        s.cor = corAtual;
        addShape(s);
        return;
    }
    if (!strncmp(lo, "triangle(", 9))
    {
        if (modoConsole)
            return;
        char inner[400] = "";
        extractParens(cmd, inner, 400);
        char a[6][300] = {{""}, {""}, {""}, {""}, {""}, {""}};
        splitArgs(inner, a, 6);
        Shape s = {0};
        s.tipo = SH_TRIANGLE;
        s.x = evalExpr(a[0]);
        s.y = evalExpr(a[1]);
        s.x2 = evalExpr(a[2]);
        s.y2 = evalExpr(a[3]);
        s.x3 = evalExpr(a[4]);
        s.y3 = evalExpr(a[5]);
        s.cor = corAtual;
        s.preenchido = preenchido;
        addShape(s);
        return;
    }
    if (!strncmp(lo, "text(", 5))
    {
        if (modoConsole)
            return;
        char inner[512] = "";
        extractParens(cmd, inner, 512);
        char a[4][300] = {{""}, {""}, {""}, {""}};
        splitArgs(inner, a, 4);
        Shape s = {0};
        s.tipo = SH_TEXT;
        s.x = evalExpr(a[0]);
        s.y = evalExpr(a[1]);
        s.h = evalExpr(a[2]);
        s.cor = corAtual;
        char msg[256] = "";
        evalFunc(a[3], msg);
        strncpy(s.texto, msg, 255);
        addShape(s);
        return;
    }
    if (!strncmp(lo, "pixel(", 6))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[2][300] = {{""}, {""}};
        splitArgs(inner, a, 2);
        Shape s = {0};
        s.tipo = SH_PIXEL;
        s.x = evalExpr(a[0]);
        s.y = evalExpr(a[1]);
        s.cor = corAtual;
        addShape(s);
        return;
    }
    if (!strncmp(lo, "pixelblock(", 11))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[4][300] = {{""}, {""}, {""}, {""}};
        splitArgs(inner, a, 4);
        Shape s = {0};
        s.tipo = SH_RECT;
        s.x = evalExpr(a[0]);
        s.y = evalExpr(a[1]);
        s.w = evalExpr(a[2]);
        s.h = evalExpr(a[3]);
        s.cor = corAtual;
        s.preenchido = 1;
        addShape(s);
        return;
    }
    if (!strncmp(lo, "loadimage(", 10))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char args2[2][300] = {{""}, {""}};
        int nA = splitArgs(inner, args2, 2);
        char tmp[200] = "";
        strncpy(tmp, args2[0], 199);
        trim(tmp);
        if (tmp[0] == '"')
        {
            memmove(tmp, tmp + 1, strlen(tmp));
            char *fq = strrchr(tmp, '"');
            if (fq)
                *fq = '\0';
        }
        int escala = (nA >= 2) ? evalExpr(args2[1]) : 1;
        if (escala < 1)
            escala = 1;
        FILE *f = fopen(tmp, "r");
        if (!f)
        {
            erroF(ERR_ARQUIVO, "nao foi possivel abrir o arquivo de imagem '%s'", tmp);
            return;
        }
        int x, y, r, g, b, count = 0;
        char linhaBuf[256];
        while (fgets(linhaBuf, sizeof(linhaBuf), f))
        {
            trim(linhaBuf);
            if (linhaBuf[0] == '#' || linhaBuf[0] == '\0')
                continue;
            if (sscanf(linhaBuf, "%d,%d,%d,%d,%d", &x, &y, &r, &g, &b) == 5 ||
                sscanf(linhaBuf, "%d %d %d %d %d", &x, &y, &r, &g, &b) == 5)
            {
                if (hdcBuffer)
                {
                    HBRUSH br = CreateSolidBrush(RGB(r, g, b));
                    RECT rc = {x * escala, y * escala, x * escala + escala, y * escala + escala};
                    FillRect(hdcBuffer, &rc, br);
                    DeleteObject(br);
                    count++;
                }
            }
        }
        fclose(f);
        printf("loadimage: %d pixels de '%s' (escala %d)\n", count, tmp, escala);
        return;
    }
    if (!strncmp(lo, "mouse(", 6))
    {
        char inner[200] = "";
        extractParens(cmd, inner, 200);
        char a[3][300] = {{""}, {""}, {""}};
        splitArgs(inner, a, 3);
        trim(a[0]);
        trim(a[1]);
        trim(a[2]);
        POINT pt;
        GetCursorPos(&pt);
        if (hwnd)
            ScreenToClient(hwnd, &pt);
        mouse_x = pt.x;
        mouse_y = pt.y;
        mouse_left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0;
        mouse_right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 1 : 0;
        char buf[20];
        if (strlen(a[0]))
        {
            sprintf(buf, "%d", mouse_x);
            setVar(a[0], buf);
        }
        if (strlen(a[1]))
        {
            sprintf(buf, "%d", mouse_y);
            setVar(a[1], buf);
        }
        if (strlen(a[2]))
        {
            sprintf(buf, "%d", mouse_left);
            setVar(a[2], buf);
        }
        return;
    }
    if (!strcmp(lo, "mouse_update"))
    {
        POINT pt;
        GetCursorPos(&pt);
        if (hwnd)
            ScreenToClient(hwnd, &pt);
        mouse_x = pt.x;
        mouse_y = pt.y;
        mouse_left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0;
        mouse_right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 1 : 0;
        return;
    }
    if (!strncmp(lo, "sprite(", 7))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char tname[64] = "", coordStr[200] = "";
        char *q = strchr(inner, '"');
        if (q)
        {
            int bl = (int)(q - inner);
            strncpy(coordStr, inner, bl < 199 ? bl : 199);
            char *q2 = strrchr(q + 1, '"');
            if (q2)
            {
                int ml = (int)(q2 - q - 1);
                strncpy(tname, q + 1, ml < 63 ? ml : 63);
            }
        }
        else
        {
            char a[5][300] = {{""}, {""}, {""}, {""}, {""}};
            int n = splitArgs(inner, a, 5);
            if (n >= 5)
                strncpy(tname, a[4], 63);
            snprintf(coordStr, 199, "%s,%s,%s,%s", a[0], a[1], a[2], a[3]);
        }
        char ca[4][300] = {{""}, {""}, {""}, {""}};
        splitArgs(coordStr, ca, 4);
        int sx = evalExpr(ca[0]), sy = evalExpr(ca[1]), sw = evalExpr(ca[2]), sh2 = evalExpr(ca[3]);
        Sprite *sp = NULL;
        for (int i = 0; i < nSprites; i++)
            if (!strcmp(sprites[i].nome, tname))
            {
                sp = &sprites[i];
                break;
            }
        if (!sp && nSprites < MAX_SPRITES)
        {
            sp = &sprites[nSprites++];
            strncpy(sp->nome, tname, 49);
        }
        if (sp)
        {
            sp->x = sx;
            sp->y = sy;
            sp->w = sw;
            sp->h = sh2;
        }
        return;
    }
    if (!strcmp(lo, "clear"))
    {
        if (!modoConsole)
            gfxClear();
        return;
    }
    if (!strcmp(lo, "render"))
    {
        if (!modoConsole)
        {
            gfxRender();
            processMessages();
        }
        return;
    }

    if (!strcmp(lo, "3dinit"))
    {
        if (!modoConsole)
            m3Reset();
        return;
    }

    if (!strncmp(lo, "3dcam(", 6))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[5][300] = {{""}, {""}, {""}, {""}, {""}};
        ;
        int n = splitArgs(inner, a, 5);
        char rv[5][64];
        for (int i = 0; i < n && i < 5; i++)
        {
            char t[64] = "";
            evalFunc(a[i], t);
            strncpy(rv[i], t, 63);
        }
        if (n >= 3)
        {
            m3CamX = atof(rv[0]);
            m3CamY = atof(rv[1]);
            m3CamZ = atof(rv[2]);
        }
        if (n >= 4)
            m3CamYaw = atof(rv[3]) * M_PI / 180.0;
        if (n >= 5)
            m3CamPitch = atof(rv[4]) * M_PI / 180.0;
        m3Active = 1;
        return;
    }

    if (!strncmp(lo, "3dfov(", 6))
    {
        if (modoConsole)
            return;
        char inner[100] = "";
        extractParens(cmd, inner, 100);
        char t[64] = "";
        evalFunc(inner, t);
        m3Fov = atof(t);
        return;
    }

    if (!strncmp(lo, "3dmesh(", 7))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        trim(inner);
        if (inner[0] == '"')
        {
            memmove(inner, inner + 1, strlen(inner));
            char *q = strrchr(inner, '"');
            if (q)
                *q = '\0';
        }
        else
        {
            char t[300] = "";
            evalFunc(inner, t);
            strncpy(inner, t, 299);
        }
        m3NewMesh(inner);
        return;
    }

    if (!strncmp(lo, "3dcolor(", 8))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[4][300] = {{""}, {""}, {""}, {""}};
        ;
        int n = splitArgs(inner, a, 4);
        if (n < 4)
            return;
        char nm[64] = "";
        evalFunc(a[0], nm);
        if (nm[0] == '"')
        {
            memmove(nm, nm + 1, strlen(nm));
            char *q = strrchr(nm, '"');
            if (q)
                *q = '\0';
        }
        Mesh3 *m = m3GetMesh(nm);
        if (!m)
            return;
        m->cr = (unsigned char)evalExpr(a[1]);
        m->cg = (unsigned char)evalExpr(a[2]);
        m->cb = (unsigned char)evalExpr(a[3]);
        return;
    }

    if (!strncmp(lo, "3dwireframe(", 12))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[2][300] = {{""}, {""}};
        ;
        splitArgs(inner, a, 2);
        char nm[64] = "";
        evalFunc(a[0], nm);
        if (nm[0] == '"')
        {
            memmove(nm, nm + 1, strlen(nm));
            char *q = strrchr(nm, '"');
            if (q)
                *q = '\0';
        }
        Mesh3 *m = m3GetMesh(nm);
        if (!m)
            return;
        m->wireframe = evalExpr(a[1]);
        return;
    }

    if (!strncmp(lo, "3dpos(", 6))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[4][300] = {{""}, {""}, {""}, {""}};
        ;
        int n = splitArgs(inner, a, 4);
        if (n < 4)
            return;
        char nm[64] = "";
        evalFunc(a[0], nm);
        if (nm[0] == '"')
        {
            memmove(nm, nm + 1, strlen(nm));
            char *q = strrchr(nm, '"');
            if (q)
                *q = '\0';
        }
        Mesh3 *m = m3GetMesh(nm);
        if (!m)
            return;
        char rv2[3][64];
        for (int i = 1; i < 4; i++)
        {
            char t[64] = "";
            evalFunc(a[i], t);
            strncpy(rv2[i - 1], t, 63);
        }
        m->tx = atof(rv2[0]);
        m->ty = atof(rv2[1]);
        m->tz = atof(rv2[2]);
        return;
    }

    if (!strncmp(lo, "3drot(", 6))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[4][300] = {{""}, {""}, {""}, {""}};
        ;
        int n = splitArgs(inner, a, 4);
        if (n < 4)
            return;
        char nm[64] = "";
        evalFunc(a[0], nm);
        if (nm[0] == '"')
        {
            memmove(nm, nm + 1, strlen(nm));
            char *q = strrchr(nm, '"');
            if (q)
                *q = '\0';
        }
        Mesh3 *m = m3GetMesh(nm);
        if (!m)
            return;
        char rv2[3][64];
        for (int i = 1; i < 4; i++)
        {
            char t[64] = "";
            evalFunc(a[i], t);
            strncpy(rv2[i - 1], t, 63);
        }
        m->rx = atof(rv2[0]) * M_PI / 180.0;
        m->ry = atof(rv2[1]) * M_PI / 180.0;
        m->rz = atof(rv2[2]) * M_PI / 180.0;
        return;
    }

    if (!strncmp(lo, "3dscale(", 8))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[4][300] = {{""}, {""}, {""}, {""}};
        ;
        int n = splitArgs(inner, a, 4);
        if (n < 2)
            return;
        char nm[64] = "";
        evalFunc(a[0], nm);
        if (nm[0] == '"')
        {
            memmove(nm, nm + 1, strlen(nm));
            char *q = strrchr(nm, '"');
            if (q)
                *q = '\0';
        }
        Mesh3 *m = m3GetMesh(nm);
        if (!m)
            return;
        char t[64] = "";
        evalFunc(a[1], t);
        double sv = atof(t);
        if (n >= 4)
        {
            char ty[64] = "", tz[64] = "";
            evalFunc(a[2], ty);
            evalFunc(a[3], tz);
            m->sx = sv;
            m->sy = atof(ty);
            m->sz = atof(tz);
        }
        else if (n == 3)
        {

            char ty[64] = "";
            evalFunc(a[2], ty);
            m->sx = sv;
            m->sy = atof(ty);
            m->sz = atof(ty);
        }
        else
        {
            m->sx = sv;
            m->sy = sv;
            m->sz = sv;
        }
        return;
    }

    if (!strncmp(lo, "3dcube(", 7))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[2][300] = {{""}, {""}};
        ;
        int n = splitArgs(inner, a, 2);
        if (n < 1)
            return;
        char nm[64] = "";
        evalFunc(a[0], nm);
        if (nm[0] == '"')
        {
            memmove(nm, nm + 1, strlen(nm));
            char *q = strrchr(nm, '"');
            if (q)
                *q = '\0';
        }
        Mesh3 *m = m3GetMesh(nm);
        if (!m)
            m = m3NewMesh(nm);
        if (!m)
            return;
        m3ClearGeometry(m);
        double sz2 = 1.0;
        if (n >= 2)
        {
            char t[64] = "";
            evalFunc(a[1], t);
            sz2 = atof(t);
        }
        m3AddCube(m, sz2);
        return;
    }

    if (!strncmp(lo, "3dsphere(", 9))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[4][300] = {{""}, {""}, {""}, {""}};
        ;
        int n = splitArgs(inner, a, 4);
        if (n < 1)
            return;
        char nm[64] = "";
        evalFunc(a[0], nm);
        if (nm[0] == '"')
        {
            memmove(nm, nm + 1, strlen(nm));
            char *q = strrchr(nm, '"');
            if (q)
                *q = '\0';
        }
        Mesh3 *m = m3GetMesh(nm);
        if (!m)
            m = m3NewMesh(nm);
        if (!m)
            return;
        m3ClearGeometry(m);
        double r2 = 1.0;
        int stacks = 8, slices = 12;
        if (n >= 2)
        {
            char t[64] = "";
            evalFunc(a[1], t);
            r2 = atof(t);
        }
        if (n >= 3)
            stacks = evalExpr(a[2]);
        if (n >= 4)
            slices = evalExpr(a[3]);
        m3AddSphere(m, r2, stacks, slices);
        return;
    }

    if (!strncmp(lo, "3dpyramid(", 10))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[3][300] = {{""}, {""}, {""}};
        ;
        int n = splitArgs(inner, a, 3);
        if (n < 1)
            return;
        char nm[64] = "";
        evalFunc(a[0], nm);
        if (nm[0] == '"')
        {
            memmove(nm, nm + 1, strlen(nm));
            char *q = strrchr(nm, '"');
            if (q)
                *q = '\0';
        }
        Mesh3 *m = m3GetMesh(nm);
        if (!m)
            m = m3NewMesh(nm);
        if (!m)
            return;
        m3ClearGeometry(m);
        double base2 = 1.0, height = 1.5;
        if (n >= 2)
        {
            char t[64] = "";
            evalFunc(a[1], t);
            base2 = atof(t);
        }
        if (n >= 3)
        {
            char t[64] = "";
            evalFunc(a[2], t);
            height = atof(t);
        }
        m3AddPyramid(m, base2, height);
        return;
    }

    if (!strncmp(lo, "3dcylinder(", 11))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[4][300] = {{""}, {""}, {""}, {""}};
        ;
        int n = splitArgs(inner, a, 4);
        if (n < 1)
            return;
        char nm[64] = "";
        evalFunc(a[0], nm);
        if (nm[0] == '"')
        {
            memmove(nm, nm + 1, strlen(nm));
            char *q = strrchr(nm, '"');
            if (q)
                *q = '\0';
        }
        Mesh3 *m = m3GetMesh(nm);
        if (!m)
            m = m3NewMesh(nm);
        if (!m)
            return;
        m3ClearGeometry(m);
        double r2 = 0.5, height = 2.0;
        int slices = 12;
        if (n >= 2)
        {
            char t[64] = "";
            evalFunc(a[1], t);
            r2 = atof(t);
        }
        if (n >= 3)
        {
            char t[64] = "";
            evalFunc(a[2], t);
            height = atof(t);
        }
        if (n >= 4)
            slices = evalExpr(a[3]);
        m3AddCylinder(m, r2, height, slices);
        return;
    }

    if (!strncmp(lo, "3dplane(", 8))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[3][300] = {{""}, {""}, {""}};
        ;
        int n = splitArgs(inner, a, 3);
        if (n < 1)
            return;
        char nm[64] = "";
        evalFunc(a[0], nm);
        if (nm[0] == '"')
        {
            memmove(nm, nm + 1, strlen(nm));
            char *q = strrchr(nm, '"');
            if (q)
                *q = '\0';
        }
        Mesh3 *m = m3GetMesh(nm);
        if (!m)
            m = m3NewMesh(nm);
        if (!m)
            return;
        m3ClearGeometry(m);
        double w2 = 4.0, d2 = 4.0;
        if (n >= 2)
        {
            char t[64] = "";
            evalFunc(a[1], t);
            w2 = atof(t);
        }
        if (n >= 3)
        {
            char t[64] = "";
            evalFunc(a[2], t);
            d2 = atof(t);
        }
        m3AddPlane(m, w2, d2);
        return;
    }

    if (!strncmp(lo, "3dclear(", 8))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        trim(inner);
        if (inner[0] == '"')
        {
            memmove(inner, inner + 1, strlen(inner));
            char *q = strrchr(inner, '"');
            if (q)
                *q = '\0';
        }
        else
        {
            char t[300] = "";
            evalFunc(inner, t);
            strncpy(inner, t, 299);
        }
        Mesh3 *m = m3GetMesh(inner);
        if (m)
        {
            m->nVerts = 0;
            m->nEdges = 0;
            m->nFaces = 0;
        }
        return;
    }

    if (!strncmp(lo, "3dremove(", 9))
    {
        if (modoConsole)
            return;
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        trim(inner);
        if (inner[0] == '"')
        {
            memmove(inner, inner + 1, strlen(inner));
            char *q = strrchr(inner, '"');
            if (q)
                *q = '\0';
        }
        else
        {
            char t[300] = "";
            evalFunc(inner, t);
            strncpy(inner, t, 299);
        }
        Mesh3 *m = m3GetMesh(inner);
        if (m)
            m->ativo = 0;
        return;
    }

    if (!strcmp(lo, "3drender"))
    {
        if (!modoConsole && m3Active)
        {
            m3RenderAll();
            processMessages();
        }
        return;
    }

    if (!strncmp(lo, "rcloadtex(", 10))
    {
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        trim(inner);

        if (inner[0] == '"')
        {
            memmove(inner, inner + 1, strlen(inner));
            char *q = strrchr(inner, '"');
            if (q)
                *q = '\0';
        }
        else
        {
            char tmp[300] = "";
            evalFunc(inner, tmp);
            strncpy(inner, tmp, 299);
        }
        int idx = rcCarregarTextura(inner);
        char buf[20];
        sprintf(buf, "%d", idx);
        setVar("__rcloadtex_result", buf);
        return;
    }

    if (!strncmp(lo, "rcsetmap(", 9))
    {
        char inner[200] = "";
        extractParens(cmd, inner, 200);
        char a[2][300] = {{""}, {""}};
        splitArgs(inner, a, 2);
        rcMapW = evalExpr(a[0]);
        rcMapH = evalExpr(a[1]);
        if (rcMapW > RC_MAP_MAX)
            rcMapW = RC_MAP_MAX;
        if (rcMapH > RC_MAP_MAX)
            rcMapH = RC_MAP_MAX;
        memset(rcMapa, 0, sizeof(rcMapa));
        rcAtivo = 1;
        return;
    }

    if (!strncmp(lo, "rcwall(", 7))
    {
        char inner[200] = "";
        extractParens(cmd, inner, 200);
        char a[3][300] = {{""}, {""}, {""}};
        splitArgs(inner, a, 3);
        int mx = evalExpr(a[0]), my = evalExpr(a[1]), tex = evalExpr(a[2]);
        if (mx >= 0 && mx < rcMapW && my >= 0 && my < rcMapH)
            rcMapa[my][mx] = tex;
        return;
    }

    if (!strncmp(lo, "rcmoveto(", 9))
    {
        char inner[200] = "";
        extractParens(cmd, inner, 200);
        char a[2][300] = {{""}, {""}};
        splitArgs(inner, a, 2);
        rcPx = atof(a[0]);
        rcPy = atof(a[1]);
        char *vx = getVar(a[0]);
        char *vy = getVar(a[1]);
        if (vx)
            rcPx = atof(vx);
        if (vy)
            rcPy = atof(vy);
        return;
    }

    if (!strncmp(lo, "rcsetpos(", 9))
    {
        char inner[300] = "";
        extractParens(cmd, inner, 300);
        char a[4][300] = {{""}, {""}, {""}, {""}};
        splitArgs(inner, a, 4);

        char rv[4][64];
        for (int i = 0; i < 4; i++)
        {
            char tmp[64] = "";
            evalFunc(a[i], tmp);
            strncpy(rv[i], tmp, 63);
        }
        rcPx = atof(rv[0]);
        rcPy = atof(rv[1]);
        rcDx = atof(rv[2]);
        rcDy = atof(rv[3]);

        rcCx = -rcDy * 0.66;
        rcCy = rcDx * 0.66;
        return;
    }

    if (!strncmp(lo, "rcturn(", 7))
    {
        char inner[100] = "";
        extractParens(cmd, inner, 100);
        double ang = atof(inner) * M_PI / 180.0;
        char tmp[64] = "";
        evalFunc(inner, tmp);
        ang = atof(tmp) * M_PI / 180.0;
        double oldDx = rcDx, oldDy = rcDy;
        rcDx = oldDx * cos(ang) - oldDy * sin(ang);
        rcDy = oldDx * sin(ang) + oldDy * cos(ang);
        double oldCx = rcCx, oldCy = rcCy;
        rcCx = oldCx * cos(ang) - oldCy * sin(ang);
        rcCy = oldCx * sin(ang) + oldCy * cos(ang);
        return;
    }

    if (!strncmp(lo, "rcmove(", 7))
    {
        char inner[100] = "";
        extractParens(cmd, inner, 100);
        char tmp[64] = "";
        evalFunc(inner, tmp);
        double dist = atof(tmp);
        double nx = rcPx + rcDx * dist;
        double ny = rcPy + rcDy * dist;
        int cx = (int)nx, cy = (int)ny;
        if (cx >= 0 && cx < rcMapW && cy >= 0 && cy < rcMapH)
        {
            if (rcMapa[cy][(int)rcPx] == 0)
                rcPx = nx;
            if (rcMapa[(int)rcPy][cx] == 0)
                rcPy = ny;
        }
        return;
    }

    if (!strncmp(lo, "rcstrafe(", 9))
    {
        char inner[100] = "";
        extractParens(cmd, inner, 100);
        char tmp[64] = "";
        evalFunc(inner, tmp);
        double dist = atof(tmp);
        double nx = rcPx + rcCy * dist;
        double ny = rcPy - rcCx * dist;
        if ((int)nx >= 0 && (int)nx < rcMapW && (int)ny >= 0 && (int)ny < rcMapH)
        {
            if (rcMapa[(int)rcPy][(int)nx] == 0)
                rcPx = nx;
            if (rcMapa[(int)ny][(int)rcPx] == 0)
                rcPy = ny;
        }
        return;
    }

    if (!strncmp(lo, "rcsetceil(", 10))
    {
        char inner[200] = "";
        extractParens(cmd, inner, 200);
        char a[3][300] = {{""}, {""}, {""}};
        splitArgs(inner, a, 3);
        rcCeilR = evalExpr(a[0]);
        rcCeilG = evalExpr(a[1]);
        rcCeilB = evalExpr(a[2]);
        return;
    }

    if (!strncmp(lo, "rcsetfloor(", 11))
    {
        char inner[200] = "";
        extractParens(cmd, inner, 200);
        char a[3][300] = {{""}, {""}, {""}};
        splitArgs(inner, a, 3);
        rcFloorR = evalExpr(a[0]);
        rcFloorG = evalExpr(a[1]);
        rcFloorB = evalExpr(a[2]);
        return;
    }

    if (!strncmp(lo, "rcminimap(", 10))
    {
        char inner[50] = "";
        extractParens(cmd, inner, 50);
        rcMinimap = evalExpr(inner);
        return;
    }

    if (!strcmp(lo, "rcrender"))
    {
        if (!modoConsole)
        {
            rcRenderFrame();
            gfxRender();
            processMessages();
        }
        return;
    }

    if (!strncmp(lo, "rcgetpx(", 8))
    {
        char inner[64] = "";
        extractParens(cmd, inner, 64);
        trim(inner);
        char buf[64];
        sprintf(buf, "%.6f", rcPx);
        setVar(inner, buf);
        return;
    }
    if (!strncmp(lo, "rcgetpy(", 8))
    {
        char inner[64] = "";
        extractParens(cmd, inner, 64);
        trim(inner);
        char buf[64];
        sprintf(buf, "%.6f", rcPy);
        setVar(inner, buf);
        return;
    }

    if (!strncmp(lo, "key(", 4))
    {
        char inner[100] = "";
        extractParens(cmd, inner, 100);
        trim(inner);
        char val[20] = "none";
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)
            strcpy(val, "left");
        else if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
            strcpy(val, "right");
        else if (GetAsyncKeyState(VK_UP) & 0x8000)
            strcpy(val, "up");
        else if (GetAsyncKeyState(VK_DOWN) & 0x8000)
            strcpy(val, "down");
        else if (GetAsyncKeyState(VK_SPACE) & 0x8000)
            strcpy(val, "space");
        else if (GetAsyncKeyState('A') & 0x8000)
            strcpy(val, "a");
        else if (GetAsyncKeyState('D') & 0x8000)
            strcpy(val, "d");
        else if (GetAsyncKeyState('W') & 0x8000)
            strcpy(val, "w");
        else if (GetAsyncKeyState('S') & 0x8000)
            strcpy(val, "s");
        setVar(inner, val);
        return;
    }

    if (!strncmp(lo, "write(", 6) || !strncmp(lo, "print(", 6))
    {
        char inner[512] = "";
        extractParens(cmd, inner, 512);
        procWrite(inner, 0);
        return;
    }
    if (!strncmp(lo, "writeln(", 8) || !strncmp(lo, "println(", 8))
    {
        char inner[512] = "";
        extractParens(cmd, inner, 512);
        procWrite(inner, 1);
        return;
    }
    if (!strncmp(lo, "input(", 6))
    {
        char inner[400] = "";
        extractParens(cmd, inner, 400);
        trim(inner);
        char msg[200] = "", var[64] = "";
        if (inner[0] == '"')
        {
            char *eq2 = strrchr(inner + 1, '"');
            if (eq2)
            {
                int ml = (int)(eq2 - (inner + 1));
                strncpy(msg, inner + 1, ml < 199 ? ml : 199);
                char *cm = strchr(eq2, ',');
                if (cm)
                {
                    strncpy(var, cm + 1, 63);
                    trim(var);
                }
                else
                    strcpy(var, "entrada");
            }
        }
        else
        {
            strncpy(var, inner, 63);
            trim(var);
            sprintf(msg, "Digite %s", var);
        }
        printf("%s: ", msg);
        fflush(stdout);
        char val[256] = "";
        if (fgets(val, sizeof(val), stdin))
        {
            val[strcspn(val, "\n")] = '\0';
            trim(val);
            setVar(var, val);
        }
        return;
    }

    if (!strncmp(lo, "let ", 4))
    {
        char nome[64] = "", tmpRHS[400] = "";
        if (sscanf(cmd + 4, "%63s = %399[^\n]", nome, tmpRHS) == 2)
        {
            trim(tmpRHS);
            if (!strcmp(tmpRHS, "[]"))
            {
                criarArray(nome);
                return;
            }
        }
        char val[400] = "";
        if (sscanf(cmd + 4, "%63s = \"%399[^\"]\"", nome, val) == 2)
        {
            setVar(nome, val);
            return;
        }
        if (sscanf(cmd + 4, "%63s = %399[^\n]", nome, val) == 2)
        {
            trim(val);
            char valLo[400];
            strncpy(valLo, val, 399);
            toLow(valLo);
            if (!strncmp(valLo, "new ", 4))
            {
                char classeNome[64] = "", argsStr[256] = "";
                char *lp = strchr(val + 4, '('), *rp = strrchr(val + 4, ')');
                if (lp && rp && rp > lp)
                {
                    int clen = (int)(lp - (val + 4));
                    strncpy(classeNome, val + 4, clen < 63 ? clen : 63);
                    trim(classeNome);
                    int alen = (int)(rp - lp - 1);
                    strncpy(argsStr, lp + 1, alen < 255 ? alen : 255);
                }
                else
                {
                    strncpy(classeNome, val + 4, 63);
                    trim(classeNome);
                }
                int id = criarObjeto(classeNome, argsStr);
                if (id > 0)
                {
                    char idStr[20];
                    sprintf(idStr, "%d", id);
                    setVar(nome, idStr);
                }
                return;
            }
            char res[256] = "";
            evalFunc(val, res);
            setVar(nome, res);
            return;
        }
        return;
    }

    {
        char *lb = strchr(lo, '['), *rb = lb ? strchr(lb, ']') : NULL;
        if (lb && rb)
        {
            char *afterRB = rb + 1;
            while (*afterRB == ' ')
                afterRB++;
            char *eq2 = (*afterRB == '=') ? afterRB : NULL;
            if (eq2 && canExec())
            {
                char nomeArr[64] = "";
                int nl2 = (int)(lb - lo);
                strncpy(nomeArr, lo, nl2 < 63 ? nl2 : 63);
                trim(nomeArr);
                char *dotAfterRB = strchr(rb, '.');
                if (!dotAfterRB)
                {
                    char idxStr[128] = "";
                    int il = (int)(rb - lb - 1);
                    strncpy(idxStr, lb + 1, il < 127 ? il : 127);
                    trim(idxStr);
                    int idx = 0;
                    char *v = getVar(idxStr);
                    idx = v ? atoi(v) : atoi(idxStr);
                    char rhs[400] = "";
                    strncpy(rhs, cmd + (eq2 - lo) + 1, 399);
                    trim(rhs);
                    Array *arr = getArray(nomeArr);
                    if (!arr)
                        arr = criarArray(nomeArr);
                    if (!arr)
                        return;
                    if (rhs[0] == '{')
                    {
                        char body[400] = "";
                        int blen = (int)strlen(rhs);
                        if (blen >= 2)
                        {
                            strncpy(body, rhs + 1, blen - 2 < 399 ? blen - 2 : 399);
                            body[blen - 2 < 399 ? blen - 2 : 399] = '\0';
                        }
                        if (idx >= 0 && idx < MAX_ARRAY_SIZE)
                        {
                            arr->elems[idx].ativo = 1;
                            if (idx >= arr->tamanho)
                                arr->tamanho = idx + 1;
                        }
                        char pairs[MAX_ARRAY_FIELDS][300];
                        int nP = splitArgs(body, pairs, MAX_ARRAY_FIELDS);
                        for (int pi = 0; pi < nP; pi++)
                        {
                            char *col = strchr(pairs[pi], ':');
                            if (!col)
                                continue;
                            char cNome[64] = "", cVal[256] = "";
                            int cnl = (int)(col - pairs[pi]);
                            strncpy(cNome, pairs[pi], cnl < 63 ? cnl : 63);
                            trim(cNome);
                            strncpy(cVal, col + 1, 255);
                            trim(cVal);
                            char resolved[256] = "";
                            evalFunc(cVal, resolved);
                            setArrayElem(nomeArr, idx, cNome, resolved);
                        }
                        return;
                    }
                    else
                    {
                        char resolved[256] = "";
                        evalFunc(rhs, resolved);
                        setArrayElem(nomeArr, idx, "valor", resolved);
                        return;
                    }
                }
            }
        }
    }

    if (isArraySetExpr(lo) && canExec())
    {
        char *eq2 = NULL;
        int loLen = (int)strlen(lo);
        for (int i = 1; i < loLen - 1; i++)
        {
            if (lo[i] == '=' && lo[i - 1] != '!' && lo[i - 1] != '<' && lo[i - 1] != '>' && lo[i + 1] != '=')
            {
                eq2 = &lo[i];
                break;
            }
        }
        if (eq2)
        {
            char nome[128] = "", val[400] = "";
            int nl2 = (int)(eq2 - lo);
            strncpy(nome, cmd, nl2 < 127 ? nl2 : 127);
            trim(nome);
            strncpy(val, cmd + (eq2 - lo) + 1, 399);
            trim(val);
            char res[256] = "";
            evalFunc(val, res);
            setVar(nome, res);
            return;
        }
    }

    {
        char *eq2 = NULL;
        int loLen = (int)strlen(lo);
        for (int i = 1; i < loLen - 1; i++)
        {
            if (lo[i] == '=' && lo[i - 1] != '!' && lo[i - 1] != '<' && lo[i - 1] != '>' && lo[i + 1] != '=')
            {
                eq2 = &lo[i];
                break;
            }
        }
        if (eq2)
        {
            char nome[64] = "", val[400] = "";
            if (sscanf(cmd, "%63s = \"%399[^\"]\"", nome, val) == 2)
            {
                setVar(nome, val);
                return;
            }
            if (sscanf(cmd, "%63s = %399[^\n]", nome, val) == 2)
            {
                trim(val);
                char valLo[400];
                strncpy(valLo, val, 399);
                toLow(valLo);
                if (!strncmp(valLo, "new ", 4))
                {
                    char classeNome[64] = "", argsStr[256] = "";
                    char *lp = strchr(val + 4, '('), *rp = strrchr(val + 4, ')');
                    if (lp && rp && rp > lp)
                    {
                        int clen = (int)(lp - (val + 4));
                        strncpy(classeNome, val + 4, clen < 63 ? clen : 63);
                        trim(classeNome);
                        int alen = (int)(rp - lp - 1);
                        strncpy(argsStr, lp + 1, alen < 255 ? alen : 255);
                    }
                    else
                    {
                        strncpy(classeNome, val + 4, 63);
                        trim(classeNome);
                    }
                    int id = criarObjeto(classeNome, argsStr);
                    if (id > 0)
                    {
                        char idStr[20];
                        sprintf(idStr, "%d", id);
                        setVar(nome, idStr);
                    }
                    return;
                }
                char res[256] = "";
                evalFunc(val, res);
                setVar(nome, res);
                return;
            }
        }
    }

    if (!strcmp(lo, "exit") || !strcmp(lo, "quit"))
        exit(0);
    if (!strcmp(lo, "version"))
        printf("MAN v%s\n", VERSAO);
}

static void execPrograma()
{
    cursor = 0;
    while (cursor < nLines)
    {
        char linha[MAX_LINE];
        strncpy(linha, progLines[cursor], MAX_LINE - 1);
        linha[MAX_LINE - 1] = '\0';
        cursor++;
        execLinha(linha);
        processMessages();
    }

    if (topo >= 0)
    {
        for (int i = 0; i <= topo; i++)
        {
            const char *tipo = (pilha[i].tipo == NIV_WHILE) ? "while" : (pilha[i].tipo == NIV_IF) ? "if"
                                                                    : (pilha[i].tipo == NIV_FUNC) ? "func"
                                                                                                  : "bloco";
            erroF(ERR_SINTAXE, "bloco '%s' aberto sem 'end' correspondente", tipo);
        }
    }

    if (erroCount > 0)
        fprintf(stderr, "==> %d erro(s) encontrado(s) durante a execucao.\n\n", erroCount);
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {

        GdiplusStartupInput gdipInput = {1, NULL, FALSE, FALSE};
        GdiplusStartup(&rcGdiplusToken, &gdipInput, NULL);
        hwnd = h;
        gfxInit();
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        if (hdcBuffer)
            BitBlt(hdc, 0, 0, WIN_W, WIN_H, hdcBuffer, 0, 0, SRCCOPY);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        mouse_x = LOWORD(lParam);
        mouse_y = HIWORD(lParam);
        return 0;
    case WM_LBUTTONDOWN:
        mouse_left = 1;
        mouse_click = 1;
        mouse_x = LOWORD(lParam);
        mouse_y = HIWORD(lParam);
        return 0;
    case WM_LBUTTONUP:
        mouse_left = 0;
        mouse_x = LOWORD(lParam);
        mouse_y = HIWORD(lParam);
        return 0;
    case WM_RBUTTONDOWN:
        mouse_right = 1;
        mouse_x = LOWORD(lParam);
        mouse_y = HIWORD(lParam);
        return 0;
    case WM_RBUTTONUP:
        mouse_right = 0;
        mouse_x = LOWORD(lParam);
        mouse_y = HIWORD(lParam);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        if (rcGdiplusToken)
            GdiplusShutdown(rcGdiplusToken);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, msg, wParam, lParam);
}

int main(int argc, char *argv[])
{
    srand((unsigned)time(NULL));
    startTick = GetTickCount();

    char *arquivoScript = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--console") || !strcmp(argv[i], "-c"))
            modoConsole = 1;
        else if (!arquivoScript)
            arquivoScript = argv[i];
    }

    if (!modoConsole)
    {
        FILE *logErro = freopen("man_erros.log", "w", stderr);
        (void)logErro;
    }

    if (modoConsole)
    {

        if (!AttachConsole(ATTACH_PARENT_PROCESS))
            AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);

        if (!arquivoScript)
        {
            fprintf(stderr, "MAN v%s – Modo Console\n", VERSAO);
            fprintf(stderr, "Uso: man.exe --console script.man\n");
            return 1;
        }
        FILE *f = fopen(arquivoScript, "r");
        if (!f)
        {
            fprintf(stderr, "Erro: '%s' nao encontrado\n", arquivoScript);
            return 1;
        }
        char linha[MAX_LINE];
        while (fgets(linha, sizeof(linha), f) && nLines < MAX_LINES)
        {
            linha[strcspn(linha, "\r\n")] = '\0';
            strncpy(progLines[nLines++], linha, MAX_LINE - 1);
        }
        fclose(f);
        execPrograma();
        return 0;
    }

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "MANWin";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    hwnd = CreateWindowA("MANWin", "MAN Grafico v" VERSAO,
                         WS_OVERLAPPEDWINDOW, 100, 100, WIN_W + 16, WIN_H + 39,
                         NULL, NULL, wc.hInstance, NULL);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (!arquivoScript)
    {

        static const char *demo[] = {
            "# MAN v6.3 – Demo 3D",
            "# ESC = sair",
            "",
            "3dinit",
            "3dcam(0, 0, -10, 0, 0)",
            "3dfov(520)",
            "",
            "3dcube(\"cubo\", 2)",
            "3dcolor(\"cubo\", 100, 200, 255)",
            "3dwireframe(\"cubo\", 0)",
            "3dpos(\"cubo\", -3, 0, 2)",
            "",
            "3dsphere(\"esfera\", 1.2, 10, 16)",
            "3dcolor(\"esfera\", 255, 120, 60)",
            "3dpos(\"esfera\", 3, 0, 2)",
            "",
            "3dpyramid(\"piramide\", 2, 2.5)",
            "3dcolor(\"piramide\", 80, 255, 120)",
            "3dwireframe(\"piramide\", 0)",
            "3dpos(\"piramide\", 0, -1, 2)",
            "",
            "3dcylinder(\"cilindro\", 0.7, 2.5, 14)",
            "3dcolor(\"cilindro\", 255, 220, 60)",
            "3dpos(\"cilindro\", 0, 1.5, 6)",
            "",
            "3dplane(\"chao\", 20, 20)",
            "3dcolor(\"chao\", 60, 60, 100)",
            "3dwireframe(\"chao\", 0)",
            "3dpos(\"chao\", 0, -2, 5)",
            "",
            "let ang = 0",
            "let rodando = 1",
            "while (rodando == 1)",
            "begin",
            "    clear",
            "    bgcolor(10, 10, 30)",
            "",
            "    3drot(\"cubo\", ang, ang, 0)",
            "    3drot(\"esfera\", 0, ang, ang)",
            "    3drot(\"piramide\", 0, ang, 0)",
            "    3drot(\"cilindro\", ang, 0, 0)",
            "",
            "    3drender",
            "",
            "    color(255, 255, 100)",
            "    text(10, 570, 13, \"MAN 3D  |  ESC=sair\")",
            "    render",
            "",
            "    ang = ang + 2",
            "    if (ang > 360)",
            "    begin",
            "        ang = 0",
            "    end",
            "    sleep(16)",
            "end",
            NULL};
        for (int i = 0; demo[i]; i++)
        {
            strncpy(progLines[nLines], demo[i], MAX_LINE - 1);
            nLines++;
        }
    }
    else
    {
        FILE *f = fopen(arquivoScript, "r");
        if (!f)
        {
            printf("Erro: '%s' nao encontrado\n", arquivoScript);
            return 1;
        }
        char linha[MAX_LINE];
        while (fgets(linha, sizeof(linha), f) && nLines < MAX_LINES)
        {
            linha[strcspn(linha, "\r\n")] = '\0';
            strncpy(progLines[nLines++], linha, MAX_LINE - 1);
        }
        fclose(f);
    }

    execPrograma();

    if (!modoConsole && erroCount > 0)
    {
        fflush(stderr);

        FILE *logLeitura = fopen("man_erros.log", "r");
        char logBuf[4096] = "";
        if (logLeitura)
        {
            fread(logBuf, 1, sizeof(logBuf) - 1, logLeitura);
            fclose(logLeitura);
        }
        char titulo[64];
        snprintf(titulo, sizeof(titulo), "MAN – %d erro(s) encontrado(s)", erroCount);
        MessageBoxA(hwnd, logBuf[0] ? logBuf : "Erros ocorreram. Veja man_erros.log.",
                    titulo, MB_OK | MB_ICONWARNING);
    }

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}