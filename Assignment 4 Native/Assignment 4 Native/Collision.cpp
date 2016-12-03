#include "Collision.h"



Collision::Collision() : aPolygon(NULL),
	bPolygon(NULL),
	aNormal(glm::vec2()),
	aVertex(glm::vec2()),
	depth(-FLT_MAX)
{
}


