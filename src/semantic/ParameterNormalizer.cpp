#include "semantic/ParameterNormalizer.h"
#include <algorithm>
#include <cctype>

namespace semantic {

static std::string ToLower(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return result;
}

std::map<std::string, ExecutionParameterVariant> ParameterNormalizer::Normalize(const std::map<std::string, std::string>& rawParameters) const {
    std::map<std::string, ExecutionParameterVariant> result;
    for (const auto& [key, value] : rawParameters) {
        result[key] = ParseValue(value);
    }
    return result;
}

ExecutionParameterVariant ParameterNormalizer::ParseValue(const std::string& value) const {
    if (value.empty()) {
        return std::string("");
    }

    std::string lowerValue = ToLower(value);
    if (lowerValue == "true" || lowerValue == "yes" || lowerValue == "on") {
        return true;
    }
    if (lowerValue == "false" || lowerValue == "no" || lowerValue == "off") {
        return false;
    }

    // Check if integer
    try {
        size_t pos;
        int iValue = std::stoi(value, &pos);
        if (pos == value.length()) {
            return iValue;
        }
    } catch (...) {}

    // Check if double
    try {
        size_t pos;
        double dValue = std::stod(value, &pos);
        if (pos == value.length()) {
            return dValue;
        }
    } catch (...) {}

    // Fallback to string
    return value;
}

} // namespace semantic
