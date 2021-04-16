#include <gmock/gmock.h>
#include <string>
#include <list>
#include <vector>
#include "src/gc/copier.h"

namespace pos
{
class MockCopier : public Copier
{
public:
    using Copier::Copier;
    MOCK_METHOD(bool, Execute, (), (override));
};

} // namespace pos