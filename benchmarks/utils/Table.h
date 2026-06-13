#pragma once

#include <string>
#include <cstddef>

namespace Table {

// Text Formatting
std::string center(
    const std::string& text,
    std::size_t width
);

std::string repeat(
    const std::string& line,
    std::size_t count
);

template<typename T>
std::string format(
    const T& value,
    const std::string& unit = ""
);

template<template<typename...> class Container, typename T>
Container<std::string> convert(
    const Container<T>& data,
    const std::string& unit = ""
);

// Table Rendering
template<typename LengthContainer>
void border(
    const std::string& left,
    const std::string& middle,
    const std::string& right,
    const std::string& line,
    const LengthContainer& lengths
);

template<typename StringContainer, typename LengthContainer>
void content(
    const std::string& side,
    const StringContainer& titles,
    const LengthContainer& lengths
);

template<template<typename...> class Container>
void table(
    const std::string& title,
    const Container<std::string>& headers,
    const Container<Container<std::string>>& data,
    std::size_t length
);

} // namespace Table

#include "Table.tpp"