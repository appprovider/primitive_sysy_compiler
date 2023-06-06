#pragma once
#include<queue>
struct DAGnode {
	DAGnode* left=nullptr, * right=nullptr;
	string cal;

	vector<DAGnode*>father;
	SymDetail* leaf = nullptr;
	int top = 0;//�ýڵ�����⸸�ڵ㣨����Ԫ����
	queue<string> vir_op;//�ýڵ�����⸸�ڵ㣨����Ԫ��������
	bool done = false;
	vector<SymDetail*> syms;//�ڵ��ŵ�sym�б�
	DAGnode() {}
	DAGnode(SymDetail* sym) {//Ҷ�ӽڵ㹹�캯��,������ֵ���м�ڵ�
		leaf = sym;
	}
	DAGnode(DAGnode* left, DAGnode* right, string cal) {//��Ҷ�ӽڵ㹹�캯��
		this->left = left;
		if (left)left->father.push_back(this);
		this->right = right;
		if (right)right->father.push_back(this);
		this->cal = cal;
	}
};

struct DAGtable {
	SymDetail* val;
	DAGnode* node;
	DAGtable(SymDetail* val, DAGnode* node) {
		this->val = val;
		this->node = node;
	}
};

template<typename T> void delVec(vector<T> &vec);
SymDetail* getMajor(DAGnode* s);
