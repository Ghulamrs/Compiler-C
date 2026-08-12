/* one translation unit: a stack, with its storage private to this file */
int printf(char *fmt, ...);

struct Stack { int items[16]; int top; };

static struct Stack the_stack;      /* internal linkage - invisible to main.c */
int depth;                          /* external - main.c reads this one */

int push(int v);
int pop(void);

int push(int v)
{
    if (the_stack.top >= 16) { return 0; }
    the_stack.items[the_stack.top] = v;
    the_stack.top = the_stack.top + 1;
    depth = the_stack.top;
    return 1;
}

int pop(void)
{
    if (the_stack.top <= 0) { return 0; }
    the_stack.top = the_stack.top - 1;
    depth = the_stack.top;
    return the_stack.items[the_stack.top];
}
