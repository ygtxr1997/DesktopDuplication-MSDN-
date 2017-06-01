// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

// --- 复制项管理工具
// --- 包括ID3D11Device、IDXGIOutputDuplication、DXGI_OUTPUT_DESC、ID3D11Texture2D和元数据的初始化
// --- 还有获取桌面帧、鼠标指针等public方法

#ifndef _DUPLICATIONMANAGER_H_
#define _DUPLICATIONMANAGER_H_

#include "CommonTypes.h"

//
// Handles the task of duplicating an output.
//
class DUPLICATIONMANAGER
{
    public:
        DUPLICATIONMANAGER();
        ~DUPLICATIONMANAGER();
        _Success_(*Timeout == false && return == DUPL_RETURN_SUCCESS) DUPL_RETURN GetFrame(_Out_ FRAME_DATA* Data, _Out_ bool* Timeout);	// _Success_()中的参数代表函数返回成功时对应的值
        DUPL_RETURN DoneWithFrame();	// 释放帧
        DUPL_RETURN InitDupl(_In_ ID3D11Device* Device, UINT Output);	// 初始化Duplication
        DUPL_RETURN GetMouse(_Inout_ PTR_INFO* PtrInfo, _In_ DXGI_OUTDUPL_FRAME_INFO* FrameInfo, INT OffsetX, INT OffsetY);	// 获取鼠标信息
        void GetOutputDesc(_Out_ DXGI_OUTPUT_DESC* DescPtr);	// 获取输出描述

    private:

    // vars
        IDXGIOutputDuplication* m_DeskDupl;										// 桌面复制项, IDXGIOutputDuplication
        ID3D11Texture2D* m_AcquiredDesktopImage;							// 请求的桌面图片，Texture2D
        _Field_size_bytes_(m_MetaDataSize) BYTE* m_MetaDataBuffer;	// 元数据缓冲区
        UINT m_MetaDataSize;																// 元数据大小
        UINT m_OutputNumber;																// 输出编号？
        DXGI_OUTPUT_DESC m_OutputDesc;											// 输出描述, 包括设备名、桌面矩形、朝向等信息
        ID3D11Device* m_Device;															// 设备
};

#endif
