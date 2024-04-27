int add(int a, float b)
{
    return a;
}
int fib(int n)
{
    if (n+n)
    {
        return -1;
    }

    if (n <= 1)
    {
        return 1;
    }
    else if (!n)
    {
        add(1, 2.0);
    }
    return add(1, 2.0);
}

int main()
{
    int a = 1;
    int b = 2;
    int c = fib(a + b);
    int d = c;
    d = fib(5);
    int m = 5;
    while (  m)
    {
        m=m-1;
    }
    for (int i = 1; i < m; i++)
    {
        a = a + 1;
    }
    return c;
}