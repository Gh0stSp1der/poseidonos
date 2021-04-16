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

#pragma once

#include "src/array_components/mount_temp/mount_temp.h"
#include "src/state/interface/i_state_observer.h"
#include "src/state/interface/i_state_control.h"

#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>

using namespace std;

namespace pos
{
class IMountSequence;
class IAbrControl;
class VolumeManager;

class ArrayMountSequence : public IStateObserver
{
public:
    ArrayMountSequence(vector<IMountSequence*> seq, IAbrControl* abr,
                       IStateControl* iState, string name);
    virtual ~ArrayMountSequence(void);
    int Mount(void);
    int Unmount(VolumeManager* volMgr);

    void StateChanged(StateContext* prev, StateContext* next) override;

private:
    bool _WaitState(StateContext* goal);

    MountTemp temp;
    IStateControl* state;
    vector<IMountSequence*> sequence;
    StateContext* mountState = nullptr;
    StateContext* unmountState = nullptr;
    StateContext* normalState = nullptr;
    std::mutex mtx;
    std::condition_variable cv;
};
} // namespace pos