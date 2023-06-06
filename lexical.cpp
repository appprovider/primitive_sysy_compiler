#include<iostream>
#include<fstream>
#include<string>
#include<unordered_map>
#include<vector>
using namespace std;

vector<pair<string, string> >lexOut;//此函数的输出

vector<int> LineCnt;

unordered_map <string, string>Keyword{
	{"main","MAINTK"},
	{"const","CONSTTK"},
	{"int","INTTK"},
	{"break","BREAKTK"},
	{"continue","CONTINUETK"},
	{"if","IFTK"},
	{"else","ELSETK"},
	{"while","WHILETK"},
	{"getint","GETINTTK"},
	{"printf","PRINTFTK"},
	{"return","RETURNTK"},
	{"void","VOIDTK"},
};
string symList = "";
string symVal;
inline bool isDigit(char c) {
	return (c >= '0' && c <= '9');
}

inline bool is_Letter(char c) {
	return (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_');
}

inline bool isBlank(char c) {
	return (c == '\n' || c == '\t' || c == '\r' || c == ' ' || c == '\0');
}
//把List的字符串转移到Val，再把List清空。
inline void getVal() {
	symVal = "" + symList;
	symList = "";
}

//只有非空白符才加入List
inline void addList(char c) {
	if (!isBlank(c))symList += c;
}

int line_cnt = 1;
ifstream& myget(ifstream&src,char& c) {
	char ch;
	src.get(ch);
	if (ch == '\n')
		line_cnt++;
	c = ch;
	return src;
}

string checkKeyWord() {
	unordered_map<string, string>::const_iterator got = Keyword.find(symVal);
	if (got == Keyword.end())
		return "";
	else
		return got->second;
}
//每个小函数继续读入x并加入List，直到末尾或者出现非法字符。
//如果不是末尾，还要将读入的x加入List(若是空白符则不加入)
string dealIdent(ifstream& src) {
	char x;

	while (myget(src,x) && (isDigit(x) || is_Letter(x)))addList(x);

	getVal();
	if (!src.eof())
		addList(x);

	string keyType = checkKeyWord();
	if (keyType == "")
		return "IDENFR";
	else
		return keyType;
}
void dealFormatString(ifstream& src) {
	char x;
	while (1) {
		myget(src, x);
		if (x == '\"') {
			symList += x;
			break;
		}
		symList += x;//唯一的例外只有字符串允许空格
	}
	getVal();
	if (!src.eof()) {//多读一位
		myget(src, x);
		addList(x);
	}

}
void dealIntConst(ifstream& src) {
	char x;
	while (myget(src, x) && isDigit(x)) addList(x);
	getVal();
	//如果不是末尾，则还要把新读到的字符加入List
	if (!src.eof())
		addList(x);
}


string dealSym(ifstream& src, char c) {
	string typeCode;

	int flag = 0;//标记是否多读了一位
	switch (c)
	{
	case '+':
		typeCode = "PLUS"; getVal(); break;
	case '-':
		typeCode = "MINU"; getVal(); break;
	case '*':
		typeCode = "MULT"; getVal(); break;
	case '/':
		typeCode = "DIV"; getVal(); break;
	case '%':
		typeCode = "MOD"; getVal(); break;
	case ';':
		typeCode = "SEMICN"; getVal(); break;
	case ',':
		typeCode = "COMMA"; getVal(); break;
	case '(':
		typeCode = "LPARENT"; getVal(); break;
	case ')':
		typeCode = "RPARENT"; getVal(); break;
	case '[':
		typeCode = "LBRACK"; getVal(); break;
	case ']':
		typeCode = "RBRACK"; getVal(); break;
	case '{':
		typeCode = "LBRACE"; getVal(); break;
	case '}':
		typeCode = "RBRACE"; getVal(); break;
		//明确的双目
	case '&':
		typeCode = "AND"; myget(src, c); addList(c); getVal(); break;
	case '|':
		typeCode = "OR"; myget(src, c); addList(c); getVal(); break;
		//向前看才能确定的双目
	case '!':
		myget(src, c);
		if (c == '=') {
			typeCode = "NEQ"; addList(c); getVal();
		}
		else {
			typeCode = "NOT"; getVal(); addList(c); flag = 1;
		}
		break;
	case '<':
		myget(src, c);
		if (c == '=') {
			typeCode = "LEQ"; addList(c); getVal();
		}
		else {
			typeCode = "LSS"; getVal(); addList(c); flag = 1;
		}
		break;
	case '>':
		myget(src, c);
		if (c == '=') {
			typeCode = "GEQ"; addList(c); getVal();
		}
		else {
			typeCode = "GRE"; getVal(); addList(c); flag = 1;
		}
		break;
	case '=':
		myget(src, c);
		if (c == '=') {
			typeCode = "EQL"; addList(c); getVal();
		}
		else {
			typeCode = "ASSIGN"; getVal(); addList(c); flag = 1;
		}
		break;
	default:
		break;
	}
	if (flag == 0 && !src.eof()) {
		myget(src, c); addList(c);
	}

	return typeCode;

}

//每个模块函数以上一个模块的输出为参数，输出到当前的全局lexOut
void lexical(ifstream& srcFile) {
	char x;
	string typeCode;
	int outTop = 0;
	while (srcFile.peek() != EOF) {
		int sym_line = -1;//记录每个循环处理symbol前的行数(防止多读一位导致行数+1)，如果没有则为-1
		if (symList == "") {//如果已经读入字符，就不要再读(避免跳过单字符)
			myget(srcFile,x); addList(x);
		}
		//每个循环最多处理一个symbol，此处已读入该symbol的第一个字符即：symList[0]
		x = symList[0];

		if (is_Letter(x)) {
			sym_line = line_cnt;//
			typeCode = dealIdent(srcFile);
		}
		else if (x == '\"') {
			sym_line = line_cnt;//
			typeCode = "STRCON";
			dealFormatString(srcFile);
		}
		else if (isDigit(x)) {
			sym_line = line_cnt;//
			typeCode = "INTCON";
			dealIntConst(srcFile);
		}
		else if (isBlank(x)) continue;
		else if (x == '/') {
			myget(srcFile,x);
			if (x == '/') {
				char waste[1024]; srcFile.getline(waste, 1023);
				line_cnt++;				//此处要手动增加一行
				symList = ""; continue;
			}
			else if (x == '*') {
				char y;
				myget(srcFile,x);
				do {
					y = x;
					myget(srcFile,x);
				} while (!(y == '*' && x == '/'));
				symList = ""; continue;//清空List，且不需输出
			}
			else {	//是除法
				sym_line = line_cnt;//
				typeCode = "DIV";
				getVal();
				addList(x);
			}

		}
		else {
			sym_line = line_cnt;//
			typeCode = dealSym(srcFile, x);
		}
			
		LineCnt.emplace_back(sym_line);
		lexOut.emplace_back(typeCode, symVal);
		

	}
}
