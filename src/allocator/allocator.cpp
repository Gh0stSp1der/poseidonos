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

#include "src/allocator/allocator.h"

#include <fstream>
#include <sstream>
#include <string>

#include "src/allocator_service/allocator_service.h"
#include "src/array_models/interface/i_array_info.h"
#include "src/include/pos_event_id.h"
#include "src/logger/logger.h"
#include "src/meta_file_intf/mock_file_intf.h"
#include "src/sys_event/volume_event_publisher.h"

namespace pos
{
Allocator::Allocator(IArrayInfo* info, IStateControl* iState)
: VolumeEvent("Allocator", info->GetName()),
  addrInfo(nullptr),
  contextManager(nullptr),
  blockManager(nullptr),
  wbStripeManager(nullptr),
  isInitialized(false),
  iArrayInfo(info),
  iStateControl(iState)
{
    VolumeEventPublisherSingleton::Instance()->RegisterSubscriber(this, info->GetName());
    _CreateSubmodules();
}

Allocator::~Allocator(void)
{
    VolumeEventPublisherSingleton::Instance()->RemoveSubscriber(this, iArrayInfo->GetName());
    _DeleteSubmodules();
}

int
Allocator::Init(void)
{
    if (false == isInitialized)
    {
        addrInfo->Init(iArrayInfo->GetName());
        contextManager->Init();
        blockManager->Init(wbStripeManager);
        wbStripeManager->Init();

        _RegisterToAllocatorService();
        isInitialized = true;
    }

    return 0;
}

void
Allocator::_CreateSubmodules(void)
{
    addrInfo = new AllocatorAddressInfo();
    contextManager = new ContextManager(addrInfo, iArrayInfo->GetName());
    blockManager = new BlockManager(addrInfo, contextManager, iArrayInfo->GetName());
    wbStripeManager = new WBStripeManager(addrInfo, contextManager, blockManager, iArrayInfo->GetName());
}

void
Allocator::_RegisterToAllocatorService(void)
{
    std::string arrayName = iArrayInfo->GetName();
    AllocatorService* allocatorService = AllocatorServiceSingleton::Instance();
    allocatorService->RegisterAllocator(arrayName, GetIBlockAllocator());
    allocatorService->RegisterAllocator(arrayName, GetIWBStripeAllocator());
    allocatorService->RegisterAllocator(arrayName, GetIWBStripeCtx());
    allocatorService->RegisterAllocator(arrayName, GetISegmentCtx());
    allocatorService->RegisterAllocator(arrayName, GetIRebuildCtx());
    allocatorService->RegisterAllocator(arrayName, GetIAllocatorCtx());
    allocatorService->RegisterAllocator(arrayName, GetIAllocatorWbt());
}

void
Allocator::_UnregisterFromAllocatorService(void)
{
    std::string arrayName = iArrayInfo->GetName();
    AllocatorService* allocatorService = AllocatorServiceSingleton::Instance();
    allocatorService->UnregisterAllocator(arrayName);
}

void
Allocator::Dispose(void)
{
    if (isInitialized == true)
    {
        int eventId = static_cast<int>(POS_EVENT_ID::ARRAY_UNMOUNTING);

        POS_TRACE_INFO(eventId, "Start flushing all active stripes");
        wbStripeManager->FlushAllActiveStripes();

        POS_TRACE_INFO(eventId, "Start allocator contexts store");
        contextManager->StoreAllocatorCtxs();
        contextManager->Close();

        _UnregisterFromAllocatorService();
        isInitialized = false;
    }
}

void
Allocator::_DeleteSubmodules(void)
{
    delete wbStripeManager;
    delete blockManager;
    delete contextManager;
    delete addrInfo;
}

IBlockAllocator*
Allocator::GetIBlockAllocator(void)
{
    return blockManager;
}

IWBStripeAllocator*
Allocator::GetIWBStripeAllocator(void)
{
    return wbStripeManager;
}

IAllocatorCtx*
Allocator::GetIAllocatorCtx(void)
{
    return contextManager;
}

IWBStripeCtx*
Allocator::GetIWBStripeCtx(void)
{
    return contextManager;
}

IRebuildCtx*
Allocator::GetIRebuildCtx(void)
{
    return contextManager->GetRebuldCtx();
}

ISegmentCtx*
Allocator::GetISegmentCtx(void)
{
    return contextManager->GetSegmentCtx();
}

IAllocatorWbt*
Allocator::GetIAllocatorWbt(void)
{
    return this;
}

//----------------------------------------------------------------------------//

bool
Allocator::VolumeUnmounted(std::string volName, int volID, std::string arrayName)
{
    std::vector<Stripe*> stripesToFlush;
    std::vector<StripeId> vsidToCheckFlushDone;

    {
        std::unique_lock<std::mutex> lock(contextManager->GetCtxLock());
        wbStripeManager->PickActiveStripe(volID, stripesToFlush, vsidToCheckFlushDone);
    }

    wbStripeManager->FinalizeWriteIO(stripesToFlush, vsidToCheckFlushDone);
    return true;
}

void
Allocator::SetGcThreshold(uint32_t inputThreshold)
{
    contextManager->GetSegmentCtx()->SetGcThreshold(inputThreshold);
}

void
Allocator::SetUrgentThreshold(uint32_t inputThreshold)
{
    contextManager->GetSegmentCtx()->SetUrgentThreshold(inputThreshold);
}

int
Allocator::GetMeta(AllocatorCtxType type, std::string fname)
{
    MetaFileIntf* dumpFile = new MockFileIntf(fname, iArrayInfo->GetName());
    int ret = dumpFile->Create(0);
    if (ret < 0)
    {
        POS_TRACE_ERROR(EID(ALLOCATOR_START), "Failed to open output file {}", fname);
        return -EID(ALLOCATOR_START);
    }

    dumpFile->Open();
    uint64_t curOffset = 0;

    if (SEGMENT_VALID_COUNT == type)
    {
        uint32_t len = sizeof(uint32_t) * addrInfo->GetnumUserAreaSegments();
        char* buf = new char[len]();
        char* src = (char*)contextManager->GetSegmentCtx()->GetSegmentInfo()->GetValidBlockCountPool();
        memcpy(buf, src, len);

        ret = dumpFile->IssueIO(MetaFsIoOpcode::Write, 0, len, buf);
        if (ret < 0)
        {
            POS_TRACE_ERROR(EID(ALLOCATOR_META_ARCHIVE_STORE), "Sync Write to {} Failed, ret:{}", fname, ret);
            ret = -EID(ALLOCATOR_META_ARCHIVE_STORE);
        }
        delete[] buf;
    }
    else if (SEGMENT_OCCUPIED_STRIPE == type)
    {
        uint32_t len = sizeof(uint32_t) * addrInfo->GetnumUserAreaSegments();
        char* buf = new char[len]();
        char* src = (char*)contextManager->GetSegmentCtx()->GetSegmentInfo()->GetOccupiedStripeCountPool();
        memcpy(buf, src, len);

        ret = dumpFile->IssueIO(MetaFsIoOpcode::Write, 0, len, buf);
        if (ret < 0)
        {
            POS_TRACE_ERROR(EID(ALLOCATOR_META_ARCHIVE_STORE), "Sync Write to {} Failed, ret:{}", fname, ret);
            ret = -EID(ALLOCATOR_META_ARCHIVE_STORE);
        }
        delete[] buf;
    }
    else
    {
        if (WB_LSID_BITMAP == type)
        {
            uint32_t numBitsSet = contextManager->GetWbLsidBitmap()->GetNumBitsSet();
            ret = dumpFile->AppendIO(MetaFsIoOpcode::Write, curOffset, sizeof(numBitsSet), (char*)&numBitsSet);
        }
        else if (SEGMENT_BITMAP == type)
        {
            uint32_t numBitsSet = contextManager->GetSegmentCtx()->GetSegmentBitmap()->GetNumBitsSet();
            ret = dumpFile->AppendIO(MetaFsIoOpcode::Write, curOffset, sizeof(numBitsSet), (char*)&numBitsSet);
        }
        // ACTIVE_STRIPE_TAIL, CURRENT_SSD_LSID, SEGMENT_STATE
        else
        {
            // Do nothing
        }
        if (ret < 0)
        {
            POS_TRACE_ERROR(EID(ALLOCATOR_META_ARCHIVE_STORE), "Sync Write to {} Failed, ret:{}", fname, ret);
            ret = -EID(ALLOCATOR_META_ARCHIVE_STORE);
        }

        ret = dumpFile->AppendIO(MetaFsIoOpcode::Write, curOffset, contextManager->GetRawCtxInfo(type).GetSize(), contextManager->GetRawCtxInfo(type).GetAddr());
        if (ret < 0)
        {
            POS_TRACE_ERROR(EID(ALLOCATOR_META_ARCHIVE_STORE), "Sync Write to {} Failed, ret:{}", fname, ret);
            ret = -EID(ALLOCATOR_META_ARCHIVE_STORE);
        }
    }

    dumpFile->Close();
    delete dumpFile;
    return ret;
}

int
Allocator::SetMeta(AllocatorCtxType type, std::string fname)
{
    MetaFileIntf* fileProvided = new MockFileIntf(fname, iArrayInfo->GetName());
    int ret = 0;

    fileProvided->Open();
    uint64_t curOffset = 0;

    if (SEGMENT_VALID_COUNT == type)
    {
        uint32_t len = sizeof(uint32_t) * addrInfo->GetnumUserAreaSegments();
        char* pBuf = (char*)contextManager->GetSegmentCtx()->GetSegmentInfo()->GetValidBlockCountPool();

        ret = fileProvided->AppendIO(MetaFsIoOpcode::Read, curOffset, len, pBuf);
        if (ret < 0)
        {
            POS_TRACE_ERROR(EID(ALLOCATOR_META_ARCHIVE_LOAD), "Sync Read(SegmentValidBlockCount) from {} Failed, ret:{}", fname, ret);
            ret = -EID(ALLOCATOR_META_ARCHIVE_LOAD);
        }
    }
    else if (SEGMENT_OCCUPIED_STRIPE == type)
    {
        uint32_t len = sizeof(uint32_t) * addrInfo->GetnumUserAreaSegments();
        char* pBuf = (char*)contextManager->GetSegmentCtx()->GetSegmentInfo()->GetOccupiedStripeCountPool();

        ret = fileProvided->AppendIO(MetaFsIoOpcode::Read, curOffset, len, pBuf);
        if (ret < 0)
        {
            POS_TRACE_ERROR(EID(ALLOCATOR_META_ARCHIVE_LOAD), "Sync Read(SegmentOccupiedStripeCount) from {} Failed, ret:{}", fname, ret);
            ret = -EID(ALLOCATOR_META_ARCHIVE_LOAD);
        }
    }
    else
    {
        if (WB_LSID_BITMAP == type)
        {
            uint32_t numBitsSet = 0;
            ret = fileProvided->AppendIO(MetaFsIoOpcode::Read, curOffset, sizeof(numBitsSet), (char*)&numBitsSet);
            contextManager->GetWbLsidBitmap()->SetNumBitsSet(numBitsSet);
        }
        else if (SEGMENT_BITMAP == type)
        {
            uint32_t numBitsSet = 0;
            ret = fileProvided->AppendIO(MetaFsIoOpcode::Read, curOffset, sizeof(numBitsSet), (char*)&numBitsSet);
            contextManager->GetSegmentCtx()->GetSegmentBitmap()->SetNumBitsSet(numBitsSet);
        }
        // ACTIVE_STRIPE_TAIL, CURRENT_SSD_LSID, SEGMENT_STATE
        else
        {
            // do nothing
        }
    }

    fileProvided->Close();
    delete fileProvided;
    return ret;
}

int
Allocator::GetInstantMetaInfo(std::string fname)
{
    std::ostringstream oss;
    std::ofstream ofs(fname, std::ofstream::app);

    oss << "<< WriteBuffers >>" << std::endl;
    oss << "Set:" << std::dec << contextManager->GetWbLsidBitmap()->GetNumBitsSet() << " / ToTal:" << contextManager->GetWbLsidBitmap()->GetNumBits() << std::endl;
    oss << "activeStripeTail[] Info" << std::endl;
    for (int volumeId = 0; volumeId < MAX_VOLUME_COUNT; ++volumeId)
    {
        for (int idx = volumeId; idx < ACTIVE_STRIPE_TAIL_ARRAYLEN; idx += MAX_VOLUME_COUNT)
        {
            VirtualBlkAddr asTail = contextManager->GetActiveStripeTail(idx);
            oss << "Idx:" << std::dec << idx << " stripeId:0x" << std::hex << asTail.stripeId << " offset:0x" << asTail.offset << "  ";
        }
        oss << std::endl;
    }
    oss << std::endl;

    oss << "<< Segments >>" << std::endl;
    oss << "Set:" << std::dec << contextManager->GetSegmentCtx()->GetSegmentBitmap()->GetNumBitsSet() << " / ToTal:" << contextManager->GetSegmentCtx()->GetSegmentBitmap()->GetNumBits() << std::endl;
    oss << "currentSsdLsid: " << contextManager->GetSegmentCtx()->GetCurrentSsdLsid() << std::endl;
    for (uint32_t segmentId = 0; segmentId < addrInfo->GetnumUserAreaSegments(); ++segmentId)
    {
        SegmentStates& segState = contextManager->GetSegmentCtx()->GetSegmentState(segmentId);
        if ((segmentId > 0) && (segmentId % 4 == 0))
        {
            oss << std::endl;
        }
        oss << "SegmentId:" << segmentId << " state:" << static_cast<int>(segState.Getstate()) << " ValidBlockCnt:" << contextManager->GetSegmentCtx()->GetValidBlockCount(segmentId) << "  ";
    }
    oss << std::endl
        << std::endl;

    oss << "<< Rebuild >>" << std::endl;
    oss << "NeedRebuildCont:" << std::boolalpha << GetIRebuildCtx()->NeedRebuildAgain() << std::endl;
    oss << "TargetSegmentCount:" << contextManager->GetRebuldCtx()->GetTargetSegmentCnt() << std::endl;
    oss << "TargetSegnent ID" << std::endl;
    int cnt = 0;
    for (RTSegmentIter iter = contextManager->GetRebuldCtx()->RebuildTargetSegmentsBegin(); iter != contextManager->GetRebuldCtx()->RebuildTargetSegmentsEnd(); ++iter, ++cnt)
    {
        if (cnt > 0 && (cnt % 16 == 0))
        {
            oss << std::endl;
        }
        oss << *iter << " " << std::endl;
    }
    oss << std::endl;

    ofs << oss.str();
    return 0;
}

int
Allocator::GetBitmapLayout(std::string fname)
{
    std::ofstream ofs(fname, std::ofstream::app);

    ofs << "numWbStripe: 0x" << std::hex << addrInfo->GetnumWbStripes() << std::endl;
    ofs << "numUserAreaSegment: 0x" << std::hex << addrInfo->GetnumUserAreaSegments() << std::endl;
    ofs << "numUserAreaStripes: 0x" << std::hex << addrInfo->GetnumUserAreaStripes()
        << std::endl;
    ofs << "blksPerStripe: 0x" << std::hex << addrInfo->GetblksPerStripe() << std::endl;
    ofs << "ValidBlockCountSize: 0x" << std::hex << sizeof(uint32_t) << std::endl;
    ofs << std::endl;

    return 0;
}

void
Allocator::FlushAllUserdataWBT(void)
{
    std::vector<Stripe*> stripesToFlush;
    std::vector<StripeId> vsidToCheckFlushDone;

    contextManager->TurnOffBlkAllocation();
    wbStripeManager->CheckAllActiveStripes(stripesToFlush, vsidToCheckFlushDone);
    contextManager->TurnOnBlkAllocation();
    wbStripeManager->FinalizeWriteIO(stripesToFlush, vsidToCheckFlushDone);
}

} // namespace pos