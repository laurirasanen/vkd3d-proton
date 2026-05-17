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

bool vkd3d_fill_video_profile_from_dxgi_format(VkVideoProfileInfoKHR *profile, DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_YUY2:
        {
            profile->chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
            profile->lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
            profile->chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
            return true;
        }

        default:
        {
            FIXME("Unhandled format %s.\n", debug_dxgi_format(format));
            return false;
        }
    }
}

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
    const struct vkd3d_vk_device_procs *vk_procs = &device->vk_procs;
    const struct vkd3d_vulkan_info *vulkan_info = &device->vk_info;
    VkPhysicalDevice vk_physical_device = device->vk_physical_device;
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
            VkVideoDecodeH264ProfileInfoKHR vk_video_h264_profile = {0};
            VkVideoCapabilitiesKHR vk_video_caps = {0};
            VkVideoDecodeCapabilitiesKHR vk_video_decode_caps = {0};
            VkVideoDecodeH264CapabilitiesKHR vk_video_decode_h264_caps = {0};

            vk_video_profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
            vk_video_h264_profile.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR;
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

            if (data->Configuration.BitstreamEncryption != D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE)
            {
                FIXME("Bitstream encryption not supported.\n");
                return E_INVALIDARG;
            }

            if (data->Configuration.InterlaceType != D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE)
            {
                FIXME("Interlace not supported.\n");
                return E_INVALIDARG;
            }

            if (IsEqualGUID(&data->Configuration.DecodeProfile, &D3D12_VIDEO_DECODE_PROFILE_H264))
            {
                if (!vulkan_info->KHR_video_decode_h264)
                {
                    WARN("Video decode h264 not supported.\n");
                    return E_INVALIDARG;
                }
                vk_video_decode_caps.pNext = &vk_video_decode_h264_caps;
                vk_video_profile.pNext = &vk_video_h264_profile;
                vk_video_profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
                vk_video_h264_profile.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_BASELINE; // TODO
                vk_video_h264_profile.pictureLayout = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR;
            }
            else
            {
                FIXME("Unhandled video decode profile %s.\n", debugstr_guid(&data->Configuration.DecodeProfile));
                return E_INVALIDARG;
            }

            vkd3d_fill_video_profile_from_dxgi_format(&vk_video_profile, data->DecodeFormat);
            vk_video_caps.pNext = &vk_video_decode_caps;

            if ((vr = VK_CALL(vkGetPhysicalDeviceVideoCapabilitiesKHR(vk_physical_device, &vk_video_profile, &vk_video_caps))) < 0)
            {
                ERR("Failed to query video device capabilities, vr %d\n.", vr);
                return E_INVALIDARG;
            }

            if (data->Width < vk_video_caps.minCodedExtent.width || data->Width > vk_video_caps.maxCodedExtent.width)
            {
                WARN("Invalid video width: %u, min: %u, max: %u.\n", data->Width,
                    vk_video_caps.minCodedExtent.width,
                    vk_video_caps.maxCodedExtent.width);
                return E_INVALIDARG;
            }

            if (data->Height < vk_video_caps.minCodedExtent.height || data->Height > vk_video_caps.maxCodedExtent.height) {
                WARN("Invalid video height: %u, min: %u, max: %u.\n", data->Height,
                    vk_video_caps.minCodedExtent.height,
                    vk_video_caps.maxCodedExtent.height);
                return E_INVALIDARG;
            }

            data->SupportFlags = D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED;
            data->ConfigurationFlags = D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_NONE;

            return S_OK;
        }

        case D3D12_FEATURE_VIDEO_DECODE_PROFILES:
        {
            D3D12_FEATURE_DATA_VIDEO_DECODE_PROFILES *data = feature_data;

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

            data->pProfiles[0] = D3D12_VIDEO_DECODE_PROFILE_H264;
            return S_OK;
        }

        case D3D12_FEATURE_VIDEO_DECODE_FORMATS:
        {
            D3D12_FEATURE_DATA_VIDEO_DECODE_FORMATS *data = feature_data;

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

            data->pOutputFormats[0] = DXGI_FORMAT_YUY2;
            return S_OK;
        }

        case D3D12_FEATURE_VIDEO_DECODE_PROFILE_COUNT:
        {
            D3D12_FEATURE_DATA_VIDEO_DECODE_PROFILES *data = feature_data;

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

            data->ProfileCount = 1;
            return S_OK;
        }

        case D3D12_FEATURE_VIDEO_DECODE_FORMAT_COUNT:
        {
            D3D12_FEATURE_DATA_VIDEO_DECODE_FORMATS *data = feature_data;

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

            data->FormatCount = 1;
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
        const D3D12_VIDEO_DECODER_DESC *pDesc, REFIID riid, void **ppVideoDecoder)
{
    FIXME("iface %p, pDesc %p, riid %s, ppVideoDecoder %p stub!\n",
            iface, pDesc, debugstr_guid(riid), ppVideoDecoder);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d3d12_video_device_CreateVideoDecoderHeap(d3d12_video_device_iface *iface,
        const D3D12_VIDEO_DECODER_HEAP_DESC *pVideoDecoderHeapDesc, REFIID riid, void **ppVideoDecoderHeap)
{
    FIXME("iface %p, pVideoDecoderHeapDesc %p, riid %s, ppVideoDecoderHeap %p stub!\n",
            iface, pVideoDecoderHeapDesc, debugstr_guid(riid), ppVideoDecoderHeap);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d3d12_video_device_CreateVideoProcessor(d3d12_video_device_iface *iface,
        UINT NodeMask, const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC *pOutputStreamDesc,
        UINT NumInputStreamDescs, const D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC *pInputStreamDescs,
        REFIID riid, void **ppVideoProcessor)
{
    FIXME("iface %p, NodeMask %zu, pOutputStreamDesc %p, NumInputStreamDescs %zu, pInputStreamDescs %p, riid %s, ppVideoProcessor %p stub!\n",
            iface, NodeMask, pOutputStreamDesc, NumInputStreamDescs, pInputStreamDescs, debugstr_guid(riid), ppVideoProcessor);
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
