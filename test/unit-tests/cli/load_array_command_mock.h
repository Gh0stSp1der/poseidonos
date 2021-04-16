#include <gmock/gmock.h>

#include <list>
#include <string>
#include <vector>

#include "src/cli/load_array_command.h"

namespace pos_cli
{
class MockLoadArrayCommand : public LoadArrayCommand
{
public:
    using LoadArrayCommand::LoadArrayCommand;
    MOCK_METHOD(string, Execute, (json & doc, string rid), (override));
};

} // namespace pos_cli