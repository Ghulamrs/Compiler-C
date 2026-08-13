// expect: 21
enum Flags { One = 1, Two = 1 << 1, Four = 1 << 2, All = One + Two + Four };
enum More { Base = 10, Next = Base + 1 };
int main(void)
{
    return All + Base + Next - 7;
}
