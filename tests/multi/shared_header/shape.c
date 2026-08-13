#include "shape.h"
int boxes_made = 0;
int area(struct Box *b)
{
    boxes_made = boxes_made + 1;
    return b->w * b->h;
}
