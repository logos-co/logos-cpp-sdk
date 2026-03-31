#pragma once
#include <string>
#include <vector>
#include <cstdint>

class SampleModuleImpl {
public:
    SampleModuleImpl() = default;
    ~SampleModuleImpl() = default;

    // Simple types
    std::string greet(const std::string& name);
    bool isValid(const std::string& input);
    int64_t getCount();
    uint64_t getSize();
    double getScore();
    void doNothing();

    // Compound types
    std::vector<std::string> getNames();
    std::vector<uint8_t> getData();
    std::vector<int64_t> getIds();

    // Multiple params
    std::string combine(const std::string& a, const std::string& b, int64_t count);

    // Const ref return and param variations
    bool check(bool flag, double threshold);

private:
    void internalHelper();
    int64_t m_count = 0;
};
