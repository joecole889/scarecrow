#ifndef _STATE_MACHINE_H
#define _STATE_MACHINE_H

#include <mutex>
 
struct StateStruct;
 
// base class for state machines
class StateMachine
{
public:
    StateMachine(unsigned char maxStates);
    virtual ~StateMachine() {}
protected:
    enum { EVENT_IGNORED = 0xFE, CANNOT_HAPPEN };
    unsigned char currentState;
    void ExternalEvent(unsigned char);
    void InternalEvent(unsigned char);
    virtual const StateStruct* GetStateMap() = 0;
private:
    const unsigned char _maxStates;
    bool _eventGenerated;
    void StateEngine(void);
    std::mutex mtx;
};
 
typedef void (StateMachine::*StateFunc)();
struct StateStruct 
{
    StateFunc pStateFunc;    
};
 
#define BEGIN_STATE_MAP \
public:\
const StateStruct* GetStateMap() {\
    static const StateStruct StateMap[] = { 
 
#define STATE_MAP_ENTRY(stateFunc)\
    { reinterpret_cast<StateFunc>(stateFunc) },
 
#define END_STATE_MAP \
    }; \
    return &StateMap[0]; }
 
#define BEGIN_TRANSITION_MAP \
    static const unsigned char TRANSITIONS[] = {\
 
#define TRANSITION_MAP_ENTRY(entry)\
    entry,
 
#define END_TRANSITION_MAP \
    0 };\
    ExternalEvent(TRANSITIONS[currentState]);
 
#endif //STATE_MACHINE_H
