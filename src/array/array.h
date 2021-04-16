/*
 *   BSD LICENSE
 *   Copyright (c) 2021 Samsung Electronics Corporation
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ARRAY_H_
#define ARRAY_H_

#include <list>
#include <string>

#include "array_interface.h"
#include "src/array/device/array_device_manager.h"
#include "src/array/meta/array_meta.h"
#include "src/array/partition/partition_manager.h"
#include "src/array/rebuild/i_array_rebuilder.h"
#include "src/array/service/io_device_checker/i_device_checker.h"
#include "src/array/state/array_state.h"
#include "src/array_models/interface/i_array_info.h"
#include "src/array_models/interface/i_mount_sequence.h"
#include "src/bio/ubio.h"
#include "src/include/address_type.h"
#include "src/include/array_config.h"

#ifdef _ADMIN_ENABLED
#include "src/array/device/i_array_device_manager.h"
#endif
using namespace std;

namespace pos
{
class DeviceManager;
class MbrManager;
class UBlockDevice;
class IAbrControl;
class IStateControl;

class Array : public IArrayInfo, public IMountSequence, public IDeviceChecker
{
    friend class ParityLocationWbtCommand;
    friend class GcWbtCommand;

public:
    Array(string name, IArrayRebuilder* rbdr, IAbrControl* abr, IStateControl* iState);
    Array(string name, IArrayRebuilder* rbdr, IAbrControl* abr, ArrayDeviceManager* devMgr, DeviceManager* sysDevMgr, PartitionManager* ptnMgr, ArrayState* arrayState, ArrayInterface* arrayInterface);
    virtual ~Array(void);
    int Init(void) override;
    void Dispose(void) override;
    int Load(void);
    int Create(DeviceSet<string> nameSet, string dataRaidType = "RAID5");
    int Delete(void);
    int AddSpare(string devName);
    int RemoveSpare(string devName);
    int DetachDevice(UblockSharedPtr uBlock);
    void MountDone(void);
    int CheckUnmountable(void);
    int CheckDeletable(void);
    void
    SetMetaRaidType(string raidType)
    {
        meta_.metaRaidType = raidType;
    };
    void
    SetDataRaidType(string raidType)
    {
        meta_.dataRaidType = raidType;
    };

    const PartitionLogicalSize* GetSizeInfo(PartitionType type) override;
    DeviceSet<string> GetDevNames(void) override;
    string GetName(void) override;
    string GetMetaRaidType(void) override;
    string GetDataRaidType(void) override;
    string GetCreateDatetime(void) override;
    string GetUpdateDatetime(void) override;
    ArrayStateType GetState(void) override;
    StateContext* GetStateCtx(void) override;
    uint32_t GetRebuildingProgress(void) override;
    bool IsRecoverable(IArrayDevice* target, UBlockDevice* uBlock) override;
    IArrayDevice* FindDevice(string devSn) override;
    bool TriggerRebuild(ArrayDevice* target);
#ifdef _ADMIN_ENABLED
    IArrayDevMgr* GetArrayManager(void);
#endif

private:
    int _LoadImpl(void);
    int _CreatePartitions(void);
    void _DeletePartitions(void);
    int _Flush(void);
    int _ResumeRebuild(ArrayDevice* target);
    void _RebuildDone(RebuildResult result);
    void _DetachSpare(ArrayDevice* target);
    void _DetachData(ArrayDevice* target);
    void _RegisterService(void);
    void _UnregisterService(void);
    void _ResumeRebuild(void);
    void _ResetMeta(void);

    ArrayState* state = nullptr;
    ArrayInterface* intf = nullptr;
    PartitionManager* ptnMgr = nullptr;

    ArrayMeta meta_;
    string name_;
    pthread_rwlock_t stateLock;
    ArrayDeviceManager* devMgr_;
    DeviceManager* sysDevMgr = nullptr;
    IArrayRebuilder* rebuilder = nullptr;
    static const int LOCK_ACQUIRE_FAILED;
    IAbrControl* abrControl = nullptr;
};
} // namespace pos
#endif // ARRAY_H_