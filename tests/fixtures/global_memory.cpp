int counter = 3;
bool enabled = true;

int increment()
{
    counter = counter + 1;
    return counter;
}

int main()
{
    int first = increment();
    int second = increment();
    if (enabled) {
        counter = first + second;
    }
    return counter;
}