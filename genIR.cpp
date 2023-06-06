#include<stack>
#include"all.h"
#include"genIR.h"
node* nodeRoot;
extern void getCompUnit(node*& get_node);//vs����all.h���裬�ֶ�����˺�����ֹƵ���﷨����
vector<IRstmt> IR;
vector<IRstmt> DataStr;

//����ast������id��Ӧ�ķ��ű������IR
//IR��Ԫʽ�а������ű�ָ�롣ȫ����������IR�Ŀ�ͷ��


//number��ֵҲҪ�����﷨��,stringֵҲҪ���������+-��
//���һ��������tempName tn

//�����߼����ʽ,��·��ֵ��Ϊ���ڴ���label���ô��﷨����Ϊ�ҵݹ飺LAndExp �� EqExp | EqExp '&&' LAndExp
SymDetail* calLogicExp(node* LogicExp,SymDetail*res) {
	if (LogicExp->boolVal)return new SymDetail(nullptr, LogicExp->boolVal);//��ֵ�������أ������һ�μ���
	//���������˫Ŀ���̳�,�����޷���ֵ���ҽ������̳������޷���ֵ��˫Ŀ����һ���޷���ֵ
	//�߼��������STL��Ĵ���Ϊ0��1��Ȼ��λ������ʵ��
	if (LogicExp->children.size() == 3) {
		string cal = *LogicExp->strVal;//�����֮ǰ���ڼ������ڵ�
		SymDetail* a1, * a2;
		if (LogicExp->Sname == "LAndExp")//a1������EqExp��ΪLAndExp����������LAndExp��
			a1 = calExp(LogicExp->children[0]);
		else a1 = calLogicExp(LogicExp->children[0], res);
		
		if ((a1->constBool && *a1->constBool || a1->constInt && *a1->constInt != 0) && cal == "||") {//�г�ֱֵ�ӷ��ض�·ֵ
			res->constBool = new bool(true);
			return res;
		}
		if ((a1->constBool && !*a1->constBool || a1->constInt && *a1->constInt == 0) && cal == "&&") {
			res->constBool = new bool(false);
			return res;
		}
		string branch = cal == "||" ? "bnez" : "beqz";//�򣺲�Ϊ0��Ϊ��·�� �ң�Ϊ0��Ϊ��·
		IR.emplace_back("=", a1, nullptr, res);
		IR.emplace_back(branch, res, nullptr, nullptr);
		int short_jump = IR.size() - 1;
		a2 = calLogicExp(LogicExp->children[2], res);//a2����ͬ�����ڵ㡣�����ҵݹ�����Ӧ���м����
		IR.emplace_back(cal, a1, a2, res);
		IR[short_jump].to_label = addLabel("short");//�����������
		return res;
	}
	else if (LogicExp->children.size() == 1 && LogicExp->Sname == "LOrExp")
		res = calLogicExp(LogicExp->children[0], res);//�̳�ֱ�ӷ��أ����ز��������м����
	else if (LogicExp->children.size() == 1 && LogicExp->Sname == "LAndExp") 
		res = calExp(LogicExp->children[0]);
	return res;
}

//����ConstExp��Exp���Լ�����XExp
SymDetail* calExp(node* constExp) {
	node* XExp = constExp;
	if (constExp->Sname == "ConstExp"||constExp->Sname=="Exp")
		XExp = constExp->children[0];
	if (XExp->numVal)return new SymDetail(XExp->numVal, nullptr);//��ֵ�������أ������һ�μ���
	if (XExp->boolVal)return new SymDetail(nullptr, XExp->boolVal);
	//�����������Ŀ��˫Ŀ���̳�,�����޷���ֵ���ҽ�������Ŀor�̳������޷���ֵ��˫Ŀ����һ���޷���ֵ
	//�߼��������STL��Ĵ���Ϊ0��1��Ȼ��λ������ʵ��
	SymDetail* res=nullptr;
	if (XExp->children.size() == 3 && XExp->children[0]->Sname!="IDENFR") {
		string cal = *XExp->strVal;//��������ڼ������ڵ�
		SymDetail *a1=calExp(XExp->children[0]), *a2;
		a2 = calExp(XExp->children[2]);
		if (cal == ">" || cal == "<" || cal == ">=" || cal == "<="|| cal == "==" || cal == "!=")//bool->bool��int->bool
			res = new SymDetail(true, "bool");
		else if (cal == "*" || cal == "/" || cal == "+" || cal == "-" || cal == "%")//int->int
			res = new SymDetail(true, "int");
		IR.emplace_back(cal, a1, a2, res);
	}
	else if (XExp->children[0]->Sname == "IDENFR") {//����
		res = dealFuncCall(XExp);
	}
	else if (XExp->children.size() == 2) {
		string cal = *XExp->strVal;
		SymDetail* a1;
		a1 = calExp(XExp->children[1]);
		if (cal == "!") {//!ֻ�����������ʽ�������ڲ���ͬ������û��˫�ط�
			res = new SymDetail(true, "bool");
			IR.emplace_back(cal, a1, nullptr, res);
		}
		else if (cal == "-") {
			bool b1 = XExp->children[1]->children[0]->Sname == "UnaryOp";//-(+)-i,�������� UnaryOp ������ͬ�����Ա�����ǰ��2λ
			b1 = b1 ? XExp->children[1]->children[1]->children[0]->Sname == "UnaryOp" : false;//-+(-)i
			b1 = b1 ? *XExp->children[1]->children[1]->strVal == "-" : false;//-+(-i)ע����Ŵ��ڽ���ڵ㣬Ҳ���������ڵ�
			if (b1)//̫���˲�д,ע���ж�·��ֵ
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
		res= calExp(XExp->children[0]);//�̳�ֱ�ӷ��أ����ز��������м����
	}
	return res;
}



SymDetail* calPrimaryExp(node* primaryExp) {
	SymDetail* res=nullptr;
	if (primaryExp->children.size() == 3) {
		res = calExp(primaryExp->children[1]);
	}
	else if (primaryExp->children[0]->Sname == "LVal") {//��ֵ��Ϊ���ʽ�ͱ���ֵ�ı���Ҫ���֣�������[]=��=[]
		node* LVal = primaryExp->children[0];
		SymDetail *detail = LVal->children[0]->detailPtr;
		string type = detail->type;

		if (type == "int"|| (type == "array1"||type == "array2") && LVal->children.size() == 1)//id,��Ϊ���������������ַ
			res = detail;//���������ַ��Ŀ���������fp_offset+$fpȻ����Ϊ��ַ����ѹջ
		else if (type == "array1") {
			SymDetail* i=calExp(LVal->children[2]); //id[i]
			res = new SymDetail(true, "int");
			if (!detail->constArr.empty() && i->constInt)//�±�ȷ���ĳ�������(�зǿյĳ�ֵ����constArr)����ֱ��ʹ��constArr�ĳ�ֵ
				res->constInt = new int(detail->constArr[*i->constInt]);
			else
				IR.emplace_back("=[]", detail, i, res);
		}
		else if (type == "array2") {
			if (LVal->children.size() == 4)//����Ŀ�����ʱͨ��$arr_offset + detail->fp_offset + $fp ��������Ե�ַ
			{				//����$arr_offset + lw(detail->fp_offset + $fp)���������Ǹ���������Ļ���array��para��
				res = new SymDetail(*detail);//����type��isGlobal����Ϣ
				res->baseArray = detail;//������Ŀ����׶λ�ȡfp_offset���м����׶θ�ƫ���в���ȷ��
				SymDetail* i = calExp(LVal->children[2]); //id[i]   
				SymDetail* arr_offset;
				if (i->constInt)arr_offset = new SymDetail(new int(*i->constInt * (*res->dim2)), nullptr);//ƫ��ֵ�ǳ���
				else {
					arr_offset = new SymDetail(true, "int");
					IR.emplace_back("*", i, new SymDetail(new int(*res->dim2),nullptr), arr_offset);//arr_offset=i*dim2
				}
				res->arr_offset = arr_offset;
			}
			else {//ֱ��ȡ����int ��a[i][j]
				res = new SymDetail(true,"int");
				SymDetail* t2 = calOffset(LVal);
				if (!detail->constArr.empty() && t2->constInt) //�±�ȷ���ĳ�������
					res->constInt = new int(detail->constArr[*t2->constInt]);
				else
					IR.emplace_back("=[]", detail, t2, res);//res=detail[t2] 
			}

		}
	}
	else if (primaryExp->children[0]->Sname == "Number") {
		res = new SymDetail(primaryExp->numVal, nullptr); //����Ԫʽ��ֱ�ӹ��쳣��
	}
	return res;
}

void dealArrayVal(SymDetail* detail,node* InitVal) {
	static int cnt = 0;
	if (InitVal->children.size() == 1)//(Exp|ConstExp)��Ƕ��
	{
		SymDetail* val;
		int* numVal = InitVal->children[0]->numVal;
		if (numVal)
			val = new SymDetail(numVal, nullptr);//(Const)InitVal->(Const)Expȫ��Ϊ����ͬ��int
		else
			val = calExp(InitVal->children[0]);//�ֲ�����Ϊ������ȫ�ֳ�ʼֵҲ�����ǳ��������Ԫ�أ�ͨ�����������constArr��ȡ
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
	if (cnt >= (detail->dim2 ? *detail->dim1 * (*detail->dim2) : *detail->dim1))cnt = 0;//�������ֵ��cnt�����Ա��´���
}

//�����﷨����ʱ���������ֵ��Exp�Ͱ�ֵ����node�����ܣ��������û�������ľ�����IR����ʱֵ
void dealXDecl(node* xDecl,bool isGlobal) {//����ȫ�ֺ;ֲ� ConstDecl | VarDecl
	string type;
	for (int j = 0; j < xDecl->children.size(); j++)//ConstDecl �� 'const' BType ConstDef { ',' ConstDef } ';varͬ��
	{
		if (xDecl->children[j]->Sname != "ConstDef" && xDecl->children[j]->Sname != "VarDef")continue;
		node* def = xDecl->children[j];		//a[2][2]={{1,2},{3,4}}
		SymDetail* detail = def->children[0]->detailPtr;
		type = detail->type;
		detail->isGlobal = isGlobal;
		IR.emplace_back(def->Sname, nullptr, nullptr, detail);

		node* InitVal = def->children.back();//��ֵconstinitval��initval
		if (InitVal->Sname == "ConstInitVal" || InitVal->Sname == "InitVal") {
			if (type == "int") {
				if (InitVal->children[0]->numVal && InitVal->Sname == "ConstInitVal")//ȫ�ֳ�ֵ(Exp|ConstExp)��Ϊ��ֵ,�ֲ�Exp���ܲ��ǳ�ֵ
					IR.back().arg1=new SymDetail(InitVal->children[0]->numVal, nullptr);//����������ֵ def initval _ detail
				else
					IR.emplace_back("=", calExp(InitVal->children[0]), nullptr, detail);//����������ֵ
			}
			else if (type == "array1" || type == "array2") dealArrayVal(detail, InitVal);
		}//����г�ʼ��
	}

}

//��������������������һ������һ��FuncDef��
int dealGlobalDecl() {
	vector<node*> ConstDecl = nodeRoot->children[0]->children;
	int i = 0;
	for (; i < nodeRoot->children.size() && nodeRoot->children[i]->Sname=="Decl"; i++) {
		node* xDecl = nodeRoot->children[i]->children[0];//Decl->ConstDecl | VarDecl
		dealXDecl(xDecl,true);
	}
	return i;
}

//������ͨ������main
void dealFuncDef(int i) {
	for (; i < nodeRoot->children.size(); i++) {
		SymDetail* funcDetail = nodeRoot->children[i]->children[1]->detailPtr;	//FuncDef �� FuncType Ident '(' [FuncFParams] ')' Block
		IR.emplace_back("FuncDef", nullptr, nullptr, funcDetail);
		SymTable* funcTable = funcDetail->childTable;
		for (int j = 0; j < funcTable->para_n; j++) {
			SymDetail* para = funcTable->tableList[j];
			IR.emplace_back("para", nullptr, nullptr, para);
		}
		dealBlock(nodeRoot->children[i]->children.back());
		if(funcDetail->type=="void"&&IR.back().op!="ret")//void����û�е�return��������Ϊmips�ķ��ر��
			IR.emplace_back("lastret", nullptr, nullptr, nullptr);
		IR.back().op = "lastret";//���һ��ret���Ϊlastret
	}
}

SymDetail* dealFuncCall(node* unaryExp) {
	node* FuncRParams = unaryExp->children[2];
	SymDetail* funcDetail = unaryExp->children[0]->detailPtr;
	vector<IRstmt> Rpara;
	for (int i = 0; i < funcDetail->childTable->para_n; i++)
		Rpara.emplace_back("push", nullptr, nullptr, calExp(FuncRParams->children[i*2]));//ʵ��ʹ��push�����β�para����
	for (int i = 0; i < Rpara.size(); i++)
		IR.push_back(Rpara[i]);
	IR.emplace_back("call", nullptr, nullptr, funcDetail);
	
	SymDetail* ret = nullptr;
	if (funcDetail->type != "void") {
		ret= new SymDetail(true,funcDetail->type);
		ret->kind = "ret";//Ŀ�����ʶ�����ڽ���$v0�ķ���ֵ
	}
	IR.back().arg1 = ret;//����Ŀ�������call���ʹ�arg1�õ�����ֵ���Ӷ�����������move $ret(ret�ļĴ���) $v0���
	return ret;//�����ķ���ֵ����ʱֵ������Ϊ��ֵ
}

//�﷨����ֱ��������BlockItem
void dealBlock(node* block) {
	for (int i=0; i < block->children.size(); i++) {
		if (block->children[i]->Sname == "Decl")
			dealXDecl(block->children[i]->children[0], false);//Decl->XDecl
		else if(block->children[i]->Sname == "Stmt")
			dealStmt(block->children[i]);
	}
}

void dealStmt(node* stmt) {
	if (stmt->children[0]->Sname == "LVal") {//��ֵ���
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
	else {//if else while break continue ������
		dealCtrlStream(stmt);
	}
}

string creatLabel(string type) {
	static long long  cnt = 0;
	return "Label" + to_string(cnt++) +"_" + type;
}

//����ָ�����͵ı�ǩ�����뵽��һ�У�����Ψһ��ǩ��
string addLabel(string type) {
	IRstmt* label = new IRstmt("Label:", nullptr, nullptr, nullptr);
	label->label = creatLabel(type);
	IR.push_back(*label);
	return label->label;
}



stack<int> break_to;//����Ҫ�����break����λ�ã�һ��ѭ����һ��-1
stack<string> currentBegin;

//if else while break continue ������
void dealCtrlStream(node* stmt) {
	if (stmt->children[0]->Sname == "IFTK") {
		node* cond = stmt->children[2];
		SymDetail* val_base = new SymDetail(true, "bool");//����Ϊtemp���Ա㱻Ŀ����׶�saveTemp����
		SymDetail* cond_val = calLogicExp(cond->children[0],val_base);
		IR.emplace_back("beqz", cond_val,nullptr, nullptr);
		int ifFalse = IR.size()-1;//������push_back����������ȡ��ַ�ķ�ʽ����ΪvectorԪ�ص�ַ���ƶ��޷���֤��Ч

		dealStmt(stmt->children[4]);
		
		if (stmt->children.size()>5 && stmt->children[5]->Sname == "ELSETK") {
			IR.emplace_back("goto", nullptr, nullptr, nullptr);
			int jump = IR.size() - 1;

			IR[ifFalse].to_label = addLabel("false");//����

			dealStmt(stmt->children[6]);
			IR[jump].to_label = addLabel("goto");//����
		}else
			IR[ifFalse].to_label = addLabel("false");//����
	}
	else if (stmt->children[0]->Sname == "WHILETK") {
		string beginLabel = addLabel("begin");
		currentBegin.push(beginLabel);
		break_to.push(-1);//��һ��-1

		node* cond = stmt->children[2];
		SymDetail* val_base = new SymDetail(true, "bool");//����һ���нӽ������ʱֵ��������ȷĿ����׶ε�ջ�ռ�
		SymDetail* cond_val = calLogicExp(cond->children[0], val_base);
		IR.emplace_back("beqz", cond_val,nullptr, nullptr);
		int ifFalse = IR.size() - 1;

		dealStmt(stmt->children[4]);

		IR.emplace_back("goto", nullptr, nullptr, nullptr);
		int jump = IR.size() - 1;
		IR[jump].to_label = beginLabel;

		string endLabel= addLabel("end");
		IR[ifFalse].to_label = endLabel;//����
		currentBegin.pop();
		int goto_end=break_to.top();
		break_to.pop();
		while (goto_end!=-1) {	//�Ըò������break����
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

//�����ά����ƫ����
SymDetail* calOffset(node* LVal) {
	SymDetail* detail = LVal->children[0]->detailPtr;
	SymDetail* dim2_sym = new SymDetail(detail->dim2, nullptr);
	SymDetail* i = calExp(LVal->children[2]); //detail[i][j]
	SymDetail* t1 = new SymDetail(true, "int");
	IR.emplace_back("*", dim2_sym, i, t1);//dim2*i=t1
	SymDetail* j = calExp(LVal->children[5]);
	if (i->constInt && j->constInt) {
		IR.pop_back();//���i��j�ǳ�������ʡ��IR���������±곣ֵ
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
		if (pos1 != pos2)//����%d���ڣ���%d�ڿ�ͷ�����Ӵ��ǿմ�ʱ���Ͳ�������Ӵ�
		{
			string sub = format.substr(pos1, pos2 - pos1);
			string strName = "str_" + to_string(strCnt++);
			DataStr.emplace_back(strName, nullptr, nullptr, new SymDetail(sub));
			printArg.emplace_back("printf", nullptr, nullptr, new SymDetail(strName));//strName����ʽ��ӡ��IR����Ϊ��ʶ�����ݴ��ҵ���Ӧ��format
		}
		printArg.emplace_back("printf", nullptr, nullptr, calExp(Exp));
		pos1 = pos2 + 2;//����һ��d ��1%d2
	}
	if (pos1 < format.size()) {//û�б�ʶ���������һ���Ӵ�
		string sub = format.substr(pos1, format.size() - pos1);
		string strName = "str_" + to_string(strCnt++);
		DataStr.emplace_back(strName, nullptr, nullptr, new SymDetail(sub));
		printArg.emplace_back("printf", nullptr, nullptr, new SymDetail(strName));
	}
	for (int i = 0; i < printArg.size(); i++)
		IR.push_back(printArg[i]);//�ȼ�����ʽ�������к������������������
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