#include "../Game.h"
#include "../Item/Item.h"
#include "../Item/Elevator/Queue.h"
#include "../Item/Elevator/Car.h"
#include "../Route.h"
#include "Visitor.h"

using namespace OT;


Visitor::Visitor(Item::Item *parent) : Person(parent->game), parent(parent), destination(NULL), routeNeedsUpdate(false) {
	arrivalTime = 0;
	destArrivalTime = 0;
	Type types[] = {kMan, kWoman1, kWoman2, kWomanWithChild1};
	type = types[rand() % 4];
	state = OUTSIDE;
}

void Visitor::advance(double dt) {
	switch(state) {
	case OUTSIDE:
		if(game->time.absolute > arrivalTime) {
			// Arrived at tower
			game->mainLobby->addPerson(this);
			journey.toFloor = 0;
			atArrivalTime = game->time.absolute;
			state = VISITING;
		}
		break;
	case TRAVELLING:
		if(game->time.checkHour(4)) {
			// Kick out of tower if still travelling at this unearthly hour!
			LOG(DEBUG, "%p exiting tower", this);
			Item::Item *i = at;
			i->removePerson(this);
			state = OUTSIDE;
			break;
		}

		if(!destination->isOpen() || (game->buildingChanged && game->items.count(destination) == 0)) {
			// Destination has closed before visitor is able to reach it
			// OR destination has been deleted before visitor can reach it
			// Leave the tower instead
			decideDestination();
			
			LOG(DEBUG, "%s closed/deleted before %p can reach it.", destination->desc().c_str(), this);
		}
		break;
	case VISITING:
		if(at == destination) {
			if(destination == game->mainLobby) {
				game->mainLobby->removePerson(this);
				break;
			} else {
				atArrivalTime = game->time.absolute;
				destination = NULL;
			}
		}

		if(game->time.absolute >= atArrivalTime + at->visitDuration * Time::kBaseSpeed || !at->isOpen()) {
			// Time to leave item
			decideDestination();
		}

		break;
	}
}

void Visitor::decideDestination() {
	/*
		Called only when:
		1) Time to leave building
		2) Destination deleted (when travelling)
		3) Destination closed (when travelling)
	*/
	
	if(at == game->mainLobby) {
		destination = parent;
	} else {
		if(state == TRAVELLING) {
			destination = game->mainLobby;
		} else if(state == VISITING) {
			// Possible to decide to visit other nearby buildings in future instead of leaving tower immediately.
			destination = game->mainLobby;
		}
	}

	routeNeedsUpdate = true;
	updateRoute();
}

void Visitor::advanceRoute() {
	if(routeNeedsUpdate)
		return;
	else if(!newRoute.empty()) {
		int numStairs = journey.numStairs;
		int numEscalators = journey.numEscalators;
		int numElevators = journey.numElevators;
		journey.set(newRoute); // Change to new route immediately
		journey.numStairs = numStairs;
		journey.numEscalators = numEscalators;
		journey.numElevators = numElevators;
		newRoute.clear();
	} else
		journey.next();

	assert(at != NULL);
	atArrivalTime = game->time.absolute;
	if(!at->canHaulPeople()) {
		state = VISITING;
	} else {
		state = TRAVELLING;
	}
}

void Visitor::updateRoute() {
	routeNeedsUpdate = false;
	if(destination == NULL) return;

	switch(state) {
	case TRAVELLING: {
		int fromFloor = journey.toFloor;
		Item::Elevator::Car *c = NULL;

		if(at->isElevator()) {
			// Find actual floor to start from
			bool floorFound = false;
			Item::Elevator::Elevator *e = (Item::Elevator::Elevator *)at;

			if(!e->connectsFloor(fromFloor)) { // It BUGS!! If it doesn't connect, code will still create newRoute and WAIT for next to be called, which may be too late
				// Destination floor is now unconnected
				// Check queue 1st
				Item::Elevator::Queue *q = NULL;
				Item::Elevator::Elevator::Direction dir;
				if(fromFloor < journey.toFloor)
					dir = Item::Elevator::Elevator::kUp;
				else
					dir = Item::Elevator::Elevator::kDown;
				for (Item::Elevator::Elevator::Queues::iterator iq = e->queues.begin(); iq != e->queues.end(); ++iq) {
					if ((*iq)->floor == fromFloor && (*iq)->direction == dir) {
						q = *iq;
						break;
					}
				}

				if(q) {
					for(Item::Elevator::Queue::People::const_iterator ip = q->people.begin(); ip != q->people.end() && !floorFound; ++ip) {
						if(*ip == this) floorFound = true;
					}
				}

				if(floorFound) {
					// Still waiting in queue. Get out of elevator queue NOW and start on new route.
					journey.toFloor = journey.fromFloor;
				} else {
					// Check elevator cars
					for(Item::Elevator::Elevator::Cars::const_iterator ic = e->cars.begin(); ic != e->cars.end() && !floorFound; ++ic) {
						c = *ic;
						if(c->passengers.count(this) != 0) {
							if(dir == Item::Elevator::Elevator::kUp) {
								for(int i = (int)ceil(c->altitude); i < e->getRect().maxY() && !floorFound; ++i) {
									if(e->connectsFloor(i)) {
										fromFloor = i;
										floorFound = true;
									}
								}

								for(int i = (int)floor(c->altitude); i >= e->position.y && !floorFound; --i) {
									if(e->connectsFloor(i)) {
										fromFloor = i;
										floorFound = true;
									}
								}
							} else {
								for(int i = (int)floor(c->altitude); i >= e->position.y && !floorFound; --i) {
									if(e->connectsFloor(i)) {
										fromFloor = i;
										floorFound = true;
									}
								}

								for(int i = (int)ceil(c->altitude); i < e->getRect().maxY() && !floorFound; ++i) {
									if(e->connectsFloor(i)) {
										fromFloor = i;
										floorFound = true;
									}
								}
							}

							if(floorFound) {
								journey.toFloor = fromFloor; // For cases where visitor must get out of elevator car ASAP and start on the new route
							} else {
								// Elevator is unconnected to any floor!
								LOG(DEBUG, "%s is unconnected to any floor!", e->desc().c_str());
								e->removePerson(this);
								return;
							}
						}
					}
				}
			}
		}


		LOG(DEBUG, "Reassessing route for %p from %s@%d to %s", this, at->desc().c_str(), fromFloor, destination->desc().c_str());
		newRoute = game->findRoute(at, fromFloor, destination, journey);
		if(newRoute.empty()) {
			Item::Item *i = at;
			i->removePerson(this);
			state = OUTSIDE;
			//game->floorItems[fromFloor]->addPerson(this);
			//LOG(DEBUG, "Route for %p is empty! Throw to floor %d & retry again later.", this, fromFloor);
			LOG(DEBUG, "Route for %p is empty! Exiting tower.");
		} else if(journey.toFloor == journey.fromFloor) { // It BUGS! Cars are always cleared when new escalator is built. When escalator destroyed, no new passengers from middle floors.
			// Need to get out of queue and start on new route now
			int numStairs = journey.numStairs;
			int numEscalators = journey.numEscalators;
			int numElevators = journey.numElevators;
			journey.set(newRoute); // Change to new route immediately
			journey.numStairs = numStairs;
			journey.numEscalators = numEscalators;
			journey.numElevators = numElevators;
			newRoute.clear();
		}
		break;
	}
	case VISITING: {
		if(destination == at) {
			// Strange, but in any case, provide a dummy route
			newRoute.clear();
			newRoute.add(at, journey.fromFloor);
			newRoute.add(destination, destination->position.y + destination->prototype->entrance_offset);
			LOG(DEBUG, "%p Destination same as Start", this);
		} else if(at == game->mainLobby)	newRoute = destination->lobbyRoute;
		else								newRoute = game->findRoute(at, destination);

		if(newRoute.empty() && at != game->mainLobby && destination != game->mainLobby) {
			// In case the destination pick is inaccessible, just try to leave tower.
			destination = game->mainLobby;
			newRoute = game->findRoute(at, destination);
		}

		if (newRoute.empty()) {
			LOG(DEBUG, "%p has no route to leave", this);
			destination = NULL;
			if (game->time.hour > game->time.absoluteToHour(atArrivalTime + at->visitDuration * Time::kBaseSpeed) + 2 ||
				!at->isOpen()) {
					LOG(DEBUG, "%p exiting tower", this);
					Item::Item *i = at;
					i->removePerson(this);
					state = OUTSIDE;
			}
		} else {
			LOG(DEBUG, "%p leaving", this);
			Item::Item *i = at;
			i->removePerson(this);
			journey.set(newRoute);
			state = TRAVELLING;
			newRoute.clear();
		}
		break;
	}
	}
}

void Visitor::advance1(double dt) {
	/*
		at = NULL		: visitor hasn't arrived at tower yet (check arrival time)
		at = mainLobby	: visitor arrived at tower, in main lobby
						: check route to destination. if route available, set it. it not, check for available route for up to 1hr before quiting (leave the tower)
						: travel along route. if destination closed before reaching, stop travelling.
						: check route while travelling. if route becomes unavailable, check for available route for up to 1hr before quiting (leave the tower)
		at = destination: visitor reached destination. update destArrivalTime.
						: if visitor has stayed at destination for visitDuration, find route to leave destination. 
						: if no available route, keep trying for up to 1hr before quiting (leave the tower)
						: travel along route to main lobby.
	*/

	switch(state) {
	case OUTSIDE:
		if(game->time.absolute > arrivalTime) {
			// Arrived at tower
			game->mainLobby->addPerson(this);
			journey.toFloor = 0;
			atArrivalTime = game->time.absolute;
			state = TRAVELLING;
		}
		break;
	case TRAVELLING:
		if(at == destination) {
			if(destination == game->mainLobby) game->mainLobby->removePerson(this);
			else state = VISITING;
			break;
		}
		
		if((game->buildingChanged && game->items.count(destination) == 0) || !destination->isOpen()) {
			// Destination has closed before visitor is able to reach it
			// OR destination has been deleted before visitor can reach it (person should have been deleted by destination)
			// Leave the tower instead
			LOG(DEBUG, "%s closed before %p can reach it.", destination->desc().c_str(), this);
			destination = game->mainLobby;
			assessRoute();
			if(game->time.checkHour(4)) {
				LOG(DEBUG, "%p exiting tower", this);
				Item::Item *i = at;
				i->removePerson(this);
				state = OUTSIDE;
			}
		} else if(at == game->mainLobby) {
			// Just arrived, try to set journey to destination
			if(!destination->lobbyRoute.empty()) {
				LOG(DEBUG, "Starting journey to %s", destination->desc().c_str());
				journey.set(destination->lobbyRoute);
			} else if(game->time.hour > game->time.absoluteToHour(arrivalTime) + 2 || !destination->isOpen()) {
				LOG(DEBUG, "No route from main lobby to %s, %p exiting tower", destination->desc().c_str(), this);
				game->mainLobby->removePerson(this);
			}
		} else {
			//assessRoute(); // ?
		}
		break;
	case VISITING:
		if(game->time.absolute >= destArrivalTime + destination->visitDuration * Time::kBaseSpeed || !destination->isOpen()) {
			// Leave destination for lobby
			const Route &r = game->findRoute(at, game->mainLobby); // Customers may leave for different destinations besides main lobby, so this is not precomputed
			destination = game->mainLobby;
			destArrivalTime = 0;
			if (r.empty()) {
				LOG(DEBUG, "%p has no route to leave", this);
				if (game->time.hour > game->time.absoluteToHour(destArrivalTime + destination->visitDuration * Time::kBaseSpeed) + 2 ||
					!destination->isOpen()) {
						LOG(DEBUG, "%p exiting tower", this);
						Item::Item *i = at;
						i->removePerson(this);
						state = OUTSIDE;
				}
			} else {
				LOG(DEBUG, "%p leaving", this);
				Item::Item *i = at;
				i->removePerson(this);
				journey.set(r);
				state = TRAVELLING;
			}
		}
		break;
	}
}

void Visitor::advanceRoute1() {
	if(at) {
		if(!newRoute.empty()) {
			int numStairs = journey.numStairs;
			int numEscalators = journey.numEscalators;
			int numElevators = journey.numElevators;
			journey.set(newRoute); // Change to new route immediately
			journey.numStairs = numStairs;
			journey.numEscalators = numEscalators;
			journey.numElevators = numElevators;
			newRoute.clear();
		} else
			journey.next();
	}
}

void Visitor::assessRoute() {
	if(game->buildingChanged) {
		if(game->items.count(destination) == 0) {
			// Building deleted. Exit tower
			Item::Item *i = at;
			i->removePerson(this);
			state = OUTSIDE;
		}
		return;
	}

	// Re-plan route based on current location

	// Assume new transport constructed for now
	/*
		Find current position & floor: transport? building?
		If visitor not at destination already, need to reassess route.
		Find route from (AT, fromFloor) to destination, keeping in mind the transports already used to stay within limits
	*/

	int fromFloor = journey.toFloor;
	Item::Elevator::Car *c = NULL;

	if(at->isElevator()) {
		// Find actual floor to start from
		bool floorFound = false;
		Item::Elevator::Elevator *e = (Item::Elevator::Elevator *)at;

		if(!e->connectsFloor(fromFloor)) { // It BUGS!! If it doesn't connect, code will still create newRoute and WAIT for next to be called, which may be too late
			// Destination floor is now unconnected
			// Check queue 1st
			Item::Elevator::Queue *q = NULL;
			Item::Elevator::Elevator::Direction dir;
			if(fromFloor < journey.toFloor)
				dir = Item::Elevator::Elevator::kUp;
			else
				dir = Item::Elevator::Elevator::kDown;
			for (Item::Elevator::Elevator::Queues::iterator iq = e->queues.begin(); iq != e->queues.end(); ++iq) {
				if ((*iq)->floor == fromFloor && (*iq)->direction == dir) {
					q = *iq;
					break;
				}
			}

			if(q) {
				for(Item::Elevator::Queue::People::const_iterator ip = q->people.begin(); ip != q->people.end() && !floorFound; ++ip) {
					if(*ip == this) floorFound = true;
				}
			}

			if(!floorFound) {
				// Check elevator cars
				for(Item::Elevator::Elevator::Cars::const_iterator ic = e->cars.begin(); ic != e->cars.end() && !floorFound; ++ic) {
					c = *ic;
					if(c->passengers.count(this) != 0) {
						if(dir == Item::Elevator::Elevator::kUp) {
							for(int i = (int)ceil(c->altitude); i < e->getRect().maxY() && !floorFound; ++i) {
								if(e->connectsFloor(i)) {
									fromFloor = i;
									floorFound = true;
								}
							}

							for(int i = (int)floor(c->altitude); i >= e->position.y && !floorFound; --i) {
								if(e->connectsFloor(i)) {
									fromFloor = i;
									floorFound = true;
								}
							}
						} else {
							for(int i = (int)floor(c->altitude); i >= e->position.y && !floorFound; --i) {
								if(e->connectsFloor(i)) {
									fromFloor = i;
									floorFound = true;
								}
							}

							for(int i = (int)ceil(c->altitude); i < e->getRect().maxY() && !floorFound; ++i) {
								if(e->connectsFloor(i)) {
									fromFloor = i;
									floorFound = true;
								}
							}
						}

						if(!floorFound) {
							// Elevator is unconnected to any floor!
							LOG(DEBUG, "%s is unconnected to any floor!", e->desc().c_str());
							e->removePerson(this);
							return;
						}
					}
				}
			}

			if(!floorFound) {
				LOG(DEBUG, "Unexpected error looking for elevator fromFloor");
			}
		}
	}


	LOG(DEBUG, "Reassessing route for %p from %s@%d to %s", this, at->desc().c_str(), fromFloor, destination->desc().c_str());
	newRoute = game->findRoute(at, fromFloor, destination, journey);
	if(newRoute.empty()) {
		Item::Item *i = at;
		i->removePerson(this);
		state = OUTSIDE;
		//game->floorItems[fromFloor]->addPerson(this);
		//LOG(DEBUG, "Route for %p is empty! Throw to floor %d & retry again later.", this, fromFloor);
		LOG(DEBUG, "Route for %p is empty! Exiting tower.");
	} /*else {
		// Set journey properly. Need to take into account current position
		/ *std::vector<Route::Node>::const_iterator start = r.nodes.begin(); ++start;
		if(start->item->isElevator() && start->item == at) {
			// Continue journey on elevator to new floor
			// But next() will definitely be called. Can we workaround to ensure visitor stays in car to continue?
			journey.toFloor = start->toFloor;
		} else * /{
			/ *int numStairs = journey.numStairs;
			int numEscalators = journey.numEscalators;
			int numElevators = journey.numElevators;
			Item::Item *i = at;
			i->removePerson(this);
			journey.set(r); // Change to new route immediately
			journey.numStairs = numStairs;
			journey.numEscalators = numEscalators;
			journey.numElevators = numElevators;* /
		}
	}*/

	// Handle Building destruction
}
