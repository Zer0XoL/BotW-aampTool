#include "to_aamp.h"
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include "crc32.h"
#include "tinyxml2.h"

using namespace std;

int getType(string type)
{
	if (type == "bool")
		return 0x0;
	if (type == "int")
		return 0x2;
	if (type == "float")
		return 0x1;
	if (type == "vector2")
		return 0x3;
	if (type == "vector3")
		return 0x4;
	if (type == "vector4")
		return 0x6;
	if (type == "string")
		return 0x7;
	if (type == "string2")
		return 0x14;
	if (type == "actor")
		return 0x8;
	if (type == "path")
		return 0xF;
	return std::stoi(type);
}

fstream output;

map<string, vector<int>> string_buffer;	//vector holds offsets to write
vector < pair<uint32_t, vector<int> > > data_buffer;
map<int, vector<int>> pointer_table;	//used to restore the original structure

void write_uint32(uint32_t data, int location = -1)
{
	int current = output.tellg();
	if (location != -1)
		output.seekg(location);
	output.write((char*)&data, sizeof(uint32_t));
	if (location != -1)
		output.seekg(current);
}

void write_uint16(uint16_t data, int location = -1)
{
	int current = output.tellg();
	if (location != -1)
		output.seekg(location);
	output.write((char*)&data, sizeof(uint16_t));
	if (location != -1)
		output.seekg(current);
}

void write_byte(uint8_t data, int location = -1)
{
	int current = output.tellg();
	if (location != -1)
		output.seekg(location);
	output.write((char*)&data, sizeof(uint8_t));
	if (location != -1)
		output.seekg(current);
}

void write_string(string data, int location = -1)
{
	int current = output.tellg();
	if (location != -1)
		output.seekg(location);
	for (auto c : data)
	{
		output.write((char*)&c, sizeof(uint8_t));
		cout << "wrote char: " << c << std::endl;
	}
	if (location != -1)
		output.seekg(current);
}

void prewrite_header()
{
	write_string("AAMP");	//0
	write_uint32(2);	//version //4
	write_uint32(3);	//unknown 3	//8
	write_uint32(1337);	//filesize, write later	//12
	write_uint32(0);	//padding of 0s	//16
	write_uint32(4);	//format length of "XML"	//20
	write_uint32(0);	//number of root nodes	//24
	write_uint32(0);	//number of direct child nodes	//28
	write_uint32(0);	//number of extra nodes		//32
	write_uint32(1337);	//data buffer size	//36
	write_uint32(1337);	//string buffer size	//40
	write_uint32(0);	//unknown 0 padding	//44
	write_string("xml");//format
	write_byte(0);		//format 0 terminal
	cout << "prewrote header.\n";
}

void allocate_data(vector<uint32_t> data, int offset_address)
{
	vector<uint32_t> temp;
	std::transform(data_buffer.begin(), data_buffer.end(), std::back_inserter(temp), 
		[](auto pair) -> uint32_t
		{
			return pair.first;
		}
	);
	auto it = std::search(temp.begin(), temp.end(), data.begin(), data.end());
	if (it == temp.end())	//didnt find the data, push back, todo: (search again with one fewer element and see if its the last one)
	{
		data_buffer.push_back({ data[0], {offset_address} });
		for (int i = 1; i < data.size(); ++i)
			data_buffer.push_back({ data[i], {} });
	}
	else	//update offset address of databuffer location
	{
		data_buffer.at(it - temp.begin()).second.push_back(offset_address);
	}
}

int write_databuffer()
{
	int total = 0;
	while (output.tellg() % 4 != 0)
	{
		write_byte(0);
		total++;
	}

	int databuffer_address = output.tellg();
	for (auto p : data_buffer)
	{
		for(auto addr : p.second)
			write_uint16(((int)output.tellg() + 4 - addr) / 4, addr);
		write_uint32(p.first);
		total += 4;
		cout << "wrote data: " << p.first << std::endl;
	}
	//36
	write_uint32(total, 36);
	return total;
}

void allocate_string(string data, int offset_address)
{
	if (string_buffer.find(data) == string_buffer.end())	//not in map yet
	{
		string_buffer[data] = { offset_address };
	}
	else	//add additional offsets
	{
		string_buffer[data].push_back(offset_address);
	}
}

int write_stringbuffer()
{
	int total = 0;
	while (output.tellg() % 4 != 0)
	{
		write_byte(0);
		total++;
	}
	for (auto thing : string_buffer)
	{
		for (auto addr : thing.second)
		{
			write_uint16(((int)output.tellg()+4 - addr) / 4, addr);
		}
		write_string(thing.first.c_str());
		total += thing.first.length();
		cout << "wrote string: " << thing.first << std::endl;
		do
		{
			write_byte(0);
			total++;
		} while (output.tellg() % 4 != 0);
	}
	//write total to 40
	write_uint32(total, 40);
	return total;
}

vector<pair<int, XMLElement*>> write_roots(vector<XMLElement*> &roots)	//returns collected children
{
	vector<pair<int, XMLElement*>> collected_children;
	for (auto r : roots)
	{
		if (string(r->Name()) == "root_node")
			write_uint32(r->IntAttribute("hash"));
		else	//get the crc32 hash from the name
		{
			string name = r->Name();
			uint32_t hash = crc32c(0, (unsigned char*)name.c_str(), name.length());
			write_uint32(hash);
		}
		write_uint32(r->IntAttribute("extra",0));	//write the unknown extra value

		int data_offset_address = -(int)output.tellg();	//store the address for child nodes to write to Negative

		write_uint16(1337);	//write data offset, write later from child node

		uint16_t children = r->IntAttribute("children", 0);

		write_uint16(children);	//write number of child nodes


		if (children > 0)
		{
			collected_children.push_back({data_offset_address, r->FirstChildElement()});
		}
		else //do the pointer table thing here
		{
			pointer_table[r->IntAttribute("pointer", 0)].push_back(-data_offset_address);
		}
	}
	return collected_children;
}

void write_children(vector<pair<int, XMLElement*>> nodes)
{
	//sorts the nodes, makes the order the same as the original, but causes the data allocation order to be different:
	std::sort(nodes.begin(), nodes.end(), [](auto a, auto b) {
		if (a.second->IntAttribute("address", 0) < b.second->IntAttribute("address", 0))
			return true;
		return false;
	});
	vector<pair<int, XMLElement*>> collected_children;
	for (auto n : nodes)
	{
		auto addr = n.first;
		auto elem = n.second;

		if(addr > 0)
			write_uint16(((int)output.tellg() + 4 - addr)/4, addr);	//write the offset to parent normal node
		else	//negative means parent is a root node
		{
			addr = -addr;
			write_uint16(((int)output.tellg() + 8 - addr) / 4, addr);	//write the offset to parent root
		}

		//write root pointers that point to the same thing:
		if (pointer_table.find(elem->IntAttribute("address", 0)) != pointer_table.end())
		{
			for (auto p : pointer_table[(elem->IntAttribute("address", 0))])
			{
				write_uint16(((int)output.tellg() + 8 - p) / 4, p);	//write the offset to parent root
			}
		}

		do
		{
			cout << "writing child: " << string(elem->Name()) << std::endl;
			if (string(elem->Name()) == "node")
				write_uint32(elem->IntAttribute("hash"));
			else	//get the crc32 hash from the name
			{
				string name = elem->Name();
				uint32_t hash = crc32c(0, (unsigned char*)name.c_str(), name.length());
				write_uint32(hash);
			}
			int data_offset_address = output.tellg();
			cout << "child offset: " << data_offset_address << std::endl;
			

			write_uint16(0);	//write offset, write later from child or data/string buffer
			uint8_t children = elem->IntAttribute("children", 0);
			write_byte(children);
			if (children > 0)
			{
				collected_children.push_back({ data_offset_address, elem->FirstChildElement() });
				write_byte(0);	//0 means datatype = children
			}
			else
			{
				cout << "child type: " << elem->Attribute("type") << std::endl;
				float f;
				stringstream vectorstring;
				string token;
				vector<uint32_t> uints;
				switch (getType(elem->Attribute("type")))
				{
				case DataType::Bool:
					allocate_data({ (uint32_t)(string(elem->GetText()) == "true" ? 1 : 0) }, data_offset_address);
					cout << "allocated bool: " << elem->GetText() << std::endl;
					break;
				case DataType::Int:
					allocate_data({ (uint32_t)(std::strtoul(elem->GetText(), NULL, 0)) }, data_offset_address);
					cout << "allocated int: " << elem->GetText() << std::endl;
					break;
				case DataType::Float:
					f = std::stof(elem->GetText());
					allocate_data({ *reinterpret_cast<uint32_t*>(&f) }, data_offset_address);
					cout << "allocated float: " << elem->GetText() << std::endl;
					break;
				case DataType::Vector2:
				case DataType::Vector3:
				case DataType::Vector4:
					vectorstring = stringstream(string(elem->GetText()));
					cout << "vector: ";
					while (std::getline(vectorstring, token, ','))
					{
						cout << token <<" ,";
						f = std::stof(token);
						uints.push_back(*reinterpret_cast<uint32_t*>(&f));
					}
					cout << std::endl;
					allocate_data(uints, data_offset_address);
					break;
				case DataType::String:
				case DataType::String2:
				case DataType::Actor:
				case DataType::Path:
					if(elem->GetText())
						allocate_string(elem->GetText(), data_offset_address);
					else
						allocate_string("", data_offset_address);
					break;
				default:
					//getType(elem->Attribute("type"))
					allocate_data({ (uint32_t)(std::strtoul(elem->GetText(), NULL, 0)) }, data_offset_address);
					cout << "allocated unknown type "<< getType(elem->Attribute("type")) <<": " << elem->GetText() << std::endl;
					break;
				}
				write_byte(getType(elem->Attribute("type")));
			}

		} while (elem = elem->NextSiblingElement());
	}
	if(collected_children.size() > 0)
		write_children(collected_children);
}

void to_aamp(string filename)
{
	output = fstream(filename + ".aamp", ios::out | ios::binary);	//open target file for writing
	XMLDocument xmlDoc;
	xmlDoc.LoadFile(filename.c_str());	//open xml for reading
	prewrite_header();	//prewrite the header
	vector<XMLElement*> collected_roots;
	XMLElement* r = xmlDoc.FirstChildElement();
	int number_roots = 0;
	int number_directchildren = 0;
	do
	{
		collected_roots.push_back(r);
		number_roots++;
		number_directchildren += r->IntAttribute("children", 0);
	} while (r = r->NextSiblingElement());
	write_uint32(number_roots, 24);	//write root node count
	write_uint32(number_directchildren, 28);	//write direct child count

	auto children = write_roots(collected_roots);

	write_children(children);
	int databuffer_size = write_databuffer();
	int stringbuffer_size = write_stringbuffer();
	output.seekg(0, output.end);
	int filesize = output.tellg();
	write_uint32(filesize, 12);	//write filesize

	int number_extrachildren = (((((filesize - number_roots * 12) - number_directchildren * 8) - databuffer_size) - stringbuffer_size)-52) / 8;
	
	write_uint32(number_extrachildren, 32);	//write extra child count

	output.close();
}