// expect: 20
enum Colour { Red, Green, Blue };
int main(void)
{
    enum Colour c = Green;
    switch (c) {
    case Red: return 10;
    case Green: return 20;
    case Blue: return 30;
    }
    return 0;
}
