// assert_local_important_args("f", 3);

int f(int *p, int x);

int f(int *p, int x)
{
	return *(p + x);
}