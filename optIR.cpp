#include"all.h"
#include"optIR.h"
//��ȫ������ʼ����ϣ���һ���������忪ʼ�Ż���ÿ������������ÿ�����������Ż���
extern vector<IRstmt> IR;//��ʼ��ʽ
vector<IRstmt> SubExpKilledIR;//���������ӱ��ʽ���Ż���ʽ
vector<IRstmt> optedIR;//�����Ż���ʽ
vector<IRstmt> pushes;//ѹջ��IR���
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

	SymDetail* new_sym = new SymDetail(ci, cb);//û�ҵ����½�
	DAGnode* new_node = new DAGnode(new_sym);
	DAGlist.push_back(new DAGtable( new_sym, new_node ));
	return new_node;
}

set<string> maximal = { "printf","push","=[]","[]="};//���ɸ��õļ���Ԫ
set<string> unson = { "printf","push","[]=" };//���������ӽڵ�ļ���Ԫ��=[]����ʱֵ��֤��Ψһ�ԣ�����

//Ѱ��һ��sym�����˳�������
DAGnode* getNode(SymDetail* arg) {
	for (int i = 0; i < DAGlist.size(); i++) {
		if (unson.count(DAGlist[i]->node->cal) > 0)continue;//����Ԫ������Ϊ�ӽڵ�
		if (DAGlist[i]->val==arg)
			return DAGlist[i]->node;
	}
	//û�ҵ����½�,����para��var��getint��temp��ret��temp������sym
	DAGnode* new_node = new DAGnode(arg);
	DAGlist.push_back(new DAGtable(arg, new_node));
	return new_node;
}

//ʵ������//syms.erase(remove(syms.begin(), syms.end(), res), syms.end())��Ч��
//removeɾȥn����һ����ĩβ������//��sizeδ�ı�,Ҫ��erase�ѷ�����ĩβɾ��
template<typename T>
inline void my_erase(vector<T> &vec,T res) {
	for (int j = 0; j < vec.size(); j++)
		if (vec[j] == res)
			vec.erase(vec.begin() + j--);//eraseɾ��Ԫ�أ�����ǰ��
}

//���ýڵ��Ƿ�ʹ�ù����������д��ж�ֱ�ӱ��ݣ������������ڵ㵼������
inline bool node_used(DAGnode* n) {
	for (int i = 0; i < DAGlist.size(); i++) {
		if (DAGlist[i]->node->left == n)
			return true;
		if (DAGlist[i]->node->right == n)
			return true;
		if (DAGlist[i]->val == nullptr && DAGlist[i]->node == n)//printf��push�����⼫��Ԫ
			return true;
	}
	return false;
}

//�ܴ���һ�����㡢��ֵ���������㼫��Ԫ
void buildOp() {
	SymDetail* arg1 = IR[stmt_n].arg1;
	SymDetail* arg2 = IR[stmt_n].arg2;
	SymDetail* res = IR[stmt_n].res;
	DAGnode* node1 = nullptr, * node2 = nullptr;
	if (arg1->constInt || arg1->constBool)node1 = getConstNode(arg1);
	else node1 = getNode(arg1);
	if (arg2){//��Ϊ��Ŀ��������ü���arg2
		if (arg2->constInt || arg2->constBool)node2 = getConstNode(arg2);
		else node2 = getNode(arg2);
	}
	DAGnode* res_node = nullptr;
	for (int i = 0; i < DAGlist.size(); i++) {//Ѱ�����޿ɸ��ã����Ҳ�����������������ͬ���Ľڵ�
		if (maximal.count(DAGlist[i]->node->cal) > 0)continue;//[]=��res�����鼫��Ԫ�����ã�=[]��ɱ��,����������buildOp
		DAGnode* n = DAGlist[i]->node;
		if (n->cal == IR[stmt_n].op && n->left == node1 && n->right == node2) {//��Ϊ��Ŀleft��node1��null������Ҳ����
			res_node = n;
			break;
		}
	}
	//if (IR[stmt_n].op == "=" || (!arg2 && IR[stmt_n].op == "+")) //��ֵ�͵�Ŀ+��������ֱ�Ӹ���node1
	//	res_node = node1;
	if (!res_node)
		res_node = new DAGnode(node1, node2, IR[stmt_n].op);//û�о��½����������ҵ�father�������resnode

	if(res->type == "array1" || res->type == "array2")//[]=��res������ֱ�Ӵ�������Ԫ��=[]����ֵ����������������
		DAGlist.push_back(new DAGtable(res, res_node));//���´���[sym,node]
	else {
		bool reAssigned=false;
		for (int i = 0; i < DAGlist.size(); i++) 
			if (res == DAGlist[i]->val) {//�Ƿ��Ѿ�����[sym,node]������ֻ�����node��
				vector<SymDetail*>& syms = DAGlist[i]->node->syms;//��ȡsymԭ��node��syms�б�		
				my_erase<SymDetail*>(syms, res);//��syms�б������sym
				if (syms.empty()) { //����ڵ����ֵΪ�գ��ҽڵ��ѱ�ʹ�ã������������ʱֵ����
					if (node_used(DAGlist[i]->node)) {
						SymDetail* ttemp = new SymDetail(true, res->type);
						syms.push_back(ttemp);//Ϊ�˴�n_queue���������뱣֤�ڵ�Ĵ���ֵ��Ϊ��
						DAGlist.insert(DAGlist.begin() + i, new DAGtable(ttemp, DAGlist[i]->node));//�ڵ�ǰ[sym,node]������[ttemp,node]
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
		if (!reAssigned) DAGlist.push_back(new DAGtable(res, res_node));//�����״θ�ֵ��δ����table,���½�һ��
	}
	res_node->syms.push_back(res);//��sym���뵽�µ�node��syms�б�
	stmt_n++;
}

//����print��push��������⼫��Ԫ
void buildVirtualTop() {
	SymDetail* res = IR[stmt_n].res;
	DAGnode* node = nullptr;
	if (!res->constString.empty()) node = new DAGnode(res);//printf�����ж��ַ������ַ���������Ψһ�ģ��ұ�ΪҶ�ڵ�
	else if (res->constInt || res->constBool)node = getConstNode(res);
	else node = getNode(res);//��Ҷ�ڵ�syms�ز�Ϊ��(ֵ����temp����֤)��Ҷ�ڵ�����leaf������sym
	node->vir_op.push(IR[stmt_n].op);//���⼫��Ԫ�Ĳ�����������vir_op
	node->top++;//Ϊ�˱��򣬵���DAG��ݹ�ʱ��top>0��ֹͣ������DAGListҲҪ�ȴ�����printf����Ԫ(top�ݼ���0)�ſɴ���node����
	DAGlist.push_back(new DAGtable(nullptr, node));//���⼫��ԪsymΪ�ա�����n_queueʱ���û�ж�Ӧ��sym��Ϊmajor��ֱ��
	stmt_n++;
}

void buildDAG() {
	set<string> BinOp = { "||","&&","==","!=",">","<",">=","<=","+","-","*","/","%" };//�߼���·��ֵ����branch���������ؿ��Ǳ���
	set<string> MonOp = { "+","-","!" };
	set<string> EndOp = { "beqz","bnez","goto","ret","lastret" };
	while (stmt_n < IR.size()) {
		//��ʼ���ţ��հ�
		if (IR[stmt_n].op == "FuncDef")
			SubExpKilledIR.push_back(IR[stmt_n++]);
		//��Ҫ����IR�Ľڵ㣨��Ҫ�����Ҷ�ڵ�getint��called��
		else if (IR[stmt_n].op == "para")
			SubExpKilledIR.push_back(IR[stmt_n++]);
		else if (IR[stmt_n].op == "VarDef")
			SubExpKilledIR.push_back(IR[stmt_n++]);
		else if (IR[stmt_n].op == "ConstDef")
			SubExpKilledIR.push_back(IR[stmt_n++]);
		else if (IR[stmt_n].op == "getint")//�൱�ڻ������ڣ������Ϊ�첽��ǰ�������ܱ���
			SubExpKilledIR.push_back(IR[stmt_n++]);
		else if (IR[stmt_n].op == "called") {//��Ϊ����ֵ�ı�ǣ�����IR֮ǰ��ԭ
			IR[stmt_n].op = "call";
			SubExpKilledIR.push_back(IR[stmt_n++]);
		}

		//һ��ڵ�
		else if (BinOp.count(IR[stmt_n].op) > 0)
			buildOp();
		else if (MonOp.count(IR[stmt_n].op) > 0)
			buildOp();
		else if (IR[stmt_n].op == "=")//��ֵ=
			buildOp();
		//����ڵ�(DAG����Ԫ)
		else if (IR[stmt_n].op == "printf")
			buildVirtualTop();
		else if (IR[stmt_n].op == "push")
			buildVirtualTop();
		else if (IR[stmt_n].op == "[]=")//������Ϊ����Ԫ
			buildOp();
		else if (IR[stmt_n].op == "=[]")//��ʱ�ᱻɱ����Ҳ��Ϊ����Ԫ
			buildOp();
		//�������,���������ɵ�IR������ĩβ
		else if (EndOp.count(IR[stmt_n].op) > 0)
			break;
		else if (!IR[stmt_n].label.empty())
			break;
		else if (IR[stmt_n].op == "call") {//����ed��Ϊ����ֵ�ı��,
			IR[stmt_n].op = "called";//����Ψһ�����⣬��ʱ������û�н������,����Ϊ��һ��������Ŀ�ͷ
			break;
		}
	}
}

//���ĳ�����ĵ�ǰֵ��node���Ƿ񻹵��ڳ�ʼֵ��initial_node��
inline bool reDefVar(DAGnode* initial_node) {
	if (initial_node->leaf->type != "int")return false;//ֻ����int�������ض���������
	for (int i = 0; i < DAGlist.size(); i++) {
		if (DAGlist[i]->node->done == true)continue;//�Ѿ�����Ĳ�����
		if (DAGlist[i]->val == initial_node->leaf && DAGlist[i]->node != initial_node)
			return true;//ֵ��leaf��ͬ����node���ǳ�ʼ�ı�����para
	}
	return false;
}

//ֻҪfatherΪ���ҷ�Ҷ�ڵ㣬��δ�����ʣ��ͼ�������
inline void iterLeft(DAGnode* n) {
	for (DAGnode* i=n; i->father.empty() && !i->leaf && !i->done; i = i->left) {
		if (i->top > 0)break;//���⼫��Ԫδ�����꣬�ýڵ㲻�ɶ�

		if (i->left&&i->left->leaf && reDefVar(i->left)) break;//����Ǳ����¸�ֵ��var��para����ֱ��var�Ľڵ�
		if (i->right&&i->right->leaf && reDefVar(i->right)) break;
		
		if (i->left)my_erase<DAGnode*>(i->left->father, i);
		if (i->right)my_erase<DAGnode*>(i->right->father, i);

		i->done = true;
		n_queue.push_back(i);
	}
}

//���뷴�����DAGlist��ѹ���stack��pop�������Ǳ����
void exportDAG() {
	bool finished ;
	do{
		finished = true;
		bool keep_order = false;
		for (int i = DAGlist.size() - 1; i >= 0; i--) {
			DAGnode* n = DAGlist[i]->node;
			if (DAGlist[i]->val && n->top > 0) 
				continue;//���⼫��Ԫδ������(�ڵ��Ǳ��ض���ı�����ʼֵ��reDefVar)��������ڵ㲻�ɶ�
			if (!DAGlist[i]->val && n->top > 0) {//�������⼫��Ԫ
				if (keep_order == true)continue;//�����ٴ����������⼫��Ԫ
				if (n->leaf && reDefVar(n)) {//���ض���ı�����ʼֵҲ������
					keep_order = true;//Ϊ�˱��򣬱��˲����ٴ����������⼫��Ԫ
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
	} while (!finished);//ֱ��DAGlist����Ԫ��״̬���ٷ����ı䣬���㴦����

	DAGnode* s = nullptr;
	while (!n_queue.empty()) {
		s = n_queue.back();
		n_queue.pop_back();
		string op = s->cal;
		if (!s->vir_op.empty()) {//����ýڵ�������⼫��Ԫ
			if (s->cal != "")s->cal = "";//�״����������������ֵ
			else {//������⼫��Ԫ����n_queue��[...prin prin caln...]���caln֮���ٴ�ӡ��
				op = s->vir_op.front();//ע��vir_op��n_queue���򣬺�ԭIRͬ��
				s->vir_op.pop();
				SymDetail* res = getMajor(s);//����printf(str)��������leaf����һ���м�����syms.front
				if (op == "push")//����ǰ��IR��ƣ�push�������call������ǰ����������pushes����IR
					pushes.emplace_back(op, nullptr, nullptr, res);
				else
					SubExpKilledIR.emplace_back(op, nullptr, nullptr, res);
				continue;
			}
		}
		SymDetail* left = getMajor(s->left);
		SymDetail* right = getMajor(s->right);
		for (int i = 0; i < s->syms.size(); i++) {//һ��node�ϵ�����symȫ������
			SymDetail* res = s->syms[i];
			if (i == 0)SubExpKilledIR.emplace_back(op, left, right, res);//��һ���������㣬�����Ķ����Ƶ�һ��
			else SubExpKilledIR.emplace_back("=", s->syms.front(), nullptr, res);
		}
	}
	if (IR[stmt_n].op != "called")
		SubExpKilledIR.push_back(IR[stmt_n++]);//�������,���ڻ�����ĩβ(called����)
	else{
		SubExpKilledIR.insert(SubExpKilledIR.end(), pushes.begin(), pushes.end());
		pushes.clear();
	}

	delVec<DAGtable*>(DAGlist);//��ո���DAG��Ͷ���
	delVec<DAGnode*>(n_queue);
}

template<typename T> void delVec(vector<T> &vec) {
	while (!vec.empty()) {
		T ptr = vec.back();
		delete ptr;
		ptr = nullptr;//һ���������в�����DAGtable��DAGnode������ʹ�ã��ÿա�
		vec.pop_back();
	}
}

SymDetail* getMajor(DAGnode* s) {
	if (!s)return nullptr;
	if (s->leaf)return s->leaf;//��leaf�ͷ���leaf
	return s->syms.front();//����ֱ�ӷ���node��syms���׸�
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
	optedIR(0)����>optedIR(1)����>optedIR(2)����>	����
	��					��				��
	IR--------->SubExpKilledIR------>otherIR----> ���� 
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
		//label:		##label���
		if (IR[i].label != "")
			IRFile << IR[i].label + ": " << std::endl;
		//beqz (arg1) label		#��ת������beqz bnez goto
		else if (IR[i].to_label != "") {
			IRFile << IR[i].op + ", ";
			if (IR[i].op == "beqz" || IR[i].op == "bnez")IRFile << IR[i].arg1->name + ",";
			IRFile << IR[i].to_label << std::endl;
		}
		//�����(push|printf) _ _ res , (ret|lastret) _ _ (res) , op�Ľ��
		//����or������ (vardef|constdef) _ _ res, (getint|para) _ _ res , call (ret) _ funcDetail
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