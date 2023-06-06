#pragma once
#include<queue>
struct DAGnode {
	DAGnode* left=nullptr, * right=nullptr;
	string cal;

	vector<DAGnode*>father;
	SymDetail* leaf = nullptr;
	int top = 0;//该节点的虚拟父节点（极大元）数
	queue<string> vir_op;//该节点的虚拟父节点（极大元）操作符
	bool done = false;
	vector<SymDetail*> syms;//节点存放的sym列表
	DAGnode() {}
	DAGnode(SymDetail* sym) {//叶子节点构造函数,包括常值和中间节点
		leaf = sym;
	}
	DAGnode(DAGnode* left, DAGnode* right, string cal) {//非叶子节点构造函数
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
