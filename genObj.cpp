#include<climits> 
#include"all.h"
#include"genObj.h"
extern node* nodeRoot;
extern void getCompUnit(node*& get_node);//vs����all.h���裬�ֶ�����˺�����ֹƵ���﷨����
extern vector<IRstmt> IR;
extern vector<IRstmt> DataStr;
extern int dealGlobalDecl();
extern void dealFuncDef(int i);
extern void printIR(ofstream& IRFile, vector<IRstmt>& IR);
extern vector<IRstmt> optedIR;//�Ż����IR
extern void optIR(int level);

vector<Reg*> Regs;
vector<SymDetail*> BlockTemp;
int regCnt = 0;//0-17��Ӧ8-25�żĴ���
int reg_turn = 0;//����ʹ�õ���һ����ÿ�δ�turn��ʼ��17������С��


vector<Objstmt> Obj;
extern int stmt_n;
vector<int> Offsets;
int current_offset = 0;//�ڴ�sp��Ե�ǰ����fpָ���ƫ��

//�����ӱ��ʽ�Ż����ص㣺��ʱֵ���θ���

//��̬data���ַ�����ȫ�ֱ�����ȫ�ֳ������飨intֱ�Ӳ���ű����ص�һ���������崦
void globalAlloc() {
	Obj.emplace_back(".data", "", "", "");
	for (int i = 0; i < DataStr.size(); i++) {
		string name = DataStr[i].op;
		string str = DataStr[i].res->constString;
		Obj.emplace_back(name + ":", ".asciiz " + string("\"") + str + "\"", "", "");
		Obj.emplace_back(".align 2","","","");//���ֽڶ���
	}
	vector<IRstmt>initVal;
	int j = stmt_n;
	for (; IR[j].op !="FuncDef"; j++) {
		if (IR[j].op == "ConstDef" && IR[j].res->type == "int") {//��IR.[j].arg1->constIntȡ����ֵ
			IR[j].res->constInt = new int(*IR[j].arg1->constInt);
		}
		else if (IR[j].op == "ConstDef" || IR[j].op == "VarDef") {
			string name = IR[j].res->name;
			if (IR[j].res->type == "int")
				Obj.emplace_back(name + ": .word", IR[j].arg1 ? to_string(*IR[j].arg1->constInt) : "0", "", "");//Ĭ�ϳ�ʼ��Ϊ0�������޷�����ռ�
			else if (IR[j].res->type == "array1")
				Obj.emplace_back(name + ": .space", to_string(*IR[j].res->dim1 * 4), "", "");
			else if (IR[j].res->type == "array2") {
				int dim1 = *IR[j].res->dim1, dim2 = *IR[j].res->dim2;
				Obj.emplace_back(name + ": .space", to_string(dim1 * dim2 * 4), "", "");
			}
		}
		else if (IR[j].op == "=" && IR[j - 1].op == "VarDef") {//ȫ�ֱ��������ĳ�ֵ���ǳ������ʽ
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
		int offset = (arr_size - 1) * 4 - *initVal[i].arg1->constInt * 4;//�;ֲ����鱣��һ�£��Ӹߵ�ַ��͵�ַ����
		Obj.emplace_back("sw", "$v0", initVal[i].res->name + "+ " + to_string(offset), "");//label+ offset(mars�ƺ�Ҫ��+���һ��)
	}
	Obj.emplace_back("j", "main", "", "");
	Obj.emplace_back("nop", "", "", "");
	Obj.emplace_back("", "", "", "");
}

//ld��Ϊ��ֵ�ĵ�����׼������ؾ�ֵ�����¼Ĵ���״̬��.
//ldtemp�� ���ڵ�˫Ŀ����������ʱ����Ĵ���ֵ������ֵ���ӣ����ںϲ��������ʽ��������ֵΪ��ʱֵ����ô��ld��������
//����la  ld(�޸�Rx����x��int) st���޸�x����x��int�� as(��ֵ,��ֵ�Ǳ���) cal (cal������������������ʱֵ����ld����,��ldҪ���ڴ����ֵ)
void LDST(string op,SymDetail* x,Reg* R) {
	if (op == "st" &&( x->type == "int"||x->type=="bool")) {
		if (x->fp_offset) //��fp_offset˵���Ǿֲ����������Ծ��ʱֵ����Ϊ����
			Obj.emplace_back("sw", x->regs.back()->regName, to_string(*x->fp_offset) + "($fp)", "");
		else if (x->isGlobal && x->constInt == nullptr) //ȫ�ֱ���
			Obj.emplace_back("sw", x->regs.back()->regName, x->name + "", "");
		x->valid = true;
	}
	else if (op == "ld"|| op == "la" || op=="cal"||op=="as" || op == "ldtemp") {
		for (int i = 0; i < R->val.size(); i++) //������ֵ������ֵ�ļĴ������а�Rɾ����x��Ҳ�ǣ������ظ�����Ĵ�����
			for (int j = 0; j < R->val[i]->regs.size(); j++)
				if (R->val[i]->regs[j] == R) {
					R->val[i]->regs.erase(R->val[i]->regs.begin() + j);
					j--;//eraseɾ��Ԫ�أ�����ǰ��
				}
		R->val.clear();
		if(op=="ld" || op == "as")R->val.push_back(x);//������ʱֵ�����飬����Ĵ�����ֵ���������ֵ(�ܹ������ʱ��صĲż��룬�����ѻ�)
		if (op == "cal" || op == "as" && x->isTemp)BlockTemp.push_back(x);//��ǰ��������ʱֵ���������tempToBeUsedʹ�ã��ҳ�����Ҫ�����ʱ������
		if (op == "cal" || op == "as") x->regs.clear();//���������͸�ֵ��������ֵ����
		x->regs.push_back(R);//x�¼���

		if (op == "la") {
			if (!x->fp_offset && x->baseArray)
				x->fp_offset = x->baseArray->fp_offset;//Ϊ��ַ������fp_offsetֵ�����d��fp_offset����d[i]

			if (x->isGlobal && x->type != "int") {
				Obj.emplace_back("la", R->regName, x->name, "");//ȫ����ַ�������ڵ�ַ���Σ�
				int arr_size = *x->dim1 * (x->dim2 ? *x->dim2 : 1);
				Obj.emplace_back("add", R->regName, R->regName, to_string(arr_size*4-4));//��mipsȫ������ָ���ջ���ض�λ��ջ�������ڵ�ַ�ݼ�Ѱַ
			}
			else if ((x->type == "array1" || x->type == "array2") && x->kind == "para")
				Obj.emplace_back("lw", R->regName, to_string(*x->fp_offset) + "($fp)", "");//local�����ַ��lw (offset+fp)
			else if (x->type == "array1" || x->type == "array2")
				Obj.emplace_back("add", R->regName, "$fp", to_string(*x->fp_offset)); //localֱ����ַ.offset+fp
		}
		else if (op == "ld"||op=="ldtemp") {
			if (x->isGlobal)
				Obj.emplace_back("lw", R->regName, x->name, "");//ȫ����ֵ
			else 
				Obj.emplace_back("lw", R->regName, to_string(*x->fp_offset)+ "($fp)","");//local��ֵ
		}
	}
}

//Ϊ����Ѱ�ҼĴ���
Reg* getReg(SymDetail* x) {
	Reg* reg;

	if (x->constInt) {//��������ʱֵ��������أ�ÿ�α�����������,ʹ�üĴ�����back��������Чֵ
		reg = getNewReg(nullptr);
		LDST("cal", x, reg);//������Ĵ�����ֵ��
		Obj.emplace_back("li", reg->regName, to_string(*x->constInt), "");
	}

	else if (x->constBool) {
		reg = getNewReg(nullptr);
		LDST("cal", x, reg);
		int num = *x->constBool ? 1 : 0;
		Obj.emplace_back("li", reg->regName, to_string(num), "");
	}
	else if (x->type == "array1"|| x->type == "array2") {//������� (��ʱֵ�����ڴ����롣Ĭ��ȡ�Ĵ�������β��)
		reg = getNewReg(nullptr);
		LDST("la", x, reg);
	}
	else if (x->regs.size() > 0)//��ʱ�����ļĴ�������β�������µģ���Ч
		return x->regs.back();
	else if (x->kind == "ldtemp") {//�Ѵ洢��δ�������ʱֵ��
		reg = getNewReg(nullptr);
		LDST("ldtemp", x, reg);
	}
	else if (x->kind == "ret") {//һ�㺯������ֵ��
		reg = getNewReg(nullptr);//Ϊʵ��tempToBeUsed������ת�浽8-25�Ĵ���
		LDST("cal", x, reg);
	}
	else if (x->isTemp) {//�н�getint����ʱ����ֵ��
		reg = getNewReg(nullptr);
		LDST("cal", x, reg);
	}
	else {//ȫ�ֺ;ֲ�int��bool���Ĵ�����Ϊ�մ��ڴ����
		reg = getNewReg(nullptr);
		LDST("ld", x, reg);
	}
	return reg;
}

//10���ģ��Ժ������������������������ڴ棬����Ĵ���ֵ�������Ĵ�����ռ�����ڴ棬�޼Ĵ��������¼���
//����ΪnullptrΪ����Ѱ��һ�����õļĴ����������Ǹ��ã�����Ϊ���Ѱ�ҼĴ���
//���ҿ���reg��������ʱ����������κ�ʱ��Ҫ����regʱ����������صı������·��䡣
const int RegUsed = 21;
Reg* getNewReg(IRstmt* stmt) {
	Reg* reg;
	if (regCnt <= RegUsed-1) {//17
		reg = new Reg("$" + to_string(5+regCnt++));
		Regs.push_back(reg);
	}
	else {
		int less_spill = INT_MAX, less_pos = 0;
		for (int cnt = reg_turn; cnt < reg_turn + RegUsed - 1; cnt++) {//��0��0+17=17����ֻ�ܵ�16		//17
			int i = cnt > RegUsed - 1 ? cnt % RegUsed : cnt;//��$26->$8, pos=0							//17 18
			int spill = 0;
			vector<SymDetail*> spillVals = Regs[i]->val;
			for (int j = 0; j < spillVals.size(); j++) {//�����Ĵ�������ֵ���ж��Ƿ���Ҫstore
				if (spillVals[j]->type == "array1" || spillVals[j]->type == "array2")continue; //����
				if( spillVals[j]->valid == true || spillVals[j]->regs.size() > 1)continue;//�ڴ�ֵͬ�����б���
				if (spillVals[j]->constBool || spillVals[j]->constInt)continue;//��������
				if (spillVals[j]->isTemp && spillVals[j]->kind == "ldtemp" && !tempToBeUsed(spillVals[j])) {//ʮ��֮�ڲ����õ�ldtemp
					spillVals.erase(spillVals.begin() + j);
					j--;//eraseɾ��Ԫ�أ�����ǰ��
					continue;
				}
				//if (stmt&& checkUsable(Regs[i]->val[j],stmt))continue;
				spill++;
			}
			if (spill < less_spill) {
				less_spill = spill;
				less_pos = i;
				if (spill == 0)break;//�����Ϊ0����Ϊ�ҵ���
			}
		}
		reg_turn = less_pos == RegUsed - 1 ? 0 : less_pos + 1;//��25����ô��һ����8				//17
		//�������
		reg = Regs[less_pos];
		vector<SymDetail*> spillVals = Regs[less_pos]->val;
		for (int j = 0; j < spillVals.size(); j++) {
			if (spillVals[j]->type == "array1" || spillVals[j]->type == "array2")continue; //����
			if (spillVals[j]->valid == true || spillVals[j]->regs.size() > 1)continue;//�ڴ�ֵͬ�����б���
			if (spillVals[j]->constBool || spillVals[j]->constInt)continue;//��������,��ʱֵҪ�棨�Ĵ���ֵ��ֻ��ldtemp����10���ڻᱻ�õ���
			//if (stmt && checkUsable(spillVals[i], stmt))continue;//�ɸ��õļĴ���
			LDST("st", spillVals[j], nullptr);
		}
	}
	return reg;
}

//��ֵ����ʱֵĬ�Ͽ��Ը���
bool checkUsable(SymDetail* sym,IRstmt* stmt) {
	if (stmt->op == "=") {
		return (sym == stmt->arg1 || sym == stmt->res);
	}
	else {//��������ַ��
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
			Reg* baseReg = getReg(paraR);//��������¼Ĵ�������LDST��ȡ{ȫ�֡��ֲ�������}�����������ַ

			if (paraR->arr_offset)//$arr_offset + $baseReg ע��arr_offset������Ƿ���ƫ�����м�������ɽ׶α���ֵ
			{
				if (paraR->arr_offset->constInt)
					Obj.emplace_back("add", baseReg->regName, baseReg->regName, to_string(-4 * (*paraR->arr_offset->constInt)));
				else {
					Reg* arr_offset = getReg(paraR->arr_offset);
					Obj.emplace_back("sll", "$v0", arr_offset->regName, "2");//ƫ��*4(����2λ)
					Obj.emplace_back("sub", baseReg->regName, baseReg->regName,"$v0");
				}
			}
			Obj.emplace_back("sw", baseReg->regName, to_string(-4 * (stmt_n - bottom))+"($sp)","");
		}	
		stmt_n++;
	}

	clearAll();//������ĩβ��fp����֮ǰ�ѱ���������
	Obj.emplace_back("add", "$sp", "$sp", to_string(-4 * (stmt_n - bottom)));
	Obj.emplace_back("sw", "$ra", "0($sp)", "");//���÷������Լ���ra
	Obj.emplace_back("sw", "$fp", "-4($sp)", "");//����--ra--fp--�ֲ�����
	Obj.emplace_back("move", "$fp", "$sp","");//ra=0($fp)
	Obj.emplace_back("add", "$sp", "$sp", to_string(-8));
	IRstmt& func = IR[stmt_n];
	stmt_n++;
	Obj.emplace_back("jal", func.res->name, "", "");//��֮��ķ�����䣨��һ�䣩��fp�ָ�ԭ��,����������һ��������
	
	Obj.emplace_back("lw", "$ra", "0($sp)", "");//���÷�ȡ���Լ���ra
	Obj.emplace_back("add", "$sp", "$sp", to_string(4 * func.res->childTable->para_n));//������ջ
	Obj.emplace_back("nop", "", "", "");
	Obj.emplace_back("", "", "", "");

	if (func.arg1) {
		Reg* retreg = getReg(func.arg1);//Ϊ����arg1�ķ���ֵ��ret���ͣ�����Ĵ���
		LDST("cal", func.arg1, retreg);//��func.arg1�ļĴ���������retreg
		Obj.emplace_back("move", retreg->regName, "$v0", "");
	}
}

void transFuncDef() {
	SymDetail* func = IR[stmt_n].res;
	transFuncPara();
	dealIRstmt();//Ƕ�׻�ȥ������������ͨ���
	if (func->name == "main") {//main����ֱ�����������䣬������
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
	if (func->name == "main")Obj.emplace_back("move", "$fp", "$sp", "");//main����������fp���ڳ�ʼsp
	for (int i = 0; i < funcTable->para_n; i++, stmt_n++)
		funcTable->tableList[i]->fp_offset = new int(4 * (funcTable->para_n - i));//Ϊ���ű�Ĳ������ַfp_offset
	
	if (func->name == "main")Offsets.push_back(0);
	else Offsets.push_back(-8);//0Ϊ$ra,-4Ϊoldfp���ֲ�����ƫ�ƽ��ڱ���ʱȷ�����ʿ���������ȫ���Ը��ǣ�����ʱ�����ڴ��϶
}

//para_nΪ��ֵ(-1)��ʾ��������return����ʱҪclearAll����Offsets����ջ��һֱ���䵽������������ŵ�ջ��
void transFuncRet(int para_n) {
	if (IR[stmt_n].res) {//���з���ֵ,
		Reg* reg = getReg(IR[stmt_n].res);
		Obj.emplace_back("move", "$v0", reg->regName, "");//v0�������κ�ֵ��һ�з���ֵ��ȡ��
	}
	if (para_n >= 0) {
		Offsets.pop_back();
		if (!Offsets.empty())
			Offsets.back() += 4 * para_n;//����main��Offsets�ں������忪ͷ�ͷ��ش��ֱ�ѹջ����ջ�������ڱ���ʱ����ȷ����ƫ����
	}

	Obj.emplace_back("move", "$sp", "$fp", "");

	clearAll();//������ĩβ��fp����֮ǰ�ѱ��������ˣ�������Ҫ�Ǵ��������ʱֵ�������ڶ��ս��ˣ�

	Obj.emplace_back("lw", "$fp", "-4($fp)", "");//�ָ�old_fp
	Obj.emplace_back("jr", "$ra", "", "");
	Obj.emplace_back("nop", "", "", "");
	Obj.emplace_back("", "", "", "");
	stmt_n++;

}

//ֻ�����ű��sp�����ڴ棬�����ʼ���ɸ�ֵ��䴦��
void transLocalDef() {
	if (IR[stmt_n].arg1) {//�ǳ�����(����������ֵ�� def initval _ detail
		IR[stmt_n].res->constInt = new int(*IR[stmt_n].arg1->constInt);
	}
	IR[stmt_n].res->fp_offset = new int (Offsets.back());

	int span=4;
	if (IR[stmt_n].res->type == "int"&&IR[stmt_n].res->constInt)//�г�ʼֵ
	{
		Reg* reg=getNewReg(nullptr);
		LDST("as", IR[stmt_n].res, reg);//��������ֵ�Ǳ�����Ҫ����Ĵ�����ֵ��
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

//��Ŀ
void transUnary() {
	string op = IR[stmt_n].op;
	Reg* reg1 = getReg(IR[stmt_n].arg1);
	Reg* resReg = getNewReg(&IR[stmt_n]);
	LDST("cal", IR[stmt_n].res, resReg);
	if (op == "-")
		Obj.emplace_back("sub", resReg->regName, "$0", reg1->regName);
	else if (op == "!")
		Obj.emplace_back("seq", resReg->regName, reg1->regName, "$0");// ע�����߼�ȡ��������0->0;��Ϊ0->1
	
	IR[stmt_n].res->valid = false;//����������Чλ
	stmt_n++;
}

//�Ӽ��˳�%
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
	IR[stmt_n].res->valid = false;//����������Чλ
	stmt_n++;
}

void transAssign() {
	string op = IR[stmt_n].op;
	SymDetail* res = IR[stmt_n].res;
	Reg* reg1 ,*resReg,*reg2;
	if (op == "=") {
		reg1 = getReg(IR[stmt_n].arg1);
		res->regs.clear();//���в��֣���ֵ����ѱ����ļĴ��������
		if (IR[stmt_n].arg1->constInt || IR[stmt_n].arg1->isTemp) {//������ֵ�ǳ�������ʱֵҪ������Ĵ�����
			resReg = getNewReg(nullptr);
			LDST("as", res, resReg);//resReg��ֵ���ֻ��res�������뵽res�ļĴ�����(��ֵһ���Ǳ������Կ������ʱ���)
			Obj.emplace_back("move", resReg->regName, reg1->regName, "");
		}
		else
		{
			reg1->val.push_back(res);//��reg1ֵ�������ֵ
			res->regs.push_back(reg1);//reg1���뵽res�ļĴ�����
		}
	}
	else if (op == "=[]") {//[]=[]�Ȳ�����
		reg1 = getReg(IR[stmt_n].arg1);//�����ַ
		if (IR[stmt_n].arg2->constInt)//ƫ�����ǳ��������
			Obj.emplace_back("add", "$v0", reg1->regName, to_string(*IR[stmt_n].arg2->constInt * -4));
		else {
			reg2 = getReg(IR[stmt_n].arg2);//ƫ����
			Obj.emplace_back("sll", "$v0", reg2->regName, "2");//ƫ��*4(����2λ)��ʹ��v0����ı�ƫ��ֵ���������ֵ����forѭ����i��
			Obj.emplace_back("sub", "$v0", reg1->regName, "$v0");
		}
		resReg = getNewReg(nullptr);
		LDST("cal", res, resReg);
		Obj.emplace_back("lw", resReg->regName, "($v0)", "");//lw $t1 ($reg2+$reg1)
	}
	else if (op == "[]=") {//regres[reg1*4]=reg2
		resReg = getReg(res);//�����ַ
		if (IR[stmt_n].arg1->constInt)//ƫ�����ǳ��������
			Obj.emplace_back("add", "$v0", resReg->regName, to_string(*IR[stmt_n].arg1->constInt * -4));
		else {
			reg1= getReg(IR[stmt_n].arg1);
			Obj.emplace_back("sll", "$v0", reg1->regName, "2");//ƫ��*4(����2λ)
			Obj.emplace_back("sub", "$v0", resReg->regName,"$v0" );//ע��ƫ���Ǹ���
		}
		reg2 = getReg(IR[stmt_n].arg2);//��ֵ
		Obj.emplace_back("sw", reg2->regName, "($v0)", "");
	}
	res->valid = false;//����������Чλ
	stmt_n++;
}


//��ϵ���߼�����
void transRLExpCal() {
	string op = IR[stmt_n].op;
	Reg* reg1=getReg(IR[stmt_n].arg1), * reg2=getReg(IR[stmt_n].arg2);
	Reg* resReg = getNewReg(&IR[stmt_n]);
	LDST("cal", IR[stmt_n].res, resReg);
	if (op == "&&"||op == "||") {//�﷨������int�����߼���򣬱���ת��0|1������mips�İ�λ���
		op = op == "&&" ? "and " : "or";
		string reg1name = reg1->regName, reg2name = reg2->regName;
		if (IR[stmt_n].arg1->constInt || IR[stmt_n].arg1->type == "int") {//ע��4��1��2��v0
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

//�鿴10��֮���Ƿ��õ���������Ծ����������stmt_n+1�����¸���ʼ�����¸���ʹ���Ѿ�ͨ��reg_turn+1���ƽ���ˣ�
#define LOOKAHEAD 10
bool tempToBeUsed(SymDetail* x) {
	for (int i = stmt_n+1;  i < IR.size(); i++) {
		if (IR[i].arg1 == x || IR[i].arg2 == x || IR[i].res == x)//�����м����ʮ�������ǰ��ʹ��
			return true;
	}
	return false;
}

//������һ��������ǰ���κ�һЩ��ʱֵ���ɺ󣬰ѿ��ܱ�ʹ�õ�ȫ����ã�������ǻ�����ĩβ����Ҫ������������������ռĴ�������
void saveTemp(bool nextBlock) {
	for (SymDetail* x : BlockTemp) {
		if (tempToBeUsed(x) && x->isTemp && (x->type == "int" || x->type == "bool") && x->valid == false) {//����ʹ�õ���ʱ����������ʵ�Ρ����ʽ���ӣ�����Ҫ����ռ�
			if (!x->fp_offset) {//���֮ǰû�з��䣬������ַ������ֻ�ô��
				x->fp_offset = new int(Offsets.back());
				Offsets.back() -= 4;
			}
			Obj.emplace_back("add", "$sp", "$fp", to_string(Offsets.back()));
			LDST("st", x, nullptr);
			x->kind = "ldtemp";//��ret����ʾ���ݴ�ķ���ֵ����������Ҫ�������ʱֵ���͸ĳ�ldtemp
			x->regs.back()->val.push_back(x);//��Ĵ�����ֵ���������ʱֵ�����ɼĴ������亯��newReg��������Ƿ�Ҫˢ�µ���ֵ(����tempToBeUsed�ж�)
		}
		if(nextBlock)
			x->regs.clear();//������һ��������ǰ���Ĵ�����������ա�
	}
}

//������ĩβ�ѼĴ���ȫ����ã����㡣jal��j��jr��ret��beqz��goto��Label:
void clearAll() {
	saveTemp(true);//��ʱֵ���뵽�Ĵ���ֵ������������Ȼ�������ֵ�ļĴ�������regs.clear����˳��ߵ��ᵼ�¼Ĵ�����Ϊ��
	BlockTemp.clear();

	for (Reg* R : Regs)
		for (SymDetail* x : R->val) //������ֵ������int����ֵȫ�����µ��ڴ�
		{
			if (x->valid == false && x->type == "int" && (x->fp_offset || x->isGlobal && x->constInt == nullptr)) {//�ֲ����������ǳ�������ʱֵ����ȫ�ֱ���
				LDST("st", x, nullptr);
				x->valid = true;//
			}
			x->regs.clear();//������һ��������ǰ��(��ʱֵ�ͷ���ʱֵ)�Ĵ�����������ա�
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
	else if (op == "beqz") {//����ifFalse�Ͷ�·��ֵ
		Reg* reg1 = getReg(IR[stmt_n].arg1);
		Obj.emplace_back("move", "$v0", reg1->regName,  "");//beqz�Ĵ�һ����ʱֵ
		clearAll();
		Obj.emplace_back("beqz", "$v0", IR[stmt_n].to_label, "");
	}	
	else if (op == "bnez") {//ר���ڶ�·��ֵ
		Reg* reg1 = getReg(IR[stmt_n].arg1);
		Obj.emplace_back("move", "$v0", reg1->regName, "");//bnez�Ĵ�һ����ʱֵ
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
	LDST("cal", IR[stmt_n].res, reg);//��res��ֵ����reg
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
			saveTemp(false);//ר��Ϊ�˱�����ʱ����ֵ,���ǻ�����ĩβ���Բ�����ռĴ������п����ã�
		}
		else if (IR[stmt_n].arg2 == nullptr && (op == "-" || op == "!")){//�м����û��+��ֱ�ӵ���ͬһ��ֵ
			transUnary();
			saveTemp(false);//����10���ڱ��õ���ʱֵ
		}
		else if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
			transExpCal();
			saveTemp(false);//����10���ڱ��õ���ʱֵ
		}
		else if (op == "=" || op == "[]=" || op == "=[]") {
			transAssign();
			saveTemp(false);//����10���ڱ��õ���ʱֵ
		}
			
		else if (op == ">" || op == "<" || op == ">=" || op == "<=" || op == ">" || op == "==" || op == "!=" || op == "&&" || op == "||") {
			transRLExpCal();
			saveTemp(false);//����10���ڱ��õ���ʱֵ
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
		else if (op == "ret")//��������ret
			transFuncRet(-1);
		else if(op=="lastret")//���һ��return�ѿ���Ȩ����transFuncDef
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

	//����lexical
	/*
	for (int i = 0; i < lexOut.size(); i++) {
		dstFile << lexOut[i].first << " " << lexOut[i].second <<" "<<LineCnt[i]<< endl;
	}*/

	getCompUnit(nodeRoot);
	srcFile.close();

	//����semantic
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

	//�����������Ϣ
	for (int i = 0; i < errTXT.size(); i++)
	{
		errFile << errTXT[i].first << " " << errTXT[i].second << endl;
	}
	errFile.close();

	//����﷨��
/*	printTree(treeFile, nodeRoot);
	treeFile.close();*/

	int func_begin = dealGlobalDecl();
	dealFuncDef(func_begin);//�м����

	//����м����
	printIR(IRFile,IR);
	IRFile.close();
	
	//����Ż����м����
	optIR(1);
	printIR(optedIRFile,IR);
	optedIRFile.close();

	
	//����mipsĿ�����
	
	globalAlloc();
	dealIRstmt();

	//Ŀ�����
	for (int i = 0;i<Obj.size(); i++) {
		ObjFile << Obj[i].arg[0] << " " << Obj[i].arg[1] << (Obj[i].arg[2].empty() ? "" : ", " )<< Obj[i].arg[2] << (Obj[i].arg[3].empty() ? "" : ", ") << Obj[i].arg[3] << std::endl;
	}
	ObjFile.close();

}