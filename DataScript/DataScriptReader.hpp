#pragma once

#include <string>
#include <vector>
#include <unordered_map>

enum : unsigned char
{
	DsBad		= 1 << 0,
	DsBadtype	= 1 << 1,
	DsEof		= 1 << 2
};

class DsReader
{
public:

	DsReader(std::string const& filepath);

	operator unsigned char() const;
	std::string Message() const;

	DsReader& operator>>(auto& var);

	void Goto(unsigned location);
	unsigned Pointer() const;
	void Goto(std::string const& label);
	unsigned Label(std::string const& name) const;

private:

	unsigned char state;
	std::string message;

	struct RawData { char type; std::string value; };
	std::vector<RawData> raw_datas;
	std::unordered_map<std::string, unsigned> labels;
	unsigned read_pointer;
};