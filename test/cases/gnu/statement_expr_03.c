// EXPECTED: 1
int main() {
    ({ 0; return 1; 2; });
    return 3;
}
