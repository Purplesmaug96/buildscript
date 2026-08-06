// cpp: a small C++ program
//
// OpenXeChain ships no C++ standard library (no libc++/libstdc++), so
// iostream and the STL are not available. But clang++ compiles C++ fine and
// links against the same Newlib C runtime, so plain C++ works: classes,
// member functions, references, operator overloading, and global
// constructors (crt0 runs __CTOR_LIST__ before main()).

#include <stdio.h>

class Counter
{
public:
    explicit Counter(int start) : value_(start) {}
    void tick() { value_++; }
    int get() const { return value_; }

private:
    int value_;
};

static Counter g_counter(100); // global constructor must run before main()

extern "C" void main(void)
{
    for (int i = 0; i < 5; i++)
    {
        g_counter.tick();
        printf("cpp: counter = %d\n", g_counter.get());
    }

    const char *s = "and from C++ on the 360";
    printf("cpp: %s\n", s);
}