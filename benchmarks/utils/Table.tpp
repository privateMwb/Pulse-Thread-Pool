#include "sstream"

namespace Table {

// Text Formatting
inline std::string center(
    const std::string& text,
    std::size_t width)
{
	if(text.size() >= width) {
		return text;
	}

	auto left = (width - text.size()) / 2;
	auto right = width - text.size() - left;

	return std::string(left, ' ')
	       + text
	       + std::string(right, ' ');
}

template<typename T>
std::string format(
    const T& value,
    const std::string& unit)
{
	std::ostringstream oss;
	oss << value;

	return oss.str()
	       + " "
	       + unit;
}

inline std::string repeat(
    const std::string& line,
    std::size_t count)
{
	std::string lines;

	for(std::size_t i = 0; i < count; ++i) {
		lines += line;
	}

	return lines;
}

template<template<typename...> class Container, typename T>
Container<std::string> convert(
    const Container<T>& data,
    const std::string& unit)
{
	Container<std::string> converted;

	for (const auto& value : data) {
		converted.push_back(format(value, unit));
	}

	return converted;
}

// Table Rendering
template<typename LengthContainer>
inline void border(
    const std::string& left,
    const std::string& middle,
    const std::string& right,
    const std::string& line,
    const LengthContainer& lengths)
{
	std::cout << left;

	for(std::size_t i = 0; i < lengths.size(); ++i) {
		std::cout << repeat(line, lengths[i]) << (i < lengths.size() - 1 ? middle : "");
	}

	std::cout << right << "\n";
}

template<typename StringContainer, typename LengthContainer>
inline void content(
    const std::string& side,
    const StringContainer& titles,
    const LengthContainer& lengths)
{
	if(titles.size() != lengths.size()) {
		return;
	}

	for(std::size_t i = 0; i < lengths.size(); ++i) {
		std::cout << side
		          << center(titles[i], lengths[i])
		          << (i == lengths.size() - 1 ? side + "\n" : "");
	}
}

template<template<typename...> class Container>
void table(
    const std::string& title,
    const Container<std::string>& headers,
    const Container<Container<std::string>>& data,
    std::size_t length)
{
	std::size_t each = length / data.size();
	std::size_t margin =
	    ((each * data.size()) + data.size() + 1) - length - 2;

	Container<std::size_t> top{ length + margin };
	Container<std::size_t> section;

	for (std::size_t i = 0; i < data.size(); ++i) {
		section.push_back(each);
	}

	border("┌", "─", "┐", "─", top);

	Container<std::string> titleRow{ title };
	content("│", titleRow, top);

	border("├", "┬", "┤", "─", section);
	content("│", headers, section);

	border("├", "┼", "┤", "─", section);

	for (std::size_t i = 0; i < data[0].size(); ++i) {
		Container<std::string> row;

		for (std::size_t j = 0; j < data.size(); ++j) {
			row.push_back(data[j][i]);
		}

		content("│", row, section);
	}

	border("└", "┴", "┘", "─", section);
}
};