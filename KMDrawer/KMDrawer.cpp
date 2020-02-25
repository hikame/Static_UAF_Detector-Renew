//============================================================================
// Name        : KMDrawer.cpp
// Author      : Kame Wang
//============================================================================
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <set>
#include <vector>
#include <sstream>
#include <map>
#include <cgraph.h>
#include <gvc.h>
using namespace std;

#define Simplified_Output 1

Agraph_t* graph;
GVC_t* graphContext;
bool simplify_output = false;

map<std::string, Agnode_t*> nodeMap;
map<std::string, Agraph_t*> graphMap;

std::string sourceStr;
std::string freeStr;
std::string sinkStr;

std::string str_replace(std::string str, std::string org, std::string news){
	while(true){
		std::string::size_type pos = str.find(org);
		if(pos == std::string::npos)
			return str;
		str = str.replace(pos, org.length(), news);
	}
}

void split(std::string s, std::string delim, vector<std::string>& ret){
	size_t last = 0;
	size_t index = s.find_first_of(delim,last);
	while (index != std::string::npos){
		ret.push_back(s.substr(last, index-last));
		last = index+1;
		index = s.find_first_of(delim, last);
	}
	if (index - last > 0)  {
		ret.push_back(s.substr(last, index - last));
	}
}

std::string subStringBetween(std::string target, std::string left, std::string right){
	if(target.length() == 0)
		return "";

	std::string::size_type leftpos = target.find(left);
	std::string::size_type rightpos = target.find(right);
	if(leftpos == std::string::npos)
		leftpos = std::string::size_type(0);
	if(rightpos == std::string::npos)
		rightpos = target.size();
	std::string ret = target.substr(leftpos + 1, rightpos - leftpos - 1);
	return ret;
}

Agraph_t* getGraph(std::string func){
	Agraph_t* ret = graphMap[func];
	if(ret != NULL)
		return ret;

	stringstream tmpstream;
	tmpstream << "cluster_" << func;
	ret = agsubg(graph, const_cast<char*>(tmpstream.str().c_str()), 1);

	agset(ret, const_cast<char*>("label"), const_cast<char*>(tmpstream.str().c_str()));
	graphMap[func] = ret;
	return ret;
}

bool firstNode = true;
Agnode_t* getNode(std::string node, std::string func){
	if(node.length() == 0 || func.length() == 0)
		return NULL;

	Agnode_t* ret = nodeMap[node];
	if(ret != NULL)
		return ret;
	Agraph_t* graph = getGraph(func);
	ret = agnode(graph, const_cast<char*>(node.c_str()), 1);
	std::string::size_type pos = node.find("(");
	std::string label = node.substr(0, pos);
	agset(ret, const_cast<char*>("label"), const_cast<char*>(label.c_str()));

	bool isSource = (node == sourceStr); //blue
	bool isFree = (node == freeStr); //yellow
	bool isSink= (node == sinkStr); //red

	std::string fillcolor = "white";
	std::string fontcolor = "black";
	if(isSource && isFree && isSink){
		fillcolor = "black";
		fontcolor = "white";
	}
	else if(!isSource && isFree && isSink)
		fillcolor = "orange";
	else if(isSource && !isFree && isSink)
		fillcolor = "mediumpurple";
	else if(isSource && isFree && !isSink)
		fillcolor = "lightgreen";
	else if(isSource && !isFree && !isSink){
		fillcolor = "lightskyblue";
		// fontcolor = "white";
	}
	else if(!isSource && isFree && !isSink)
		fillcolor = "lightyellow";
	else if(!isSource && !isFree && isSink)
		fillcolor = "lightpink";

	agset(ret, const_cast<char*>("fillcolor"), const_cast<char*>(fillcolor.c_str()));
	agset(ret, const_cast<char*>("fontcolor"), const_cast<char*>(fontcolor.c_str()));

	nodeMap[node] = ret;

	if(firstNode){
		agset(ret, const_cast<char*>("shape"), const_cast<char*>("ellipse"));
		firstNode = false;
	}
	return ret;
}

void HandleHeadRecord(std::string line){
	vector<std::string> spResult;
	split(line, "|", spResult);
	sourceStr = spResult.at(0);
	freeStr = spResult.at(1);
	sinkStr = spResult.at(2);
}

void SetBasicBlockLabel(std::string line, bool important){
	if(simplify_output)
		if(!important)
			return;
	vector<std::string> spResult;
	split(line, "=", spResult);
	std::string basicBlock = spResult[0];
	std::string func = subStringBetween(basicBlock, "(", ")");

	std::string lwinfo = spResult[1];
	spResult.clear();
	split(lwinfo, "|", spResult);
	Agnode_t* node = getNode(basicBlock, func);
	stringstream labelStream;
	labelStream << "L: " << str_replace(spResult[0], ",", "\\l&");
	if(spResult[1].length() > 0){
		labelStream << "\\l!W! " << spResult[1].substr(1);
	}

	std::string todel = labelStream.str();
	agset(node, const_cast<char*>("label"), const_cast<char*>(labelStream.str().c_str()));

	if(!simplify_output && !important)
		agset(node, const_cast<char*>("fillcolor"), const_cast<char*>("grey"));
}

void SetFunctionLabel(std::string line, char* emitPrefix, bool important){
	if(simplify_output)
		if(!important)
			return;
	vector<std::string> spResult;
	split(line, "=", spResult);
	std::string func = spResult[0];
	Agraph_t* graph = getGraph(func);
	stringstream labelStream;
	std::string realpath = spResult[1];
	if(emitPrefix && realpath.find(emitPrefix) == 0)
		realpath = realpath.substr(strlen(emitPrefix));
	labelStream << func << "(...) @ \\l." << realpath;
	agset(graph, const_cast<char*>("label"), const_cast<char*>(labelStream.str().c_str()));
	agattr(graph, AGRAPH, const_cast<char*>("style"), const_cast<char*>("dotted"));
}

std::string simplify_edgestr(std::string str){
	str = str_replace(str, "Start&", "");
	str = str_replace(str, "Symb_Index", "");
	str = str_replace(str, "Call", " C");

	str = str_replace(str, "Branch_True", " Y");
	str = str_replace(str, "Branch_False", " N");
	str = str_replace(str, "Branch_Undet", " ?");
	str = str_replace(str, "Branch_UnCon", " J");

	str = str_replace(str, "Switch_Sure", " SW");
	str = str_replace(str, "Switch_NSure", " SW(?)");

	str = str_replace(str, "Select_True", " SL(T)");
	str = str_replace(str, "Select_False", " SL(F)");

	str = str_replace(str, "Return", " R");
	str = str_replace(str, "Ret_MB", " R(Not Null)");
	str = str_replace(str, "Ret_Null", " R(Null)");

	str = str_replace(str, "CMP_True&", "");
	str = str_replace(str, "CMP_False&", "");

	str = str_replace(str, "Malloc_Success&", "");
	str = str_replace(str, "Malloc_Failed&", "");

	return str;
}

class EdgeRecord{
public:
	Agraph_t* graph;
	Agnode_t* node1;
	Agnode_t* node2;
	Agedge_t* edge;
	set<int> indexs;
	EdgeRecord(Agraph_t* g, Agnode_t* n1, Agnode_t* n2, Agedge_t* e){
		graph = g;
		node1 = n1;
		node2 = n2;
		edge = e;
	}
};

set<EdgeRecord*> edgeRecords;

EdgeRecord* getEdge(Agraph_t* graph, Agnode_t* node1, Agnode_t* node2, string nstr){
	EdgeRecord* ret = NULL;
	int idx = stoi(nstr);
	for(auto er : edgeRecords){
		if(er->graph == graph && er->node1 == node1 && er->node2 == node2){
			ret = er;
			break;
		}
	}
	if(ret == NULL){
		Agedge_t* edge = agedge(graph, node1, node2, 0, 1);
		ret = new EdgeRecord(graph, node1, node2, edge);
		edgeRecords.insert(ret);
	}
	if(idx != -1)
		ret->indexs.insert(idx);
	return ret;
}

void AddNewEdge(std::string line, bool important){
	if(simplify_output)
		if(!important)
			return;
	vector<std::string> spResult;
	split(line, "|", spResult);
	std::string nodeStr1 = spResult.at(0);
	std::string edgeStr = spResult.at(1);
	std::string nodeStr2 = spResult.at(2);
	std::string nmCount = "-1";
	std::string spCount = "-1";
	bool hasLineNum = (spResult.size() == 5);
	if(hasLineNum){
		nmCount = spResult.at(3);
		spCount = spResult.at(4);
	}
	std::string func1 = subStringBetween(nodeStr1, "(", ")");
	std::string func2 = subStringBetween(nodeStr2, "(", ")");

	Agnode_t* node1 = getNode(nodeStr1, func1);
	Agnode_t* node2 = getNode(nodeStr2, func2);
	if(node2 == NULL)
		return;

	Agraph_t* targetGraph = (func1 == func2 ? getGraph(func1) : graph);
	edgeStr = edgeStr.substr(0, edgeStr.length() - 1);
	edgeStr = simplify_edgestr(edgeStr);
	if(edgeStr.length() == 0)
		return;
	while(true){
		std::string::size_type pos = edgeStr.find("&");
		if(pos == std::string::npos)
			break;
		edgeStr = edgeStr.replace(pos, 1, ", ");
	}

	EdgeRecord* er = NULL;
	if(simplify_output) 
		er = getEdge(targetGraph, node1, node2, spCount);
	else
		er = getEdge(targetGraph, node1, node2, nmCount);

	stringstream stream;
	stream << edgeStr;
	size_t isize = er->indexs.size();
	if(isize > 0){
		stream << "(#";
		int c = 0;
		for(int idxrec : er->indexs){
			stream << idxrec;
			if(c != isize - 1)
				stream << ", ";
			c++;
		}
		stream << ")";
	}
	string label = stream.str();
	agset(er->edge, const_cast<char*>("label"), const_cast<char*>(label.c_str()));
}

bool HandleResultFile(std::string filePath, char* emitPrefix){
	unsigned pos = filePath.find_last_of(".kmd");
	unsigned lenth = filePath.length();
	if(pos != lenth - 1){
		return false;
	}

	cout << "Handling File: " << filePath << "\n";
  	graphContext = gvContext(); /* library function */

	graph = agopen(const_cast<char*>(filePath.c_str()), Agdirected, 0);
	agattr(graph, AGEDGE, const_cast<char*>("label"), const_cast<char*>(""));
	agattr(graph, AGNODE, const_cast<char*>("shape"), const_cast<char*>("rect"));
	agattr(graph, AGNODE, const_cast<char*>("label"), const_cast<char*>(""));
	agattr(graph, AGNODE, const_cast<char*>("xlabel"), const_cast<char*>(""));
	agattr(graph, AGNODE, const_cast<char*>("style"), const_cast<char*>("filled"));
	agattr(graph, AGNODE, const_cast<char*>("color"), const_cast<char*>("black"));
	agattr(graph, AGNODE, const_cast<char*>("fillcolor"), const_cast<char*>("white"));
	agattr(graph, AGNODE, const_cast<char*>("fontcolor"), const_cast<char*>("black"));

	ifstream rdStream;
	rdStream.open(filePath.c_str(), ios::in);
	std::string line;
	vector<std::string> spResult;
	while(getline(rdStream, line)){
		if(line.length() == 0)
			continue;
		spResult.clear();
		split(line, ":", spResult);
		if(spResult[0] == "H")
			HandleHeadRecord(spResult[1]);
		else if(spResult[0] == "B(I)")
			SetBasicBlockLabel(spResult[1], true);
		else if(spResult[0] == "B(U)")
			SetBasicBlockLabel(spResult[1], false);
		else if(spResult[0] == "F(I)")
			SetFunctionLabel(spResult[1], emitPrefix, true);
		else if(spResult[0] == "F(U)")
			SetFunctionLabel(spResult[1], emitPrefix, false);
		else if(spResult[0] == "E(I)")
			AddNewEdge(spResult[1], true);
		else if(spResult[0] == "E(U)")
			AddNewEdge(spResult[1], false);
		else if(spResult[0] == "Warning Types"){
			string ls = spResult[1];
			ls = str_replace(ls, "; ", "\n");
			ls = str_replace(ls, " -", ":");
			ls = str_replace(ls, "_", " ");
			agattr(graph, AGRAPH, const_cast<char*>("label"), 
				const_cast<char*>(ls.c_str()));
		}

	}
	rdStream.close();
	gvLayout(graphContext, graph, "dot");
	
	stringstream stream;

	stream << filePath;
	if(simplify_output)
		stream << "_simplified.png";
	else
		stream << "_normal.png";
	std::string outputPath = stream.str();
	gvRenderFilename (graphContext, graph, const_cast<char*>("png"), const_cast<char*>(outputPath.c_str()));
	agclose(graph); /* library function */
	graphMap.clear();
	nodeMap.clear();
	firstNode = true;
	return true;
}


void help(){
	printf("Usage: kmdrawer [options] {path_to_kmd_folder}\n");
	printf("Options: \n");
	printf("-e emit_path_prefix: the prefix wanted to be emitted present in the figure.\n");
	printf("-s simplify the output figure.\n");
	exit(-1);
}

int main(int argc,char *argv[]){
	if(argc == 1 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
		help();

	char* emitPrefix = NULL;
	char* targetPath = NULL;
	for(int i = 1; i < argc; i++){
		char* arg = argv[i];
		if(strcmp(arg, "-e") == 0){
			i = i + 1;
			if(i >= argc)
				help();
			emitPrefix = argv[i];
			size_t len = strlen(emitPrefix);
			if(emitPrefix[len - 1] == '/')
				emitPrefix[len - 1] = 0;
		}
		else if(strcmp(arg, "-s") == 0){
			if(i + 1 >= argc)
				help();
			simplify_output = true;
		}
		if(i == argc - 1)
			targetPath = arg;
	}

	DIR *dir;
	if ((dir = opendir(targetPath)) == NULL){
		// maybe target is a file
		bool handled = HandleResultFile(targetPath, emitPrefix);
		if(!handled){
			printf("Target file is not a folder or kmd file: %s.\n", targetPath);
			help();
		}
		return 0;
	}

	struct dirent *ptr;
	std::stringstream stream;
	while ((ptr = readdir(dir)) != NULL){
		if(ptr->d_type == 8){ //file
			stream.str("");
			stream << targetPath << "/" << ptr->d_name;
			HandleResultFile(stream.str(), emitPrefix);
		}
	}
	closedir(dir);
	
	for(auto er : edgeRecords)
		delete er;
	return 0;
}

