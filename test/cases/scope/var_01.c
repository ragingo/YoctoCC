// EXPECTED: 2
int main() {
    int x = 2;
    {
        int x = 3;
    }
    return x;
}
