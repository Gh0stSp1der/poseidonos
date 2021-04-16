#include <gmock/gmock.h>

#include <list>
#include <string>
#include <vector>

#include "src/array/array.h"

namespace pos
{
class MockArray : public Array
{
public:
    using Array::Array;
    MOCK_METHOD(int, Init, (), (override));
    MOCK_METHOD(void, Dispose, (), (override));
    MOCK_METHOD(const PartitionLogicalSize*, GetSizeInfo, (PartitionType type), (override));
    MOCK_METHOD(DeviceSet<string>, GetDevNames, (), (override));
    MOCK_METHOD(string, GetName, (), (override));
    MOCK_METHOD(string, GetMetaRaidType, (), (override));
    MOCK_METHOD(string, GetDataRaidType, (), (override));
    MOCK_METHOD(string, GetCreateDatetime, (), (override));
    MOCK_METHOD(string, GetUpdateDatetime, (), (override));
    MOCK_METHOD(ArrayStateType, GetState, (), (override));
    MOCK_METHOD(uint32_t, GetRebuildingProgress, (), (override));
    MOCK_METHOD(bool, IsRecoverable, (IArrayDevice * target, UBlockDevice* uBlock), (override));
    MOCK_METHOD(IArrayDevice*, FindDevice, (string devSn), (override));
};

} // namespace pos