/* A file-scope pointer initialised with the address of another object. C90
   6.5.7 makes an address constant a valid initialiser there - it is what the
   linker resolves, not something the program computes - and cc1 takes integer
   constants only, so a table of pointers to globals cannot be written at all. */
int value = 7;
int array[4];

int *pointer = &value;
int *into_array = array;

int main(void)
{
    return *pointer - 7;
}
