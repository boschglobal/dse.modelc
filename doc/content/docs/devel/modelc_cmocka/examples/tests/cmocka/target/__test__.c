
#include <dse/testing.h>

extern int run_foo_tests(void);

int main()
{
    int rc = 0;
    rc |= run_foo_tests();
    return rc;
}
