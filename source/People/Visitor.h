#pragma once
#include "../Application.h"
#include "Person.h"

namespace OT {
	namespace Item { class Item; }

	class Visitor : public Person
	{
	public:
		Visitor(Item::Item *dest);
		virtual ~Visitor() { LOG(DEBUG, "visitor %p killed", this); };

		Item::Item *destination;
		double arrivalTime; //when the vistor arrives at the lobby of the tower
		double destArrivalTime; //when the visitor arrives at the primary destination
		double atArrivalTime; //when the visitor arrives at the current item

		struct laterThan : public std::binary_function<Visitor *, Visitor *, bool> {
			bool operator() (const Visitor * _Left, const Visitor * _Right) const {
				return (_Left->arrivalTime > _Right->arrivalTime);
			}
		};

		bool routeNeedsUpdate;
		virtual void advance(double dt);	// Decides if person stays or leaves the item
		virtual void advanceRoute();		// Decides which item to move to next in the route, or to decide whether to turn around if building closed/destroyed i.e. decide route
		virtual void updateRoute();			// Re-assesses route to destination from current position

        // Old testing code (kept for reference, will be removed in a future update)
        virtual void advance1(double dt);	// Decides if person stays or leaves building.
        virtual void advanceRoute1();		// Decides which item to move to next in the route, or to decide whether to turn around if building closed/destroyed i.e. decide route
        virtual void assessRoute();			// Re-assesses route to destination from current position

	private:
		typedef enum {
			OUTSIDE,
			TRAVELLING,
			VISITING
		} State;
		State state;

		Route newRoute;
		Item::Item *parent;

		virtual void decideDestination();	// Decides on the next destination
	};
}
