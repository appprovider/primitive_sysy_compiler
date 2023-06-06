#pragma once
bool checkUsable(SymDetail* sym, IRstmt* stmt);
void dealIRstmt();
Reg* getNewReg(IRstmt* stmt);
void transFuncPara();
void transFuncRet(int para_n);
void clearAll();
void saveTemp(bool nextBlock);
bool tempToBeUsed(SymDetail* x);