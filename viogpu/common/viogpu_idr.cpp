/*
 * Copyright (C) 2019-2020 Red Hat, Inc.
 *
 * Written By: Vadim Rozenfeld <vrozenfe@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met :
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and / or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of their contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "viogpu_idr.h"
#include "viogpu.h"
#include "baseobj.h"
#if !DBG
#include "viogpu_idr.tmh"
#endif

VioGpuIdr::VioGpuIdr()
{
    KeInitializeSpinLock(&m_lock);
    m_startId = 0;
    m_bitmapBits = 0;
    m_bitmap = NULL;
    m_lastIdx = 0;
}

VioGpuIdr::~VioGpuIdr()
{
    Close();
}

BOOLEAN VioGpuIdr::Init(_In_ ULONG start)
{
    Close();

    m_startId = start;
    m_bitmapBits = kMaxBitmapBits;
    ULONG sizeBytes = (m_bitmapBits + 31) / 32 * sizeof(ULONG);
    m_bitmap = reinterpret_cast<PULONG>(new (NonPagedPoolNx) BYTE[sizeBytes]);
    if (!m_bitmap)
    {
        DbgPrint(TRACE_LEVEL_FATAL, ("[%s] failed to allocate %u byte bitmap\n", __FUNCTION__, sizeBytes));
        m_bitmapBits = 0;
        return FALSE;
    }
    RtlZeroMemory(m_bitmap, sizeBytes);
    RtlInitializeBitMap(&m_bitmapHdr, m_bitmap, m_bitmapBits);
    m_lastIdx = 0;
    return TRUE;
}

ULONG VioGpuIdr::GetId(VOID)
{
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);

    ULONG idx = RtlFindClearBitsAndSet(&m_bitmapHdr, 1, m_lastIdx);
    if (idx == 0xFFFFFFFF)
    {
        // Wrap and try from 0.
        idx = RtlFindClearBitsAndSet(&m_bitmapHdr, 1, 0);
    }
    if (idx == 0xFFFFFFFF)
    {
        KeReleaseSpinLock(&m_lock, oldIrql);
        DbgPrint(TRACE_LEVEL_FATAL, ("[%s] no free ids (bitmap exhausted)\n", __FUNCTION__));
        return 0;
    }
    m_lastIdx = idx + 1;
    if (m_lastIdx >= m_bitmapBits)
    {
        m_lastIdx = 0;
    }

    KeReleaseSpinLock(&m_lock, oldIrql);

    ULONG id = m_startId + idx;
    DbgPrint(TRACE_LEVEL_VERBOSE, ("[%s] id = %u (idx=%u)\n", __FUNCTION__, id, idx));
    return id;
}

VOID VioGpuIdr::PutId(_In_ ULONG id)
{
    if (id < m_startId || (id - m_startId) >= m_bitmapBits)
    {
        DbgPrint(TRACE_LEVEL_ERROR, ("[%s] out-of-range id=%u start=%u bits=%u\n",
                                     __FUNCTION__, id, m_startId, m_bitmapBits));
        return;
    }
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);
    ULONG idx = id - m_startId;
    if (RtlCheckBit(&m_bitmapHdr, idx))
    {
        RtlClearBit(&m_bitmapHdr, idx);
    }
    else
    {
        DbgPrint(TRACE_LEVEL_WARNING, ("[%s] double-put id=%u\n", __FUNCTION__, id));
    }
    KeReleaseSpinLock(&m_lock, oldIrql);

    DbgPrint(TRACE_LEVEL_VERBOSE, ("[%s] id = %u\n", __FUNCTION__, id));
}

VOID VioGpuIdr::Close(VOID)
{
    if (m_bitmap)
    {
        delete[] reinterpret_cast<PBYTE>(m_bitmap);
        m_bitmap = NULL;
    }
    m_bitmapBits = 0;
    m_lastIdx = 0;
}
