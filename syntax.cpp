#include<algorithm>
#include"all.h"
#include"syntax.h"
using namespace std;


vector<pair<string, string> >& inTXT = lexOut;//给lexical输出改个名，不复制
vector<pair<string, string> >outTXT;
vector<pair<int, string> >errTXT;

int inTOP=0;
SymTable* symRoot=new SymTable();
SymTable* currentTable = symRoot;

int loopCnt=0;

//约定每个子程序开始时，top位于产生式右第一项
//约定每个子程序处理完之后top来到下一个开始处

//检查是否都输出了，以及是否加入node

//终结符检查（待实现）、输出、加入父节点children.
//后期考虑每个非终结符函数改成一个类
inline void Tmove(node* set_node, string typeCode) {
	set_node->children.push_back(new node(typeCode));
	if (inTXT[inTOP].first == typeCode) {
		outTXT.emplace_back(typeCode, inTXT[inTOP].second);
		inTOP++;
	}
	//*** i j k缺少右边符号(行号为前一个非终结符)
	else if(typeCode=="SEMICN")
		errTXT.emplace_back(LineCnt[inTOP-1], "i");
	else if(typeCode == "RPARENT")
		errTXT.emplace_back(LineCnt[inTOP-1], "j");
	else if (typeCode == "RBRACK")
		errTXT.emplace_back(LineCnt[inTOP-1], "k");

}

void getCompUnit(node*&get_node) {
	get_node = new node("CompUnit");
	node* ptr = getDecl();
	while (ptr != nullptr) {
		get_node->children.push_back(ptr);
		ptr = getDecl();
	}
	ptr = getFuncDef();
	while (ptr != nullptr) {
		get_node->children.push_back(ptr);
		ptr = getFuncDef();
	}
	ptr = getMainFuncDef();
	get_node->children.push_back(ptr);

	outTXT.emplace_back("<CompUnit>", "");
}

//通过返回值判断是否存在{Decl} {FuncDef}
node* getDecl() {
	node* set_node = new node("Decl");
	node* ptr=nullptr;
	//确定是ConstDecl
	if (inTXT[inTOP].second == "const") {
		ptr = getConstDecl();
		set_node->children.push_back(ptr);
	}
	//排除 MainFuncDef 和 FuncDef, 确定是VarDecl
	else if (inTXT[inTOP].second == "int"&& inTXT[inTOP+1].second!="main"&& inTXT[inTOP+2].second!="(") {
		ptr = getVarDecl();
		set_node->children.push_back(ptr);
	}

	if (ptr == nullptr) {
		delete(set_node);
		return nullptr;
	}

	outTXT.emplace_back("<Decl>", "");
	return set_node;
}

node* getConstDecl() {
	node* set_node = new node("ConstDecl");
	node* ptr;
	Tmove(set_node, "CONSTTK");
	Tmove(set_node, "INTTK");//BType直接当成int
	ptr = getConstDef();
	set_node->children.push_back(ptr);
	while (inTXT[inTOP].second == ",") {
		Tmove(set_node, "COMMA");//还有个逗号
		ptr = getConstDef();
		set_node->children.push_back(ptr);	
	}
	Tmove(set_node, "SEMICN");

	outTXT.emplace_back("<ConstDecl>", "");
	return set_node;
}

//***b 当前定义域名字重定义
void redefined(string name) {
	for (int i = currentTable->tableList.size() - 1; i >= 0; i--) {
		if (currentTable->tableList[i]->name == name) {
			errTXT.emplace_back(LineCnt[inTOP], "b");
			break;
		}
	}
}

node* getConstDef() {
	node* set_node = new node("ConstDef");
	node* ptr;
	
	redefined(inTXT[inTOP].second);
	string name = inTXT[inTOP].second;


	Tmove(set_node, "IDENFR");
	node* symTag = set_node->children.back();
	int dim = 0;
	vector<int*> dimSize;
	while (inTXT[inTOP].second == "[") {
		Tmove(set_node, "LBRACK");
		ptr = getConstExp();
		dimSize.push_back(ptr->numVal);//保存每个维度的长度
		set_node->children.push_back(ptr);
		Tmove(set_node, "RBRACK");
		dim++;
	}
	string type = dim == 0 ? "int" : "array" + to_string(dim);
	int* dim1 = dimSize.size() > 0 ? dimSize[0] : nullptr;
	int* dim2 = dimSize.size() > 1 ? dimSize[1] : nullptr;
	currentTable->tableList.push_back(new SymDetail("const",type,name,dim1,dim2));
	symTag->detailPtr = currentTable->tableList.back();//AST symTag,这里的symTag只是符号表项对应的根节点

	Tmove(set_node, "ASSIGN");
	ptr = getConstInitVal();
	set_node->children.push_back(ptr);
	
	if (symTag->detailPtr->type == "int")
		symTag->detailPtr->constInt = ptr->numVal;//给根节点的常量赋值（ptr->numVal）
	if (symTag->detailPtr->type == "array1" || symTag->detailPtr->type == "array2")
		calArrVal(ptr, nullptr, set_node, "[]=");//给常量数组的constArr赋值

	outTXT.emplace_back("<ConstDef>", "");
	return set_node;
}

//有递归
node* getConstInitVal() {
	node* set_node = new node("ConstInitVal");
	node* ptr;
	if (inTXT[inTOP].second == "{") {
		Tmove(set_node, "LBRACE");
		if (inTXT[inTOP].second != "}") {
			ptr = getConstInitVal();
			set_node->children.push_back(ptr);
			while (inTXT[inTOP].second == ",") {
				Tmove(set_node, "COMMA");
				ptr = getConstInitVal();
				set_node->children.push_back(ptr);
			}
		}
		Tmove(set_node, "RBRACE");
	}
	else {
		ptr = getConstExp();
		set_node->children.push_back(ptr);
		calVal(ptr, nullptr, set_node, "");//全局int常量必须在语法树生成时就赋值。否则常量声明表达式里若有已声明的常量，将无法计算初值
	}

	outTXT.emplace_back("<ConstInitVal>", "");
	return set_node;
}

node* getVarDecl() {
	node* set_node = new node("VarDecl");
	node* ptr;
	Tmove(set_node, "INTTK");//BType直接当成int
	ptr = getVarDef();
	set_node->children.push_back(ptr);
	while (inTXT[inTOP].second == ","){
		Tmove(set_node, "COMMA");
		ptr = getVarDef();
		set_node->children.push_back(ptr);
	}
	Tmove(set_node, "SEMICN");

	outTXT.emplace_back("<VarDecl>", "");
	return set_node;
}

node* getVarDef() {
	node* set_node = new node("VarDef");
	node* ptr;

	redefined(inTXT[inTOP].second);
	string name = inTXT[inTOP].second;

	Tmove(set_node, "IDENFR");
	node* symTag = set_node->children.back();
	int dim = 0;
	vector<int*> dimSize;
	while (inTXT[inTOP].second == "[") {
		Tmove(set_node, "LBRACK");
		ptr = getExp();
		dimSize.push_back(ptr->numVal);//保存每个维度的长度
		set_node->children.push_back(ptr);
		Tmove(set_node, "RBRACK");
		dim++;
	}
	string type = dim == 0 ? "int" : "array" + to_string(dim);
	int* dim1 = dimSize.size() > 0 ? dimSize[0] : nullptr;
	int* dim2 = dimSize.size() > 1 ? dimSize[1] : nullptr;
	currentTable->tableList.push_back(new SymDetail("var", type, name, dim1, dim2));
	symTag->detailPtr = currentTable->tableList.back();//AST symTag

	if (inTXT[inTOP].second == "=") {
		Tmove(set_node, "ASSIGN");
		ptr = getInitVal();
		set_node->children.push_back(ptr);
	}

	outTXT.emplace_back("<VarDef>", "");
	return set_node;
}

node* getInitVal() {
	node* set_node = new node("InitVal");
	node* ptr;
	if (inTXT[inTOP].second == "{") {
		Tmove(set_node, "LBRACE");
		if (inTXT[inTOP].second != "}") {
			ptr = getInitVal();
			set_node->children.push_back(ptr);
			while (inTXT[inTOP].second == ",") {
				Tmove(set_node, "COMMA");
				ptr = getInitVal();
				set_node->children.push_back(ptr);
			}
		}
		Tmove(set_node, "RBRACE");
	}
	else
	{
		ptr = getExp();
		set_node->children.push_back(ptr);
	}

	outTXT.emplace_back("<InitVal>", "");
	return set_node;
}

node* getFuncDef() {
	node* set_node = new node("FuncDef");
	node* ptr = new node("FuncType");

	if (inTXT[inTOP + 1].second == "main") {
		delete set_node, ptr;
		return nullptr;
	}

	redefined(inTXT[inTOP+1].second);

	SymTable* oldCurrent=currentTable;

	currentTable->tableList.push_back(new SymDetail("func", inTXT[inTOP].second, inTXT[inTOP+1].second));//要先存入否则parentAt寻址不对
	SymTable* funcTable=new SymTable(currentTable);
	funcTable->isFuncDef=funcTable->outerFunc = true;
	currentTable->tableList.back()->childTable = funcTable;//指向子表，便于找到函数形参
	currentTable = funcTable;


	Tmove(ptr, inTXT[inTOP].first);//void或int
	outTXT.emplace_back("<FuncType>", "");
	set_node->children.push_back(ptr);//不再单独写FuncType函数
	Tmove(set_node, "IDENFR");
	set_node->children.back()->detailPtr = currentTable->parent->tableList.back();//AST symTag
	Tmove(set_node, "LPARENT");
	if (inTXT[inTOP].second != ")"&& inTXT[inTOP].second != "{") {//***j 考虑出现右小括号缺失
		ptr = getFuncFParams();
		set_node->children.push_back(ptr);
	}
	Tmove(set_node, "RPARENT");

	ptr = getBlock();
	set_node->children.push_back(ptr);

	currentTable = oldCurrent;//返回当前表

	outTXT.emplace_back("<FuncDef>", "");
	return set_node;
}

node* getMainFuncDef() {
	node* set_node = new node("MainFuncDef");
	node* ptr;

	currentTable->tableList.push_back(new SymDetail("func", "int", "main"));
	SymTable* funcTable = new SymTable(currentTable);
	funcTable->isFuncDef = funcTable->outerFunc = true;//***e 标记一下，便于错误e的判定
	currentTable->tableList.back()->childTable = funcTable;//指向子表，便于找到函数形参
	currentTable = funcTable;//程序的最后一部分，不用返回

	Tmove(set_node, "INTTK");
	Tmove(set_node, "MAINTK");
	set_node->children.back()->detailPtr = currentTable->parent->tableList.back();//AST symTag
	Tmove(set_node, "LPARENT");
	Tmove(set_node, "RPARENT");
	ptr = getBlock();
	set_node->children.push_back(ptr);
	
	outTXT.emplace_back("<MainFuncDef>", "");
	return set_node;
};

//形参
node* getFuncFParams() {
	node* set_node = new node("FuncFParams");
	node* ptr;
	ptr = getFuncFParam();
	set_node->children.push_back(ptr);

	currentTable->para_n = 1;
	while (inTXT[inTOP].second == ",") {
		Tmove(set_node, "COMMA");
		ptr = getFuncFParam();
		set_node->children.push_back(ptr);
		currentTable->para_n++;
	}

	outTXT.emplace_back("<FuncFParams>", "");
	return set_node;
}

node* getFuncFParam() {
	node* set_node = new node("FuncFParam");
	node* ptr;

	redefined(inTXT[inTOP + 1].second);
	string name = inTXT[inTOP + 1].second;

	Tmove(set_node, "INTTK");
	Tmove(set_node, "IDENFR");
	node* symTag = set_node->children.back();
	int dim = 0,dim2Size=0;
	if (inTXT[inTOP].second == "[") {
		Tmove(set_node, "LBRACK");
		Tmove(set_node, "RBRACK");
		dim++;
		while (inTXT[inTOP].second == "[") {
			Tmove(set_node, "LBRACK");
			ptr = getConstExp();
			dim2Size = *ptr->numVal;
			set_node->children.push_back(ptr);
			Tmove(set_node, "RBRACK");
			dim++;
		}
	}
	string type = dim == 0 ? "int" : "array" + to_string(dim);
	currentTable->tableList.push_back(new SymDetail("para", type, name, new int(0), new int(dim2Size)));
	symTag->detailPtr = currentTable->tableList.back();//AST symTag
	
	outTXT.emplace_back("<FuncFParam>", "");
	return set_node;
}

node* getBlock() {
	node* set_node = new node("Block");
	node* ptr;

	SymTable* oldCurrent=currentTable;
	bool func_flag = currentTable->outerFunc;//外层已经由函数创建了符号表
	currentTable->outerFunc = false;
	if (!func_flag)//如果没有函数来创建，就创建一个
	{
		currentTable->tableList.push_back(new SymDetail());
		SymTable* blockTable = new SymTable(currentTable);
		currentTable->tableList.back()->childTable = blockTable;//指向子表，便于找到函数形参
		currentTable = blockTable;
	}
	else;//否则沿用函数的符号表
		

	Tmove(set_node, "LBRACE");
	bool has_return = false;
	while (inTXT[inTOP].second != "}") {
		if (inTXT[inTOP].second == "return")
			has_return = true;
		if (inTXT[inTOP].second == "const" || inTXT[inTOP].second == "int")
			ptr = getDecl();
		else
			ptr = getStmt();
		set_node->children.push_back(ptr);
	}
	if(currentTable->isFuncDef &&currentTable->parent->tableList[currentTable->parentAt]->type!="void" && !has_return)//***g 缺少return
		errTXT.emplace_back(LineCnt[inTOP], "g");
	Tmove(set_node, "RBRACE");

	if(!func_flag)
		currentTable = oldCurrent;//返回当前表

	outTXT.emplace_back("<Block>", "");
	return set_node;
}

//返回-1表示错误，其它表示格式字符数
int checkFormatString(string str) {
	int FormatCnt = 0;
	for (int i = 0; i < str.size(); i++) {//去除头尾的双引号
		if (str[i] >= 32 && str[i] <= 33 || str[i] >= 40 && str[i] <= 126 && str[i] != '\\'|| str[i] == '\\' && str[i + 1] == 'n');
		else if (str[i] == '%'&&str[i + 1] == 'd')
			FormatCnt++;
		else return -1;
	}
	return FormatCnt;
}

//***c 未定义的名字(不包括函数),如果有定义就返回符号表项的指针,否则返回空指针
SymDetail* nameDefined(string name) {
	int i = currentTable->tableList.size() - 1;
	SymTable* cursor = currentTable;
	while (i >= 0 || cursor->parent != nullptr) {
		while (i < 0) {
			i = cursor->parentAt;
			cursor = cursor->parent;
		}
		for (; i >= 0; i--) {
			if (cursor->tableList[i]->name == name && cursor->tableList[i]->kind != "func")
				return cursor->tableList[i];
		}
	}
	return nullptr;
}

//有递归
node* getStmt() {
	node* set_node = new node("Stmt");
	node* ptr;
	string str = inTXT[inTOP].second;
	if (str == "{") {
		ptr = getBlock();
		set_node->children.push_back(ptr);
	}
	else if (str == "if" || str == "while")
	{
		int block_pos = currentTable->tableList.size();
		if (str == "while")
			loopCnt ++;		//***m 标记多一层循环

		Tmove(set_node, inTXT[inTOP].first);
		Tmove(set_node, "LPARENT");
		ptr = getCond();
		set_node->children.push_back(ptr);
		Tmove(set_node, "RPARENT");
		ptr = getStmt();
		set_node->children.push_back(ptr);
		
		if (str == "while")
			loopCnt--;

		//前面一样，if-else还有 [ 'else' Stmt ] 从句
		if (str == "if" && inTXT[inTOP].second == "else") {
			Tmove(set_node, "ELSETK");
			ptr = getStmt();
			set_node->children.push_back(ptr);
		}
	}
	else if (str == "break" || str == "continue")
	{
		if(loopCnt<=0)//当前块不是循环块
			errTXT.emplace_back(LineCnt[inTOP], "m");//***m 不是循环块却break、continue

		Tmove(set_node, inTXT[inTOP].first);
		Tmove(set_node, "SEMICN");
	}
	else if (str == "return")
	{
		SymTable* func = currentTable;
		while (!func->isFuncDef)func = func->parent;
		if((func->parent->tableList)[func->parentAt]->type=="void"&& inTXT[inTOP+1].second!=";")//***f 无返回值的函数有多余的return
			errTXT.emplace_back(LineCnt[inTOP], "f");

		Tmove(set_node, "RETURNTK");
		string f = inTXT[inTOP].first, s = inTXT[inTOP].second;
		if (s == "+" || s == "-" || s == "(" || f == "IDENFR" || f == "INTCON") {//***i 考虑分号缺失，不能再用来判定，改用Exp的first集
			ptr = getExp();
			set_node->children.push_back(ptr);
		}
		Tmove(set_node, "SEMICN");
	}
	else if (str == "printf")
	{
		Tmove(set_node, "PRINTFTK");
		Tmove(set_node, "LPARENT");
		string str =inTXT[inTOP].second.substr(1, inTXT[inTOP].second.size()-2);
		int FormatCnt = checkFormatString(str),line= LineCnt[inTOP];
		if (FormatCnt < 0) //***a 非法符号
			errTXT.emplace_back(line, "a");
		Tmove(set_node, "STRCON");
		set_node->children.back()->strVal = new string(str);//字符串常量添加到语法树

		int ExpCnt=0;
		while (inTXT[inTOP].second == ",") {
			Tmove(set_node, "COMMA");
			ptr = getExp();
			set_node->children.push_back(ptr);
			ExpCnt++;
		}
		if(FormatCnt>=0&&ExpCnt!=FormatCnt)//***l 表达式个数与格式字符不匹配(前提是表达式无错)
			errTXT.emplace_back(line, "l");
		Tmove(set_node, "RPARENT");
		Tmove(set_node, "SEMICN");
	}
	else if (inTXT[inTOP].first == "IDENFR")//LVal
	{
		SymDetail* symbol = nameDefined(inTXT[inTOP].second);

		int oldTop = inTOP,oldSize=(int)outTXT.size();//Exp的first集与LVal相交，必须回溯
		ptr = getLVal();
		ptr->children[0]->detailPtr = symbol;//AST symTag
		
		if (inTXT[inTOP].second == "=") {
			if (symbol == nullptr)
				errTXT.emplace_back(LineCnt[inTOP], "c");//***c 未定义的名字（非函数）,确定了是LVal再输出，避免回溯时重复输出

			if(symbol!=nullptr && symbol->kind == "const")
				errTXT.emplace_back(LineCnt[inTOP], "h");//***h 不能改变常量

			set_node->children.push_back(ptr);
			Tmove(set_node, "ASSIGN");
			if (inTXT[inTOP].second == "getint") {
				Tmove(set_node, "GETINTTK");
				Tmove(set_node, "LPARENT");
				Tmove(set_node, "RPARENT");
			}
			else
			{
				ptr = getExp();
				set_node->children.push_back(ptr);
			}
			Tmove(set_node, "SEMICN");
		}
		else//回溯
		{
			inTOP = oldTop;
			outTXT.resize(oldSize);
			delete ptr;//未递归
			goto dealExp;//确定是IDENFR开头的Exp，前往处理
		}
	}
	else if (str == ";")
	{
		Tmove(set_node, "SEMICN");
	}
	else
	{
		dealExp:
		ptr = getExp();
		set_node->children.push_back(ptr);
		Tmove(set_node, "SEMICN");
	}

	outTXT.emplace_back("<Stmt>", "");
	return set_node;
}

node* getExp() {
	node* set_node = new node("Exp");
	node* ptr = getXExp("AddExp");
	set_node->children.push_back(ptr);

	calVal(ptr, nullptr, set_node, ""); //calVal 预先计算表达式
	
	outTXT.emplace_back("<Exp>", "");
	return set_node;
}

node* getCond() {
	node* set_node = new node("Cond");
	set_node->children.push_back(getXExp("LOrExp"));

	outTXT.emplace_back("<Cond>", "");
	return set_node;
}

node* getLVal() {
	node* set_node = new node("LVal");
	node* ptr;

	Tmove(set_node, "IDENFR");
	string sym_name = inTXT[inTOP - 1].second;
	
	while (inTXT[inTOP].second == "[") {
		Tmove(set_node, "LBRACK");
		ptr = getExp();
		set_node->children.push_back(ptr);
		Tmove(set_node, "RBRACK");
	}

	node* ptr1 = set_node->children.size() >= 4 ? set_node->children[2] : nullptr;
	node* ptr2 = set_node->children.size() >= 7 ? set_node->children[5] : nullptr;//LVal → Ident  [ Exp ] [ Exp ]
	set_node->children[0]->detailPtr = nameDefined(sym_name);
	if (ptr1) calArrVal(ptr1, ptr2, set_node, "=[]");//calVal 计算常量数组元素值(能求就求，calVal来判断)

	outTXT.emplace_back("<LVal>", "");
	return set_node;
}

//cal: []=是给constArr赋值，=[]是求constArr的值
void calArrVal(node* ptr1, node* ptr2, node* set_node, string cal) {
	if (cal == "=[]") {//求常量数组元素值,并传递给LVal（set_node）
		SymDetail* arr = set_node->children[0]->detailPtr;
		if (arr->constArr.empty())return;//非常量数组无法求元素值
		if (ptr1 && ptr1->numVal && !ptr2) {//下标确定的一维数组
			set_node->numVal = new int(arr->constArr[*ptr1->numVal]);
		}
		else if (ptr1 && ptr1->numVal && ptr2 && ptr2->numVal) {//下标确定的二维数组
			int offset = *ptr1->numVal * (*arr->dim2) + *ptr2->numVal;//注意[i][j]是 i * dim2 +j
			set_node->numVal = new int(arr->constArr[offset]);
		}
		return;
	}
	if (cal == "[]=") {//使用ConstInitVal(ptr1)给常量数组arr的constArr赋值
		SymDetail* arr = set_node->children[0]->detailPtr;
		if (arr->type == "array1")
			for (int i = 1; i < ptr1->children.size(); i += 2) //{init,init,init}->1,3,5
				arr->constArr.push_back(*ptr1->children[i]->children[0]->numVal);//ConstInitVal → ConstExp
		else //   array2 
			for (int i = 1; i < ptr1->children.size(); i += 2) { //{init,init,init}->1,3,5
				node* son_init = ptr1->children[i];
				for (int j = 1; j < son_init->children.size(); j += 2)
					arr->constArr.push_back(*son_init->children[j]->children[0]->numVal);
			}
	}
}

//calVal 预先计算表达式，如果set_node的值可求就求。ptr2为空表示单值运算。cal为空表示继承。
void calVal(node* ptr1,node*ptr2,node* set_node,string cal) {


	if (ptr2) {
		if (cal=="||") {
			if (ptr1->boolVal && *ptr1->boolVal || ptr1->numVal && *ptr1->numVal != 0) //短路(包括数字)
				set_node->boolVal = new bool(true);
			else if (ptr1->boolVal && !*ptr1->boolVal && ptr2->boolVal && !*ptr2->boolVal) 
				set_node->boolVal = new bool(false);
			else if (ptr1->numVal && *ptr1->numVal == 0 && ptr2->numVal && *ptr2->numVal == 0) 
				set_node->boolVal = new bool(false);
		}
		else if (cal == "&&") {
			if (ptr1->boolVal && !*ptr1->boolVal || ptr1->numVal && *ptr1->numVal == 0) //短路
				set_node->boolVal = new bool(false);
			else if (ptr1->boolVal && *ptr1->boolVal && ptr2->boolVal && *ptr2->boolVal)
				set_node->boolVal = new bool(true);
			else if (ptr1->numVal && *ptr1->numVal != 0 && ptr2->numVal && *ptr2->numVal != 0) 
				set_node->boolVal = new bool(true);
		}
		else if (ptr1->numVal && ptr2->numVal) {
			int n1 = *ptr1->numVal, n2 = *ptr2->numVal;
			if (cal == ">" )
				set_node->boolVal = new bool(n1>n2);
			else if(cal == "<" )
				set_node->boolVal = new bool(n1 < n2);
			else if(cal == ">=")
				set_node->boolVal = new bool(n1 >= n2);
			else if(cal == "<=")
				set_node->boolVal = new bool(n1 <= n2);
			else if(cal=="+")
				set_node->numVal = new int(n1 + n2);
			else if(cal=="-")
				set_node->numVal = new int(n1 - n2);
			else if(cal=="*")
				set_node->numVal = new int(n1 * n2);
			else if(cal =="/")
				set_node->numVal = new int(n1 / n2);
			else if(cal=="%")
				set_node->numVal = new int(n1 % n2);
		}
	}
	else
	{
		if (ptr1->boolVal && cal == "!")
			set_node->boolVal = new bool(!*(ptr1->boolVal));
		else if (ptr1->boolVal && cal == "")
			set_node->boolVal = new bool(*(ptr1->boolVal));
		else if (ptr1->numVal && cal == "!")//对数字取反，如!3是0(false)
			set_node->boolVal = new bool(*ptr1->numVal == 0 ? true : false);
		else if (ptr1->numVal && cal == "-")
			set_node->numVal = new int(-*(ptr1->numVal));
		else if (ptr1->numVal && (cal == "" || cal == "+"))//+或者继承
			set_node->numVal = new int(*ptr1->numVal);
	}
	if(cal!="")	set_node->strVal = new string(cal);//在结果节点保存运算符,便于无法求得的表达式，后续生成四元式
}

node* getPrimaryExp() {
	node* set_node = new node("PrimaryExp");
	node* ptr;
	if (inTXT[inTOP].second == "(") {
		Tmove(set_node, "LPARENT");
		ptr = getExp();
		set_node->children.push_back(ptr);
		calVal(ptr, nullptr, set_node, "");//calVal 预先计算表达式
		Tmove(set_node, "RPARENT");
	}
	else if (inTXT[inTOP].first == "IDENFR") {
		SymDetail* symTag = nameDefined(inTXT[inTOP].second);
		if(!symTag)
			errTXT.emplace_back(LineCnt[inTOP], "c");//***c 未定义的名字（非函数）
		ptr = getLVal();
		ptr->children[0]->detailPtr = symTag;//AST symTag ，此处的symTag正是符号表项。通过它获得对应常量的值
		set_node->children.push_back(ptr);
		if (ptr->numVal)
			calVal(ptr, nullptr, set_node, "");//calVal 同下，把常量数组元素值从LVal传给上层的PrimaryExp
		if (symTag->constInt)
			set_node->numVal = symTag->constInt;//calVal 把int常量赋值给PrimaryExp表达式(常量初值可以是包含常量的表达式)
	}
	else if (inTXT[inTOP].first == "INTCON") {
		int* numVal = new int(stoi(inTXT[inTOP].second));
		ptr = getNumber();
		set_node->children.push_back(ptr);
		set_node->numVal = numVal;//calVal 预先计算表达式
	}

	outTXT.emplace_back("<PrimaryExp>", "");
	return set_node;
}

node* getNumber() {
	node* set_node = new node("Number");
	node* ptr = new node("IntConst");//IntConst不再另写函数
	Tmove(ptr, "INTCON");
	set_node->children.push_back(ptr);

	outTXT.emplace_back("<Number>", "");
	return set_node;
}

//查找函数定义的detail表项。函数必须用这个查而不能用undefine，考虑（函数未定义而有同名变量）这种情况。
inline SymDetail* findFuncDetail(string name) {
	SymDetail* ret = nullptr;
	for (int i = 0; i < symRoot->tableList.size(); i++) 
		if (symRoot->tableList[i]->name == name) {
			ret = symRoot->tableList[i];
			break;
		}
	return ret;
}
//查找函数的符号表
inline SymTable* findFuncTable(string name){
	SymDetail* detail =findFuncDetail(name);
	if (detail)return detail->childTable; else return nullptr;
}

//避免重复的错误处理。若参数数目出错，就不再检测参数类型；名字不存在，同样也不再检查类型
node* getUnaryExp() {
	node* set_node = new node("UnaryExp");
	node* ptr;
	if (inTXT[inTOP].first == "IDENFR" && inTXT[inTOP + 1].second == "(") {

		string name = inTXT[inTOP].second;
		SymDetail* funcDetail = findFuncDetail(name);
		if(funcDetail==nullptr)//***c 未定义的函数名
			errTXT.emplace_back(LineCnt[inTOP], "c");
			
		Tmove(set_node, "IDENFR");
		set_node->children.back()->detailPtr= funcDetail;//AST symTag
		Tmove(set_node, "LPARENT");
		
		string f= inTXT[inTOP].first,s = inTXT[inTOP].second;
		if (s=="+"||s=="-"||s=="(" || f == "IDENFR" || f=="INTCON") {//***j 考虑右小括号缺失，不能再用小括号来判定，改用FuncRParams的first集
			ptr = getFuncRParams();
			set_node->children.push_back(ptr);
		}
		else if (funcDetail && funcDetail->childTable->para_n>0)//函数有至少一个参数
			errTXT.emplace_back(LineCnt[inTOP], "d");

		Tmove(set_node, "RPARENT");
	}
	else if (inTXT[inTOP].second == "+" || inTXT[inTOP].second == "-" || inTXT[inTOP].second == "!") {
		string cal = inTXT[inTOP].second;

		ptr = new node("UnaryOp");
		Tmove(ptr, inTXT[inTOP].first);//不再单独写UnaryOp函数
		outTXT.emplace_back("<UnaryOp>", "");
		set_node->children.push_back(ptr);
		ptr = getUnaryExp();
		set_node->children.push_back(ptr);

		calVal(ptr, nullptr, set_node, cal); //calVal 预先计算表达式
	}
	else
	{
		ptr = getPrimaryExp();
		set_node->children.push_back(ptr);
		calVal(ptr, nullptr, set_node, ""); //calVal 预先计算表达式
	}

	outTXT.emplace_back("<UnaryExp>", "");
	return set_node;
}

string checkLValType(int inTOP, string symType) {
	string type;
	if (inTXT[inTOP + 1].second == "[")
		for (int i = inTOP + 2, brack = 1; i < inTXT.size() ;i++) {//栈操作算出有多少个[]
			if (inTXT[i].second == "]")brack--;
			else if (inTXT[i].second == "[")brack++;

			if (brack == 0) {
				if (inTXT[i + 1].second == "[")//a[][]
					type = "int";
				else if (symType == "array1")//a[]
					type = "int";
				else type = "array1";
				break;
			}
		}
	else
		type = symType;
	return type;
}

node* getFuncRParams() {
	node* set_node = new node("FuncRParams");
	node* ptr;
	
	string name = inTXT[inTOP - 2].second;
	SymTable* funcTable = findFuncTable(name);

	int i=0;
	string type;
	while (1) {
		type = "";
		i++;
		if (inTXT[inTOP].first == "IDENFR" )
		{
			if (inTXT[inTOP+1].second == "(") {//函数
				SymDetail* symbol = findFuncDetail(inTXT[inTOP].second);
				if (symbol != nullptr)type = symbol->type;
			}
			else//变量常量
			{
				SymDetail* symbol = nameDefined(inTXT[inTOP].second);
				if (symbol != nullptr)
					type=checkLValType(inTOP, symbol->type);
			}
		}
		else
			type = "int";

		ptr = getExp();//用语法树确定类型，int或几维数组
		set_node->children.push_back(ptr);
		
		if(type!="" && i<=funcTable->para_n && type != funcTable->tableList[i-1]->type)//名字有定义、数目没超出但类型不匹配
			errTXT.emplace_back(LineCnt[inTOP], "e");

		if (inTXT[inTOP].second == ",")
			Tmove(set_node, "COMMA");
		else break;
	}
	if(i!=funcTable->para_n)
		errTXT.emplace_back(LineCnt[inTOP], "d");


	outTXT.emplace_back("<FuncRParams>", "");
	return set_node;
}

unordered_map <string, set<string> >Exp2sym{
	{"LOrExp",{"||"} },
	{"LAndExp",{"&&"}},
	{"EqExp",{"==","!="}},
	{"RelExp",{"<",">","<=",">="}},
	{"AddExp",{"+","-"}},
	{"MulExp",{"*","/","%"}},
};
vector<string> XExp = { "LOrExp" ,"LAndExp", "EqExp", "RelExp", "AddExp", "MulExp" };
inline node* getChild(string X) {
	int childExp = (int)(find(XExp.begin(), XExp.end(), X) - XExp.begin());
	return childExp == XExp.size() - 1 ? getUnaryExp() : getXExp(XExp[childExp + 1]);
}

//四个左递归，两个逻辑改为右递归
node* getXExp(string X) {
	set<string> symbols = Exp2sym[X];
	node* set_node = new node(X);
	node* ptr= getChild(X);
	set_node->children.push_back(ptr);
	calVal(ptr, nullptr, set_node, "");//calVal 预先计算表达式

	node* rec_node;
	if (X == "LOrExp" || X == "LAndExp") 
		while (symbols.count(inTXT[inTOP].second) > 0) {//符号仍然为||或&&，说明有两项及以上
			string cal = inTXT[inTOP].second;
			Tmove(set_node, inTXT[inTOP].first);//移走计算符
			node* rec_node = getXExp(X);//右递归
			set_node->children.push_back(rec_node);
			calVal(ptr, rec_node, set_node, cal);//calVal 预先计算表达式
		}
	else
		while (symbols.count(inTXT[inTOP].second) > 0) {//两项及以上
			rec_node = set_node;//改变指针指向，rec_node作为左边项
			set_node = new node(X);
			set_node->children.push_back(rec_node);//生成新的根节点，把旧的作为其左子节点
			outTXT.emplace_back("<" + X + ">", "");

			string cal = inTXT[inTOP].second;
			Tmove(set_node, inTXT[inTOP].first);
			ptr = getChild(X);//下一项即右边项
			set_node->children.push_back(ptr);
			calVal(rec_node, ptr, set_node, cal);//calVal 预先计算表达式
		}

	outTXT.emplace_back("<" + X + ">", "");
	return set_node;
}

node* getConstExp() {
	node* set_node = new node("ConstExp");
	node* ptr = getXExp("AddExp");
	set_node->children.push_back(ptr);//还要判断必须是常量
	calVal(ptr, nullptr, set_node, "");//calVal 预先计算表达式

	outTXT.emplace_back("<ConstExp>", "");
	return set_node;
}


