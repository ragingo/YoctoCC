void ASSERT(int expected, int actual);

extern int ext1;
extern int *ext2;

int main() {
    ASSERT(5, ext1);
    ASSERT(5, *ext2);

    return 0;
}
