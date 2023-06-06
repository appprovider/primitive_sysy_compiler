#pragma once
#include"all.h"
SymDetail* calExp(node* constExp);
void dealAssign(node* stmt);
void dealBlock(node* block);
void dealCtrlStream(node* stmt);
void dealXDecl(node* xDecl, bool isGlobal);
SymDetail* dealFuncCall(node* unaryExp);
void dealFuncDef(int i);
int dealGlobalDecl();
void dealPrintf(node* stmt);
void dealStmt(node* stmt);
SymDetail* calOffset(node* LVal);
SymDetail* calPrimaryExp(node* primaryExp);
void calVal(node* ptr1, node* ptr2, node* set_node, string cal);
string addLabel(string type);