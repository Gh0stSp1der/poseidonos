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

#include "src/gc/copier_read_completion.h"

#include "Air.h"
#include "src/include/backend_event.h"
#include "src/gc/gc_flush_submission.h"
#include "src/io/frontend_io/write_submission.h"
#include "src/include/branch_prediction.h"
#include "src/bio/volume_io.h"
#include "src/logger/logger.h"
#include "src/event_scheduler/event_scheduler.h"
#include "src/io/general_io/translator.h"
#include "src/io/backend_io/flush_submission.h"
#include "src/gc/gc_flush_submission.h"
#include "src/allocator_service/allocator_service.h"
#include "src/allocator/i_wbstripe_allocator.h"
#include "src/allocator/i_block_allocator.h"
#include "src/volume/volume_service.h"

namespace pos
{
CopierReadCompletion::CopierReadCompletion(VictimStripe* victimStripe,
    uint32_t listIndex,
    void* buffer,
    CopierMeta* meta,
    StripeId stripeId)
: Callback(false),
  victimStripe(victimStripe),
  listIndex(listIndex),
  buffer(buffer),
  meta(meta),
  stripeId(stripeId),
  allocatedCnt(0)
{
    list<BlkInfo> blkInfoList = victimStripe->GetBlkInfoList(listIndex);
    blkCnt = blkInfoList.size();
}

CopierReadCompletion::~CopierReadCompletion(void)
{
}

bool
CopierReadCompletion::_DoSpecificJob(void)
{
    GcStripeManager* gcStripeManager = meta->GetGcStripeManager();
    IVolumeManager* volumeManager
        = VolumeServiceSingleton::Instance()->GetVolumeManager(meta->GetArrayName());
    list<BlkInfo> blkInfoList = victimStripe->GetBlkInfoList(listIndex);

    uint32_t volId = blkInfoList.begin()->volID;
    uint32_t remainCnt = blkCnt - allocatedCnt;
    while (remainCnt)
    {
        uint32_t startOffset;
        uint32_t numBlks;

        bool ret = gcStripeManager->AllocateWriteBufferBlks(volId, remainCnt, startOffset, numBlks);
        if (false == ret)
        {
            return false;
        }

        GcWriteBuffer* dataBuffer = gcStripeManager->GetWriteBuffer(volId);

        std::vector<BlkInfo>* allocatedBlkInfoList = gcStripeManager->GetBlkInfoList(volId);
        for (uint32_t i = 0; i < numBlks; i++)
        {
            list<BlkInfo>::iterator it = blkInfoList.begin();
            std::advance(it, offset);
            BlkInfo blkInfo = *it;
            uint32_t vsaOffset = startOffset + i;
            gcStripeManager->SetBlkInfo(volId, vsaOffset, blkInfo);
            _MemCopyValidData(dataBuffer, vsaOffset, blkInfo);

            offset++;
        }

        allocatedCnt += numBlks;
        remainCnt = blkCnt - allocatedCnt;

        if (gcStripeManager->DecreaseRemainingAndCheckIsFull(volId, numBlks))
        {
            if (unlikely(static_cast<int>(POS_EVENT_ID::SUCCESS)
                != volumeManager->IncreasePendingIOCountIfNotZero(volId, VolumeStatus::Unmounted)))
            {
                gcStripeManager->SetFlushed(volId);
                allocatedBlkInfoList->clear();
                delete allocatedBlkInfoList;
                gcStripeManager->SetFinished(dataBuffer);
                break;
            }
            else
            {
                EventSmartPtr flushEvent(new GcFlushSubmission(meta->GetArrayName(),
                            allocatedBlkInfoList,
                            volId,
                            dataBuffer,
                            gcStripeManager));
                EventSchedulerSingleton::Instance()->EnqueueEvent(flushEvent);

                gcStripeManager->SetFlushed(volId);
            }
        }
    }

    volumeManager->DecreasePendingIOCount(volId, VolumeStatus::Unmounted, blkCnt);
    meta->ReturnBuffer(stripeId, buffer);
    meta->SetDoneCopyBlks(blkCnt);
    AIRLOG(PERF_COPY, 0, AIR_READ, BLOCK_SIZE * blkCnt);

    return true;
}

void
CopierReadCompletion::_MemCopyValidData(GcWriteBuffer* dataBuffer, uint32_t offset, BlkInfo blkInfo)
{
    uint32_t bufferIndex = offset / BLOCKS_IN_CHUNK;
    uint32_t bufferOffset = offset % BLOCKS_IN_CHUNK;

    GcWriteBuffer::iterator bufferIt = dataBuffer->begin();
    std::advance(bufferIt, bufferIndex);

    void* srcBuffer = (void*)((char*)buffer + ((blkInfo.vsa.offset % BLOCKS_IN_CHUNK) * BLOCK_SIZE));
    void* destBuffer = (void*)((char*)(*bufferIt) + (bufferOffset * BLOCK_SIZE));
    memcpy(destBuffer, srcBuffer, BLOCK_SIZE);
}

} // namespace pos