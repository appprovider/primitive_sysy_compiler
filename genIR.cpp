#include<stack>
#include"all.h"
#include"genIR.h"
node* nodeRoot;
extern void getCompUnit(node*& get_node);//vs载入all.h懒惰，手动载入此函数防止频繁语法报错
vector<IRstmt> IR;
vector<IRstmt> DataStr;

//根据ast（包含id对应的符号表项）生成IR
//IR四元式中包含符号表指针。全局量声明在IR的开头。


//number数值也要加入语法树,string值也要（如运算符+-）
//设计一个自增的tempName tn

//计算逻辑表达式,短路求值。为便于打上label，该处语法树改为右递归：LAndExp → EqExp | EqExp '&&' LAndExp
SymDetail* calLogicExp(node* LogicExp,SymDetail*res) {
	if (LogicExp->boolVal)return new SymDetail(nullptr, LogicExp->boolVal);//有值立即返回，避免多一次计算
	//二种情况，双目、继承,树根无法求值当且仅当：继承子树无法求值、双目至少一个无法求值
	//逻辑与可以先STL设寄存器为0、1，然后按位与或非来实现
	if (LogicExp->children.size() == 3) {
		string cal = *LogicExp->strVal;//运算符之前存在计算结果节点
		SymDetail* a1, * a2;
		if (LogicExp->Sname == "LAndExp")//a1可能是EqExp（为LAndExp的子树）或LAndExp，
			a1 = calExp(LogicExp->children[0]);
		else a1 = calLogicExp(LogicExp->children[0], res);
		
		if ((a1->constBool && *a1->constBool || a1->constInt && *a1->constInt != 0) && cal == "||") {//有常值直接返回短路值
			res->constBool = new bool(true);
			return res;
		}
		if ((a1->constBool && !*a1->constBool || a1->constInt && *a1->constInt == 0) && cal == "&&") {
			res->constBool = new bool(false);
			return res;
		}
		string branch = cal == "||" ? "bnez" : "beqz";//或：不为0视为短路。 且：为0视为短路
		IR.emplace_back("=", a1, nullptr, res);
		IR.emplace_back(branch, res, nullptr, nullptr);
		int short_jump = IR.size() - 1;
		a2 = calLogicExp(LogicExp->children[2], res);//a2必是同类树节点。生成右递归树对应的中间代码
		IR.emplace_back(cal, a1, a2, res);
		IR[short_jump].to_label = addLabel("short");//回填在最外层
		return res;
	}
	else if (LogicExp->children.size() == 1 && LogicExp->Sname == "LOrExp")
		res = calLogicExp(LogicExp->children[0], res);//继承直接返回，不必产生多余中间变量
	else if (LogicExp->children.size() == 1 && LogicExp->Sname == "LAndExp") 
		res = calExp(LogicExp->children[0]);
	return res;
}

//计算ConstExp或Exp，以及其它XExp
SymDetail* calExp(node* constExp) {
	node* XExp = constExp;
	if (constExp->Sname == "ConstExp"||constExp->Sname=="Exp")
		XExp = constExp->children[0];
	if (XExp->numVal)return new SymDetail(XExp->numVal, nullptr);//有值立即返回，避免多一次计算
	if (XExp->boolVal)return new SymDetail(nullptr, XExp->boolVal);
	//三种情况，单目，双目，继承,树根无法求值当且仅当：单目or继承子树无法求值、双目至少一个无法求值
	//逻辑与可以先STL设寄存器为0、1，然后按位与或非来实现
	SymDetail* res=nullptr;
	if (XExp->children.size() == 3 && XExp->children[0]->Sname!="IDENFR") {
		string cal = *XExp->strVal;//运算符存在计算结果节点
		SymDetail *a1=calExp(XExp->children[0]), *a2;
		a2 = calExp(XExp->children[2]);
		if (cal == ">" || cal == "<" || cal == ">=" || cal == "<="|| cal == "==" || cal == "!=")//bool->bool、int->bool
			res = new SymDetail(true, "bool");
		else if (cal == "*" || cal == "/" || cal == "+" || cal == "-" || cal == "%")//int->int
			res = new SymDetail(true, "int");
		IR.emplace_back(cal, a1, a2, res);
	}
	else if (XExp->children[0]->Sname == "IDENFR") {//函数
		res = dealFuncCall(XExp);
	}
	else if (XExp->children.size() == 2) {
		string cal = *XExp->strVal;
		SymDetail* a1;
		a1 = calExp(XExp->children[1]);
		if (cal == "!") {//!只能在条件表达式，且相邻不相同，所以没有双重否定
			res = new SymDetail(true, "bool");
			IR.emplace_back(cal, a1, nullptr, res);
		}
		else if (cal == "-") {
			bool b1 = XExp->children[1]->children[0]->Sname == "UnaryOp";//-(+)-i,相邻两个 UnaryOp 不能相同，所以必须向前看2位
			b1 = b1 ? XExp->children[1]->children[1]->children[0]->Sname == "UnaryOp" : false;//-+(-)i
			b1 = b1 ? *XExp->children[1]->children[1]->strVal == "-" : false;//-+(-i)注意符号存在结果节点，也就是括号内的
			if (b1)//太长了拆开写,注意有短路求值
				res = calExp(XExp->children[1]->children[1]->children[1]);//[-] [ [+] [ [-] [(i)] ] ]
			else
			{
				res = new SymDetail(true, "int");
				IR.emplace_back(cal, a1, nullptr, res);
			}
		}
		else
			res = a1;
	}
	else if(XExp->children.size() == 1&&XExp->children[0]->Sname=="PrimaryExp"){
		res = calPrimaryExp(XExp->children[0]);
	}
	else
	{
		res= calExp(XExp->children[0]);//继承直接返回，不必产生多余中间变量
	}
	return res;
}



SymDetail* calPrimaryExp(node* primaryExp) {
	SymDetail* res=nullptr;
	if (primaryExp->children.size() == 3) {
		res = calExp(primaryExp->children[1]);
	}
	else if (primaryExp->children[0]->Sname == "LVal") {//左值作为表达式和被赋值的变量要区分，尤其是[]=和=[]
		node* LVal = primaryExp->children[0];
		SymDetail *detail = LVal->children[0]->detailPtr;
		string type = detail->type;

		if (type == "int"|| (type == "array1"||type == "array2") && LVal->children.size() == 1)//id,作为函数参数的数组地址
			res = detail;//对于数组地址，目标代码会计算fp_offset+$fp然后作为地址参数压栈
		else if (type == "array1") {
			SymDetail* i=calExp(LVal->children[2]); //id[i]
			res = new SymDetail(true, "int");
			if (!detail->constArr.empty() && i->constInt)//下标确定的常量数组(有非空的常值数组constArr)，可直接使用constArr的常值
				res->constInt = new int(detail->constArr[*i->constInt]);
			else
				IR.emplace_back("=[]", detail, i, res);
		}
		else if (type == "array2") {
			if (LVal->children.size() == 4)//生成目标代码时通过$arr_offset + detail->fp_offset + $fp 来计算绝对地址
			{				//或者$arr_offset + lw(detail->fp_offset + $fp)，如果这就是个参数数组的话（array、para）
				res = new SymDetail(*detail);//复制type、isGlobal等信息
				res->baseArray = detail;//便于在目标码阶段获取fp_offset（中间代码阶段该偏移尚不明确）
				SymDetail* i = calExp(LVal->children[2]); //id[i]   
				SymDetail* arr_offset;
				if (i->constInt)arr_offset = new SymDetail(new int(*i->constInt * (*res->dim2)), nullptr);//偏移值是常量
				else {
					arr_offset = new SymDetail(true, "int");
					IR.emplace_back("*", i, new SymDetail(new int(*res->dim2),nullptr), arr_offset);//arr_offset=i*dim2
				}
				res->arr_offset = arr_offset;
			}
			else {//直接取出了int 如a[i][j]
				res = new SymDetail(true,"int");
				SymDetail* t2 = calOffset(LVal);
				if (!detail->constArr.empty() && t2->constInt) //下标确定的常量数组
					res->constInt = new int(detail->constArr[*t2->constInt]);
				else
					IR.emplace_back("=[]", detail, t2, res);//res=detail[t2] 
			}

		}
	}
	else if (primaryExp->children[0]->Sname == "Number") {
		res = new SymDetail(primaryExp->numVal, nullptr); //在四元式中直接构造常数
	}
	return res;
}

void dealArrayVal(SymDetail* detail,node* InitVal) {
	static int cnt = 0;
	if (InitVal->children.size() == 1)//(Exp|ConstExp)无嵌套
	{
		SymDetail* val;
		int* numVal = InitVal->children[0]->numVal;
		if (numVal)
			val = new SymDetail(numVal, nullptr);//(Const)InitVal->(Const)Exp全局为常量同上int
		else
			val = calExp(InitVal->children[0]);//局部可能为变量。全局初始值也可能是常量数组的元素，通过常量数组的constArr获取
		SymDetail* offset = new SymDetail(new int(cnt), nullptr);
		IR.emplace_back("[]=", offset, val, detail);//detail[offset]=val
		cnt++;
	}
	else 
		for (int k = 0; k < InitVal->children.size(); k++) {
			if (InitVal->children[k]->Sname != "ConstInitVal" && InitVal->children[k]->Sname != "InitVal")continue;
			node* sonVal = InitVal->children[k];
			dealArrayVal(detail,sonVal);
		}
	if (cnt >= (detail->dim2 ? *detail->dim1 * (*detail->dim2) : *detail->dim1))cnt = 0;//如果读完值，cnt归零以便下次用
}

//生成语法树的时候，能求出常值的Exp就把值放在node。不能（函数调用或变量）的就生成IR和临时值
void dealXDecl(node* xDecl,bool isGlobal) {//兼容全局和局部 ConstDecl | VarDecl
	string type;
	for (int j = 0; j < xDecl->children.size(); j++)//ConstDecl → 'const' BType ConstDef { ',' ConstDef } ';var同理
	{
		if (xDecl->children[j]->Sname != "ConstDef" && xDecl->children[j]->Sname != "VarDef")continue;
		node* def = xDecl->children[j];		//a[2][2]={{1,2},{3,4}}
		SymDetail* detail = def->children[0]->detailPtr;
		type = detail->type;
		detail->isGlobal = isGlobal;
		IR.emplace_back(def->Sname, nullptr, nullptr, detail);

		node* InitVal = def->children.back();//初值constinitval或initval
		if (InitVal->Sname == "ConstInitVal" || InitVal->Sname == "InitVal") {
			if (type == "int") {
				if (InitVal->children[0]->numVal && InitVal->Sname == "ConstInitVal")//全局初值(Exp|ConstExp)必为常值,局部Exp可能不是常值
					IR.back().arg1=new SymDetail(InitVal->children[0]->numVal, nullptr);//常量声明初值 def initval _ detail
				else
					IR.emplace_back("=", calExp(InitVal->children[0]), nullptr, detail);//给变量赋初值
			}
			else if (type == "array1" || type == "array2") dealArrayVal(detail, InitVal);
		}//如果有初始化
	}

}

//返回所有声明结束的下一条（第一个FuncDef）
int dealGlobalDecl() {
	vector<node*> ConstDecl = nodeRoot->children[0]->children;
	int i = 0;
	for (; i < nodeRoot->children.size() && nodeRoot->children[i]->Sname=="Decl"; i++) {
		node* xDecl = nodeRoot->children[i]->children[0];//Decl->ConstDecl | VarDecl
		dealXDecl(xDecl,true);
	}
	return i;
}

//包括普通函数和main
void dealFuncDef(int i) {
	for (; i < nodeRoot->children.size(); i++) {
		SymDetail* funcDetail = nodeRoot->children[i]->children[1]->detailPtr;	//FuncDef → FuncType Ident '(' [FuncFParams] ')' Block
		IR.emplace_back("FuncDef", nullptr, nullptr, funcDetail);
		SymTable* funcTable = funcDetail->childTable;
		for (int j = 0; j < funcTable->para_n; j++) {
			SymDetail* para = funcTable->tableList[j];
			IR.emplace_back("para", nullptr, nullptr, para);
		}
		dealBlock(nodeRoot->children[i]->children.back());
		if(funcDetail->type=="void"&&IR.back().op!="ret")//void函数没有的return，添上作为mips的返回标记
			IR.emplace_back("lastret", nullptr, nullptr, nullptr);
		IR.back().op = "lastret";//最后一个ret标记为lastret
	}
}

SymDetail* dealFuncCall(node* unaryExp) {
	node* FuncRParams = unaryExp->children[2];
	SymDetail* funcDetail = unaryExp->children[0]->detailPtr;
	vector<IRstmt> Rpara;
	for (int i = 0; i < funcDetail->childTable->para_n; i++)
		Rpara.emplace_back("push", nullptr, nullptr, calExp(FuncRParams->children[i*2]));//实参使用push，和形参para区分
	for (int i = 0; i < Rpara.size(); i++)
		IR.push_back(Rpara[i]);
	IR.emplace_back("call", nullptr, nullptr, funcDetail);
	
	SymDetail* ret = nullptr;
	if (funcDetail->type != "void") {
		ret= new SymDetail(true,funcDetail->type);
		ret->kind = "ret";//目标代码识别，用于接收$v0的返回值
	}
	IR.back().arg1 = ret;//便于目标代码在call语句就从arg1拿到返回值。从而紧接着生成move $ret(ret的寄存器) $v0语句
	return ret;//函数的返回值（临时值），作为右值
}

//语法树里直接跳过了BlockItem
void dealBlock(node* block) {
	for (int i=0; i < block->children.size(); i++) {
		if (block->children[i]->Sname == "Decl")
			dealXDecl(block->children[i]->children[0], false);//Decl->XDecl
		else if(block->children[i]->Sname == "Stmt")
			dealStmt(block->children[i]);
	}
}

void dealStmt(node* stmt) {
	if (stmt->children[0]->Sname == "LVal") {//赋值语句
		dealAssign(stmt);
	}else if(stmt->children[0]->Sname == "Exp"|| stmt->children[0]->Sname=="SEMICN") {//[Exp] ';'
		if (stmt->children[0]->Sname == "Exp")
			calExp(stmt->children[0]);
	}
	else if (stmt->children[0]->Sname == "Block") {
		dealBlock(stmt->children[0]);
	}
	else if (stmt->children[0]->Sname == "RETURNTK") {
		SymDetail* ret = nullptr;
		if (stmt->children.size() == 3)
			ret = calExp(stmt->children[1]);
		IR.emplace_back("ret", nullptr, nullptr, ret);
	}
	else if (stmt->children[0]->Sname == "PRINTFTK") {
		dealPrintf(stmt);
	}
	else {//if else while break continue 控制流
		dealCtrlStream(stmt);
	}
}

string creatLabel(string type) {
	static long long  cnt = 0;
	return "Label" + to_string(cnt++) +"_" + type;
}

//生成指定类型的标签，加入到新一行，返回唯一标签名
string addLabel(string type) {
	IRstmt* label = new IRstmt("Label:", nullptr, nullptr, nullptr);
	label->label = creatLabel(type);
	IR.push_back(*label);
	return label->label;
}



stack<int> break_to;//存需要回填的break语句的位置，一层循环隔一个-1
stack<string> currentBegin;

//if else while break continue 控制流
void dealCtrlStream(node* stmt) {
	if (stmt->children[0]->Sname == "IFTK") {
		node* cond = stmt->children[2];
		SymDetail* val_base = new SymDetail(true, "bool");//设置为temp，以便被目标码阶段saveTemp保存
		SymDetail* cond_val = calLogicExp(cond->children[0],val_base);
		IR.emplace_back("beqz", cond_val,nullptr, nullptr);
		int ifFalse = IR.size()-1;//不采用push_back（拷贝）再取地址的方式。因为vector元素地址会移动无法保证有效

		dealStmt(stmt->children[4]);
		
		if (stmt->children.size()>5 && stmt->children[5]->Sname == "ELSETK") {
			IR.emplace_back("goto", nullptr, nullptr, nullptr);
			int jump = IR.size() - 1;

			IR[ifFalse].to_label = addLabel("false");//回填

			dealStmt(stmt->children[6]);
			IR[jump].to_label = addLabel("goto");//回填
		}else
			IR[ifFalse].to_label = addLabel("false");//回填
	}
	else if (stmt->children[0]->Sname == "WHILETK") {
		string beginLabel = addLabel("begin");
		currentBegin.push(beginLabel);
		break_to.push(-1);//隔一个-1

		node* cond = stmt->children[2];
		SymDetail* val_base = new SymDetail(true, "bool");//传入一个承接结果的临时值，用于明确目标码阶段的栈空间
		SymDetail* cond_val = calLogicExp(cond->children[0], val_base);
		IR.emplace_back("beqz", cond_val,nullptr, nullptr);
		int ifFalse = IR.size() - 1;

		dealStmt(stmt->children[4]);

		IR.emplace_back("goto", nullptr, nullptr, nullptr);
		int jump = IR.size() - 1;
		IR[jump].to_label = beginLabel;

		string endLabel= addLabel("end");
		IR[ifFalse].to_label = endLabel;//回填
		currentBegin.pop();
		int goto_end=break_to.top();
		break_to.pop();
		while (goto_end!=-1) {	//对该层次所有break回填
			IR[goto_end].to_label = endLabel;
			goto_end=break_to.top();
			break_to.pop();
		}
		
	}
	else if(stmt->children[0]->Sname=="CONTINUETK") {
		IR.emplace_back("goto", nullptr, nullptr, nullptr);
		int jump = IR.size() - 1;
		IR[jump].to_label = currentBegin.top();
	}
	else if (stmt->children[0]->Sname == "BREAKTK") {
		IR.emplace_back("goto", nullptr, nullptr, nullptr);
		int jump = IR.size() - 1;
		break_to.push(jump);
	}
}

//计算二维数组偏移量
SymDetail* calOffset(node* LVal) {
	SymDetail* detail = LVal->children[0]->detailPtr;
	SymDetail* dim2_sym = new SymDetail(detail->dim2, nullptr);
	SymDetail* i = calExp(LVal->children[2]); //detail[i][j]
	SymDetail* t1 = new SymDetail(true, "int");
	IR.emplace_back("*", dim2_sym, i, t1);//dim2*i=t1
	SymDetail* j = calExp(LVal->children[5]);
	if (i->constInt && j->constInt) {
		IR.pop_back();//如果i、j是常数，可省略IR；并返回下标常值
		int* n = new int(*i->constInt * (*dim2_sym->constInt) + (*j->constInt));
		return new SymDetail(n,nullptr);
	}
	SymDetail* t2 = new SymDetail(true, "int");
	IR.emplace_back("+", t1, j, t2);//t1+j=t2;
	return t2;
}

void dealAssign(node* stmt) {
	SymDetail* RVal;
	if (stmt->children[2]->Sname == "Exp")
		RVal = calExp(stmt->children[2]);
	else if (stmt->children[2]->Sname == "GETINTTK")
	{
		RVal = new SymDetail(true, "int");
		IR.emplace_back("getint", nullptr, nullptr, RVal);
	}
		

	node* LVal = stmt->children[0];
	SymDetail* detail = LVal->children[0]->detailPtr;
	if (detail->type == "int") {
		IR.emplace_back("=", RVal, nullptr, detail);
	}
	else if (detail->type == "array1") {
		SymDetail* i = calExp(LVal->children[2]); //detail[i]
		IR.emplace_back("[]=", i,RVal ,detail);
	}
	else if (detail->type == "array2") {
		SymDetail* t2=calOffset(LVal);
		IR.emplace_back("[]=", t2, RVal, detail);//detail[t2] = res
	}
}

void dealPrintf(node* stmt) {
	static int strCnt=0;
	string format = *stmt->children[2]->strVal;
	int i = 4;
	size_t pos1=0, pos2=0;
	vector<IRstmt> printArg;
	for (node* Exp = stmt->children[i]; Exp->Sname == "Exp";i+=2,Exp= stmt->children[i]) {
		pos2= format.find_first_of("%",pos1);
		if (pos1 != pos2)//两个%d紧邻，或%d在开头导致子串是空串时，就不用输出子串
		{
			string sub = format.substr(pos1, pos2 - pos1);
			string strName = "str_" + to_string(strCnt++);
			DataStr.emplace_back(strName, nullptr, nullptr, new SymDetail(sub));
			printArg.emplace_back("printf", nullptr, nullptr, new SymDetail(strName));//strName都显式打印到IR，作为标识符，据此找到相应的format
		}
		printArg.emplace_back("printf", nullptr, nullptr, calExp(Exp));
		pos1 = pos2 + 2;//跳过一个d 如1%d2
	}
	if (pos1 < format.size()) {//没有标识符或处理最后一个子串
		string sub = format.substr(pos1, format.size() - pos1);
		string strName = "str_" + to_string(strCnt++);
		DataStr.emplace_back(strName, nullptr, nullptr, new SymDetail(sub));
		printArg.emplace_back("printf", nullptr, nullptr, new SymDetail(strName));
	}
	for (int i = 0; i < printArg.size(); i++)
		IR.push_back(printArg[i]);//先计算表达式（可能有函数造成输出），再输出
}

void printTree(ofstream& dstFile,node *cursor) {
	static int depth = 0;
	for (int i = 0; i < depth; i++)
		dstFile << " ";
	dstFile << depth<<cursor->Sname <<":";
	if(cursor->boolVal)
		dstFile  <<"boolVal=" << *cursor->boolVal ;
	if (cursor->numVal)
		dstFile << "numVal=" << *cursor->numVal;
	if (cursor->strVal)
		dstFile << "strVal=" << *cursor->strVal;
	if(cursor->Sname=="IDENFR")
		dstFile << "name=" << cursor->detailPtr->name;
	dstFile << std::endl;
	
	for (int i = 0; i < cursor->children.size(); i++) {
		depth++;
		printTree(dstFile,cursor->children[i]);
		depth--;
	}
}

/*
void printIR(ofstream& IRFile) {
	for (int i = 0; i < IR.size(); i++) {
		if (IR[i].label != "")
			IRFile << IR[i].label + ": " << std::endl;
		else if (IR[i].to_label != "") {
			IRFile << IR[i].op + ", ";
			if (IR[i].op == "beqz"|| IR[i].op == "bnez")IRFile << IR[i].arg1->name + ",";
			IRFile<< IR[i].to_label << std::endl;
		}
			
		else {
			IRFile << IR[i].op + ", ";
			if (IR[i].arg1) {
				IRFile << IR[i].arg1->name + ", ";
				if (IR[i].arg1->constString != "")IRFile << IR[i].arg1->constString;
				if (IR[i].arg1->constInt)IRFile << to_string(*IR[i].arg1->constInt) + ", ";
				if (IR[i].arg1->constBool)IRFile << to_string(*IR[i].arg1->constBool) + ", ";
			}
			if (IR[i].arg2) {
				IRFile << IR[i].arg2->name + ", ";
				if (IR[i].arg2->constInt)IRFile << to_string(*IR[i].arg2->constInt) + ", ";
				if (IR[i].arg2->constBool)IRFile << to_string(*IR[i].arg2->constBool) + ", ";
			}
			if (IR[i].res)	IRFile << IR[i].res->name + ", ";
			IRFile << std::endl;
		}
	}
}
*/