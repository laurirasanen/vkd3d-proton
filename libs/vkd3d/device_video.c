/*
 * Copyright 2026 Lauri Räsänen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define VKD3D_DBG_CHANNEL VKD3D_DBG_CHANNEL_API

#include "vkd3d_private.h"



static inline struct d3d12_device *d3d12_device_from_ID3D12VideoDevice(d3d12_video_device_iface *iface)
{
    return CONTAINING_RECORD(iface, struct d3d12_device, ID3D12VideoDevice_iface);
}

extern HRESULT STDMETHODCALLTYPE d3d12_device_QueryInterface(d3d12_device_iface *iface,
        REFIID riid, void **object);

static HRESULT STDMETHODCALLTYPE d3d12_video_device_QueryInterface(d3d12_video_device_iface *iface,
        REFIID iid, void **out)
{
    struct d3d12_device *device = d3d12_device_from_ID3D12VideoDevice(iface);
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);
    return d3d12_device_QueryInterface(&device->ID3D12Device_iface, iid, out);
}

ULONG STDMETHODCALLTYPE d3d12_video_device_AddRef(d3d12_video_device_iface *iface)
{
    struct d3d12_device *device = d3d12_device_from_ID3D12VideoDevice(iface);
    return d3d12_device_add_ref(device);
}

static ULONG STDMETHODCALLTYPE d3d12_video_device_Release(d3d12_video_device_iface *iface)
{
    struct d3d12_device *device = d3d12_device_from_ID3D12VideoDevice(iface);
    return d3d12_device_release(device);
}

static HRESULT STDMETHODCALLTYPE d3d12_video_device_CheckFeatureSupport(d3d12_video_device_iface *iface,
        D3D12_FEATURE_VIDEO feature, void *feature_data, UINT feature_data_size)
{
    struct d3d12_device *device = d3d12_device_from_ID3D12VideoDevice(iface);
    struct vkd3d_vulkan_info *vulkan_info = &device->vk_info;
    VkInstance *vk_instance = device->vkd3d_instance->vk_instance;
    VkPhysicalDevice *vk_physical_device = device->vk_physical_device;
    VkDevice *vk_device = device->vk_device;

    VkResult vr;

    if (!vulkan_info->KHR_video_queue)
    {
        WARN("Video queue not supported.\n");
        return E_INVALIDARG;
    }

    switch (feature)
    {
        case D3D12_FEATURE_VIDEO_DECODE_SUPPORT:
        {
            D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT *data = feature_data;

            VkVideoProfileInfoKHR vk_video_profile = {0};
            VkVideoCapabilitiesKHR vk_video_caps = {0};
            VkVideoDecodeCapabilitiesKHR vk_video_decode_caps = {0};
            VkVideoDecodeH264CapabilitiesKHR vk_video_decode_h264_caps = {0};

            vk_video_profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
            vk_video_caps.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
            vk_video_decode_caps.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
            vk_video_decode_h264_caps.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR;

            if (!vulkan_info->KHR_video_decode_queue)
            {
                WARN("Video decode queue not supported.\n");
                return E_INVALIDARG;
            }

            if (feature_data_size != sizeof(*data))
            {
                WARN("Invalid size %u.\n", feature_data_size);
                return E_INVALIDARG;
            }

            if (data->NodeIndex)
            {
                FIXME("Multi-adapter not supported.\n");
                return E_INVALIDARG;
            }

            vk_video_caps.pNext = &vk_video_decode_caps;

            if (IsEqualGUID(data->Configuration.Profile, D3D12_VIDEO_DECODE_PROFILE_H264))
            {
                vk_video_decode_caps.pNext = &vk_video_decode_h264_caps;
            }
            else
            {
                FIXME("Unhandled video decode profile %s.\n", debugstr_guid(data->Configuration.Profile))
                return E_INVALIDARG;
            }

            if ((vr = VK_CALL(vkGetPhysicalDeviceVideoCapabilitiesKHR(*vk_physical_device, &vk_video_profile, &vk_video_caps) < 0)
            {
                ERR("Failed to query video device capabilities, vr %d\n.", vr);
                return E_INVALIDARG;
            }

            FIXME("Video decode support assumed.\n"); // TODO actually query stuff
            data->SupportFlags = D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED;
            data->ConfigurationFlags = D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_NONE;

            return S_OK;
        }

        default:
        {
            FIXME("Unhandled feature %d.\n", feature);
            return E_NOTIMPL;
        }
    }
}

static HRESULT STDMETHODCALLTYPE d3d12_video_device_CreateVideoDecoder(d3d12_video_device_iface *iface,
        const D3D12_VIDEO_DECODER_DESC* pDesc, REFIID riid, void** ppVideoDecoder)
{
    FIXME("iface %p, pDesc %p, riid %s, ppVideoDecoder %p stub!\n", iface, pDesc, debugstr_guid(riid), ppVideoDecoder);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d3d12_video_device_CreateVideoDecoderHeap(d3d12_video_device_iface *iface,
        const D3D12_VIDEO_DECODER_HEAP_DESC* pVideoDecoderHeapDesc, REFIID riid, void** ppVideoDecoderHeap)
{
    FIXME("iface %p, pVideoDecoderHeapDesc %p, riid %s, ppVideoDecoderHeap %p stub!\n", iface, pVideoDecoderHeapDesc, debugstr_guid(riid), ppVideoDecoderHeap);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d3d12_video_device_CreateVideoProcessor(d3d12_video_device_iface *iface,
        UINT NodeMask, const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC* pOutputStreamDesc,
        UINT NumInputStreamDescs, const D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC *pInputStreamDescs,
        REFIID riid, void **ppVideoProcessor)
{
    FIXME("iface %p, NodeMask %zu, pOutputStreamDesc %p, NumInputStreamDescs %zu, pInputStreamDescs %p, riid %s, ppVideoProcessor %p stub!\n", iface, NodeMask, pOutputStreamDesc, NumInputStreamDescs, pInputStreamDescs, debugstr_guid(riid), ppVideoProcessor);
    return E_NOTIMPL;
}

CONST_VTBL struct ID3D12VideoDeviceVtbl d3d12_video_device_vtbl =
{
    /* IUnknown methods */
    d3d12_video_device_QueryInterface,
    d3d12_video_device_AddRef,
    d3d12_video_device_Release,

    /* ID3DVideoDevice methods */
    d3d12_video_device_CheckFeatureSupport,
    d3d12_video_device_CreateVideoDecoder,
    d3d12_video_device_CreateVideoDecoderHeap,
    d3d12_video_device_CreateVideoProcessor
};
