/* What both units agree on. Included by each, seen once by neither. */
#ifndef SHAPE_H
#define SHAPE_H
#define SIDES 4
struct Box { int w; int h; };
int area(struct Box *);
extern int boxes_made;
#endif
