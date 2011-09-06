#include "Event.h"
#include "util/Assert.h"


//==============================================================================
// Constructor                                                                 =
//==============================================================================
Event::Event(EventType type_, float startTime_, float duration_)
:	type(type_),
	startTime(startTime_),
	duration(duration_)
{}


//==============================================================================
// operator=                                                                   =
//==============================================================================
Event& Event::operator=(const Event& b)
{
	type = b.type;
	startTime = b.startTime;
	duration = b.duration;
	return *this;
}


//==============================================================================
// update                                                                      =
//==============================================================================
void Event::update(float prevUpdateTime, float crntTime)
{
	ASSERT(!isDead(crntTime));

	// Dont update if its not the right time yet
	if(startTime <= crntTime)
	{
		updateSp(prevUpdateTime, crntTime);
	}
}