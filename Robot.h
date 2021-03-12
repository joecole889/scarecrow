#ifndef ROBOT_H
#define ROBOT_H
#include "StateMachine.h"
#include "jetsonGPIO.h"
#include "dynamixel_sdk.h"
#include "scarecrow-target.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <stdexcept>
 
#define GPIO_VALVE_PIN        gpio219
#define POLL_TIME             250 //in microseconds
#define ON_TARGET_THRESH      10
#define DEVICENAME            "/dev/ttyUSB0"
#define BAUDRATE              57600
#define PROTOCOL_VERSION      1.0
#define LEN_AX_GOAL_POS       2
#define LEN_AX_PRES_POS       2
#define ADDR_AX_TORQUE_ENABLE 24
#define ADDR_AX_GOAL_POS      30
#define ADDR_AX_PRES_POS      36
#define DXL_AZMTH_ID          1
#define DXL_ELEV_ID           2

// the Robot state machine class
class Robot : public StateMachine {
  public:
    Robot(ScarecrowTarget*);
    ~Robot();
   
    // external events taken by this state machine
    void AddTarget();
    void DelTarget();
    void Halt();

    // state map to define state function order
    BEGIN_STATE_MAP
      STATE_MAP_ENTRY(&Robot::ST_PowerOff)
      STATE_MAP_ENTRY(&Robot::ST_Sentry)
      STATE_MAP_ENTRY(&Robot::ST_OffTarget)
      STATE_MAP_ENTRY(&Robot::ST_OnTarget)
    END_STATE_MAP
   
  private:
    // state machine state functions
    void ST_PowerOff();
    void ST_Sentry();
    void ST_OffTarget();
    void ST_OnTarget();
   
    // state enumeration order must match the order of state
    // method entries in the state map
    enum E_States { 
      ST_POWEROFF = 0,
      ST_SENTRY,
      ST_OFFTARGET,
      ST_ONTARGET,
      ST_MAX_STATES
    };

    // utility functions
    bool CannonOffTarget();
    bool UpdateGoal();
    void Torque(bool);
    void DepthCorrection();
    static inline uint16_t GoalTypeConvert(const uint8_t*);
    
    // member data
    ScarecrowTarget* target;

    uint8_t goal_x[2], goal_y[2];
    uint16_t depth;
    uint16_t on_target_threshold_x, on_target_threshold_y;
    unsigned int target_poll_time;
    std::atomic_flag pending_state_change;
    unsigned int valveStatus;

    jetsonGPIO valve;
    dynamixel::PortHandler *port_handler;
    dynamixel::PacketHandler *packet_handler;
    dynamixel::GroupSyncWrite *group_sync_write_goal;
};
#endif //ROBOT_H
