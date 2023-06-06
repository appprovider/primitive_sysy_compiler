#pragma once
#include<iostream>
#include<fstream>
#include<string>
#include<unordered_map>
#include<vector>
#include<set>
#include"all.h"
using namespace std;

static node* getDecl();
static node* getConstDecl();
static node* getConstDef();
static node* getConstInitVal();
static node* getVarDecl();
static node* getVarDef();
static node* getInitVal();
static node* getFuncDef();
static node* getMainFuncDef();
static node* getFuncFParams();
static node* getFuncFParam();
static node* getBlock();
static node* getStmt();
static node* getExp();
static node* getCond();
static node* getLVal();
static node* getPrimaryExp();
static node* getNumber();
static node* getUnaryExp();
static node* getFuncRParams();
static node* getXExp(string X);
static node* getConstExp();
void calVal(node* ptr1, node* ptr2, node* set_node, string cal);
void calArrVal(node* ptr1, node* ptr2, node* set_node, string cal);