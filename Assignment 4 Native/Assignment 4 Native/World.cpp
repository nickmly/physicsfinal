#include "World.h"
#include "Collision.h"
#include "Face.h"

// PRIVATE

// Returns a new unique HANDLE each time it is called that the World uses to make it possible 
// to look up Polygons uniquely using integer values.
POLYGON_HANDLE World::GeneratePolygonHandle()
{
	return __nextHandle++;
}

// Handles a single physics step.
void World::Step( float deltaTimeSeconds )
{
	// Clean out collisions from last frame.
	__collisions.clear();

	// Collision detection.
	for( auto aIterator = __polygons.begin(); aIterator != __polygons.end(); ++aIterator )
	{
		Polygon* aPolygon = aIterator->second;

		for( auto bIterator = aIterator; bIterator != __polygons.end(); ++bIterator )
		{
			Polygon* bPolygon = bIterator->second;

			// We don't want to test collisions between this polygon and itself!
			if( aPolygon == bPolygon )
			{
				continue;
			}

			// Actually test whether this pair collides and store the collision if so.
			Collision collision;
			if( TestCollision( aPolygon, bPolygon, &collision ) )
			{
				__collisions.push_back( collision );
			}
		}
	}

	// Collision resolution.
	for( auto collision : __collisions )
	{
		CollisionResponse(collision.facePolygon, collision.contactPolygon, collision);		
	}

	// Integrate force -> acceleration -> velocity -> position.
	for ( auto aIterator = __polygons.begin(); aIterator != __polygons.end(); ++aIterator )
	{
		Polygon* aPolygon = aIterator->second;

		// Apply gravity if this polygon is expecting it.
		if ( aPolygon->GetUseGravity() )
		{
			aPolygon->Accelerate( glm::vec2( 0.0f, __gravityAcceleration * deltaTimeSeconds ) );
		}

		// Maybe here is a good place to account for any forces acting on your polygons...
		// TODO

		// Integrate velocity to position (Euler method).
		aPolygon->Translate( aPolygon->GetVelocity() * deltaTimeSeconds );

		// Integrate rotational velocity to rotation (Euler method).
		aPolygon->Rotate( aPolygon->GetRotationalVelocity() * deltaTimeSeconds );
	}
}

bool World::TestCollision( Polygon* aPolygon, Polygon* bPolygon, Collision* maybeCollision )
{
	// Test SAT with the faces of aPolygon and the vertices of bPolygon.
	if( !TestSeparateAxisTheorem( aPolygon, bPolygon, maybeCollision ) )
	{
		return false;
	}

	// Test SAT with the faces of bPolygon and the vertices of aPolygon.
	if( !TestSeparateAxisTheorem( bPolygon, aPolygon, maybeCollision ) )
	{
		return false;
	}

	// If we haven't exited out yet, return the collision object.
	return true;
}

bool World::TestSeparateAxisTheorem( Polygon* facePolygon, Polygon* vertexPolygon, Collision* maybeCollision )
{
	// For each face in bPolygon
	for( Face aFace : facePolygon->GetFaces() )
	{
		// Find the vertex in bPolygon with the minimum distance from the face.
		float minDistance = FLT_MAX;
		glm::vec2 minVertex;
		for( glm::vec2 bVertex : vertexPolygon->GetGlobalVertices() )
		{
			float distance = aFace.GetGlobalDistance( bVertex );
			if( distance < minDistance )
			{
				minDistance = distance;
				minVertex = bVertex;
			}
		}

		// If the distance to the nearest vertex in b is greater than 0, we can't be in collision.
		if( minDistance > 0 )
		{
			return false;
		}

		// The least negative distance is the best candidate for depenetration.
		if( minDistance > maybeCollision->depth )
		{
			maybeCollision->facePolygon = facePolygon;
			maybeCollision->contactPolygon = vertexPolygon;
			maybeCollision->contactVertex = minVertex;
			maybeCollision->depth = minDistance;
			maybeCollision->faceNormal = aFace.GetGlobalNormal();
		}
	}

	return true;
}

void World::CollisionResponse(Polygon * aPolygon, Polygon * bPolygon, Collision collision)
{

	glm::vec2 relativeVelocity = bPolygon->GetVelocity() - aPolygon->GetVelocity(); // Get relative velocity between both polygons
	float contactVelocity = glm::dot(relativeVelocity, collision.faceNormal); // Use dot product to get the contact velocity on the face normal
	//No need to continue if velocities are separating already
	if (contactVelocity > 0)
		return;

	//Default restitution is 1.0f
	//TODO add restitution to polygon
	float restitution = 1.0f;

	float impulseScalar = -(1.0f + restitution) * contactVelocity;
	float invMassA = 1 / aPolygon->GetMass();
	float invMassB = 1 / bPolygon->GetMass();
	impulseScalar /= invMassA + invMassB;
	glm::vec2 impulse = impulseScalar * collision.faceNormal; // Impulse is the impulse scalar multiplied by the normal of the face polygon

	if(!aPolygon->GetIsStatic()) // if polygon is not static then we react to the collision
		aPolygon->SetVelocity(aPolygon->GetVelocity() - (invMassA * impulse)); // Add impulse velocity
	if (!bPolygon->GetIsStatic()) 
	{
		bPolygon->SetVelocity(bPolygon->GetVelocity() + (invMassB * impulse));
		bPolygon->SetRotationalVelocity(-collision.GetAngularMomentum());
	}

}



// PUBLIC

// Constructor: Defaults a bunch of values on startup.
World::World( float fixedTimestepSeconds, float gravityAcceleration )
	: __accumulatedTimeSeconds( 0.0f )
	, __gravityAcceleration( gravityAcceleration )
	, __fixedTimestepSeconds( fixedTimestepSeconds )
	, __nextHandle( 1 )
	, __polygons( std::map<POLYGON_HANDLE, Polygon*>() )
{

}

// Destructor: Doesn't need to do anything.
World::~World()
{

}

// Update the World's clock by taking the in deltaTimeSeconds and calling Step() once for each 
// interval of __fixedTimestepSeconds so the simulations catches up to real time.
// Note: This is the time accumulator pattern that we'll discuss in class!
void World::Update( float deltaTimeSeconds )
{
	__accumulatedTimeSeconds += deltaTimeSeconds;
	while( __accumulatedTimeSeconds >= __fixedTimestepSeconds )
	{
		__accumulatedTimeSeconds -= __fixedTimestepSeconds;
		Step( __fixedTimestepSeconds );
		__currentTimeSeconds += __fixedTimestepSeconds;
	}
}

// Create a new Polygon instance and store it in the __polygons map so we can look it up by its
// handle later.
POLYGON_HANDLE World::CreatePolygon( std::vector<glm::vec2>* vertices, glm::vec2 position, float rotation, float mass, bool useGravity, bool isStatic )
{
	auto handle = GeneratePolygonHandle();
	__polygons.emplace( handle, new Polygon( vertices, position, rotation, mass, useGravity, isStatic ) );
	return handle;
}

// Destroy the Polygon at the provided handle by erasing the mapping in __polygons and deleting 
// the Polygon instance from the heap.
void World::DestroyPolygon( POLYGON_HANDLE handle )
{
	auto pair = __polygons.find( handle );
	Polygon* polygon = pair->second;
	__polygons.erase( pair );
	delete polygon;
}

// If a Polygon exists at the provided handle, return a reference to it.
Polygon* World::GetPolygon( POLYGON_HANDLE handle )
{
	auto hasHandle = __polygons.count( handle );
	if( !hasHandle )
	{
		throw std::exception( "No polygon exists at this handle!" );
	}
	return __polygons.at( handle );
}

// Get the current physics clock time. This time exactly reflects the amount of time that the 
// World has simulated up to now and does not include accumulated time that has not factored 
// into a simulation step yet.
float World::GetCurrentTimeSeconds()
{
	return __currentTimeSeconds;
}

// Check if 2 polygons are intersecting.
bool World::IsPolygonColliding( Polygon* polygon )
{
	for( Collision collision : __collisions )
	{
		if( polygon == collision.facePolygon )
		{
			return true;
		}
		else if( polygon == collision.contactPolygon )
		{
			return true;
		}
	}
	return false;
}