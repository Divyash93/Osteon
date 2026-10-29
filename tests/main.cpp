#include "Test.h"
int main() {
    int failedCases = 0;
    for (auto& c : test::cases()) {
        int before = test::failures();
        c.fn();
        bool ok = test::failures() == before;
        if (!ok) failedCases++;
        std::printf("%s %s\n", ok ? "[ OK ]" : "[FAIL]", c.name);
    }
    std::printf("\n%zu tests, %d failed\n", test::cases().size(), failedCases);
    return failedCases ? 1 : 0;
}
