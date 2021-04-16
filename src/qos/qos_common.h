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

#ifndef __IBOFOS_QOS_COMMON_H__
#define __IBOFOS_QOS_COMMON_H__

#include "src/device/base/ublock_device.h"
#include "src/event_scheduler/event.h"
#include "src/volume/volume_list.h"
#define IBOF_QOS_TIMESLICE_IN_USEC (1000)
#define M_MAX_REACTORS (100)
#define M_MAX_SUBSYSTEMS (1024)
#define M_KBYTES (1024ULL)
#define MAX_QOS_LIMIT (1000000 * M_KBYTES * M_KBYTES / IBOF_QOS_TIMESLICE_IN_USEC)
#define M_VALID_ENTRY (1)
#define M_INVALID_ENTRY (0)
#define M_NOBW_IN_MS (0)
#define M_NO_MAP (0)
#define M_DEFAULT_DEFICIET_WEIGHT (0)
#define M_RESET_TO_ZERO (0)
#define M_UINT32MAX (4294967295)
#define M_EQUAL_WEIGHT (16)
#define M_MAX_NVRAM_STRIPES (1023)
#define M_PING_PONG_BUFFER (2)
#define M_QOS_CORRECTION_CYCLE (2000)
#define M_STRIPES_CONSUMED_HIGH_THRESHOLD (1000)
#define MAX_IO_WORKER 2 // Currently this is hardcoded, will be taken from affinity manager in next revision
const int PRIO_CRIT_WT_1 = 16;
const int PRIO_CRIT_WT_2 = 10;
const int PRIO_CRIT_WT_3 = 0;
const int PRIO_HIGH_WT_1 = -4;
const int PRIO_HIGH_WT_2 = -20;
const int PRIO_HIGH_WT_3 = -100;
const int PRIO_LOW_WT_1 = -200;
const int PRIO_LOW_WT_2 = -400;
const int PRIO_LOW_WT_3 = -1000;

const uint64_t M_DEFAULT_MIN_BW = 100 * 1024 * 1024 / IBOF_QOS_TIMESLICE_IN_USEC;
const uint32_t M_BW_10_KB = 10 * 1024;
const uint16_t M_INVALID_SUBSYSTEM = 0;
const uint16_t M_VALID_SUBSYSTEM = 1;

namespace pos
{
class QosReturnCode
{
public:
    static const int SUCCESS = 0;
    static const int FAILURE = -1;
    static const int EVENT_POLICY_IN_EFFECT = 9000;
    static const int VOLUME_POLICY_IN_EFFECT = 9001;
    static const int MINIMUM_BW_APPLIED = 9002;
    static const int MINIMUM_BW_COMPROMISED = 9003;
    static const int INCREASE_BW_THROTTLING = 9004;
    static const int DECREASE_BW_THROTTLING = 9005;
};

// Parameters collected per millisecond
struct volume_qos_params
{
    volume_qos_params(void)
    {
        timestamp = 0;
        pendingIOsTransport = 0;
        currentIOs = 0;
        pendingBWTransport = 0;
        currentBW = 0;
        pendingIOs = 0;
        pendingBW = 0;
        usedBufferStripe = 0;
        valid = 0;
        for (int i = 0; i < 7; i++)
        {
            pad[i] = 0;
        }
    }
    uint32_t timestamp;
    uint32_t pendingIOsTransport;
    uint32_t currentIOs;         // per milli second
    uint32_t pendingBWTransport; // per milli second
    uint64_t currentBW;          // per milli second
    uint32_t pendingIOs;         // per milli second
    uint32_t pendingBW;          // per milli second
    uint32_t usedBufferStripe;
    uint32_t valid;
    uint32_t pad[7];
};

struct event_qos_params
{
    event_qos_params(void)
    {
        timestamp = 0;
        currentIOs = 0;
        currentBW = 0;
        pendingIO = 0;
        pendingBW = 0;
        valid = 0;
        for (int i = 0; i < 10; i++)
        {
            pad[i] = 0;
        }
    }
    uint32_t timestamp;
    uint32_t currentIOs; // per milli second
    uint64_t currentBW;  // per milli second
    uint32_t pendingIO;
    uint32_t pendingBW;
    uint32_t valid;
    uint32_t pad[10];
};

struct poller_structure
{
    poller_structure(void)
    {
        nextTimeStamp = 0;
        qosTimeSlice = 0;
        id = 0;
        for (int i = 0; i < 5; i++)
        {
            pad[i] = 0;
        }
    }
    uint64_t nextTimeStamp;
    uint64_t qosTimeSlice;
    uint64_t id;
    uint64_t pad[5];
};

enum QosState
{
    QosState_Start = 0,
    QosState_Init = QosState_Start,
    QosState_IssueDetect,
    QosState_CauseIdentify,
    QosState_ApplyCorrection,
    QosState_ResetThrottling,
    QosState_IssueResolved,
    QosState_End,
    QosStateCount = QosState_End - QosState_Start,
    QosState_Unknown
};
enum QosPriority
{
    QosPriority_Start = 0,
    QosPriority_Low = QosPriority_Start,
    QosPriority_High,
    QosPriority_End,
    QosPriorityCount = QosPriority_End - QosPriority_Start,
    QosPriority_Unknown
};

enum QosWorkloadType
{
    QosWorkloadType_Start = 0,
    QosWorkloadType_Read = QosWorkloadType_Start,
    QosWorkloadType_Write,
    QosWorkloadType_Mixed,
    QosWorkloadType_End,
    QosWorkloadTypeCount = QosWorkloadType_End - QosWorkloadType_Start,
    QosWorkloadType_Unknown
};

enum QosWorkloadRatio
{
    QosWorkloadRatio_Start = 0,
    QosWorkloadRatio_FullRead = QosWorkloadRatio_Start,
    QosWorkloadRatio_75Read,
    QosWorkloadRatio_50Read,
    QosWorkloadRatio_25Read,
    QosWorkloadRation_FullWrite,
    QosWorkloadRatio_End,
    QosWorkloadRatioCount = QosWorkloadRatio_End - QosWorkloadRatio_Start,
    QosWorkloadRatio_Unknown,
};

struct qos_vol_policy
{
    qos_vol_policy(void)
    : workLoad(QosWorkloadType_Unknown),
      ioSize(UINT32_MAX),
      rwRatio(QosWorkloadRatio_Unknown),
      minBW(UINT32_MAX),
      priority(QosPriority_Unknown),
      minGurantee(false)
    {
    }
    QosWorkloadType workLoad;
    uint32_t ioSize;
    QosWorkloadRatio rwRatio; // This can be a enum based value 0, 1 ,2 ,3,4
    uint32_t minBW;
    QosPriority priority;
    bool minGurantee;
};
enum QosCause
{
    QosCause_Start = 0,
    QosCause_Disk = QosCause_Start,
    QosCause_CPU,
    QosCause_NVRAM,
    QosCause_None,
    QosCause_End,
    QosCauseCount = QosCause_End - QosCause_Start,
    QosCause_Unknown,
};

struct qos_state_ctx
{
    qos_state_ctx(void)
    {
        causeType = QosCause_Unknown;
        previousCauseType = QosCause_Unknown;
        victimVol = MAX_VOLUME_COUNT + 1;
        correctionValue = 0;
        previousAvgBW = 0;
        correctionEvent = BackendEvent_Unknown;
        iterationCnt = 0;
    }
    QosCause causeType;
    QosCause previousCauseType;
    int victimVol;
    uint64_t correctionValue;
    uint64_t previousAvgBW;
    BackendEvent correctionEvent;
    uint16_t iterationCnt; // This iteration count will increment once the Cause is found.
};

struct oldVolState
{
    oldVolState(void)
    {
        currentData = 0;
        nvramUsage = 0;
    }
    uint32_t currentData;
    uint32_t nvramUsage; // NVRAM usage only for write usecase
};
struct volState
{
    volState(void)
    {
        currentData = 0;
        pendingData = 0;
        entries = 0;
        nvramUsage = 0;
        minBW = 0;
        maxBW = 0;
        priority = QosPriority_Unknown;
        workload = QosWorkloadType_Unknown;
    }
    uint32_t currentData;
    uint32_t pendingData;
    uint32_t entries;
    uint32_t nvramUsage; // NVRAM usage only for write usecase
    oldVolState pState;
    uint32_t minBW;
    uint32_t maxBW;
    QosPriority priority;
    QosWorkloadType workload;
};
struct flushEventData
{
    flushEventData(void)
    {
        nvramStripes = 0;
        reserved = 0;
    }
    uint32_t nvramStripes;
    uint32_t reserved;
};
struct gcEventData
{
    gcEventData(void)
    {
        freeSegment = 0;
        reserved = 0;
    }
    uint32_t freeSegment;
    uint32_t reserved;
};
struct rebuildEventData
{
    rebuildEventData(void)
    {
        metaRebuildStripe = 0;
        userRebuidSegment = 0;
    }
    uint32_t metaRebuildStripe;
    uint32_t userRebuidSegment;
};
struct metaEventData
{
    metaEventData(void)
    {
        freeLogBuffer = 0;
        reserved = 0;
    }
    uint32_t freeLogBuffer;
    uint32_t reserved;
};
struct oldEventState
{
    oldEventState(void)
    {
        currentData = 0;
        pendingCPUEvents = 0;
        totalEvents = 0;
    }
    uint32_t currentData;
    uint32_t pendingCPUEvents;
    uint32_t totalEvents; // No of events in this interval
};
struct eventState
{
    eventState(void)
    {
        currentData = 0;
        pendingCPUEvents = 0;
        totalEvents = 0;
    }
    uint32_t currentData;
    uint32_t pendingCPUEvents;
    uint32_t totalEvents; // No of events in this interval
    oldEventState pState;
    union
    {
        flushEventData flushData;
        gcEventData gcData;
        rebuildEventData rebuildData;
        metaEventData metaData;
    };
};
} // namespace pos
#endif // __IBOFOS_QOS_COMMON_H__