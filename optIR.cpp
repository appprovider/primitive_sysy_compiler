#include"all.h"
#include"optIR.h"
//从全局量初始化完毕，第一个函数定义开始优化。每个函数、其中每个基本块内优化。
extern vector<IRstmt> IR;//初始形式
vector<IRstmt> SubExpKilledIR;//消除公共子表达式的优化形式
vector<IRstmt> optedIR;//最终优化形式
vector<IRstmt> pushes;//压栈的IR语句
int stmt_n = 0;

vector<DAGtable*> DAGlist;
vector<DAGnode*> n_queue;

DAGnode* getConstNode(SymDetail* arg) {
	int* ci = arg->constInt;
	bool* cb = arg->constBool;
	SymDetail* leaf;
	for (int i = 0; i < DAGlist.size(); i++) {
		leaf = DAGlist[i]->node->leaf;
		if (!leaf)continue;
		if (!leaf->constInt && !leaf->constBool)continue;
		if (ci && leaf->constInt && *ci == *leaf->constInt)
			return DAGlist[i]->node;
		if (cb && leaf->constBool && *cb == *leaf->constBool)
			return DAGlist[i]->node;
	}

	SymDetail* new_sym = new SymDetail(ci, cb);//没找到就新建
	DAGnode* new_node = new DAGnode(new_sym);
	DAGlist.push_back(new DAGtable( new_sym, new_node ));
	return new_node;
}

set<string> maximal = { "printf","push","=[]","[]="};//不可复用的极大元
set<string> unson = { "printf","push","[]=" };//不可用作子节点的极大元。=[]是临时值保证了唯一性，可用

//寻找一切sym，除了常量常数
DAGnode* getNode(SymDetail* arg) {
	for (int i = 0; i < DAGlist.size(); i++) {
		if (unson.count(DAGlist[i]->node->cal) > 0)continue;//极大元不能作为子节点
		if (DAGlist[i]->val==arg)
			return DAGlist[i]->node;
	}
	//没找到就新建,包括para、var、getint的temp、ret的temp等所有sym
	DAGnode* new_node = new DAGnode(arg);
	DAGlist.push_back(new DAGtable(arg, new_node));
	return new_node;
}

//实现类似//syms.erase(remove(syms.begin(), syms.end(), res), syms.end())的效果
//remove删去n返回一个新末尾迭代器//但size未改变,要用erase把废弃的末尾删掉
template<typename T>
inline void my_erase(vector<T> &vec,T res) {
	for (int j = 0; j < vec.size(); j++)
		if (vec[j] == res)
			vec.erase(vec.begin() + j--);//erase删除元素，后面前移
}

//检查该节点是否被使用过，若不进行此判断直接备份，可能增加死节点导致锁死
inline bool node_used(DAGnode* n) {
	for (int i = 0; i < DAGlist.size(); i++) {
		if (DAGlist[i]->node->left == n)
			return true;
		if (DAGlist[i]->node->right == n)
			return true;
		if (DAGlist[i]->val == nullptr && DAGlist[i]->node == n)//printf、push等虚拟极大元
			return true;
	}
	return false;
}

//能处理一般运算、赋值、数组运算极大元
void buildOp() {
	SymDetail* arg1 = IR[stmt_n].arg1;
	SymDetail* arg2 = IR[stmt_n].arg2;
	SymDetail* res = IR[stmt_n].res;
	DAGnode* node1 = nullptr, * node2 = nullptr;
	if (arg1->constInt || arg1->constBool)node1 = getConstNode(arg1);
	else node1 = getNode(arg1);
	if (arg2){//若为单目运算符则不用计算arg2
		if (arg2->constInt || arg2->constBool)node2 = getConstNode(arg2);
		else node2 = getNode(arg2);
	}
	DAGnode* res_node = nullptr;
	for (int i = 0; i < DAGlist.size(); i++) {//寻找有无可复用（左右操作数、操作符均相同）的节点
		if (maximal.count(DAGlist[i]->node->cal) > 0)continue;//[]=的res是数组极大元不可用，=[]被杀死,其它不经过buildOp
		DAGnode* n = DAGlist[i]->node;
		if (n->cal == IR[stmt_n].op && n->left == node1 && n->right == node2) {//若为单目left和node1都null，所以也适用
			res_node = n;
			break;
		}
	}
	//if (IR[stmt_n].op == "=" || (!arg2 && IR[stmt_n].op == "+")) //赋值和单目+是特例可直接复用node1
	//	res_node = node1;
	if (!res_node)
		res_node = new DAGnode(node1, node2, IR[stmt_n].op);//没有就新建，并向左右的father集里添加resnode

	if(res->type == "array1" || res->type == "array2")//[]=的res是数组直接创建极大元，=[]的左值不是数组正常更新
		DAGlist.push_back(new DAGtable(res, res_node));//重新创建[sym,node]
	else {
		bool reAssigned=false;
		for (int i = 0; i < DAGlist.size(); i++) 
			if (res == DAGlist[i]->val) {//是否已经创建[sym,node]，若是只需更新node。
				vector<SymDetail*>& syms = DAGlist[i]->node->syms;//获取sym原来node的syms列表		
				my_erase<SymDetail*>(syms, res);//从syms列表中清除sym
				if (syms.empty()) { //如果节点代表值为空，且节点已被使用，则必须新增临时值备份
					if (node_used(DAGlist[i]->node)) {
						SymDetail* ttemp = new SymDetail(true, res->type);
						syms.push_back(ttemp);//为了从n_queue导出，必须保证节点的代表值不为空
						DAGlist.insert(DAGlist.begin() + i, new DAGtable(ttemp, DAGlist[i]->node));//在当前[sym,node]处插入[ttemp,node]
						i++;
						//DAGlist.push_back( new DAGtable(ttemp, DAGlist[i]->node));
					}
					else {
						DAGnode* old_n = DAGlist[i]->node;
						if (old_n->left)my_erase<DAGnode*>(old_n->left->father, old_n);
						if (old_n->right)my_erase<DAGnode*>(old_n->right->father, old_n);
					}
				}
				DAGlist[i]->node = res_node;
				reAssigned = true;
				break;
			}
		if (!reAssigned) DAGlist.push_back(new DAGtable(res, res_node));//该量首次赋值尚未创建table,则新建一个
	}
	res_node->syms.push_back(res);//将sym加入到新的node的syms列表
	stmt_n++;
}

//处理print、push这类的虚拟极大元
void buildVirtualTop() {
	SymDetail* res = IR[stmt_n].res;
	DAGnode* node = nullptr;
	if (!res->constString.empty()) node = new DAGnode(res);//printf还需判断字符串。字符串名都是唯一的，且必为叶节点
	else if (res->constInt || res->constBool)node = getConstNode(res);
	else node = getNode(res);//非叶节点syms必不为空(值必有temp，易证)，叶节点则在leaf保留了sym
	node->vir_op.push(IR[stmt_n].op);//虚拟极大元的操作符保存在vir_op
	node->top++;//为了保序，导出DAG左递归时，top>0则停止。遍历DAGList也要先处理完printf极大元(top递减到0)才可处理node本身
	DAGlist.push_back(new DAGtable(nullptr, node));//虚拟极大元sym为空。导出n_queue时如果没有对应的sym作为major，直接
	stmt_n++;
}

void buildDAG() {
	set<string> BinOp = { "||","&&","==","!=",">","<",">=","<=","+","-","*","/","%" };//逻辑短路求值会用branch隔开，不必考虑保序
	set<string> MonOp = { "+","-","!" };
	set<string> EndOp = { "beqz","bnez","goto","ret","lastret" };
	while (stmt_n < IR.size()) {
		//开始符号，照搬
		if (IR[stmt_n].op == "FuncDef")
			SubExpKilledIR.push_back(IR[stmt_n++]);
		//需要加入IR的节点（需要保序的叶节点getint、called）
		else if (IR[stmt_n].op == "para")
			SubExpKilledIR.push_back(IR[stmt_n++]);
		else if (IR[stmt_n].op == "VarDef")
			SubExpKilledIR.push_back(IR[stmt_n++]);
		else if (IR[stmt_n].op == "ConstDef")
			SubExpKilledIR.push_back(IR[stmt_n++]);
		else if (IR[stmt_n].op == "getint")//相当于基本块内，输入改为异步提前。但是能保序
			SubExpKilledIR.push_back(IR[stmt_n++]);
		else if (IR[stmt_n].op == "called") {//作为返回值的标记，加入IR之前还原
			IR[stmt_n].op = "call";
			SubExpKilledIR.push_back(IR[stmt_n++]);
		}

		//一般节点
		else if (BinOp.count(IR[stmt_n].op) > 0)
			buildOp();
		else if (MonOp.count(IR[stmt_n].op) > 0)
			buildOp();
		else if (IR[stmt_n].op == "=")//赋值=
			buildOp();
		//保序节点(DAG极大元)
		else if (IR[stmt_n].op == "printf")
			buildVirtualTop();
		else if (IR[stmt_n].op == "push")
			buildVirtualTop();
		else if (IR[stmt_n].op == "[]=")//保序，设为极大元
			buildOp();
		else if (IR[stmt_n].op == "=[]")//随时会被杀死，也设为极大元
			buildOp();
		//结束语句,最后加在生成的IR基本块末尾
		else if (EndOp.count(IR[stmt_n].op) > 0)
			break;
		else if (!IR[stmt_n].label.empty())
			break;
		else if (IR[stmt_n].op == "call") {//增加ed作为返回值的标记,
			IR[stmt_n].op = "called";//这是唯一的例外，此时基本块没有结束语句,而作为下一个基本块的开头
			break;
		}
	}
}

//检查某个量的当前值（node）是否还等于初始值（initial_node）
inline bool reDefVar(DAGnode* initial_node) {
	if (initial_node->leaf->type != "int")return false;//只考虑int，数组重定义无意义
	for (int i = 0; i < DAGlist.size(); i++) {
		if (DAGlist[i]->node->done == true)continue;//已经处理的不考虑
		if (DAGlist[i]->val == initial_node->leaf && DAGlist[i]->node != initial_node)
			return true;//值和leaf相同，但node不是初始的变量、para
	}
	return false;
}

//只要father为空且非叶节点，且未曾访问，就继续遍历
inline void iterLeft(DAGnode* n) {
	for (DAGnode* i=n; i->father.empty() && !i->leaf && !i->done; i = i->left) {
		if (i->top > 0)break;//虚拟极大元未处理完，该节点不可动

		if (i->left&&i->left->leaf && reDefVar(i->left)) break;//如果是被重新赋值的var、para，则直到var的节点
		if (i->right&&i->right->leaf && reDefVar(i->right)) break;
		
		if (i->left)my_erase<DAGnode*>(i->left->father, i);
		if (i->right)my_erase<DAGnode*>(i->right->father, i);

		i->done = true;
		n_queue.push_back(i);
	}
}

//必须反向遍历DAGlist，压入的stack再pop出来才是保序的
void exportDAG() {
	bool finished ;
	do{
		finished = true;
		bool keep_order = false;
		for (int i = DAGlist.size() - 1; i >= 0; i--) {
			DAGnode* n = DAGlist[i]->node;
			if (DAGlist[i]->val && n->top > 0) 
				continue;//虚拟极大元未处理完(节点是被重定义的变量初始值：reDefVar)，该虚拟节点不可动
			if (!DAGlist[i]->val && n->top > 0) {//处理虚拟极大元
				if (keep_order == true)continue;//不可再处理其它虚拟极大元
				if (n->leaf && reDefVar(n)) {//被重定义的变量初始值也需跳过
					keep_order = true;//为了保序，本趟不可再处理其它虚拟极大元
					//keep_order = n->vir_op[n->top];
					continue;
				}
				n->top--;
				//n->done = true;
				n_queue.push_back(n);
				finished = false;
			}
			if (n->father.empty() && !n->leaf && !n->done) {
				iterLeft(n);
				finished = false;
			}
		}
	} while (!finished);//直到DAGlist所有元素状态不再发生改变，才算处理完

	DAGnode* s = nullptr;
	while (!n_queue.empty()) {
		s = n_queue.back();
		n_queue.pop_back();
		string op = s->cal;
		if (!s->vir_op.empty()) {//如果该节点存在虚拟极大元
			if (s->cal != "")s->cal = "";//首次遇到，先输出基本值
			else {//输出虚拟极大元。如n_queue中[...prin prin caln...]输出caln之后，再打印。
				op = s->vir_op.front();//注意vir_op和n_queue反序，和原IR同序
				s->vir_op.pop();
				SymDetail* res = getMajor(s);//对于printf(str)、常数等leaf；对一般中间量是syms.front
				if (op == "push")//按当前的IR设计，push必须紧邻call放在其前，所以最后从pushes导入IR
					pushes.emplace_back(op, nullptr, nullptr, res);
				else
					SubExpKilledIR.emplace_back(op, nullptr, nullptr, res);
				continue;
			}
		}
		SymDetail* left = getMajor(s->left);
		SymDetail* right = getMajor(s->right);
		for (int i = 0; i < s->syms.size(); i++) {//一个node上的所有sym全部导出
			SymDetail* res = s->syms[i];
			if (i == 0)SubExpKilledIR.emplace_back(op, left, right, res);//第一个正常计算，其它的都复制第一个
			else SubExpKilledIR.emplace_back("=", s->syms.front(), nullptr, res);
		}
	}
	if (IR[stmt_n].op != "called")
		SubExpKilledIR.push_back(IR[stmt_n++]);//结束语句,加在基本块末尾(called例外)
	else{
		SubExpKilledIR.insert(SubExpKilledIR.end(), pushes.begin(), pushes.end());
		pushes.clear();
	}

	delVec<DAGtable*>(DAGlist);//清空辅助DAG表和队列
	delVec<DAGnode*>(n_queue);
}

template<typename T> void delVec(vector<T> &vec) {
	while (!vec.empty()) {
		T ptr = vec.back();
		delete ptr;
		ptr = nullptr;//一个基本块中产生的DAGtable和DAGnode不会再使用，置空。
		vec.pop_back();
	}
}

SymDetail* getMajor(DAGnode* s) {
	if (!s)return nullptr;
	if (s->leaf)return s->leaf;//有leaf就返回leaf
	return s->syms.front();//否则直接返回node的syms的首个
}

void optSubExp() {
	for (; IR[stmt_n].op != "FuncDef"; stmt_n++)
		SubExpKilledIR.push_back(IR[stmt_n]);
	while (stmt_n < IR.size()) {
		buildDAG();
		exportDAG();
	}		
}
/*
	optedIR(0)……>optedIR(1)……>optedIR(2)……>	……
	↑					↑				↑
	IR--------->SubExpKilledIR------>otherIR----> …… 
*/
void optIR(int level) {
	switch (level)
	{
	case 0:
		break;
	case 1:
		optSubExp();
		stmt_n = 0;
		IR.assign(SubExpKilledIR.begin(), SubExpKilledIR.end());
		break;
	default:
		break;
	}
}

void printIR(ofstream& IRFile, vector<IRstmt>& IR) {
	for (int i = 0; i < IR.size(); i++) {
		//label:		##label标记
		if (IR[i].label != "")
			IRFile << IR[i].label + ": " << std::endl;
		//beqz (arg1) label		#跳转。包括beqz bnez goto
		else if (IR[i].to_label != "") {
			IRFile << IR[i].op + ", ";
			if (IR[i].op == "beqz" || IR[i].op == "bnez")IRFile << IR[i].arg1->name + ",";
			IRFile << IR[i].to_label << std::endl;
		}
		//输出：(push|printf) _ _ res , (ret|lastret) _ _ (res) , op的结果
		//输入or产生： (vardef|constdef) _ _ res, (getint|para) _ _ res , call (ret) _ funcDetail
		else {
			IRFile << IR[i].op + ", ";
			if (IR[i].arg1) {
				if (!IR[i].arg1->name.empty())IRFile << IR[i].arg1->name + ", ";
				if (IR[i].arg1->constString != "")IRFile << IR[i].arg1->constString;
				if (IR[i].arg1->constInt)IRFile << to_string(*IR[i].arg1->constInt) + ", ";
				if (IR[i].arg1->constBool)IRFile << to_string(*IR[i].arg1->constBool) + ", ";
			}
			if (IR[i].arg2) {
				if (IR[i].arg2->name != "")IRFile << IR[i].arg2->name + ", ";
				if (IR[i].arg2->constInt)IRFile << to_string(*IR[i].arg2->constInt) + ", ";
				if (IR[i].arg2->constBool)IRFile << to_string(*IR[i].arg2->constBool) + ", ";
			}
			if (IR[i].res) {
				if (IR[i].res->name != "")IRFile << IR[i].res->name + ", ";
				if (IR[i].res->constString != "")IRFile << IR[i].res->constString + ", ";
				if (IR[i].res->constInt)IRFile << to_string(*IR[i].res->constInt) + ", ";
				if (IR[i].res->constBool)IRFile << to_string(*IR[i].res->constBool) + ", ";
			}
			IRFile << std::endl;
		}
	}
}