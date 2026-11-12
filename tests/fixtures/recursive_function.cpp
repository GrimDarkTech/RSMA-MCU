int recurse(int count)
{
    if (count <= 1) {
        return 1;
    }
    return recurse(count - 1);
}

int main()
{
    return recurse(4);
}