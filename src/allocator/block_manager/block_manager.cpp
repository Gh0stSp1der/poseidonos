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

#include "src/allocator/context_manager/active_stripe_index_info.h"
#include "src/allocator/block_manager/block_manager.h"
#include "src/allocator/context_manager/segment/segment_ctx.h"
#include "src/mapper_service/mapper_service.h"
#include "src/include/branch_prediction.h"
#include "src/qos/qos_manager.h"
#include <string>

namespace pos
{

BlockManager::BlockManager(AllocatorAddressInfo* info, ContextManager* ctxMgr, std::string arrayName)
: addrInfo(info),
  contextManager(ctxMgr),
  iWBStripeInternal(nullptr),
  arrayName(arrayName)
{
}

void
BlockManager::Init(IWBStripeInternal* iwbstripeInternal)
{
    iWBStripeInternal = iwbstripeInternal;
}

VirtualBlks
BlockManager::AllocateWriteBufferBlks(uint32_t volumeId, uint32_t numBlks, bool forGC)
{
    VirtualBlks allocatedBlks;

    if (contextManager->IsblkAllocProhibited(volumeId) || ((forGC == false) && contextManager->IsuserBlkAllocProhibited()))
    {
        allocatedBlks.startVsa = UNMAP_VSA;
        allocatedBlks.numBlks = 0;
        return allocatedBlks;
    }

    ActiveStripeTailArrIdxInfo info = {volumeId, forGC};
    allocatedBlks = _AllocateBlks(info.GetActiveStripeTailArrIdx(), numBlks);
    return allocatedBlks;
}

void
BlockManager::InvalidateBlks(VirtualBlks blks)
{
    SegmentId segId = blks.startVsa.stripeId / addrInfo->GetstripesPerSegment();
    SegmentCtx* segCtx = contextManager->GetSegmentCtx();

    uint32_t validCount = segCtx->DecreaseValidBlockCount(segId, blks.numBlks);
    if (validCount == 0)
    {
        segCtx->FreeUserDataSegment(segId);
    }
}

void
BlockManager::ValidateBlks(VirtualBlks blks)
{
    SegmentId segId = blks.startVsa.stripeId / addrInfo->GetstripesPerSegment();
    SegmentCtx* segCtx = contextManager->GetSegmentCtx();

    segCtx->IncreaseValidBlockCount(segId, blks.numBlks);
}

void
BlockManager::ProhibitUserBlkAlloc(void)
{
    contextManager->ProhibitUserBlkAlloc();
}

void
BlockManager::PermitUserBlkAlloc(void)
{
    contextManager->PermitUserBlkAlloc();
}

bool
BlockManager::BlockAllocating(uint32_t volumeId)
{
    return contextManager->TurnOffVolumeBlkAllocation(volumeId);
}

void
BlockManager::UnblockAllocating(uint32_t volumeId)
{
    contextManager->TurnOnVolumeBlkAllocation(volumeId);
}
//----------------------------------------------------------------------------//
VirtualBlks
BlockManager::_AllocateBlks(ASTailArrayIdx asTailArrayIdx, int numBlks)
{
    assert(numBlks != 0);
    std::unique_lock<std::mutex> volLock(contextManager->GetActiveStripeTailLock(asTailArrayIdx));
    VirtualBlks allocatedBlks;
    VirtualBlkAddr curVsa = contextManager->GetActiveStripeTail(asTailArrayIdx);

    if (_IsStripeFull(curVsa) || IsUnMapStripe(curVsa.stripeId))
    {
        StripeId newVsid = UNMAP_STRIPE;
        int ret = _AllocateStripe(asTailArrayIdx, newVsid);
        if (likely(ret == 0))
        {
            allocatedBlks = _AllocateWriteBufferBlksFromNewStripe(asTailArrayIdx, newVsid, numBlks);
        }
        else
        {
            allocatedBlks.startVsa = UNMAP_VSA;
            allocatedBlks.numBlks = UINT32_MAX;
            return allocatedBlks;
        }
    }
    else if (_IsValidOffset(curVsa.offset + numBlks - 1) == false)
    {
        allocatedBlks.startVsa = curVsa;
        allocatedBlks.numBlks = addrInfo->GetblksPerStripe() - curVsa.offset;

        VirtualBlkAddr vsa = {.stripeId = curVsa.stripeId, .offset = addrInfo->GetblksPerStripe()};
        contextManager->SetActiveStripeTail(asTailArrayIdx, vsa);
    }
    else
    {
        allocatedBlks.startVsa = curVsa;
        allocatedBlks.numBlks = numBlks;

        VirtualBlkAddr vsa = {.stripeId = curVsa.stripeId, .offset = curVsa.offset + numBlks};
        contextManager->SetActiveStripeTail(asTailArrayIdx, vsa);
    }

    return allocatedBlks;
}

VirtualBlks
BlockManager::_AllocateWriteBufferBlksFromNewStripe(ASTailArrayIdx asTailArrayIdx, StripeId vsid, int numBlks)
{
    VirtualBlkAddr curVsa = {.stripeId = vsid, .offset = 0};

    VirtualBlks allocatedBlks;
    allocatedBlks.startVsa = curVsa;

    if (_IsValidOffset(numBlks))
    {
        allocatedBlks.numBlks = numBlks;
    }
    else
    {
        allocatedBlks.numBlks = addrInfo->GetblksPerStripe();
    }
    curVsa.offset = allocatedBlks.numBlks;

    // Temporally no lock required, as AllocateBlks and this function cannot be executed in parallel
    // TODO(jk.man.kim): add or move lock to wbuf tail manager
    contextManager->SetActiveStripeTail(asTailArrayIdx, curVsa);

    return allocatedBlks;
}

int
BlockManager::_AllocateStripe(ASTailArrayIdx asTailArrayIdx, StripeId& vsid)
{
    // 1. WriteBuffer Logical StripeId Allocation
    StripeId wbLsid = _AllocateWriteBufferStripeId();
    if (wbLsid == UNMAP_STRIPE)
    {
        return -EID(ALLOCATOR_CANNOT_ALLOCATE_STRIPE);
    }

    // 2. SSD Logical StripeId Allocation
    bool isUserStripeAlloc = _IsUserStripeAllocation(asTailArrayIdx);
    StripeId arrayLsid = _AllocateUserDataStripeIdInternal(isUserStripeAlloc);
    if (IsUnMapStripe(arrayLsid))
    {
        std::lock_guard<std::mutex> lock(contextManager->GetCtxLock());
        _RollBackStripeIdAllocation(wbLsid, UINT32_MAX);
        return -EID(ALLOCATOR_CANNOT_ALLOCATE_STRIPE);
    }
    // If arrayLsid is the front and first stripe of Segment
    if (_IsSegmentFull(arrayLsid))
    {
        SegmentId segId = arrayLsid / addrInfo->GetstripesPerSegment();
        contextManager->GetSegmentCtx()->UsedSegmentStateChange(segId, SegmentState::NVRAM);
    }
    StripeId newVsid = arrayLsid;

    // 3. Get Stripe object for wbLsid and link it with reverse map for vsid
    Stripe* stripe = iWBStripeInternal->GetStripe(wbLsid);
    stripe->Assign(newVsid, wbLsid, asTailArrayIdx);

    // TODO (jk.man.kim): Don't forget to insert array name in the future.
    IReverseMap* iReverseMap = MapperServiceSingleton::Instance()->GetIReverseMap(arrayName);
    if (unlikely(iReverseMap->LinkReverseMap(stripe, wbLsid, newVsid) < 0))
    {
        std::lock_guard<std::mutex> lock(contextManager->GetCtxLock());
        _RollBackStripeIdAllocation(wbLsid, arrayLsid);
        return -EID(ALLOCATOR_CANNOT_LINK_REVERSE_MAP);
    }

    // 4. Update the stripe map
    // TODO (jk.man.kim): Don't forget to insert array name in the future.
    IStripeMap* iStripeMap = MapperServiceSingleton::Instance()->GetIStripeMap(arrayName);
    iStripeMap->SetLSA(newVsid, wbLsid, IN_WRITE_BUFFER_AREA);

    vsid = newVsid;
    return 0;
}

StripeId
BlockManager::_AllocateWriteBufferStripeId(void)
{
    std::lock_guard<std::mutex> lock(contextManager->GetCtxLock());
    StripeId wbLsid = contextManager->GetWbLsidBitmap()->SetNextZeroBit();

    if (contextManager->GetWbLsidBitmap()->IsValidBit(wbLsid) == false)
    {
        return UNMAP_STRIPE;
    }
    QosManagerSingleton::Instance()->IncreaseUsedStripeCnt();
    return wbLsid;
}

StripeId
BlockManager::_AllocateUserDataStripeIdInternal(bool isUserStripeAlloc)
{
    std::lock_guard<std::mutex> lock(contextManager->GetCtxLock());
    SegmentCtx* segCtx = contextManager->GetSegmentCtx();
    RebuildCtx* rbCtx = contextManager->GetRebuldCtx();

    segCtx->SetPrevSsdLsid(segCtx->GetCurrentSsdLsid());
    StripeId ssdLsid = segCtx->GetCurrentSsdLsid() + 1;

    if (_IsSegmentFull(ssdLsid))
    {
        uint32_t freeSegments = contextManager->GetSegmentCtx()->GetNumOfFreeUserDataSegment();
        if (contextManager->GetSegmentCtx()->GetUrgentThreshold() >= freeSegments)
        {
            contextManager->ProhibitUserBlkAlloc();
            if (isUserStripeAlloc)
            {
                return UNMAP_STRIPE;
            }
        }

        SegmentId segmentId = contextManager->AllocateUserDataSegmentId();
        if (segmentId == UNMAP_SEGMENT)
        {
            // Under Rebuiling...
            if (rbCtx->IsRebuidTargetSegmentsEmpty() == false)
            {
                POS_TRACE_INFO(EID(ALLOCATOR_REBUILDING_SEGMENT), "Couldn't Allocate a SegmentId, seems Under Rebuiling");
                return UNMAP_STRIPE;
            }
            else
            {
                assert(false);
            }
        }
        ssdLsid = segmentId * addrInfo->GetstripesPerSegment();
    }

    segCtx->SetCurrentSsdLsid(ssdLsid);

    return ssdLsid;
}

void
BlockManager::_RollBackStripeIdAllocation(StripeId wbLsid, StripeId arrayLsid)
{
    if (wbLsid != UINT32_MAX)
    {
        contextManager->GetWbLsidBitmap()->ClearBit(wbLsid);
    }

    if (arrayLsid != UINT32_MAX)
    {
        SegmentCtx* segCtx = contextManager->GetSegmentCtx();
        if (_IsSegmentFull(arrayLsid))
        {
            SegmentId SegmentIdToClear = arrayLsid / addrInfo->GetstripesPerSegment();
            segCtx->GetSegmentBitmap()->ClearBit(SegmentIdToClear);
            segCtx->UsedSegmentStateChange(SegmentIdToClear, SegmentState::FREE);
        }
        segCtx->SetCurrentSsdLsid(segCtx->GetPrevSsdLsid());
    }
}

} // namespace pos