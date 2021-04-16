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

#include "src/gc/stripe_copier.h"

#include "Air.h"
#include "src/event_scheduler/event_scheduler.h"
#include "src/gc/copier_read_completion.h"
#include "src/io_submit_interface/i_io_submit_handler.h"
#include "src/logger/logger.h"
#include "src/include/backend_event.h"

#include <string>

namespace pos
{
StripeCopier::StripeCopier(StripeId victimStripeId,
    CopierMeta* meta, uint32_t copyIndex)
: victimStripeId(victimStripeId),
  meta(meta),
  loadedValidBlock(false),
  listIndex(0),
  stripeOffset(victimStripeId % STRIPES_PER_SEGMENT),
  copyIndex(copyIndex)
{
    SetEventType(BackendEvent_GC);
}

StripeCopier::~StripeCopier(void)
{
}

bool
StripeCopier::Execute(void)
{
    if (false == loadedValidBlock)
    {
        bool isLoaded = meta->GetVictimStripe(copyIndex, stripeOffset)->LoadValidBlock();

        if (false == isLoaded)
        {
            return false;
        }

        loadedValidBlock = true;
        POS_TRACE_DEBUG((int)POS_EVENT_ID::GC_GET_VALID_BLOCKS,
            "Get valid blocks, (victimStripeId:{})",
            victimStripeId);
    }

    uint32_t listSize = meta->GetVictimStripe(copyIndex, stripeOffset)->GetBlkInfoListSize();
    if (0 != listSize)
    {
        uint32_t requestCount = 0;

        for (; listIndex < listSize;)
        {
            void* buffer;
            buffer = meta->GetBuffer(victimStripeId);

            if (nullptr == buffer)
            {
                meta->SetStartCopyBlks(requestCount);
                POS_TRACE_INFO((int)POS_EVENT_ID::GC_GET_BUFFER_FAILED,
                    "Get gc buffer failed and retry, victimStripeId:{}",
                    victimStripeId);
                return false;
            }

            std::list<BlkInfo> blkInfoList = meta->GetVictimStripe(copyIndex, stripeOffset)->GetBlkInfoList(listIndex);
            LogicalBlkAddr lsa;
            auto startBlkInfo = blkInfoList.begin();

            uint32_t startOffset = startBlkInfo->vsa.offset;

            uint32_t offset = (startOffset / BLOCKS_IN_CHUNK) * BLOCKS_IN_CHUNK;
            lsa = {victimStripeId, offset};

            EventSmartPtr copyEvent(new CopyEvent(buffer, lsa, listIndex, meta, victimStripeId, copyIndex));
            EventSchedulerSingleton::Instance()->EnqueueEvent(copyEvent);
            requestCount += blkInfoList.size();
            listIndex++;
        }
        meta->SetStartCopyBlks(requestCount);
    }
    meta->SetStartCopyStripes();

    victimStripeId += CopierMeta::GC_CONCURRENT_COUNT;
    if ((victimStripeId % meta->GetStripePerSegment() /*STRIPES_PER_SEGMENT*/) >= CopierMeta::GC_CONCURRENT_COUNT)
    {
        EventSmartPtr stripeCopier(new StripeCopier(victimStripeId, meta, copyIndex));
        EventSchedulerSingleton::Instance()->EnqueueEvent(stripeCopier);
    }

    return true;
}

StripeCopier::CopyEvent::CopyEvent(void* buffer,
    LogicalBlkAddr lsa,
    uint32_t listIndex,
    CopierMeta* meta,
    uint32_t stripeId,
    uint32_t copyIndex)
: buffer(buffer),
  lsa(lsa),
  listIndex(listIndex),
  meta(meta),
  stripeId(stripeId),
  copyIndex(copyIndex)
{
}
StripeCopier::CopyEvent::~CopyEvent(void)
{
}
bool
StripeCopier::CopyEvent::Execute(void)
{
    uint64_t numPage;
    const string& arrayName = meta->GetArrayName();

    numPage = BLOCKS_IN_CHUNK;

    BufferEntry bufferEntry(buffer, numPage /*block count*/);
    std::list<BufferEntry> bufferList;
    bufferList.push_back(bufferEntry);

    PartitionType partitionType = PartitionType::USER_DATA;

    CallbackSmartPtr callback(
        new CopierReadCompletion(meta->GetVictimStripe(copyIndex, stripeId % meta->GetStripePerSegment()),
            listIndex,
            buffer, meta,
            stripeId));
    callback->SetEventType(BackendEvent_GC);
    IIOSubmitHandler::GetInstance()->SubmitAsyncIO(IODirection::READ,
        bufferList, lsa, numPage,
        partitionType, callback, arrayName);
    return true;
}

} // namespace pos