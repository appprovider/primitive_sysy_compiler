#pragma once
#include<iostream>
#include<fstream>
#include<string>
#include<unordered_map>
#include<vector>
#include<set>
using namespace std;

//声明上一个模块的输出，和最外层的根函数
extern vector<pair<string, string> >lexOut;
extern vector<int> LineCnt;
extern void lexical(ifstream& srcFile);

extern vector<pair<string, string> >outTXT;
extern vector<pair<int, string> >errTXT;



struct SymDetail;
struct node {
	string Sname;
	SymDetail* detailPtr;//如果是标识符，指向符号表项
	string* strVal=nullptr;
	int* numVal=nullptr;
	bool* boolVal=nullptr;//生成语法树时能求出来的表达式值。求不出为空指针。
	vector<node*> children;
	node(string name) {
		Sname = name;
	}
	node(const node& n1) {  //拷贝构造函数
		this->Sname = n1.Sname;
		this->children = n1.children;
	}
};

struct Reg;
struct SymTable;//提前声明一下
struct SymDetail {
	string kind,type,name;//kind{var,func,para,const,ret} type{array1,array2,int,void,bool(仅用于临时变量)} name
	SymTable* childTable=nullptr;//指向子表（如果有的话，主要用于找函数形参）
	int* dim1 = nullptr, * dim2 = nullptr;// dim1 dim2表示各维度的长度
	int	*constInt=nullptr;//constInt是四元式中的常数值。（能真正计算出来的，否则为空）
	bool* constBool=nullptr;//同上，bool常量
	string constString ;
	vector<int> constArr;//存放编译时的常量数组元素内容
	static int tempCnt ;
	bool isTemp=false;//临时值不用保存值，只用于指示寄存器或栈空间,但要确定值的类型（int|bool）
	bool isGlobal=false;//是全局变量。目标代码中访存不同，为：name（offset）
	int* fp_offset=nullptr;//fp_offset($fp)查找局部变量，；如果是数组参数要间接取值，lw $s1 fp_offset($fp)，lw $t1 $s1 其中$s1是绝对地址
	bool valid=true;//内存有效位，赋值、运算后为false，初始、save之后true。对数组无意义（修改立刻访存，valid可能为真或假）
	SymDetail* arr_offset = nullptr;//数组偏移。临时值。在目标码中动态生成，仅用于传地址实参时记录偏移。
	SymDetail* baseArray = nullptr;//指向地址参数对应的数组本身。便于有偏移的地址参数获取fp_offset，从而计算fp_offset+arr_offset
	vector<Reg*> regs;//存放了该值的所有寄存器
	SymDetail(){}
	SymDetail(string k1, string t1, string n1) :kind(k1), type(t1), name(n1) {}
	SymDetail(string k1, string t1, string n1, int* d1, int* d2) :kind(k1), type(t1), name(n1) { dim1 = d1; dim2 = d2; }
	SymDetail(bool temp, string t) { 
		isTemp = temp; 
		type = t; 
		name = "t" + to_string(tempCnt++); //自增的tempName tn。
		valid = false;//只有确定了要存并且已经存了的的临时值才是true
	}
	SymDetail(int* ci, bool* cb) { 
		constInt = ci;
		constBool = cb;
		if (ci)type = "int";
		else type = "bool";
	}
	SymDetail(string cs) { constString = cs; }
};

struct SymTable {
	SymTable* parent;
	int parentAt,para_n=0;
	vector<SymDetail*> tableList;//这已经生成tableList实例了（在栈上），其生存期就是symtable实例生存期
	bool isFuncDef,isLoop,outerFunc;//outerFunc表示外层是funcdef已经创建的符号表，内部block不用再创建，但是接收到后要改为false，此后再有block就得正常创建
	SymTable() {
		parent = nullptr;
		parentAt = -1;
		outerFunc=isFuncDef=isLoop = false;
	}
	SymTable(SymTable* current) {
		this->parent = current;
		this->parentAt = current->tableList.size()-1;
		outerFunc=isFuncDef = isLoop = false;//默认不是函数
	}
};


struct IRstmt {
	string op;
	SymDetail *arg1=nullptr, *arg2=nullptr, *res=nullptr;
	string label,to_label;//仅当op是goto、beqz、bnez时，res改为to_label字符串输出到IR
	IRstmt() {}
	IRstmt(string o, SymDetail* a1, SymDetail* a2, SymDetail* r) :op(o) ,arg1(a1),arg2(a2),  res(r) {}

};

struct Objstmt {
	string arg[4];
	Objstmt(string s1, string s2, string s3, string s4) { arg[0] = s1; arg[1] = s2; arg[2] = s3; arg[3] = s4; }
};

struct Reg {
	string regName;
	vector<SymDetail*> val;
	Reg(string n) { regName = n; }
};