#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Forward declaration
class SomeOtherClass;

// Module with various edge cases
class ComplexModuleImpl {
public:
    ComplexModuleImpl() = default;
    ~ComplexModuleImpl() = default;

    // Typedef and using should be skipped
    using StringVec = std::vector<std::string>;

    // Methods in public section
    std::string firstMethod(const std::string& arg);

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
