#include <gmock/gmock.h>

#include <list>
#include <string>
#include <vector>

#include "src/cli/device_monitoring_state_command.h"

namespace pos_cli
{
class MockDeviceMonitoringStateCommand : public DeviceMonitoringStateCommand
{
public:
    using DeviceMonitoringStateCommand::DeviceMonitoringStateCommand;
    MOCK_METHOD(string, Execute, (json & doc, string rid), (override));
};

} // namespace pos_cli