extern int external_foo(int);
extern int my_extern_var;

int squared(int x) {
    int y = external_foo(x);
    return x * y * my_extern_var;
}
