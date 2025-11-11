// Compiler.c — AlphaDelta simplificado (print, strings, arrays, números)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Tokens */
typedef enum {
    T_EOF, T_NUM, T_STR, T_IDENT,
    T_PLUS, T_MINUS, T_MUL, T_DIV,
    T_LPAREN, T_RPAREN, T_LBRACK, T_RBRACK,
    T_SEMI, T_EQ, T_COMMA, T_COLON, T_DOT,
    T_GT, T_LT, T_GTE, T_LTE, T_EQEQ, T_NEQ
} TokenType;

typedef struct { TokenType type; double num; char *s; } Token;

char *src; size_t pos = 0;
Token curtok;

/* Helpers */
void error(const char *msg) { fprintf(stderr, "Erro: %s\n", msg); exit(1); }

/* Lexer */
void next_token() {
    while (isspace((unsigned char)src[pos])) pos++;
    if (src[pos] == '\0') { curtok.type = T_EOF; return; }

    /* string literal */
    if (src[pos] == '"') {
        pos++;
        size_t start = pos;
        while (src[pos] && src[pos] != '"') {
            if (src[pos] == '\\' && src[pos+1]) pos += 2; else pos++;
        }
        if (src[pos] != '"') error("Unterminated string literal");
        size_t len = pos - start;
        curtok.s = strndup(src + start, len);
        pos++; curtok.type = T_STR; return;
    }

    /* number */
    if (isdigit((unsigned char)src[pos]) || (src[pos] == '.' && isdigit((unsigned char)src[pos+1]))) {
        char *end; curtok.num = strtod(src + pos, &end);
        pos = end - src; curtok.type = T_NUM; return;
    }

    /* identifier */
    if (isalpha((unsigned char)src[pos]) || src[pos] == '_') {
        size_t start = pos;
        while (isalnum((unsigned char)src[pos]) || src[pos] == '_') pos++;
        size_t len = pos - start;
        curtok.s = strndup(src + start, len);
        curtok.type = T_IDENT;
        return;
    }

    char c = src[pos++];
    switch (c) {
        case '+': curtok.type = T_PLUS; break;
        case '-': curtok.type = T_MINUS; break;
        case '*': curtok.type = T_MUL; break;
        case '/': curtok.type = T_DIV; break;
        case '(': curtok.type = T_LPAREN; break;
        case ')': curtok.type = T_RPAREN; break;
        case '[': curtok.type = T_LBRACK; break;
        case ']': curtok.type = T_RBRACK; break;
        case ';': curtok.type = T_SEMI; break;
        case ',': curtok.type = T_COMMA; break;
        case ':': curtok.type = T_COLON; break;
        case '.': curtok.type = T_DOT; break;
        case '=':
            if (src[pos] == '=') { pos++; curtok.type = T_EQEQ; }
            else curtok.type = T_EQ;
            break;
        case '>':
            if (src[pos] == '=') { pos++; curtok.type = T_GTE; }
            else curtok.type = T_GT;
            break;
        case '<':
            if (src[pos] == '=') { pos++; curtok.type = T_LTE; }
            else curtok.type = T_LT;
            break;
        case '!':
            if (src[pos] == '=') { pos++; curtok.type = T_NEQ; }
            else error("Caracter '!' inválido");
            break;
        default: fprintf(stderr, "Caractere desconhecido: %c\n", c); exit(1);
    }
}

/* Parser helpers */
int accept(TokenType t) { if (curtok.type == t) { next_token(); return 1; } return 0; }
void expect(TokenType t) { if (!accept(t)) { fprintf(stderr,"Syntax error: expected %d\n", t); exit(1); } }

/* AST node types */
typedef enum { N_NUM, N_STR, N_VAR, N_BIN, N_ASSIGN, N_PRINT, N_EXPR_STMT, N_ARR_LITERAL, N_ARR_ACCESS } NodeType;
typedef struct Node {
    NodeType type;
    double num;
    char *s;
    char op;
    struct Node *left, *right;
    struct Node *next;
} Node;

/* Node constructors */
Node* newnum(double v){ Node *n=calloc(1,sizeof(Node)); n->type=N_NUM; n->num=v; return n; }
Node* newstr(char *s){ Node *n=calloc(1,sizeof(Node)); n->type=N_STR; n->s=s; return n; }
Node* newvar(char *name){ Node *n=calloc(1,sizeof(Node)); n->type=N_VAR; n->s=name; return n; }
Node* newbin(char op, Node *l, Node *r){ Node *n=calloc(1,sizeof(Node)); n->type=N_BIN; n->op=op; n->left=l; n->right=r; return n; }
Node* newassign(char *name, Node *expr){ Node *n=calloc(1,sizeof(Node)); n->type=N_ASSIGN; n->s=name; n->left=expr; return n; }
Node* newprint(Node *expr){ Node *n=calloc(1,sizeof(Node)); n->type=N_PRINT; n->left=expr; return n; }
Node* newexprstmt(Node *e){ Node *n=calloc(1,sizeof(Node)); n->type=N_EXPR_STMT; n->left=e; return n; }
Node* newarrlit(){ Node *n=calloc(1,sizeof(Node)); n->type=N_ARR_LITERAL; return n; }
Node* newarraccess(char *name, Node *idx){ Node *n=calloc(1,sizeof(Node)); n->type=N_ARR_ACCESS; n->s=name; n->left=idx; return n; }

/* Forward declarations */
Node* parse_block();
Node* parse_stmt();
Node* parse_expr();
Node* parse_term();
Node* parse_factor();

/* parse block */
Node* parse_block() {
    Node *head=NULL,*tail=NULL;
    while(curtok.type!=T_EOF){
        Node *s=parse_stmt();
        if(!head) head=tail=s; else { tail->next=s; tail=s; }
    }
    return head;
}

/* parse statement */
Node* parse_stmt() {
    if(curtok.type==T_IDENT && strcmp(curtok.s,"print")==0){
        next_token(); expect(T_LPAREN);
        Node *expr=parse_expr();
        expect(T_RPAREN); expect(T_SEMI);
        return newprint(expr);
    }
    if(curtok.type==T_IDENT){
        char *name=curtok.s; next_token();
        if(accept(T_EQ)){
            Node *expr=parse_expr(); expect(T_SEMI);
            return newassign(name,expr);
        }
        error("Sintaxe inválida após identificador");
    }
    Node *e=parse_expr(); expect(T_SEMI);
    return newexprstmt(e);
}

/* parse expressions */
Node* parse_expr(){
    Node *n=parse_term();
    while(curtok.type==T_PLUS||curtok.type==T_MINUS){
        char op=(curtok.type==T_PLUS?'+':'-'); next_token();
        Node *r=parse_term();
        n=newbin(op,n,r);
    }
    return n;
}

Node* parse_term(){
    Node *n=parse_factor();
    while(curtok.type==T_MUL||curtok.type==T_DIV){
        char op=(curtok.type==T_MUL?'*':'/'); next_token();
        Node *r=parse_factor();
        n=newbin(op,n,r);
    }
    return n;
}

Node* parse_factor(){
    if(curtok.type==T_NUM){ double v=curtok.num; next_token(); return newnum(v); }
    if(curtok.type==T_STR){ char *s=curtok.s; next_token(); return newstr(s); }
    if(curtok.type==T_IDENT){
        char *name=curtok.s; next_token();
        if(accept(T_LBRACK)){ Node *idx=parse_expr(); expect(T_RBRACK); return newarraccess(name,idx); }
        return newvar(name);
    }
    if(accept(T_LPAREN)){ Node *n=parse_expr(); expect(T_RPAREN); return n; }
    if(accept(T_LBRACK)){
        Node *arr=newarrlit();
        if(curtok.type!=T_RBRACK){
            while(1){ Node *el=parse_expr(); if(!arr->left) arr->left=el; else { Node *it=arr->left; while(it->next) it=it->next; it->next=el; } if(accept(T_COMMA)) continue; else break; }
        }
        expect(T_RBRACK);
        return arr;
    }
    error("Token inesperado no fator");
    return NULL;
}

/* --- Runtime --- */
typedef enum { VT_NUM, VT_STR, VT_ARR } VType;
typedef struct Value { VType t; double num; char *s; struct Value **items; size_t len; } Value;

Value* vnum(double x){ Value *v=calloc(1,sizeof(Value)); v->t=VT_NUM; v->num=x; return v; }
Value* vstr(char *s){ Value *v=calloc(1,sizeof(Value)); v->t=VT_STR; v->s=strdup(s); return v; }
Value* varr(size_t n){ Value *v=calloc(1,sizeof(Value)); v->t=VT_ARR; v->items=calloc(n,sizeof(Value*)); v->len=n; return v; }

typedef struct { char *name; Value *val; } Var;
Var vars[2048]; int varcount=0;
void set_var_val(char *name, Value *val){ for(int i=0;i<varcount;i++) if(strcmp(vars[i].name,name)==0){ vars[i].val=val; return; } vars[varcount].name=strdup(name); vars[varcount].val=val; varcount++; }
Value* get_var_val(char *name){ for(int i=0;i<varcount;i++) if(strcmp(vars[i].name,name)==0) return vars[i].val; return NULL; }

/* Evaluate */
Value* eval_node(Node *n);

Value* eval_array_literal(Node *arr){
    size_t count=0; Node *it=arr->left; while(it){ count++; it=it->next; }
    Value *v=varr(count); it=arr->left;
    for(size_t i=0;i<count;i++){ v->items[i]=eval_node(it); it=it->next; }
    return v;
}

Value* eval_node(Node *n){
    if(!n) return vnum(0);
    switch(n->type){
        case N_NUM: return vnum(n->num);
        case N_STR: return vstr(n->s);
        case N_VAR: { Value *vv=get_var_val(n->s); if(!vv){ fprintf(stderr,"Variavel indefinida: %s\n",n->s); exit(1); } return vv; }
        case N_BIN: {
            Value *L=eval_node(n->left); Value *R=eval_node(n->right);
            double a=(L->t==VT_NUM?L->num:0), b=(R->t==VT_NUM?R->num:0);
            switch(n->op){ case '+': return vnum(a+b); case '-': return vnum(a-b); case '*': return vnum(a*b); case '/': return vnum(a/b); default: error("Operador desconhecido"); return NULL; }
        }
        case N_ASSIGN: { Value *val=eval_node(n->left); set_var_val(n->s,val); return val; }
        case N_ARR_LITERAL: return eval_array_literal(n);
        case N_ARR_ACCESS: { Value *arrv=get_var_val(n->s); if(!arrv||arrv->t!=VT_ARR){ fprintf(stderr,"Nao e array: %s\n",n->s); exit(1); } Value *idxv=eval_node(n->left); int idx=(int)idxv->num; if(idx<0||(size_t)idx>=arrv->len){ fprintf(stderr,"Indice fora do limite: %d\n",idx); exit(1); } return arrv->items[idx]; }
        case N_PRINT: {
            Value *v=eval_node(n->left);
            if(v->t==VT_NUM) printf("%g\n",v->num);
            else if(v->t==VT_STR) printf("%s\n",v->s);
            else if(v->t==VT_ARR){ printf("["); for(size_t i=0;i<v->len;i++){ if(v->items[i]->t==VT_NUM) printf("%g",v->items[i]->num); else if(v->items[i]->t==VT_STR) printf("\"%s\"",v->items[i]->s); if(i+1<v->len) printf(", "); } printf("]\n"); }
            return v;
        }
        case N_EXPR_STMT: return eval_node(n->left);
        default: error("No eval for node type"); return vnum(0);
    }
}

/* --- Main --- */
int main(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"Uso: %s file.adl\n",argv[0]); return 1; }
    FILE *f=fopen(argv[1],"rb"); if(!f){ perror("open"); return 1; }
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    src=malloc(sz+1);
    if(fread(src,1,sz,f)!=(size_t)sz){ fprintf(stderr,"Erro ao ler arquivo\n"); fclose(f); free(src); return 1; }
    src[sz]=0; fclose(f);
    pos=0; next_token();
    Node *program=parse_block();
    Node *it=program; while(it){ eval_node(it); it=it->next; }
    return 0;
}
