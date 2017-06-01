// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "DuplicationManager.h"

//
// Constructor sets up references / variables
// 初始化列表构造函数
//
DUPLICATIONMANAGER::DUPLICATIONMANAGER() : m_DeskDupl(nullptr),
                                           m_AcquiredDesktopImage(nullptr),
                                           m_MetaDataBuffer(nullptr),
                                           m_MetaDataSize(0),
                                           m_OutputNumber(0),
                                           m_Device(nullptr)
{
    RtlZeroMemory(&m_OutputDesc, sizeof(m_OutputDesc));
}

//
// Destructor simply calls CleanRefs to destroy everything
// 利用引用计数销毁
// 
DUPLICATIONMANAGER::~DUPLICATIONMANAGER()
{
    if (m_DeskDupl)
    {
        m_DeskDupl->Release();		// Release()是设备指针特有的函数
        m_DeskDupl = nullptr;
    }

    if (m_AcquiredDesktopImage)
    {
        m_AcquiredDesktopImage->Release();
        m_AcquiredDesktopImage = nullptr;
    }

    if (m_MetaDataBuffer)
    {
        delete [] m_MetaDataBuffer;
        m_MetaDataBuffer = nullptr;
    }

    if (m_Device)
    {
        m_Device->Release();
        m_Device = nullptr;
    }
}

//
// Initialize duplication interfaces
// 初始化复制项接口
// --- 功能: 初始化m_Device, m_OutputDesc, m_DeskDupl(IDXGIOutputDuplication类型)
//
DUPL_RETURN DUPLICATIONMANAGER::InitDupl(_In_ ID3D11Device* Device, UINT Output)
{
    m_OutputNumber = Output;

    // Take a reference on the device
	// 增加私有成员m_Device的引用计数
    m_Device = Device;
    m_Device->AddRef();

    // Get DXGI device
	// 获取DXGI设备, DXGI即"DirectX图形基础架构"
    IDXGIDevice* DxgiDevice = nullptr;
    HRESULT hr = m_Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));	// __uuidof()可以获取结构、接口及其指针、引用等变量的GUID; QueryInterface()可以返回指向此接口的指针, 失败则返回错误代码
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI for DXGI Device", L"Error", hr);
    }

    // Get DXGI adapter
	// 获取DXGI适配器
    IDXGIAdapter* DxgiAdapter = nullptr;
    hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));	// 根据DXGI层次关系, Adapter包括Device和Output
    DxgiDevice->Release();
    DxgiDevice = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to get parent DXGI Adapter", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    // Get output
	// 获取DXGI输出
    IDXGIOutput* DxgiOutput = nullptr;
    hr = DxgiAdapter->EnumOutputs(Output, &DxgiOutput);		// Adapter的EnumOutputs()可以枚举适配器的输出数 
    DxgiAdapter->Release();
    DxgiAdapter = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to get specified output in DUPLICATIONMANAGER", L"Error", hr, EnumOutputsExpectedErrors);
    }

    DxgiOutput->GetDesc(&m_OutputDesc);								// 将输出描述保存至私有成员m_OutputDesc

    // QI for Output 1
    IDXGIOutput1* DxgiOutput1 = nullptr;									// DxgiOutput1继承自DxgiOutput
    hr = DxgiOutput->QueryInterface(__uuidof(DxgiOutput1), reinterpret_cast<void**>(&DxgiOutput1));
    DxgiOutput->Release();
    DxgiOutput = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI for DxgiOutput1 in DUPLICATIONMANAGER", L"Error", hr);
    }

    // Create desktop duplication
	// 创建桌面复制项
    hr = DxgiOutput1->DuplicateOutput(m_Device, &m_DeskDupl);	// 输入: m_Device; 输出: m_DeskDupl; 方法DuplicateOutput()可以从一个Adapter的Output创建一个IDXGIOutputDuplication
    DxgiOutput1->Release();
    DxgiOutput1 = nullptr;
    if (FAILED(hr))
    {
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
        {
            MessageBoxW(nullptr, L"There is already the maximum number of applications using the Desktop Duplication API running, please close one of those applications and then try again.", L"Error", MB_OK);
            return DUPL_RETURN_ERROR_UNEXPECTED;
        }
        return ProcessFailure(m_Device, L"Failed to get duplicate output in DUPLICATIONMANAGER", L"Error", hr, CreateDuplicationExpectedErrors);
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Retrieves mouse info and write it into PtrInfo
// 检索鼠标信息并存入PtrInfo
// --- 输入: DXGI_OUTDUPL_FRAME_INFO中保存了鼠标的信息, 可以直接取用
//	--- 输出: 更新PtrInfo的所有成员(符合条件时)
//
DUPL_RETURN DUPLICATIONMANAGER::GetMouse(_Inout_ PTR_INFO* PtrInfo, _In_ DXGI_OUTDUPL_FRAME_INFO* FrameInfo, INT OffsetX, INT OffsetY)
{
    // A non-zero mouse update timestamp indicates that there is a mouse position update and optionally a shape change
	// 一个非零的鼠标更新时间戳代表有鼠标位置更新而且可能有形状变化
    if (FrameInfo->LastMouseUpdateTime.QuadPart == 0)		// QuadPart是个64位的整数, 保证计算机不支持64位long long的时候也能使用
    {
        return DUPL_RETURN_SUCCESS;
    }

    bool UpdatePosition = true;

    // Make sure we don't update pointer position wrongly
	// 确保不会错误地更新指针位置
    // If pointer is invisible, make sure we did not get an update from another output that the last time that said pointer
    // was visible, if so, don't set it to invisible or update.
	// 如果指针不可见, 确保不要从 最近一次指出指针可见的另一个输出 中获取更新, 如果这样做了, 不要把指针设为不可见或者已更新
    if (!FrameInfo->PointerPosition.Visible && (PtrInfo->WhoUpdatedPositionLast != m_OutputNumber))
    {
        UpdatePosition = false;
    }

    // If two outputs both say they have a visible, only update if new update has newer timestamp
	// 如果FrameInfo(系统实际情况)和PtrInfo(用户设置)都指出它们有可见的指针, 那么只在 新的更新有新的时间戳的条件 下更新
    if (FrameInfo->PointerPosition.Visible && PtrInfo->Visible && (PtrInfo->WhoUpdatedPositionLast != m_OutputNumber) && (PtrInfo->LastTimeStamp.QuadPart > FrameInfo->LastMouseUpdateTime.QuadPart))
    {
        UpdatePosition = false;
    }

    // Update position
	// 当UpdatePosition == true时, 才更新
    if (UpdatePosition)
    {
        PtrInfo->Position.x = FrameInfo->PointerPosition.Position.x + m_OutputDesc.DesktopCoordinates.left - OffsetX;
        PtrInfo->Position.y = FrameInfo->PointerPosition.Position.y + m_OutputDesc.DesktopCoordinates.top - OffsetY;
        PtrInfo->WhoUpdatedPositionLast = m_OutputNumber;
        PtrInfo->LastTimeStamp = FrameInfo->LastMouseUpdateTime;
        PtrInfo->Visible = FrameInfo->PointerPosition.Visible != 0;
    }

    // No new shape
	// 形状缓冲区为0时, 不用更新指针形状
    if (FrameInfo->PointerShapeBufferSize == 0)
    {
        return DUPL_RETURN_SUCCESS;
    }

    // Old buffer too small
	// 如果PtrInfo的形状缓冲区小于FrameInfo的形状缓冲区, 那么需要重新开辟空间
    if (FrameInfo->PointerShapeBufferSize > PtrInfo->BufferSize)
    {
        if (PtrInfo->PtrShapeBuffer)
        {
            delete [] PtrInfo->PtrShapeBuffer;
            PtrInfo->PtrShapeBuffer = nullptr;
        }
        PtrInfo->PtrShapeBuffer = new (std::nothrow) BYTE[FrameInfo->PointerShapeBufferSize];	// std::nothrow, 在new失败时返回NULL而不是抛出异常
        if (!PtrInfo->PtrShapeBuffer)
        {
            PtrInfo->BufferSize = 0;
            return ProcessFailure(nullptr, L"Failed to allocate memory for pointer shape in DUPLICATIONMANAGER", L"Error", E_OUTOFMEMORY);
        }

        // Update buffer size
        PtrInfo->BufferSize = FrameInfo->PointerShapeBufferSize;
    }

    // Get shape
	// 获取形状信息
    UINT BufferSizeRequired;
    HRESULT hr = m_DeskDupl->GetFramePointerShape(FrameInfo->PointerShapeBufferSize, reinterpret_cast<VOID*>(PtrInfo->PtrShapeBuffer), &BufferSizeRequired, &(PtrInfo->ShapeInfo));	// IDXGIOutputDuplication::GetFramePointerShape()方法, 可以获取当前桌面的指针形状
																																																																												// --- 原型:	_in_		UINT															...,		---- 形状缓冲区大小
																																																																												//					_out_	void*															...,		---- 形状缓冲区首地址
																																																																												//					_out_	UINT*															...,		---- 请求的形状缓冲区大小
																																																																												//					_out_	DXGI_OUTDUPL_POINTER_SHAPE_INFO		...		---- DXGI形状信息
    if (FAILED(hr))
    {
        delete [] PtrInfo->PtrShapeBuffer;
        PtrInfo->PtrShapeBuffer = nullptr;
        PtrInfo->BufferSize = 0;
        return ProcessFailure(m_Device, L"Failed to get frame pointer shape in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
    }

    return DUPL_RETURN_SUCCESS;
}


//
// Get next frame and write it into Data
// 获取下一帧信息并写入Data
//
_Success_(*Timeout == false && return == DUPL_RETURN_SUCCESS)
DUPL_RETURN DUPLICATIONMANAGER::GetFrame(_Out_ FRAME_DATA* Data, _Out_ bool* Timeout)
{
    IDXGIResource* DesktopResource = nullptr;	// Device的子类是Resource和Surface
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;		// DXGI帧信息

    // Get new frame
	// 获取新的帧
    HRESULT hr = m_DeskDupl->AcquireNextFrame(500, &FrameInfo, &DesktopResource);	// IDXGIOutputDuplication::AcquireNextFrame()方法, 指出应用程序已经准备好处理下一帧桌面图片
																																				// --- 原型:	_in_		UINT											...,		---- 多少毫秒算超时
																																				//					_out_	DXGI_OUTDUPL_FRAME_INFO		...,		---- 接收DXGI帧信息
																																				//					_out_	IDXGIResource								...,		---- 包含桌面位图的接口
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)	// 超时, 则不更新
    {
        *Timeout = true;
        return DUPL_RETURN_SUCCESS;
    }
    *Timeout = false;

    if (FAILED(hr))											// 其他错误, 则抛出异常
    {
        return ProcessFailure(m_Device, L"Failed to acquire next frame in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
    }

    // If still holding old frame, destroy it
	// 如果先前的m_AcquiredDesktopImage(IDXGITexture2D*类型)不为NULL, 则销毁它
    if (m_AcquiredDesktopImage)
    {
        m_AcquiredDesktopImage->Release();
        m_AcquiredDesktopImage = nullptr;
    }

    // QI for IDXGIResource
	// 为m_AcquiredDesktopImage重新查询接口
    hr = DesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&m_AcquiredDesktopImage));
    DesktopResource->Release();
    DesktopResource = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI for ID3D11Texture2D from acquired IDXGIResource in DUPLICATIONMANAGER", L"Error", hr);
    }

    // Get metadata
	// 获取元数据, 即更新m_MetaData相关的私有成员, 并更新Data(FRAME_DATA*类型)的成员
    if (FrameInfo.TotalMetadataBufferSize)
    {
		// 1 - 更新m_MetaDataSize
        // Old buffer too small
		// 接收的元数据缓冲区大小 > 私有成员m_MetaDataSize, 则需要重新分配空间
        if (FrameInfo.TotalMetadataBufferSize > m_MetaDataSize)
        {
            if (m_MetaDataBuffer)
            {
                delete [] m_MetaDataBuffer;
                m_MetaDataBuffer = nullptr;
            }
            m_MetaDataBuffer = new (std::nothrow) BYTE[FrameInfo.TotalMetadataBufferSize];		// std::nothrow, new失败时返回NULL而不是抛出异常
            if (!m_MetaDataBuffer)
            {
                m_MetaDataSize = 0;
                Data->MoveCount = 0;
                Data->DirtyCount = 0;
                return ProcessFailure(nullptr, L"Failed to allocate memory for metadata in DUPLICATIONMANAGER", L"Error", E_OUTOFMEMORY);
            }
            m_MetaDataSize = FrameInfo.TotalMetadataBufferSize;
        }


		// 2 - 获取移动矩形(move rectangles) 
        UINT BufSize = FrameInfo.TotalMetadataBufferSize;

        // Get move rectangles
        hr = m_DeskDupl->GetFrameMoveRects(BufSize, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(m_MetaDataBuffer), &BufSize);	// IDXGIOutputDuplication::GetFrameMoveRects(), 获取移动矩形
																																																						// --- 原型:	_in_		UINT											...,		---- 缓冲区大小
																																																						//					_out_	DXGI_OUTDUPL_MOVE_RECT*		...,		---- DXGI移动矩形缓冲区首地址, 包括SourcePoint和DestinationRect
																																																						//					_out_	UINT*											...		---- DXGI移动矩形缓冲区大小
        if (FAILED(hr))
        {
            Data->MoveCount = 0;
            Data->DirtyCount = 0;
            return ProcessFailure(nullptr, L"Failed to get frame move rects in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
        }
        Data->MoveCount = BufSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);		// 移动矩形个数 = 更新后的缓冲区大小BufSize / 一个移动矩形缓冲区的大小

		// 3 - 获取脏矩形(dirty rectangles)
        BYTE* DirtyRects = m_MetaDataBuffer + BufSize;				// 脏矩形首地址 = m_MetaDataBuffer首地址 + BufSize(移动矩形缓冲区总的大小)
        BufSize = FrameInfo.TotalMetadataBufferSize - BufSize;		// 脏矩形缓冲区大小 = FrameInfo.TotalMetadataBufferSize - BufSize(移动矩形缓冲区总的大小)

        // Get dirty rectangles
        hr = m_DeskDupl->GetFrameDirtyRects(BufSize, reinterpret_cast<RECT*>(DirtyRects), &BufSize);		// IDXGIOutputDuplication::GetFrameDirtyRects(), 获取脏矩形, 原型与移动矩形类似
        if (FAILED(hr))
        {
            Data->MoveCount = 0;
            Data->DirtyCount = 0;
            return ProcessFailure(nullptr, L"Failed to get frame dirty rects in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
        }
        Data->DirtyCount = BufSize / sizeof(RECT);						// 脏矩形个数 = 更新后的缓冲区大小BufSize / 一个矩形RECT的大小

		// 4 - 将获得的m_MetaDataBuffer首地址赋予Data->MetaData
        Data->MetaData = m_MetaDataBuffer;								// 传递首地址
    }

    Data->Frame = m_AcquiredDesktopImage;	// Frame保存IDXGITexture2D帧本身
    Data->FrameInfo = FrameInfo;						// FrameInfo保存IDXGI帧信息(DXGI_OUTDUPL_FRAME_INFO)

    return DUPL_RETURN_SUCCESS;
}

//
// Release frame
// 释放帧
//
DUPL_RETURN DUPLICATIONMANAGER::DoneWithFrame()
{
    HRESULT hr = m_DeskDupl->ReleaseFrame();
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to release frame in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
    }

    if (m_AcquiredDesktopImage)
    {
        m_AcquiredDesktopImage->Release();
        m_AcquiredDesktopImage = nullptr;
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Gets output desc into DescPtr
// 从m_OutputDesc获取DXGI_OUTPUT_DESC
//
void DUPLICATIONMANAGER::GetOutputDesc(_Out_ DXGI_OUTPUT_DESC* DescPtr)
{
    *DescPtr = m_OutputDesc;
}
