/*
 * Collision module implementation (stub).
 * See include/collision.h for intent. Basic shape tests are
 * implemented since they are cheap primitives, not gameplay logic.
 */

#include "collision.h"
#include <math.h>

bool CollisionCircleCircle(CircleShape a, CircleShape b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float distanceSq = (dx * dx) + (dy * dy);
    float radiusSum = a.radius + b.radius;
    return distanceSq <= (radiusSum * radiusSum);
}

bool CollisionCircleRect(CircleShape circle, RectShape rect)
{
    float closestX = fmaxf(rect.x, fminf(circle.x, rect.x + rect.width));
    float closestY = fmaxf(rect.y, fminf(circle.y, rect.y + rect.height));

    float dx = circle.x - closestX;
    float dy = circle.y - closestY;

    return (dx * dx + dy * dy) <= (circle.radius * circle.radius);
}
