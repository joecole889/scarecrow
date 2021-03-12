#include "Robot.h"

Robot::Robot(ScarecrowTarget* st) : StateMachine(ST_MAX_STATES),
                                    target(st),
                                    valve(GPIO_VALVE_PIN),
                                    on_target_threshold_x(ON_TARGET_THRESH),
                                    on_target_threshold_y(ON_TARGET_THRESH),
                                    target_poll_time(POLL_TIME),
                                    depth(0),
                                    pending_state_change(ATOMIC_FLAG_INIT) {
  g_object_ref(target);

  gpioExport(valve);
  gpioSetDirection(valve,outputPin);
  gpioSetValue(valve,off);
  valveStatus = off;

  port_handler = dynamixel::PortHandler::getPortHandler(DEVICENAME);
  packet_handler = dynamixel::PacketHandler::getPacketHandler(PROTOCOL_VERSION);
  group_sync_write_goal = new dynamixel::GroupSyncWrite(port_handler,
                                                        packet_handler,
                                                        ADDR_AX_GOAL_POS,
                                                        LEN_AX_GOAL_POS);
  if(!port_handler->openPort()) throw std::runtime_error("Couldn't open USB port.");
  if(!port_handler->setBaudRate(BAUDRATE)) throw std::runtime_error("Couldn't set USB port baud rate.");
}

Robot::~Robot() {
  g_object_unref(target);

  gpioSetValue(valve,off);
  valveStatus = off;
  gpioUnexport(valve);

  delete group_sync_write_goal;
  port_handler->closePort();
}

void Robot::AddTarget()
{
  pending_state_change.test_and_set();
  BEGIN_TRANSITION_MAP
    TRANSITION_MAP_ENTRY(EVENT_IGNORED)
    TRANSITION_MAP_ENTRY(ST_OFFTARGET)
    TRANSITION_MAP_ENTRY(EVENT_IGNORED)
    TRANSITION_MAP_ENTRY(EVENT_IGNORED)
  END_TRANSITION_MAP
}

void Robot::DelTarget(void)
{
  pending_state_change.test_and_set();
  BEGIN_TRANSITION_MAP
    TRANSITION_MAP_ENTRY(EVENT_IGNORED)
    TRANSITION_MAP_ENTRY(EVENT_IGNORED)
    TRANSITION_MAP_ENTRY(ST_SENTRY)
    TRANSITION_MAP_ENTRY(ST_SENTRY)
  END_TRANSITION_MAP
}

void Robot::Halt(void)
{
  pending_state_change.test_and_set();
  BEGIN_TRANSITION_MAP
    TRANSITION_MAP_ENTRY(EVENT_IGNORED)
    TRANSITION_MAP_ENTRY(ST_POWEROFF)
    TRANSITION_MAP_ENTRY(ST_POWEROFF)
    TRANSITION_MAP_ENTRY(ST_POWEROFF)
  END_TRANSITION_MAP
}

void Robot::ST_PowerOff()
{
  pending_state_change.clear();
  Torque(off);
}

void Robot::ST_Sentry()
{
  pending_state_change.clear();
  Torque(off);
}

void Robot::ST_OffTarget()
{
  pending_state_change.clear();
  gpioSetValue(valve,off);
  valveStatus = off;
  Torque(on);

  while(UpdateGoal()) {
    auto start = std::chrono::steady_clock::now();
    while(CannonOffTarget());
    auto end = std::chrono::steady_clock::now();
    auto int_usec = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
    auto sleep_time = target_poll_time - int_usec.count();
    if(sleep_time > 0)
      std::this_thread::sleep_for(std::chrono::microseconds(sleep_time));
  }

  if(!pending_state_change.test_and_set()) InternalEvent(ST_ONTARGET);
}

void Robot::ST_OnTarget()
{
  pending_state_change.clear();
  valveStatus = on;
  gpioSetValue(valve,on);

  while(UpdateGoal()) {
    auto start = std::chrono::steady_clock::now();
    while(!CannonOffTarget());
    auto end = std::chrono::steady_clock::now();
    auto int_usec = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
    auto sleep_time = target_poll_time - int_usec.count();
    if(sleep_time > 0)
      std::this_thread::sleep_for(std::chrono::microseconds(sleep_time));
  }

  if(!pending_state_change.test_and_set()) InternalEvent(ST_OFFTARGET);
}

bool Robot::CannonOffTarget() {
  int comm_result;
  uint8_t dxl_err = 0;
  uint16_t pres_x, pres_y;

  comm_result = packet_handler->read2ByteTxRx(port_handler,
                                              DXL_AZMTH_ID,
                                              ADDR_AX_PRES_POS,
                                              &pres_x,
                                              &dxl_err);
  if(comm_result != 0)
    throw std::runtime_error(packet_handler->getTxRxResult(comm_result));
  else if(dxl_err != 0)
    throw std::runtime_error(packet_handler->getRxPacketError(dxl_err));

  comm_result = packet_handler->read2ByteTxRx(port_handler,
                                              DXL_ELEV_ID,
                                              ADDR_AX_PRES_POS,
                                              &pres_y,
                                              &dxl_err);
  if(comm_result != 0)
    throw std::runtime_error(packet_handler->getTxRxResult(comm_result));
  else if(dxl_err != 0)
    throw std::runtime_error(packet_handler->getRxPacketError(dxl_err));

  return (!pending_state_change.test_and_set()) &&
         ((abs(pres_x - GoalTypeConvert(goal_x)) > on_target_threshold_x) ||
          (abs(pres_y - GoalTypeConvert(goal_y)) > on_target_threshold_y));
}

bool Robot::UpdateGoal() {
  int comm_result;

  scarecrow_target_get_location(target,goal_x,goal_y,&depth);
  DepthCorrection();

  if(!group_sync_write_goal->addParam(DXL_AZMTH_ID, goal_x)) {
    auto err_str = std::string("Failed to write goal position for Dynamixel at address ");
    throw std::runtime_error(err_str + std::to_string(DXL_AZMTH_ID));
  }
  if(!group_sync_write_goal->addParam(DXL_ELEV_ID, goal_y)) {
    auto err_str = std::string("Failed to write goal position for Dynamixel at address ");
    throw std::runtime_error(err_str + std::to_string(DXL_ELEV_ID));
  }
  comm_result = group_sync_write_goal->txPacket();
  if(comm_result != 0)
    throw std::runtime_error(packet_handler->getTxRxResult(comm_result));
  group_sync_write_goal->clearParam();
  return (!pending_state_change.test_and_set());
}

void Robot::Torque(bool on_or_off) {
  int comm_result;
  uint8_t dxl_err = 0;
  comm_result = packet_handler->write1ByteTxRx(port_handler,
                                               DXL_AZMTH_ID,
                                               ADDR_AX_TORQUE_ENABLE,
                                               static_cast<uint8_t>(on_or_off),
                                               &dxl_err);
  if(comm_result != 0)
    throw std::runtime_error(packet_handler->getTxRxResult(comm_result));
  else if(dxl_err != 0)
    throw std::runtime_error(packet_handler->getRxPacketError(dxl_err));

  comm_result = packet_handler->write1ByteTxRx(port_handler,
                                               DXL_ELEV_ID,
                                               ADDR_AX_TORQUE_ENABLE,
                                               static_cast<uint8_t>(on_or_off),
                                               &dxl_err);
  if(comm_result != 0)
    throw std::runtime_error(packet_handler->getTxRxResult(comm_result));
  else if(dxl_err != 0)
    throw std::runtime_error(packet_handler->getRxPacketError(dxl_err));

  return;
}

void Robot::DepthCorrection() {
  uint16_t corrected_y = GoalTypeConvert(goal_y);
  corrected_y += 0;
  goal_y[0] = DXL_LOBYTE(corrected_y);
  goal_y[1] = DXL_HIBYTE(corrected_y);
  return;
}

inline uint16_t Robot::GoalTypeConvert(const uint8_t* goal) {
  uint16_t g = goal[1];
  g <<= 8;
  g |= goal[0];
  return g;
}
