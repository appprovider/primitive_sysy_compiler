#include<climits> 
#include"all.h"
#include"genObj.h"
extern node* nodeRoot;
extern void getCompUnit(node*& get_node);//vs载入all.h懒惰，手动载入此函数防止频繁语法报错
extern vector<IRstmt> IR;
extern vector<IRstmt> DataStr;
extern int dealGlobalDecl();
extern void dealFuncDef(int i);
extern void printIR(ofstream& IRFile, vector<IRstmt>& IR);
extern vector<IRstmt> optedIR;//优化后的IR
extern void optIR(int level);

vector<Reg*> Regs;
vector<SymDetail*> BlockTemp;
int regCnt = 0;//0-17对应8-25号寄存器
int reg_turn = 0;//当次使用的下一个，每次从turn开始找17个里最小的


vector<Objstmt> Obj;
extern int stmt_n;
vector<int> Offsets;
int current_offset = 0;//内存sp相对当前函数fp指针的偏移

//公共子表达式优化的特点：临时值会多次复用

//静态data：字符串，全局变量，全局常量数组（int直接查符号表）返回第一条函数定义处
void globalAlloc() {
	Obj.emplace_back(".data", "", "", "");
	for (int i = 0; i < DataStr.size(); i++) {
		string name = DataStr[i].op;
		string str = DataStr[i].res->constString;
		Obj.emplace_back(name + ":", ".asciiz " + string("\"") + str + "\"", "", "");
		Obj.emplace_back(".align 2","","","");//四字节对齐
	}
	vector<IRstmt>initVal;
	int j = stmt_n;
	for (; IR[j].op !="FuncDef"; j++) {
		if (IR[j].op == "ConstDef" && IR[j].res->type == "int") {//从IR.[j].arg1->constInt取出常值
			IR[j].res->constInt = new int(*IR[j].arg1->constInt);
		}
		else if (IR[j].op == "ConstDef" || IR[j].op == "VarDef") {
			string name = IR[j].res->name;
			if (IR[j].res->type == "int")
				Obj.emplace_back(name + ": .word", IR[j].arg1 ? to_string(*IR[j].arg1->constInt) : "0", "", "");//默认初始化为0，否则将无法申请空间
			else if (IR[j].res->type == "array1")
				Obj.emplace_back(name + ": .space", to_string(*IR[j].res->dim1 * 4), "", "");
			else if (IR[j].res->type == "array2") {
				int dim1 = *IR[j].res->dim1, dim2 = *IR[j].res->dim2;
				Obj.emplace_back(name + ": .space", to_string(dim1 * dim2 * 4), "", "");
			}
		}
		else if (IR[j].op == "=" && IR[j - 1].op == "VarDef") {//全局变量声明的初值必是常量表达式
			Obj.back().arg[1] = to_string(*IR[j].arg1->constInt);
		}
		else if (IR[j].op == "[]=") {
			while (IR[j].op == "[]=") initVal.push_back(IR[j++]);
			j--;
		}
	}
	stmt_n = j;
	Obj.emplace_back(".text", "", "", "");
	for (int i = 0; i < initVal.size(); i++) {
		Obj.emplace_back("li", "$v0", to_string(*initVal[i].arg2->constInt), "");//li $v0 val
		int arr_size = *initVal[i].res->dim1 * (initVal[i].res->dim2 ? *initVal[i].res->dim2 : 1);
		int offset = (arr_size - 1) * 4 - *initVal[i].arg1->constInt * 4;//和局部数组保持一致，从高地址向低地址分配
		Obj.emplace_back("sw", "$v0", initVal[i].res->name + "+ " + to_string(offset), "");//label+ offset(mars似乎要在+后空一格)
	}
	Obj.emplace_back("j", "main", "", "");
	Obj.emplace_back("nop", "", "", "");
	Obj.emplace_back("", "", "", "");
}

//ld：为新值的到来做准备，存回旧值、更新寄存器状态等.
//ldtemp： 仅在单双目、函数调用时加入寄存器值集，赋值不加（后期合并公共表达式，会有左值为临时值，那么和ld将无区别）
//区分la  ld(修改Rx仅当x是int) st（修改x必须x是int） as(赋值,左值是变量) cal (cal是运算结果、常数的临时值。和ld类似,但ld要从内存加载值)
void LDST(string op,SymDetail* x,Reg* R) {
	if (op == "st" &&( x->type == "int"||x->type=="bool")) {
		if (x->fp_offset) //有fp_offset说明是局部变量，或活跃临时值，不为常数
			Obj.emplace_back("sw", x->regs.back()->regName, to_string(*x->fp_offset) + "($fp)", "");
		else if (x->isGlobal && x->constInt == nullptr) //全局变量
			Obj.emplace_back("sw", x->regs.back()->regName, x->name + "", "");
		x->valid = true;
	}
	else if (op == "ld"|| op == "la" || op=="cal"||op=="as" || op == "ldtemp") {
		for (int i = 0; i < R->val.size(); i++) //遍历各值，所有值的寄存器集中把R删掉（x的也是，避免重复加入寄存器）
			for (int j = 0; j < R->val[i]->regs.size(); j++)
				if (R->val[i]->regs[j] == R) {
					R->val[i]->regs.erase(R->val[i]->regs.begin() + j);
					j--;//erase删除元素，后面前移
				}
		R->val.clear();
		if(op=="ld" || op == "as")R->val.push_back(x);//不是临时值或数组，才向寄存器的值集加入这个值(能够在溢出时存回的才加入，否则会堆积)
		if (op == "cal" || op == "as" && x->isTemp)BlockTemp.push_back(x);//当前基本块临时值。用于配合tempToBeUsed使用，找出所有要存的临时变量。
		if (op == "cal" || op == "as") x->regs.clear();//如果是运算和赋值，其它旧值作废
		x->regs.push_back(R);//x新加入

		if (op == "la") {
			if (!x->fp_offset && x->baseArray)
				x->fp_offset = x->baseArray->fp_offset;//为地址参数赋fp_offset值。如把d的fp_offset赋给d[i]

			if (x->isGlobal && x->type != "int") {
				Obj.emplace_back("la", R->regName, x->name, "");//全局求址。（用于地址传参）
				int arr_size = *x->dim1 * (x->dim2 ? *x->dim2 : 1);
				Obj.emplace_back("add", R->regName, R->regName, to_string(arr_size*4-4));//把mips全局数组指针从栈底重定位到栈顶，便于地址递减寻址
			}
			else if ((x->type == "array1" || x->type == "array2") && x->kind == "para")
				Obj.emplace_back("lw", R->regName, to_string(*x->fp_offset) + "($fp)", "");//local间接求址。lw (offset+fp)
			else if (x->type == "array1" || x->type == "array2")
				Obj.emplace_back("add", R->regName, "$fp", to_string(*x->fp_offset)); //local直接求址.offset+fp
		}
		else if (op == "ld"||op=="ldtemp") {
			if (x->isGlobal)
				Obj.emplace_back("lw", R->regName, x->name, "");//全局求值
			else 
				Obj.emplace_back("lw", R->regName, to_string(*x->fp_offset)+ "($fp)","");//local求值
		}
	}
}

//为因子寻找寄存器
Reg* getReg(SymDetail* x) {
	Reg* reg;

	if (x->constInt) {//常数和临时值和数组相关：每次必须重新载入,使用寄存器集back的最新有效值
		reg = getNewReg(nullptr);
		LDST("cal", x, reg);//不加入寄存器的值集
		Obj.emplace_back("li", reg->regName, to_string(*x->constInt), "");
	}

	else if (x->constBool) {
		reg = getNewReg(nullptr);
		LDST("cal", x, reg);
		int num = *x->constBool ? 1 : 0;
		Obj.emplace_back("li", reg->regName, to_string(num), "");
	}
	else if (x->type == "array1"|| x->type == "array2") {//数组相关 (临时值不从内存载入。默认取寄存器集的尾部)
		reg = getNewReg(nullptr);
		LDST("la", x, reg);
	}
	else if (x->regs.size() > 0)//临时变量的寄存器集仅尾部（最新的）有效
		return x->regs.back();
	else if (x->kind == "ldtemp") {//已存储、未分配的临时值。
		reg = getNewReg(nullptr);
		LDST("ldtemp", x, reg);
	}
	else if (x->kind == "ret") {//一般函数返回值。
		reg = getNewReg(nullptr);//为实现tempToBeUsed，必须转存到8-25寄存器
		LDST("cal", x, reg);
	}
	else if (x->isTemp) {//承接getint的临时返回值。
		reg = getNewReg(nullptr);
		LDST("cal", x, reg);
	}
	else {//全局和局部int、bool，寄存器集为空从内存加载
		reg = getNewReg(nullptr);
		LDST("ld", x, reg);
	}
	return reg;
}

//10步的（以后再做数据流分析）变量存内存，加入寄存器值集。若寄存器被占会存回内存，无寄存器会重新加载
//输入为nullptr为任意寻找一个可用的寄存器（不考虑复用）否则为结果寻找寄存器
//查找空闲reg、不空闲时处理溢出。任何时候要分配reg时，是数组相关的必须重新分配。
const int RegUsed = 21;
Reg* getNewReg(IRstmt* stmt) {
	Reg* reg;
	if (regCnt <= RegUsed-1) {//17
		reg = new Reg("$" + to_string(5+regCnt++));
		Regs.push_back(reg);
	}
	else {
		int less_spill = INT_MAX, less_pos = 0;
		for (int cnt = reg_turn; cnt < reg_turn + RegUsed - 1; cnt++) {//如0，0+17=17，则只能到16		//17
			int i = cnt > RegUsed - 1 ? cnt % RegUsed : cnt;//如$26->$8, pos=0							//17 18
			int spill = 0;
			vector<SymDetail*> spillVals = Regs[i]->val;
			for (int j = 0; j < spillVals.size(); j++) {//遍历寄存器所有值，判断是否需要store
				if (spillVals[j]->type == "array1" || spillVals[j]->type == "array2")continue; //数组
				if( spillVals[j]->valid == true || spillVals[j]->regs.size() > 1)continue;//内存值同步、有备份
				if (spillVals[j]->constBool || spillVals[j]->constInt)continue;//常量不存
				if (spillVals[j]->isTemp && spillVals[j]->kind == "ldtemp" && !tempToBeUsed(spillVals[j])) {//十步之内不被用的ldtemp
					spillVals.erase(spillVals.begin() + j);
					j--;//erase删除元素，后面前移
					continue;
				}
				//if (stmt&& checkUsable(Regs[i]->val[j],stmt))continue;
				spill++;
			}
			if (spill < less_spill) {
				less_spill = spill;
				less_pos = i;
				if (spill == 0)break;//溢出数为0，视为找到了
			}
		}
		reg_turn = less_pos == RegUsed - 1 ? 0 : less_pos + 1;//如25，那么下一个是8				//17
		//处理溢出
		reg = Regs[less_pos];
		vector<SymDetail*> spillVals = Regs[less_pos]->val;
		for (int j = 0; j < spillVals.size(); j++) {
			if (spillVals[j]->type == "array1" || spillVals[j]->type == "array2")continue; //数组
			if (spillVals[j]->valid == true || spillVals[j]->regs.size() > 1)continue;//内存值同步、有备份
			if (spillVals[j]->constBool || spillVals[j]->constInt)continue;//常量不存,临时值要存（寄存器值集只有ldtemp，且10步内会被用到）
			//if (stmt && checkUsable(spillVals[i], stmt))continue;//可复用的寄存器
			LDST("st", spillVals[j], nullptr);
		}
	}
	return reg;
}

//常值、临时值默认可以覆盖
bool checkUsable(SymDetail* sym,IRstmt* stmt) {
	if (stmt->op == "=") {
		return (sym == stmt->arg1 || sym == stmt->res);
	}
	else {//其它三地址码
		return (sym == stmt->arg1 || sym == stmt->arg2 || sym == stmt->res);
	}
}


void transFuncCall() {
	int bottom = stmt_n;
	while (IR[stmt_n].op == "push") {
		SymDetail* paraR = IR[stmt_n].res;
		if (paraR->type == "int") {
			Reg* reg = getReg(paraR);
			Obj.emplace_back("sw", reg->regName, to_string(-4 * (stmt_n - bottom)) + "($sp)", "");
		}
		else if (paraR->type == "array1" || paraR->type == "array2") {
			Reg* baseReg = getReg(paraR);//负责分配新寄存器，用LDST获取{全局、局部、参数}三种数组基地址

			if (paraR->arr_offset)//$arr_offset + $baseReg 注意arr_offset会根据是否有偏移在中间代码生成阶段被赋值
			{
				if (paraR->arr_offset->constInt)
					Obj.emplace_back("add", baseReg->regName, baseReg->regName, to_string(-4 * (*paraR->arr_offset->constInt)));
				else {
					Reg* arr_offset = getReg(paraR->arr_offset);
					Obj.emplace_back("sll", "$v0", arr_offset->regName, "2");//偏移*4(左移2位)
					Obj.emplace_back("sub", baseReg->regName, baseReg->regName,"$v0");
				}
			}
			Obj.emplace_back("sw", baseReg->regName, to_string(-4 * (stmt_n - bottom))+"($sp)","");
		}	
		stmt_n++;
	}

	clearAll();//基本块末尾，fp更新之前把变量都存了
	Obj.emplace_back("add", "$sp", "$sp", to_string(-4 * (stmt_n - bottom)));
	Obj.emplace_back("sw", "$ra", "0($sp)", "");//调用方保存自己的ra
	Obj.emplace_back("sw", "$fp", "-4($sp)", "");//参数--ra--fp--局部变量
	Obj.emplace_back("move", "$fp", "$sp","");//ra=0($fp)
	Obj.emplace_back("add", "$sp", "$sp", to_string(-8));
	IRstmt& func = IR[stmt_n];
	stmt_n++;
	Obj.emplace_back("jal", func.res->name, "", "");//这之后的返回语句（下一句），fp又复原了,并且属于下一个基本块
	
	Obj.emplace_back("lw", "$ra", "0($sp)", "");//调用方取出自己的ra
	Obj.emplace_back("add", "$sp", "$sp", to_string(4 * func.res->childTable->para_n));//参数退栈
	Obj.emplace_back("nop", "", "", "");
	Obj.emplace_back("", "", "", "");

	if (func.arg1) {
		Reg* retreg = getReg(func.arg1);//为存在arg1的返回值（ret类型）分配寄存器
		LDST("cal", func.arg1, retreg);//向func.arg1的寄存器集加入retreg
		Obj.emplace_back("move", retreg->regName, "$v0", "");
	}
}

void transFuncDef() {
	SymDetail* func = IR[stmt_n].res;
	transFuncPara();
	dealIRstmt();//嵌套回去，继续处理普通语句
	if (func->name == "main") {//main函数直接输出以下语句，结束。
		Obj.emplace_back("li", "$v0", "10", "");
		Obj.emplace_back("syscall", "", "", "");
		stmt_n++;
		return;
	}
	transFuncRet(func->childTable->para_n);
}

void transFuncPara() {
	SymDetail* func = IR[stmt_n++].res;
	SymTable* funcTable = func->childTable;
	Obj.emplace_back(func->name + ":", "", "", "");
	if (func->name == "main")Obj.emplace_back("move", "$fp", "$sp", "");//main函数特例。fp等于初始sp
	for (int i = 0; i < funcTable->para_n; i++, stmt_n++)
		funcTable->tableList[i]->fp_offset = new int(4 * (funcTable->para_n - i));//为符号表的参数项赋地址fp_offset
	
	if (func->name == "main")Offsets.push_back(0);
	else Offsets.push_back(-8);//0为$ra,-4为oldfp。局部变量偏移将在编译时确定，故控制流将完全线性覆盖，运行时会有内存空隙
}

//para_n为负值(-1)表示不是最后的return。此时要clearAll，但Offsets不弹栈（一直分配到函数定义结束才弹栈）
void transFuncRet(int para_n) {
	if (IR[stmt_n].res) {//若有返回值,
		Reg* reg = getReg(IR[stmt_n].res);
		Obj.emplace_back("move", "$v0", reg->regName, "");//v0不保留任何值，一有返回值就取出
	}
	if (para_n >= 0) {
		Offsets.pop_back();
		if (!Offsets.empty())
			Offsets.back() += 4 * para_n;//不是main。Offsets在函数定义开头和返回处分别压栈、弹栈，属于在编译时可以确定的偏移量
	}

	Obj.emplace_back("move", "$sp", "$fp", "");

	clearAll();//基本块末尾，fp更新之前把变量都存了（这里主要是存变量，临时值生命周期都终结了）

	Obj.emplace_back("lw", "$fp", "-4($fp)", "");//恢复old_fp
	Obj.emplace_back("jr", "$ra", "", "");
	Obj.emplace_back("nop", "", "", "");
	Obj.emplace_back("", "", "", "");
	stmt_n++;

}

//只给符号表项、sp分配内存，数组初始化由赋值语句处理
void transLocalDef() {
	if (IR[stmt_n].arg1) {//是常量。(常量声明初值： def initval _ detail
		IR[stmt_n].res->constInt = new int(*IR[stmt_n].arg1->constInt);
	}
	IR[stmt_n].res->fp_offset = new int (Offsets.back());

	int span=4;
	if (IR[stmt_n].res->type == "int"&&IR[stmt_n].res->constInt)//有初始值
	{
		Reg* reg=getNewReg(nullptr);
		LDST("as", IR[stmt_n].res, reg);//声明的左值是变量，要存入寄存器的值集
		Obj.emplace_back("li", reg->regName, to_string(*IR[stmt_n].arg1->constInt), "");
	}
	else if (IR[stmt_n].res->type == "array1")
		span = *IR[stmt_n].res->dim1 * 4;
	else if (IR[stmt_n].res->type == "array2")
		span = *IR[stmt_n].res->dim1 * (*IR[stmt_n].res->dim2) * 4;
	Offsets.back() -= span;
	Obj.emplace_back("add", "$sp", "$fp", to_string(Offsets.back()));
	stmt_n++;
}

//单目
void transUnary() {
	string op = IR[stmt_n].op;
	Reg* reg1 = getReg(IR[stmt_n].arg1);
	Reg* resReg = getNewReg(&IR[stmt_n]);
	LDST("cal", IR[stmt_n].res, resReg);
	if (op == "-")
		Obj.emplace_back("sub", resReg->regName, "$0", reg1->regName);
	else if (op == "!")
		Obj.emplace_back("seq", resReg->regName, reg1->regName, "$0");// 注意是逻辑取反：若非0->0;若为0->1
	
	IR[stmt_n].res->valid = false;//更新重置有效位
	stmt_n++;
}

//加减乘除%
void transExpCal() {
	string op = IR[stmt_n].op;
	Reg* reg1 = getReg(IR[stmt_n].arg1), * reg2 = getReg(IR[stmt_n].arg2);
	Reg* resReg = getNewReg(&IR[stmt_n]);
	LDST("cal", IR[stmt_n].res,resReg);
	if (op == "+") {
		Obj.emplace_back("add", resReg->regName, reg1->regName, reg2->regName);	
	}
	else if (op == "-") {
		Obj.emplace_back("sub", resReg->regName, reg1->regName, reg2->regName);
	}
	else if (op == "*") {
		Obj.emplace_back("mult", reg1->regName, reg2->regName, "");
		Obj.emplace_back("mflo", resReg->regName,"", "");
	}
	else if (op == "/") {
		Obj.emplace_back("div", reg1->regName, reg2->regName, "");
		Obj.emplace_back("mflo", resReg->regName, "", "");
	}
	else if (op == "%") {
		Obj.emplace_back("div", reg1->regName, reg2->regName, "");
		Obj.emplace_back("mfhi", resReg->regName, "", "");
	}
	IR[stmt_n].res->valid = false;//更新重置有效位
	stmt_n++;
}

void transAssign() {
	string op = IR[stmt_n].op;
	SymDetail* res = IR[stmt_n].res;
	Reg* reg1 ,*resReg,*reg2;
	if (op == "=") {
		reg1 = getReg(IR[stmt_n].arg1);
		res->regs.clear();//共有部分，赋值必须把变量的寄存器集清空
		if (IR[stmt_n].arg1->constInt || IR[stmt_n].arg1->isTemp) {//考虑右值是常量、临时值要新申请寄存器。
			resReg = getNewReg(nullptr);
			LDST("as", res, resReg);//resReg的值清空只留res，并加入到res的寄存器集(左值一般是变量所以可在溢出时存回)
			Obj.emplace_back("move", resReg->regName, reg1->regName, "");
		}
		else
		{
			reg1->val.push_back(res);//向reg1值集添加左值
			res->regs.push_back(reg1);//reg1加入到res的寄存器集
		}
	}
	else if (op == "=[]") {//[]=[]先不考虑
		reg1 = getReg(IR[stmt_n].arg1);//数组地址
		if (IR[stmt_n].arg2->constInt)//偏移量是常量的情况
			Obj.emplace_back("add", "$v0", reg1->regName, to_string(*IR[stmt_n].arg2->constInt * -4));
		else {
			reg2 = getReg(IR[stmt_n].arg2);//偏移量
			Obj.emplace_back("sll", "$v0", reg2->regName, "2");//偏移*4(左移2位)。使用v0避免改变偏移值（尤其迭代值，如for循环的i）
			Obj.emplace_back("sub", "$v0", reg1->regName, "$v0");
		}
		resReg = getNewReg(nullptr);
		LDST("cal", res, resReg);
		Obj.emplace_back("lw", resReg->regName, "($v0)", "");//lw $t1 ($reg2+$reg1)
	}
	else if (op == "[]=") {//regres[reg1*4]=reg2
		resReg = getReg(res);//数组地址
		if (IR[stmt_n].arg1->constInt)//偏移量是常量的情况
			Obj.emplace_back("add", "$v0", resReg->regName, to_string(*IR[stmt_n].arg1->constInt * -4));
		else {
			reg1= getReg(IR[stmt_n].arg1);
			Obj.emplace_back("sll", "$v0", reg1->regName, "2");//偏移*4(左移2位)
			Obj.emplace_back("sub", "$v0", resReg->regName,"$v0" );//注意偏移是负向
		}
		reg2 = getReg(IR[stmt_n].arg2);//右值
		Obj.emplace_back("sw", reg2->regName, "($v0)", "");
	}
	res->valid = false;//更新重置有效位
	stmt_n++;
}


//关系、逻辑运算
void transRLExpCal() {
	string op = IR[stmt_n].op;
	Reg* reg1=getReg(IR[stmt_n].arg1), * reg2=getReg(IR[stmt_n].arg2);
	Reg* resReg = getNewReg(&IR[stmt_n]);
	LDST("cal", IR[stmt_n].res, resReg);
	if (op == "&&"||op == "||") {//语法中允许int进行逻辑与或，必须转成0|1才能用mips的按位与或
		op = op == "&&" ? "and " : "or";
		string reg1name = reg1->regName, reg2name = reg2->regName;
		if (IR[stmt_n].arg1->constInt || IR[stmt_n].arg1->type == "int") {//注意4个1，2个v0
			reg1name = "$v0";
			Obj.emplace_back("sne","$v0", reg1->regName, "0");
		}
		if (IR[stmt_n].arg2->constInt || IR[stmt_n].arg2->type == "int") {
			reg2name = "$v1";
			Obj.emplace_back("sne", "$v1", reg2->regName, "0");
		}
		Obj.emplace_back(op, resReg->regName, reg1name, reg2name);
	}
	else {
		if (op == ">") op = "sgt";
		else if (op == ">=")op = "sge";
		else if (op == "<")op = "slt";
		else if (op == "<=")op = "sle";
		else if (op == "==")op = "seq";
		else if (op == "!=")op = "sne";
		Obj.emplace_back(op, resReg->regName, reg1->regName, reg2->regName);
	}
	IR[stmt_n].res->valid = false;
	stmt_n++;
}

//查看10步之内是否被用到，低配版活跃变量分析，stmt_n+1从下下个开始，（下个就使用已经通过reg_turn+1机制解决了）
#define LOOKAHEAD 10
bool tempToBeUsed(SymDetail* x) {
	for (int i = stmt_n+1;  i < IR.size(); i++) {
		if (IR[i].arg1 == x || IR[i].arg2 == x || IR[i].res == x)//将在中间代码十步或结束前被使用
			return true;
	}
	return false;
}

//进入下一个基本块前，任何一些临时值生成后，把可能被使用的全部存好（如果不是基本块末尾，需要引入数据流，不能清空寄存器集）
void saveTemp(bool nextBlock) {
	for (SymDetail* x : BlockTemp) {
		if (tempToBeUsed(x) && x->isTemp && (x->type == "int" || x->type == "bool") && x->valid == false) {//将被使用的临时变量（函数实参、表达式因子）、还要申请空间
			if (!x->fp_offset) {//如果之前没有分配，则分配地址，否则只用存回
				x->fp_offset = new int(Offsets.back());
				Offsets.back() -= 4;
			}
			Obj.emplace_back("add", "$sp", "$fp", to_string(Offsets.back()));
			LDST("st", x, nullptr);
			x->kind = "ldtemp";//把ret（表示被暂存的返回值）或其它需要保存的临时值类型改成ldtemp
			x->regs.back()->val.push_back(x);//向寄存器的值集加入此临时值，并由寄存器分配函数newReg负责决定是否要刷新掉此值(利用tempToBeUsed判断)
		}
		if(nextBlock)
			x->regs.clear();//进入下一个基本块前，寄存器集必须清空。
	}
}

//基本块末尾把寄存器全部存好，归零。jal、j、jr、ret、beqz、goto、Label:
void clearAll() {
	saveTemp(true);//临时值加入到寄存器值集必须先做。然后再清空值的寄存器集（regs.clear），顺序颠倒会导致寄存器集为空
	BlockTemp.clear();

	for (Reg* R : Regs)
		for (SymDetail* x : R->val) //遍历各值，所有int变量值全部更新到内存
		{
			if (x->valid == false && x->type == "int" && (x->fp_offset || x->isGlobal && x->constInt == nullptr)) {//局部变量（不是常数或临时值）、全局变量
				LDST("st", x, nullptr);
				x->valid = true;//
			}
			x->regs.clear();//进入下一个基本块前，(临时值和非临时值)寄存器集必须清空。
		}

	Regs.clear();
	reg_turn = regCnt = 0;
}

void transStream() {
	string op = IR[stmt_n].op;
	
	if (op == "goto") {
		clearAll();
		Obj.emplace_back("j", IR[stmt_n].to_label, "", "");
	}
	else if (op == "beqz") {//用于ifFalse和短路求值
		Reg* reg1 = getReg(IR[stmt_n].arg1);
		Obj.emplace_back("move", "$v0", reg1->regName,  "");//beqz寄存一个临时值
		clearAll();
		Obj.emplace_back("beqz", "$v0", IR[stmt_n].to_label, "");
	}	
	else if (op == "bnez") {//专用于短路求值
		Reg* reg1 = getReg(IR[stmt_n].arg1);
		Obj.emplace_back("move", "$v0", reg1->regName, "");//bnez寄存一个临时值
		clearAll();
		Obj.emplace_back("bnez", "$v0", IR[stmt_n].to_label, "");
	}
	Obj.emplace_back("nop", "", "", "");
	Obj.emplace_back("", "", "", "");
	stmt_n++;
}

void transPrintf() {
	SymDetail* detail = IR[stmt_n].res;
	if (detail->constString != "") {
		Obj.emplace_back("li", "$v0", "4", "");
		Obj.emplace_back("la", "$a0", detail->constString, "");
	}
	else {
		Reg* reg = getReg(detail);
		Obj.emplace_back("li", "$v0", "1", "");
		Obj.emplace_back("move", "$a0", reg->regName, "");
	}
	Obj.emplace_back("syscall", "", "", "");
	stmt_n++;
}

void transGetint() {
	SymDetail* detail = IR[stmt_n].res;
	Reg* reg = getReg(detail);
	LDST("cal", IR[stmt_n].res, reg);//把res的值加入reg
	Obj.emplace_back("li", "$v0", "5", "");
	Obj.emplace_back("syscall", "", "", "");
	Obj.emplace_back("move", reg->regName, "$v0", "");
	stmt_n++;
}

void dealIRstmt() {
	string op;
	for (; stmt_n < IR.size(); )
	{
		op = IR[stmt_n].op;
		if (op == "FuncDef")
			transFuncDef();
		else if (op == "ConstDef" || op == "VarDef")
			transLocalDef();
		else if (op == "push" || op == "call") {
			transFuncCall();
			saveTemp(false);//专门为了保存临时返回值,并非基本块末尾所以不用清空寄存器（尚可利用）
		}
		else if (IR[stmt_n].arg2 == nullptr && (op == "-" || op == "!")){//中间代码没有+，直接当作同一个值
			transUnary();
			saveTemp(false);//保存10步内被用的临时值
		}
		else if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
			transExpCal();
			saveTemp(false);//保存10步内被用的临时值
		}
		else if (op == "=" || op == "[]=" || op == "=[]") {
			transAssign();
			saveTemp(false);//保存10步内被用的临时值
		}
			
		else if (op == ">" || op == "<" || op == ">=" || op == "<=" || op == ">" || op == "==" || op == "!=" || op == "&&" || op == "||") {
			transRLExpCal();
			saveTemp(false);//保存10步内被用的临时值
		}
			
		else if (op == "goto" || op == "beqz"||op=="bnez")
			transStream();
		else if (IR[stmt_n].label != "") {
			clearAll();
			Obj.emplace_back("", "", "", "");
			Obj.emplace_back(IR[stmt_n++].label + ":", "", "", "");
		}
		else if (op == "printf")
			transPrintf();
		else if (op == "getint")
			transGetint();
		else if (op == "ret")//不是最后的ret
			transFuncRet(-1);
		else if(op=="lastret")//最后一个return把控制权还给transFuncDef
			break;

		std::cout << stmt_n << std::endl;
	}

}
int SymDetail::tempCnt = 0;
int main() {
	ifstream srcFile("testfile.txt", ios::in);
	ofstream semanticFile("semantic.txt", ios::out);
	ofstream errFile("error.txt", ios::out);
	ofstream treeFile("ASTtree.txt", ios::out);
	ofstream IRFile("IRcode.txt", ios::out);
	ofstream optedIRFile("optedIRcode.txt", ios::out);
	ofstream ObjFile("mips.txt", ios::out);
	lexical(srcFile);

	//测试lexical
	/*
	for (int i = 0; i < lexOut.size(); i++) {
		dstFile << lexOut[i].first << " " << lexOut[i].second <<" "<<LineCnt[i]<< endl;
	}*/

	getCompUnit(nodeRoot);
	srcFile.close();

	//测试semantic
	for (int i = 0; i < outTXT.size(); i++) {
		if (outTXT[i].first[0] == '<') {
			if (outTXT[i].first != "<Decl>" && outTXT[i].first != "<BlockItem>" && outTXT[i].first != "<Btype>")
				semanticFile << outTXT[i].first << endl;
		}
		else
		{
			semanticFile << outTXT[i].first << " " << outTXT[i].second << endl;
		}
	}
	semanticFile.close();

	//输出错误处理信息
	for (int i = 0; i < errTXT.size(); i++)
	{
		errFile << errTXT[i].first << " " << errTXT[i].second << endl;
	}
	errFile.close();

	//输出语法树
/*	printTree(treeFile, nodeRoot);
	treeFile.close();*/

	int func_begin = dealGlobalDecl();
	dealFuncDef(func_begin);//中间代码

	//输出中间代码
	printIR(IRFile,IR);
	IRFile.close();
	
	//输出优化的中间代码
	optIR(1);
	printIR(optedIRFile,IR);
	optedIRFile.close();

	
	//生成mips目标代码
	
	globalAlloc();
	dealIRstmt();

	//目标代码
	for (int i = 0;i<Obj.size(); i++) {
		ObjFile << Obj[i].arg[0] << " " << Obj[i].arg[1] << (Obj[i].arg[2].empty() ? "" : ", " )<< Obj[i].arg[2] << (Obj[i].arg[3].empty() ? "" : ", ") << Obj[i].arg[3] << std::endl;
	}
	ObjFile.close();

}