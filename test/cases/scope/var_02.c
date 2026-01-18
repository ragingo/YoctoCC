// EXPECTED: 2
int main() {
    int x = 2;
    {
        int x = 3;
    }
    {
        int y = 4;
        return x;
    }
}
