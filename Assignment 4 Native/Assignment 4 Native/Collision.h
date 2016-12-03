#pragma once
#include "glm.hpp"
class Polygon;
struct Collision
{
	Polygon* aPolygon;
	Polygon* bPolygon;
	glm::vec2 aNormal;
	glm::vec2 aVertex;
	float depth;
	Collision();
};

