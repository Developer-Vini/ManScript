#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define LIMPAR_TELA "cls"
#define dormir(ms) Sleep(ms)
#else
#include <unistd.h>
#define LIMPAR_TELA "clear"
#define dormir(ms) usleep((ms) * 1000)
#endif

#define MAX_VARIAVEIS 200
#define MAX_ARRAYS 20
#define MAX_ARRAY_SZ 100
#define MAX_PILHA 200
#define MAX_LINHAS 2000
#define MAX_LINHA 512
#define MAX_TOKENS 60
#define VERSAO "2.0.1"

typedef struct
{
    char nome[50];
    char valor[200];
} Variavel;
typedef struct
{
    char nome[50];
    char itens[MAX_ARRAY_SZ][200];
    int tam;
} Array;

typedef enum
{
    NIVEL_IF,
    NIVEL_WHILE
} TipoNivel;
typedef struct
{
    TipoNivel tipo;
    int exec;
    int ifRes;
    int elseUsado;
    int linhaWhile;
    char condWhile[300];
} NivelPilha;

Variavel vars[MAX_VARIAVEIS];
int nVars = 0;
Array arrs[MAX_ARRAYS];
int nArrs = 0;
NivelPilha pilha[MAX_PILHA];
int topo = -1;

int ultimoRes = 1;
bool ultimoEhWhile = false;
int ultimaLinhaWhile = 0;
char ultimaCondWhile[300] = "";

bool modoDebug = false;
bool modoSilencioso = false;
bool flagBreak = false;

char progLinhas[MAX_LINHAS][MAX_LINHA];
int nLinhas = 0, cursor = 0;
bool modoArq = false;

FILE *stdinReal = NULL;

void trim(char *);
void toLowerCase(char *);
void toUpperCase(char *);
void setVar(char *, char *);
char *getVar(char *);
void setArr(char *, int, char *);
char *getArr(char *, int);
int evalExpr(char *);
void evalFunc(char *, char *);
int evalCond(char *);
int evalCondSimp(char *);
void execLinha(char *);
void mostrarAjuda(void);
void listarVars(void);
void logMsg(char *);

void logMsg(char *m)
{
    if (!modoSilencioso)
        printf("%s", m);
}

void trim(char *s)
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
void toLowerCase(char *s)
{
    for (int i = 0; s[i]; i++)
        s[i] = (char)tolower((unsigned char)s[i]);
}
void toUpperCase(char *s)
{
    for (int i = 0; s[i]; i++)
        s[i] = (char)toupper((unsigned char)s[i]);
}

void setVar(char *nome, char *val)
{
    trim(nome);
    trim(val);
    for (int i = 0; i < nVars; i++)
    {
        if (!strcmp(vars[i].nome, nome))
        {
            strncpy(vars[i].valor, val, 199);
            vars[i].valor[199] = '\0';
            if (modoDebug)
                printf("[D] %s=%s\n", nome, val);
            return;
        }
    }
    if (nVars < MAX_VARIAVEIS)
    {
        strncpy(vars[nVars].nome, nome, 49);
        vars[nVars].nome[49] = '\0';
        strncpy(vars[nVars].valor, val, 199);
        vars[nVars].valor[199] = '\0';
        nVars++;
        if (modoDebug)
            printf("[D] new %s=%s\n", nome, val);
    }
    else
    {
        logMsg("Erro: Limite de variaveis!\n");
    }
}
char *getVar(char *nome)
{
    char t[60];
    strncpy(t, nome, 59);
    t[59] = '\0';
    trim(t);
    for (int i = 0; i < nVars; i++)
        if (!strcmp(vars[i].nome, t))
            return vars[i].valor;
    return NULL;
}

int findArr(char *n)
{
    for (int i = 0; i < nArrs; i++)
        if (!strcmp(arrs[i].nome, n))
            return i;
    return -1;
}
void mkArr(char *n, int tam)
{
    trim(n);
    int ai = findArr(n);
    if (ai < 0)
    {
        if (nArrs >= MAX_ARRAYS)
        {
            logMsg("Erro: Limite de arrays!\n");
            return;
        }
        ai = nArrs++;
        strncpy(arrs[ai].nome, n, 49);
        arrs[ai].nome[49] = '\0';
    }
    arrs[ai].tam = tam < MAX_ARRAY_SZ ? tam : MAX_ARRAY_SZ;
    for (int i = 0; i < arrs[ai].tam; i++)
        strcpy(arrs[ai].itens[i], "0");
}
void setArr(char *n, int i, char *v)
{
    int ai = findArr(n);
    if (ai < 0)
    {
        printf("Erro: Array '%s' nao existe\n", n);
        return;
    }
    if (i < 0 || i >= arrs[ai].tam)
    {
        printf("Erro: Indice %d fora do array '%s'\n", i, n);
        return;
    }
    strncpy(arrs[ai].itens[i], v, 199);
    arrs[ai].itens[i][199] = '\0';
}
char *getArr(char *n, int i)
{
    int ai = findArr(n);
    if (ai < 0 || i < 0 || i >= arrs[ai].tam)
        return NULL;
    return arrs[ai].itens[i];
}

void push(TipoNivel tipo, int exec, int res, int lw, char *cw)
{
    if (topo >= MAX_PILHA - 1)
    {
        logMsg("Erro: Pilha cheia!\n");
        return;
    }
    topo++;
    pilha[topo].tipo = tipo;
    pilha[topo].exec = exec;
    pilha[topo].ifRes = res;
    pilha[topo].elseUsado = 0;
    pilha[topo].linhaWhile = lw;
    if (cw)
        strncpy(pilha[topo].condWhile, cw, 299);
    else
        pilha[topo].condWhile[0] = '\0';
    pilha[topo].condWhile[299] = '\0';
    if (modoDebug)
        printf("[D] PUSH tipo=%d exec=%d topo=%d\n", tipo, exec, topo);
}
void pop()
{
    if (topo >= 0)
    {
        if (modoDebug)
            printf("[D] POP topo=%d\n", topo);
        topo--;
    }
}
int canExec()
{
    for (int i = 0; i <= topo; i++)
        if (!pilha[i].exec)
            return 0;
    return 1;
}

static const char *findMatchingBracket(const char *lb)
{
    if (!lb || *lb != '[')
        return NULL;
    int dep = 1;
    const char *p = lb + 1;
    while (*p && dep > 0)
    {
        if (*p == '[')
            dep++;
        else if (*p == ']')
            dep--;
        p++;
    }
    return (dep == 0) ? p - 1 : NULL;
}

int evalExpr(char *expr);

int resolveOp(char *tok)
{
    char t[200];
    strncpy(t, tok, 199);
    t[199] = '\0';
    trim(t);
    if (!strncmp(t, "rand(", 5))
    {
        int a = 0, b = 100;
        sscanf(t + 5, "%d,%d", &a, &b);
        if (b < a)
        {
            int tmp = a;
            a = b;
            b = tmp;
        }
        return a + rand() % (b - a + 1);
    }
    if (!strncmp(t, "len(", 4))
    {
        int tl = (int)strlen(t);
        if (t[tl - 1] == ')')
            t[tl - 1] = '\0';
        char a[100] = "";
        strncpy(a, t + 4, 99);
        trim(a);
        char *v = getVar(a);
        return v ? (int)strlen(v) : 0;
    }
    if (!strncmp(t, "int(", 4))
    {
        int tl = (int)strlen(t);
        if (t[tl - 1] == ')')
            t[tl - 1] = '\0';
        char a[150] = "";
        strncpy(a, t + 4, 149);
        trim(a);
        char *v = getVar(a);
        return v ? atoi(v) : atoi(a);
    }

    {
        char *lb = strchr(t, '[');
        const char *rb = lb ? findMatchingBracket(lb) : NULL;
        if (lb && rb && rb > lb && *(rb + 1) == '\0')
        {
            char an[50];
            int nl = (int)(lb - t);
            strncpy(an, t, nl);
            an[nl] = '\0';
            trim(an);
            char is[50];
            int il = (int)(rb - lb - 1);
            strncpy(is, lb + 1, il);
            is[il] = '\0';
            int idx = evalExpr(is);
            char *v = getArr(an, idx);
            return v ? atoi(v) : 0;
        }
    }
    char *v = getVar(t);
    return v ? atoi(v) : atoi(t);
}

int evalExpr(char *expr)
{
    char buf[300];
    strncpy(buf, expr, 299);
    buf[299] = '\0';
    trim(buf);
    char toks[MAX_TOKENS][100];
    char ops[MAX_TOKENS];
    int nT = 0, nO = 0;
    char cur[100];
    int cL = 0, dep = 0;

    for (int i = 0;; i++)
    {
        char c = buf[i];
        bool isOp = (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') && dep == 0;
        bool isSig = (c == '-') && cL == 0 && dep == 0;
        if (c == '(' || c == '[')
            dep++;
        else if (c == ')' || c == ']')
            dep--;

        if (c == '\0' || (isOp && !isSig))
        {
            cur[cL] = '\0';
            trim(cur);
            if (cL > 0 && nT < MAX_TOKENS)
            {
                strncpy(toks[nT], cur, 99);
                toks[nT][99] = '\0';
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
            if (cL < 99)
                cur[cL++] = c;
        }
    }
    if (nT == 0)
        return 0;

    int v[MAX_TOKENS];
    for (int i = 0; i < nT; i++)
        v[i] = resolveOp(toks[i]);

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
                    logMsg("Erro: Divisao por zero\n");
                    return 0;
                }
                r = v[i] / v[i + 1];
            }
            else
            {
                if (!v[i + 1])
                {
                    logMsg("Erro: Modulo por zero\n");
                    return 0;
                }
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

void evalFunc(char *expr, char *out)
{
    char t[300];
    strncpy(t, expr, 299);
    t[299] = '\0';
    trim(t);

    if (t[0] == '"')
    {
        char *i = t + 1, *f = strrchr(i, '"');
        if (f)
        {
            *f = '\0';
            strncpy(out, i, 199);
        }
        else
            strncpy(out, i, 199);
        out[199] = '\0';
        return;
    }
    if (!strncmp(t, "upper(", 6))
    {
        int tl = (int)strlen(t);
        if (t[tl - 1] == ')')
            t[tl - 1] = '\0';
        char a[100] = "";
        strncpy(a, t + 6, 99);
        trim(a);
        char *v = getVar(a);
        if (!v)
            v = a;
        strncpy(out, v, 199);
        out[199] = '\0';
        toUpperCase(out);
        return;
    }
    if (!strncmp(t, "lower(", 6))
    {
        int tl = (int)strlen(t);
        if (t[tl - 1] == ')')
            t[tl - 1] = '\0';
        char a[100] = "";
        strncpy(a, t + 6, 99);
        trim(a);
        char *v = getVar(a);
        if (!v)
            v = a;
        strncpy(out, v, 199);
        out[199] = '\0';
        toLowerCase(out);
        return;
    }
    if (!strncmp(t, "str(", 4))
    {
        int tl = (int)strlen(t);
        if (t[tl - 1] == ')')
            t[tl - 1] = '\0';
        char a[150] = "";
        strncpy(a, t + 4, 149);
        trim(a);
        sprintf(out, "%d", evalExpr(a));
        return;
    }
    if (!strncmp(t, "contains(", 9))
    {
        char *cm = strchr(t + 9, ',');
        if (!cm)
        {
            strcpy(out, "0");
            return;
        }
        char a1[100] = "", a2[100] = "";
        int al = (int)(cm - (t + 9));
        strncpy(a1, t + 9, al);
        a1[al] = '\0';
        trim(a1);
        char *rest = cm + 1;
        int rl = (int)strlen(rest);
        if (rl > 0 && rest[rl - 1] == ')')
            rest[rl - 1] = '\0';
        strncpy(a2, rest, 99);
        trim(a2);
        if (a2[0] == '"')
        {
            memmove(a2, a2 + 1, strlen(a2) + 1);
            int l = (int)strlen(a2);
            if (l > 0 && a2[l - 1] == '"')
                a2[l - 1] = '\0';
        }
        char *v = getVar(a1);
        if (!v)
            v = a1;
        strcpy(out, strstr(v, a2) ? "1" : "0");
        return;
    }
    if (!strncmp(t, "len(", 4))
    {
        int tl = (int)strlen(t);
        if (t[tl - 1] == ')')
            t[tl - 1] = '\0';
        char a[100] = "";
        strncpy(a, t + 4, 99);
        trim(a);
        char *v = getVar(a);
        sprintf(out, "%d", (int)(v ? strlen(v) : 0));
        return;
    }
    if (!strncmp(t, "rand(", 5))
    {
        sprintf(out, "%d", evalExpr(t));
        return;
    }

    {
        char *lb = strchr(t, '[');
        const char *rb = lb ? findMatchingBracket(lb) : NULL;
        if (lb && rb && rb > lb && *(rb + 1) == '\0')
        {
            char an[50];
            int nl = (int)(lb - t);
            strncpy(an, t, nl);
            an[nl] = '\0';
            trim(an);
            char is[50];
            int il = (int)(rb - lb - 1);
            strncpy(is, lb + 1, il);
            is[il] = '\0';
            int idx = evalExpr(is);
            char *v = getArr(an, idx);
            strncpy(out, v ? v : "0", 199);
            out[199] = '\0';
            return;
        }
    }

    {
        bool emA = false, op = false;
        int dep = 0;
        for (int i = 0; t[i]; i++)
        {
            if (t[i] == '"')
                emA = !emA;
            if (!emA && (t[i] == '(' || t[i] == '['))
                dep++;
            if (!emA && (t[i] == ')' || t[i] == ']'))
                dep--;
            if (!emA && dep == 0)
            {
                char c = t[i];
                if (c == '+' || c == '*' || c == '/' || c == '%')
                {
                    op = true;
                    break;
                }
                if (c == '-' && i > 0)
                {
                    op = true;
                    break;
                }
            }
        }
        if (op)
        {
            sprintf(out, "%d", evalExpr(t));
            return;
        }
    }

    char *v = getVar(t);
    if (v)
    {
        strncpy(out, v, 199);
        out[199] = '\0';
        return;
    }
    strncpy(out, t, 199);
    out[199] = '\0';
}

int evalCondSimp(char *cond)
{
    char buf[300];
    strncpy(buf, cond, 299);
    buf[299] = '\0';
    trim(buf);
    char *op = NULL;
    char opS[3] = "";
    int opL = 0;

    if ((op = strstr(buf, "!=")))
    {
        strcpy(opS, "!=");
        opL = 2;
    }
    else if ((op = strstr(buf, ">=")))
    {
        strcpy(opS, ">=");
        opL = 2;
    }
    else if ((op = strstr(buf, "<=")))
    {
        strcpy(opS, "<=");
        opL = 2;
    }
    else if ((op = strstr(buf, "==")))
    {
        strcpy(opS, "==");
        opL = 2;
    }
    else if ((op = strstr(buf, ">")))
    {
        strcpy(opS, ">");
        opL = 1;
    }
    else if ((op = strstr(buf, "<")))
    {
        strcpy(opS, "<");
        opL = 1;
    }
    else
    {
        char r[200] = "";
        evalFunc(buf, r);
        return atoi(r) != 0;
    }

    char esq[150] = "", dir[150] = "";
    int el = (int)(op - buf);
    strncpy(esq, buf, el);
    esq[el] = '\0';
    trim(esq);
    strncpy(dir, op + opL, 149);
    dir[149] = '\0';
    trim(dir);

    bool dStr = false;
    if (dir[0] == '"')
    {
        dStr = true;
        memmove(dir, dir + 1, strlen(dir) + 1);
        int dl = (int)strlen(dir);
        if (dl > 0 && dir[dl - 1] == '"')
            dir[dl - 1] = '\0';
    }

    char ve[200] = "", vd[200] = "";
    evalFunc(esq, ve);
    if (!dStr)
        evalFunc(dir, vd);
    else
        strncpy(vd, dir, 199);

    if (modoDebug)
        printf("[D] cond '%s' %s '%s'\n", ve, opS, vd);

    bool ne = (ve[0] == '-' || isdigit((unsigned char)ve[0]));
    bool nd = (vd[0] == '-' || isdigit((unsigned char)vd[0]));
    if (!dStr && ne && nd)
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

int evalCond(char *cond)
{
    char buf[300];
    strncpy(buf, cond, 299);
    buf[299] = '\0';
    trim(buf);
    char *orP = strstr(buf, " or ");
    if (orP)
    {
        char e[150] = "", d[150] = "";
        int el = (int)(orP - buf);
        strncpy(e, buf, el);
        e[el] = '\0';
        strncpy(d, orP + 4, 149);
        return evalCond(e) || evalCond(d);
    }
    char *anP = strstr(buf, " and ");
    if (anP)
    {
        char e[150] = "", d[150] = "";
        int el = (int)(anP - buf);
        strncpy(e, buf, el);
        e[el] = '\0';
        strncpy(d, anP + 5, 149);
        return evalCond(e) && evalCond(d);
    }
    return evalCondSimp(buf);
}

void procWrite(char *conteudo, bool nl)
{
    trim(conteudo);
    bool temStr = false;
    {
        bool ea = false;
        for (int i = 0; conteudo[i]; i++)
            if (conteudo[i] == '"')
            {
                ea = !ea;
                temStr = true;
            }
    }

    char out[600] = "";
    if (!temStr)
    {
        char r[200] = "";
        evalFunc(conteudo, r);
        strncpy(out, r, 599);
    }
    else
    {
        int pos = 0, tot = (int)strlen(conteudo);
        bool ea = false;
        char pt[300] = "";
        int pl = 0;
        while (pos <= tot)
        {
            char c = (pos < tot) ? conteudo[pos] : '\0';
            if (c == '"')
                ea = !ea;
            if ((!ea && c == '+') || pos == tot)
            {
                pt[pl] = '\0';
                trim(pt);
                char r[200] = "";
                evalFunc(pt, r);
                strncat(out, r, sizeof(out) - strlen(out) - 1);
                pl = 0;
            }
            else
            {
                if (pl < 299)
                    pt[pl++] = c;
            }
            pos++;
        }
    }
    if (nl)
        printf("%s\n", out);
    else
    {
        printf("%s", out);
        fflush(stdout);
    }
}

void execLinha(char *cmd)
{
    cmd[strcspn(cmd, "\n")] = 0;
    char *com = strchr(cmd, '#');
    if (com)
        *com = '\0';
    trim(cmd);
    if (!strlen(cmd))
        return;

    char lo[MAX_LINHA];
    strncpy(lo, cmd, MAX_LINHA - 1);
    lo[MAX_LINHA - 1] = '\0';
    toLowerCase(lo);

    if (!strcmp(lo, "help") || !strcmp(lo, "?"))
    {
        mostrarAjuda();
        return;
    }
    if (!strcmp(lo, "list"))
    {
        listarVars();
        return;
    }
    if (!strcmp(lo, "clear") || !strcmp(lo, "cls"))
    {
        system(LIMPAR_TELA);
        return;
    }
    //vou deixar, vai ser interessante futuramente
    if (!strcmp(lo, "debug on"))
    {
        modoDebug = true;
        logMsg("Debug ativado\n");
        return;
    }
    if (!strcmp(lo, "debug off"))
    {
        modoDebug = false;
        logMsg("Debug desativado\n");
        return;
    }
    if (!strcmp(lo, "silent on"))
    {
        modoSilencioso = true;
        printf("Silencioso ativado\n");
        return;
    }
    if (!strcmp(lo, "silent off"))
    {
        modoSilencioso = false;
        printf("Silencioso desativado\n");
        return;
    }
    //------------------------------------
    if (!strcmp(lo, "exit") || !strcmp(lo, "quit"))
    {
        printf("Saindo...\n");
        exit(0);
    }
    if (!strcmp(lo, "version"))
    {
        if (!modoSilencioso)
            printf("MAN v%s\n", VERSAO);
        return;
    }

    if (!strncmp(lo, "if", 2) && (lo[2] == ' ' || lo[2] == '('))
    {
        char *ab = strchr(cmd, '('), *fe = strrchr(cmd, ')');
        if (ab && fe && fe > ab)
        {
            char cond[300];
            int cl = (int)(fe - ab - 1);
            strncpy(cond, ab + 1, cl);
            cond[cl] = '\0';

            ultimoRes = canExec() ? evalCond(cond) : 0;
            ultimoEhWhile = false;
            if (modoDebug)
                printf("[D] IF=%d\n", ultimoRes);
        }
        else
            logMsg("Erro: Sintaxe invalida para IF\n");
        return;
    }

    if (!strncmp(lo, "while", 5) && (lo[5] == ' ' || lo[5] == '('))
    {
        char *ab = strchr(cmd, '('), *fe = strrchr(cmd, ')');
        if (ab && fe && fe > ab)
        {
            char cond[300];
            int cl = (int)(fe - ab - 1);
            strncpy(cond, ab + 1, cl);
            cond[cl] = '\0';
            ultimoRes = canExec() ? evalCond(cond) : 0;
            ultimoEhWhile = true;
            ultimaLinhaWhile = cursor - 1;
            strncpy(ultimaCondWhile, cond, 299);
            ultimaCondWhile[299] = '\0';
            if (modoDebug)
                printf("[D] WHILE=%d linha=%d\n", ultimoRes, ultimaLinhaWhile);
        }
        else
            logMsg("Erro: Sintaxe invalida para WHILE\n");
        return;
    }

    if (!strcmp(lo, "begin"))
    {
        int paiExec = (topo >= 0) ? pilha[topo].exec : 1;
        int exec = paiExec && ultimoRes;
        if (ultimoEhWhile)
            push(NIVEL_WHILE, exec, ultimoRes, ultimaLinhaWhile, ultimaCondWhile);
        else
            push(NIVEL_IF, exec, ultimoRes, 0, NULL);
        if (modoDebug)
            printf("[D] BEGIN tipo=%s exec=%d\n", ultimoEhWhile ? "while" : "if", exec);
        ultimoEhWhile = false;
        return;
    }

    if (!strcmp(lo, "else"))
    {
        if (topo >= 0 && !pilha[topo].elseUsado && pilha[topo].tipo == NIVEL_IF)
        {
            int paiExec = (topo > 0) ? pilha[topo - 1].exec : 1;
            pilha[topo].exec = (!pilha[topo].ifRes) && paiExec;
            pilha[topo].elseUsado = 1;
            if (modoDebug)
                printf("[D] ELSE exec=%d\n", pilha[topo].exec);
        }
        return;
    }

    if (!strcmp(lo, "end"))
    {
        if (topo >= 0)
        {
            if (pilha[topo].tipo == NIVEL_WHILE && pilha[topo].exec && !flagBreak)
            {
                int res = evalCond(pilha[topo].condWhile);
                if (modoDebug)
                    printf("[D] END while cond=%d\n", res);
                if (res && modoArq)
                {
                    int lw = pilha[topo].linhaWhile;
                    pop();
                    cursor = lw;
                    if (modoDebug)
                        printf("[D] LOOP -> linha %d\n", cursor);
                    return;
                }
            }
            flagBreak = false;
            pop();
        }
        if (modoDebug)
            printf("[D] END topo=%d\n", topo);
        return;
    }

    if (!strcmp(lo, "break"))
    {

        if (!canExec())
        {
            if (modoDebug)
                printf("[D] break pulado (bloco inativo)\n");
            return;
        }

        if (modoArq)
        {

            int nIfsAtivos = 0;
            for (int i = topo; i >= 0; i--)
            {
                if (pilha[i].tipo == NIVEL_WHILE)
                    break;
                nIfsAtivos++;
            }

            int dep = nIfsAtivos;
            while (cursor < nLinhas)
            {
                char tmp[MAX_LINHA];
                strncpy(tmp, progLinhas[cursor], MAX_LINHA - 1);
                toLowerCase(tmp);
                trim(tmp);
                cursor++;
                if (!strcmp(tmp, "begin"))
                    dep++;
                else if (!strcmp(tmp, "end"))
                {
                    if (dep == 0)
                        break;
                    dep--;
                }
            }
        }

        while (topo >= 0)
        {
            bool ehWhile = (pilha[topo].tipo == NIVEL_WHILE);
            pop();
            if (ehWhile)
                break;
        }
        return;
    }

    if (!canExec())
    {
        if (modoDebug)
            printf("[D] pulado\n");
        return;
    }

    if (!strncmp(lo, "sleep(", 6))
    {
        char a[50] = "";
        sscanf(cmd + 6, "%49[^)]", a);
        dormir(evalExpr(a));
        return;
    }

    if (!strncmp(lo, "array ", 6))
    {
        char nome[50] = "";
        int tam = 10;
        char *lb = strchr(cmd + 6, '['), *rb = lb ? strchr(lb, ']') : NULL;
        if (lb && rb)
        {
            int nl = (int)(lb - (cmd + 6));
            strncpy(nome, cmd + 6, nl);
            nome[nl] = '\0';
            trim(nome);
            char ts[20];
            int tl = (int)(rb - lb - 1);
            strncpy(ts, lb + 1, tl);
            ts[tl] = '\0';
            tam = evalExpr(ts);
        }
        else
        {
            sscanf(cmd + 6, "%49s", nome);
        }
        mkArr(nome, tam);
        return;
    }

    if (!strncmp(lo, "let ", 4))
    {
        char *lb = strchr(cmd + 4, '['), *rb = lb ? strchr(lb, ']') : NULL;
        char *eq = strstr(cmd + 4, "=");

        if (lb && rb && eq && eq > rb)
        {
            char nome[50] = "";
            int nl = (int)(lb - (cmd + 4));
            strncpy(nome, cmd + 4, nl);
            nome[nl] = '\0';
            trim(nome);
            char is[50];
            int il = (int)(rb - lb - 1);
            strncpy(is, lb + 1, il);
            is[il] = '\0';
            int idx = evalExpr(is);
            char val[200] = "";
            strncpy(val, eq + 1, 199);
            trim(val);
            char res[200] = "";
            evalFunc(val, res);
            setArr(nome, idx, res);
            if (!modoSilencioso)
                printf("%s[%d] = \"%s\"\n", nome, idx, res);
            return;
        }
        char nome[60] = "", val[300] = "";

        if (sscanf(cmd + 4, "%59s = \"%299[^\"]\"", nome, val) == 2)
        {
            setVar(nome, val);
            if (!modoSilencioso)
                printf("'%s' = \"%s\"\n", nome, val);
            return;
        }

        if (sscanf(cmd + 4, "%59s = %299[^\n]", nome, val) == 2)
        {
            trim(val);
            char res[200] = "";
            evalFunc(val, res);
            setVar(nome, res);
            if (!modoSilencioso)
                printf("'%s' = \"%s\"\n", nome, res);
            return;
        }
        logMsg("Erro: Sintaxe invalida para LET\n");
        return;
    }

    if (!strncmp(lo, "write(", 6))
    {
        int cl = (int)strlen(cmd);
        if (cmd[cl - 1] != ')')
        {
            logMsg("Erro: ')' faltando em write\n");
            return;
        }
        char c[400] = "";
        int cn = cl - 7;
        if (cn < 0)
            cn = 0;
        strncpy(c, cmd + 6, cn);
        c[cn] = '\0';
        procWrite(c, true);
        return;
    }

    if (!strncmp(lo, "writeln(", 8))
    {
        int cl = (int)strlen(cmd);
        if (cmd[cl - 1] != ')')
        {
            logMsg("Erro: ')' faltando em writeln\n");
            return;
        }
        char c[400] = "";
        int cn = cl - 9;
        if (cn < 0)
            cn = 0;
        strncpy(c, cmd + 8, cn);
        c[cn] = '\0';
        procWrite(c, false);
        return;
    }

    if (!strncmp(lo, "input(", 6))
    {
        int cl = (int)strlen(cmd);
        if (cmd[cl - 1] != ')')
        {
            logMsg("Erro: ')' faltando em input\n");
            return;
        }
        char args[300] = "";
        int an = cl - 7;
        if (an < 0)
            an = 0;
        strncpy(args, cmd + 6, an);
        args[an] = '\0';
        trim(args);
        char msg[200] = "", var[60] = "";
        if (args[0] == '"')
        {
            char *eq2 = strrchr(args + 1, '"');
            if (eq2)
            {
                int ml = (int)(eq2 - (args + 1));
                strncpy(msg, args + 1, ml);
                msg[ml] = '\0';
                char *cm = strchr(eq2, ',');
                if (cm)
                {
                    strncpy(var, cm + 1, 59);
                    trim(var);
                }
                else
                    strncpy(var, "entrada", 59);
            }
        }
        else
        {
            strncpy(var, args, 59);
            trim(var);
            sprintf(msg, "Digite %s", var);
        }
        printf("%s: ", msg);
        fflush(stdout);
        char val[200] = "";
        FILE *src = (stdinReal && modoArq) ? stdinReal : stdin;
        if (fgets(val, sizeof(val), src))
        {
            val[strcspn(val, "\n")] = 0;
            trim(val);
            setVar(var, val);
            if (!modoSilencioso)
                printf("[%s=\"%s\"]\n", var, val);
        }
        return;
    }

    {
        char *eq = NULL;
        int loLen = (int)strlen(lo);
        for (int i = 1; i < loLen - 1; i++)
        {
            if (lo[i] == '=' && lo[i - 1] != '!' && lo[i - 1] != '<' && lo[i - 1] != '>' && lo[i + 1] != '=')
            {
                eq = &lo[i];
                break;
            }
        }
        if (eq)
        {
            int off = (int)(eq - lo);
            char *lb2 = (char *)memchr(lo, '[', off);
            char *rb2 = lb2 ? (char *)memchr(lb2, ']', off - (int)(lb2 - lo)) : NULL;
            if (lb2 && rb2)
            {
                char nome[50] = "";
                int nl = (int)(lb2 - lo);
                strncpy(nome, cmd, nl);
                nome[nl] = '\0';
                trim(nome);
                char is[50];
                int il = (int)(rb2 - lb2 - 1);
                strncpy(is, cmd + (lb2 - lo) + 1, il);
                is[il] = '\0';
                int idx = evalExpr(is);
                char val[200] = "";
                strncpy(val, cmd + off + 1, 199);
                trim(val);
                char res[200] = "";
                evalFunc(val, res);
                setArr(nome, idx, res);
                if (!modoSilencioso)
                    printf("%s[%d]=\"%s\"\n", nome, idx, res);
                return;
            }
            char nome[60] = "", val[300] = "";
            if (sscanf(cmd, "%59s = \"%299[^\"]\"", nome, val) == 2)
            {
                setVar(nome, val);
                if (!modoSilencioso)
                    printf("'%s'=\"%s\"\n", nome, val);
                return;
            }
            if (sscanf(cmd, "%59s = %299[^\n]", nome, val) == 2)
            {
                trim(val);
                char res[200] = "";
                evalFunc(val, res);
                setVar(nome, res);
                if (!modoSilencioso)
                    printf("'%s'=\"%s\"\n", nome, res);
                return;
            }
        }
    }

    logMsg("Comando desconhecido\n");
}

void execPrograma()
{
    modoArq = true;
    cursor = 0;
    while (cursor < nLinhas)
    {
        char linha[MAX_LINHA];
        strncpy(linha, progLinhas[cursor], MAX_LINHA - 1);
        linha[MAX_LINHA - 1] = '\0';
        cursor++;
        if (!modoSilencioso)
            printf("[%d] %s\n", cursor, linha);
        execLinha(linha);
    }
    modoArq = false;
}

void mostrarAjuda()
{
    if (modoSilencioso)
        return;
    printf("\n==========================================\n");
    printf("      INTERPRETADOR MAN v%s\n", VERSAO);
    printf("==========================================\n");
    printf("VARIAVEIS\n");
    printf("  let x = \"str\"          string\n");
    printf("  let x = expr           numero/expressao\n");
    printf("SAIDA\n");
    printf("  write(expr)            imprime + newline\n");
    printf("  writeln(expr)          imprime SEM newline\n");
    printf("ENTRADA\n");
    printf("  input(var)             prompt padrao\n");
    printf("  input(\"Msg\", var)      prompt customizado\n");
    printf("CONDICIONAL\n");
    printf("  if (cond) begin\n");
    printf("    ...\n");
    printf("  else\n");
    printf("    ...\n");
    printf("  end\n");
    printf("  Ops: == != > < >= <=   and   or\n");
    printf("LOOP\n");
    printf("  while (cond) begin\n");
    printf("    ...\n");
    printf("    break\n");
    printf("  end\n");
    printf("ARRAYS\n");
    printf("  array nome[N]          cria array\n");
    printf("  nome[i] = val          atribui\n");
    printf("  let x = nome[i]        le\n");
    printf("FUNCOES\n");
    printf("  rand(min,max)          numero aleatorio\n");
    printf("  len(var)               tamanho string\n");
    printf("  upper(var)             maiusculo\n");
    printf("  lower(var)             minusculo\n");
    printf("  contains(v,\"txt\")      1 se contem\n");
    printf("  str(expr)              numero p/ string\n");
    printf("  int(expr)              string p/ inteiro\n");
    printf("  sleep(ms)              pausa\n");
    printf("ARITMETICA:  + - * / %%\n");
    printf("MISC:  # comentario   clear   list   exit\n");
    printf("==========================================\n\n");
}

void listarVars()
{
    if (modoSilencioso)
        return;
    printf("\n=== VARIAVEIS ===\n");
    if (!nVars && !nArrs)
        printf("  (nenhuma)\n");
    for (int i = 0; i < nVars; i++)
        printf("  %-20s = \"%s\"\n", vars[i].nome, vars[i].valor);
    if (nArrs)
    {
        printf("=== ARRAYS ===\n");
        for (int i = 0; i < nArrs; i++)
        {
            printf("  %s[%d]:", arrs[i].nome, arrs[i].tam);
            for (int j = 0; j < arrs[i].tam && j < 5; j++)
                printf(" \"%s\"", arrs[i].itens[j]);
            if (arrs[i].tam > 5)
                printf(" ...");
            printf("\n");
        }
    }
    printf("=================\n\n");
}

static void adicionarLinha(char *linha)
{
    if (nLinhas >= MAX_LINHAS)
        return;
    strncpy(progLinhas[nLinhas], linha, MAX_LINHA - 1);
    progLinhas[nLinhas][MAX_LINHA - 1] = '\0';
    nLinhas++;
}

static void preprocessarLinha(char *linha)
{
    char lo[MAX_LINHA];
    strncpy(lo, linha, MAX_LINHA - 1);
    lo[MAX_LINHA - 1] = '\0';
    trim(lo);
    toLowerCase(lo);

    bool ehIf = (!strncmp(lo, "if", 2) && (lo[2] == ' ' || lo[2] == '('));
    bool ehWhile = (!strncmp(lo, "while", 5) && (lo[5] == ' ' || lo[5] == '('));

    if ((ehIf || ehWhile) && nLinhas < MAX_LINHAS - 1)
    {

        int ll = (int)strlen(lo);

        char *fechaParen = strrchr(lo, ')');
        if (fechaParen)
        {

            char *resto = fechaParen + 1;

            while (*resto == ' ' || *resto == '\t')
                resto++;
            if (!strcmp(resto, "begin") && ll > 6)
            {

                char semBegin[MAX_LINHA];
                strncpy(semBegin, linha, MAX_LINHA - 1);
                semBegin[MAX_LINHA - 1] = '\0';

                char *fp2 = strrchr(semBegin, ')');
                if (fp2)
                {
                    *(fp2 + 1) = '\0';
                }
                trim(semBegin);
                if (nLinhas < MAX_LINHAS - 1)
                {
                    adicionarLinha(semBegin);
                    adicionarLinha("begin");
                }
                return;
            }
        }
    }
    adicionarLinha(linha);
}

void modoInterativo()
{
    char linha[MAX_LINHA];
    if (!modoSilencioso)
    {
        printf("\n==========================================\n");
        printf("       INTERPRETADOR MAN v%s\n", VERSAO);
        printf("       Digite 'help' para ajuda\n");
        printf("==========================================\n\n");
    }
    while (1)
    {
        if (!modoSilencioso)
            printf(">> ");
        if (!fgets(linha, sizeof(linha), stdin))
            break;
        execLinha(linha);
    }
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    srand((unsigned)time(NULL));
#ifdef _WIN32
    stdinReal = fopen("CON", "r");
#else
    stdinReal = fopen("/dev/tty", "r");
#endif
    if (!stdinReal)
        stdinReal = stdin;

    bool sil = false;
    char *arq = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--silent"))
            sil = true;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            printf("Uso: %s [opcoes] [arquivo.man]\n  -s  Modo silencioso\n  -h  Ajuda\n", argv[0]);
            return 0;
        }
        else if (argv[i][0] != '-')
            arq = argv[i];
    }
    modoSilencioso = sil;

    if (arq)
    {
        FILE *f = fopen(arq, "r");
        if (!f)
        {
            printf("Erro: '%s' nao encontrado\n", arq);
            return 1;
        }
        nLinhas = 0;
        char linha[MAX_LINHA];
        while (fgets(linha, sizeof(linha), f) && nLinhas < MAX_LINHAS)
        {
            linha[strcspn(linha, "\r\n")] = '\0';
            preprocessarLinha(linha);
        }
        fclose(f);
        if (!modoSilencioso)
            printf("Executando: %s (%d linhas)\n\n", arq, nLinhas);
        execPrograma();
        if (!modoSilencioso)
            printf("\nConcluido.\n");
    }
    else
    {
        modoInterativo();
    }

    if (stdinReal && stdinReal != stdin)
        fclose(stdinReal);
    return 0;
}