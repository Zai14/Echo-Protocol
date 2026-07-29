#ifndef ECHO_COLLISION_H
#define ECHO_COLLISION_H

#include <stdbool.h>

/*
 * Collision module (stub).
 *
 * Will provide shape-vs-shape collision queries (circle/rect) used by
 * gameplay systems. Only basic primitive definitions exist for now.
 */

typedef struct CircleShape {
    float x;
    float y;
    float radius;
} CircleShape;

typedef struct RectShape {
    float x;
    float y;
    float width;
    float height;
} RectShape;

bool CollisionCircleCircle(CircleShape a, CircleShape b);
bool CollisionCircleRect(CircleShape circle, RectShape rect);

#endif /* ECHO_COLLISION_H */
