#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
using std::cout;
using std::cin;
using std::ifstream;
using std::string;
using std::stringstream;
#include "tinyxml2.h"
using namespace tinyxml2;
#include "crc32.h"
#include "to_aamp.h"

XMLDocument xmlDoc;

union {
	uint32_t uint32;
	unsigned char chars[4];
	float f;
}data;

std::map<uint32_t, std::string> hashed_names;

void init_crc2(std::string namesfile)
{
	ifstream f(namesfile);
	if (f.is_open() == false)
	{
		std::cerr << "you are missing hashed_names.txt!\n";
		cin.get();
		exit(0);
	}
	string temp;
	while (f >> temp)
	{
		uint32_t result = crc32c(0, (unsigned char*)temp.c_str(), temp.length());
		hashed_names[result] = temp;
	}
}

ifstream file;
int rootnodes;
int root_address;

void read_root_node();
void read_child_node(XMLElement *root, int level=0);

int main(int argc, char* argv[])
{
	string program_path(argv[0]);
	program_path = program_path.substr(0, program_path.length() - 12);

	init_crc2(program_path+"hashed_names.txt");

	if (argc != 2)
	{
		std::cerr << "Takes file as argument.\n";
		return 1;
	}
	file = ifstream(argv[1], ifstream::binary);
	if (!file.is_open())
	{
		std::cerr << "Could not open file.\n";
		return 1;
	}

	file.seekg(0, file.end);
	int length = file.tellg();
	file.seekg(0, file.beg);
	cout << length << std::endl;
	
	file.read((char*)&data.uint32, 4);	//AAMP header
	cout << "header: "<<data.chars <<std::endl;
	if (data.chars[0] != 'A')
	{
		file.close();
		//write aamp instead
		to_aamp(argv[1]);
		cin.get();
		exit(0);
	}

	file.read((char*)&data.uint32, 4);	//version
	cout << "version: " << data.uint32 << std::endl;

	file.read((char*)&data.uint32, 4);	//unknown 3
	cout << "unknown: " <<std::hex << "0x" << data.uint32 <<std::dec << std::endl;

	file.read((char*)&data.uint32, 4);	//version
	cout << "filesize: " << data.uint32 << ", real filesize: " << length << std::endl;

	file.read((char*)&data.uint32, 4);	//unknown 0
	cout << "unknown: " << std::hex << "0x" << data.uint32 << std::dec << std::endl;

	file.read((char*)&data.uint32, 4);	//XML (or other unknown format) length
	cout << "Format name length: " << data.uint32 << std::endl;
	int format_length = data.uint32;

	file.read((char*)&data.uint32, 4);	//Number of root nodes
	cout << "Root nodes:  " << data.uint32 << std::endl;
	rootnodes = data.uint32;

	file.read((char*)&data.uint32, 4);	//Number of direct child nodes to the root node
	cout << "Children to root:  " << data.uint32 << std::endl;

	file.read((char*)&data.uint32, 4);	//Number of nodes excluding root and root children
	cout << "Total nodes (-root and children of root):  " << data.uint32 << std::endl;

	file.read((char*)&data.uint32, 4);	//Data buffer size
	cout << "Data buffer size:  " << data.uint32 << std::endl;

	file.read((char*)&data.uint32, 4);	//String buffer size
	cout << "String buffer size:  " << data.uint32 << std::endl;

	file.read((char*)&data.uint32, 4);	//unknown 0
	cout << "unknown: " << std::hex << "0x" << data.uint32 << std::dec << std::endl;

	for (int i = 0; i < format_length; ++i)
	{
		file.read((char*)&data.uint32, 1);
		cout << "Format: " << data.chars << std::endl;
	}

	//ROOT NODE
	root_address = file.tellg();
	read_root_node();

	file.close();
	XMLError eResult = xmlDoc.SaveFile(string(string(argv[1]) + ".xml").c_str());
	cin.get();
	return 0;
}

void read_root_node()
{
	XMLElement *newRoot = xmlDoc.NewElement("root_node");
	
	cout << "////ROOT NODE////" << std::endl;
	int address = file.tellg();
	file.read((char*)&data.uint32, 4);	//ID
	cout << "ID: " << std::hex << "0x" << data.uint32 << std::dec << std::endl;
	if (hashed_names.find(data.uint32) != hashed_names.end())
		newRoot->SetName(hashed_names[data.uint32].c_str());
	else
	{
		newRoot->SetAttribute("hash", data.uint32);
	}

	file.read((char*)&data.uint32, 4);	//unknown
	cout << "unknown: " << std::hex << "0x" << data.uint32 << std::dec << std::endl;
	newRoot->SetAttribute("extra", data.uint32);

	file.read((char*)&data.uint32, 2);	//Data offset (relative to start of node)
	cout << "Data offset: " << (uint16_t)data.uint32 << std::dec << std::endl;
	int data_offset = (uint16_t)data.uint32;

	file.read((char*)&data.uint32, 2);	//Number of child nodes
	cout << "Number of child nodes: " << (uint16_t)data.uint32 << std::dec << std::endl;
	int children = (uint16_t)data.uint32;
	int here = file.tellg();
	file.seekg(address + data_offset * 4);
	cout << "jump_to: " << address + data_offset*4 << std::endl;
	if (children == 0)
	{
		newRoot->SetAttribute("pointer", address + data_offset * 4);
		cout << "No children to root node.\n";
	}
	else
	{
		newRoot->SetAttribute("pointer", address + data_offset * 4);
		newRoot->SetAttribute("children", children);
		for (int i = 0; i < children; ++i)
		{
			read_child_node(newRoot);
		}
	}
	file.seekg(here);

	xmlDoc.InsertEndChild(newRoot);

	rootnodes--;
	if (rootnodes != 0)
	{
		read_root_node();
	}
}

void read_child_node(XMLElement *parent, int level)
{
	XMLElement *newElement = xmlDoc.NewElement("node");
	parent->InsertEndChild(newElement);
	string lv = "\t";
	for (int i = 0; i < level; ++i)
		lv += "\t";

	cout <<lv<< "////CHILD NODE////" << std::endl;
	int child_address = file.tellg();
	newElement->SetAttribute("address", child_address);
	file.read((char*)&data.uint32, 4);	//ID
	cout << lv << "ID: " << std::hex << "0x" << data.uint32 << std::dec << std::endl;
	if (hashed_names.find(data.uint32) != hashed_names.end())
		newElement->SetName(hashed_names[data.uint32].c_str());
	else
	{
		newElement->SetAttribute("hash", data.uint32);
	}

	file.read((char*)&data.uint32, 2);	//Data offset
	cout << lv << "Data offset: " << (uint16_t)data.uint32 << std::endl;
	int data_offset = (uint16_t)data.uint32;

	file.read((char*)&data.uint32, 1);	//Number of child nodes
	cout << lv << "Child node count: " << (unsigned short)data.uint32 << std::endl;
	int children = (uint8_t)data.uint32;

	uint8_t datatype;
	file.read((char*)&datatype, 1);	//Data type

	int continue_address = file.tellg();

	int position = child_address + data_offset * 4;
	file.seekg(child_address + data_offset * 4);
	if (children == 0)
	{
		stringstream temp;
		string temp_string="";
		switch (datatype)
		{
		case DataType::Bool:
			newElement->SetAttribute("type", "bool");
			file.read((char*)&data.chars, 1);
			newElement->SetText((bool)data.chars[0]);
			break;
		case DataType::Float:
			newElement->SetAttribute("type", "float");
			file.read((char*)&data.uint32, sizeof(float));
			newElement->SetText(data.f);
			break;
		case DataType::Int:
			newElement->SetAttribute("type", "int");
			file.read((char*)&data.uint32, 4);
			newElement->SetText(data.uint32);
			break;
		case DataType::Vector2:
			newElement->SetAttribute("type", "vector2");
			file.read((char*)&data.uint32, sizeof(float));
			temp << data.f << ", ";
			file.read((char*)&data.uint32, sizeof(float));
			temp << data.f;
			newElement->SetText(temp.str().c_str());
			break;
		case DataType::Vector3:
			newElement->SetAttribute("type", "vector3");
			file.read((char*)&data.uint32, sizeof(float));
			temp << data.f << ", ";
			file.read((char*)&data.uint32, sizeof(float));
			temp << data.f << ", ";
			file.read((char*)&data.uint32, sizeof(float));
			temp << data.f;
			newElement->SetText(temp.str().c_str());
			break;
		case DataType::Vector4:
			newElement->SetAttribute("type", "vector4");
			file.read((char*)&data.uint32, sizeof(float));
			temp << data.f << ", ";
			file.read((char*)&data.uint32, sizeof(float));
			temp << data.f << ", ";
			file.read((char*)&data.uint32, sizeof(float));
			temp << data.f << ", ";
			file.read((char*)&data.uint32, sizeof(float));
			temp << data.f;
			newElement->SetText(temp.str().c_str());
			break;
		case DataType::String:
			newElement->SetAttribute("type", "string");
			temp_string = "";
			file >> temp_string;
			newElement->SetText(temp_string.c_str());
			break;
		case DataType::String2:
			newElement->SetAttribute("type", "string2");
			temp_string = "";
			file >> temp_string;
			newElement->SetText(temp_string.c_str());
			break;
		case DataType::Actor:
			newElement->SetAttribute("type", "actor");
			temp_string = "";
			file >> temp_string;
			newElement->SetText(temp_string.c_str());
			break;
		case DataType::Path:
			newElement->SetAttribute("type", "path");
			temp_string = "";
			file >> temp_string;
			newElement->SetText(temp_string.c_str());
			break;
		default:	//unknown type, set it's index as type
			newElement->SetAttribute("type", datatype);
			file.read((char*)&data.chars, 1);
			newElement->SetText((short)data.chars[0]);
			break;
		}
		file.seekg(continue_address);
	}
	else
	{
		newElement->SetAttribute("children", children);
		for (int i = 0; i < children; ++i)
		{
			read_child_node(newElement, level+1);
		}
		file.seekg(continue_address);
	}
}