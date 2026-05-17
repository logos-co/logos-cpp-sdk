#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Forward declaration
class SomeOtherClass;

/**
 * @class ComplexModuleImpl
 * @brief Module with various edge cases
 * 
 * This Comment should be skipped.
 */
class ComplexModuleImpl {
public:
    ComplexModuleImpl() = default;
    ~ComplexModuleImpl() = default;

    // Typedef and using should be skipped
    using StringVec = std::vector<std::string>;

    // Methods in public section
    std::string firstMethod(const std::string& arg);

    // Method with trailing comment — comment should be stripped, method still parsed
    std::string trailingComment(const std::string& arg); // this comment should be ignored

    // Multi-line declaration 
    std::string multilineMethod(
        const std::string& arg);

protected:
    void protectedHelper();

public:
    // Back to public — should be picked up
    bool secondMethod(int64_t id, const std::string& name);
    std::vector<std::string> thirdMethod();

private:
    // Private methods should be skipped
    void privateHelper();
    int m_state = 0;
};
