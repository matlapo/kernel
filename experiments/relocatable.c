int global_var = 42;

int hello() {
    int x = global_var;
    return x;
}

int main() {
    int x = hello();
    return x;
}

void _start() {
    main();
}
