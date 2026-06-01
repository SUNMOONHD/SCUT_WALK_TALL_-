#include "passenger.h"

#include <utility>

Passenger::Passenger(int passengerId, std::string startNodeValue, std::string endNodeValue)
		: id(passengerId),
			startNode(std::move(startNodeValue)),
			endNode(std::move(endNodeValue)),
			currentNode(startNode),
			targetNode(endNode)
{
}
