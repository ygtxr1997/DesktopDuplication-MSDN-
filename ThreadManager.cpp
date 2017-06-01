// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "ThreadManager.h"

DWORD WINAPI DDProc(_In_ void* Param);			// WINAPI即__stdcall, 代表函数参数从右到左入栈；函数DDProc在<DesktopDuplication.cpp>中被定义

THREADMANAGER::THREADMANAGER() : m_ThreadCount(0),
                                 m_ThreadHandles(nullptr),
                                 m_ThreadData(nullptr)
{
    RtlZeroMemory(&m_PtrInfo, sizeof(m_PtrInfo));	// Windows下的memset
}

THREADMANAGER::~THREADMANAGER()
{
    Clean();		// 调用this->Clean()释放内存和线程
}

//
// Clean up resources
// 清理资源(线程和内存)
//
void THREADMANAGER::Clean()
{
    if (m_PtrInfo.PtrShapeBuffer)
    {
        delete [] m_PtrInfo.PtrShapeBuffer;
        m_PtrInfo.PtrShapeBuffer = nullptr;
    }
    RtlZeroMemory(&m_PtrInfo, sizeof(m_PtrInfo));	// ** 清空m_PtrInfo, 鼠标指针信息

    if (m_ThreadHandles)
    {
        for (UINT i = 0; i < m_ThreadCount; ++i)
        {
            if (m_ThreadHandles[i])
            {
                CloseHandle(m_ThreadHandles[i]);			// 关闭单个线程句柄
            }
        }
        delete [] m_ThreadHandles;
        m_ThreadHandles = nullptr;
    }																			// ** 清空m_ThreadHandles, 线程句柄

    if (m_ThreadData)
    {
        for (UINT i = 0; i < m_ThreadCount; ++i)
        {
            CleanDx(&m_ThreadData[i].DxRes);			// 使用this->CleanDx()清理单个线程的D3D资源，CleanDx()在下面定义
        }
        delete [] m_ThreadData;
        m_ThreadData = nullptr;
    }																			// ** 清空m_ThreadData, 线程数据

    m_ThreadCount = 0;											// ** 线程数置0
}

//
// Clean up DX_RESOURCES
// 清空线程数据数组m_ThreadData[i]中的D3D资源
//
void THREADMANAGER::CleanDx(_Inout_ DX_RESOURCES* Data)
{
    if (Data->Device)
    {
        Data->Device->Release();
        Data->Device = nullptr;
    }

    if (Data->Context)
    {
        Data->Context->Release();
        Data->Context = nullptr;
    }

    if (Data->VertexShader)
    {
        Data->VertexShader->Release();
        Data->VertexShader = nullptr;
    }

    if (Data->PixelShader)
    {
        Data->PixelShader->Release();
        Data->PixelShader = nullptr;
    }

    if (Data->InputLayout)
    {
        Data->InputLayout->Release();
        Data->InputLayout = nullptr;
    }

    if (Data->SamplerLinear)
    {
        Data->SamplerLinear->Release();
        Data->SamplerLinear = nullptr;
    }
}

//
// Start up threads for DDA
// 启动程序的线程
// 输入 :	INT				SingleOutput,					---- 单输出？
//				UINT			OutputCount,					---- 输出数, 线程数
//				HANDLE		UnexpextedErrorEvent,	---- 非预期异常
//				HANDLE		ExpetcedErrorEvent,		---- 预期异常
//				HANDLE		TerminateThreadsEvent,	---- 线程终止
//				HANDLE		SharedHandle,					---- 共享句柄
//				_In_ RECT*	DesktopDim					---- 桌面矩形(left, top, width, height)
//
DUPL_RETURN THREADMANAGER::Initialize(INT SingleOutput, UINT OutputCount, HANDLE UnexpectedErrorEvent, HANDLE ExpectedErrorEvent, HANDLE TerminateThreadsEvent, HANDLE SharedHandle, _In_ RECT* DesktopDim)
{
    m_ThreadCount = OutputCount;
    m_ThreadHandles = new (std::nothrow) HANDLE[m_ThreadCount];			// std::nothrow在new内存不足时不抛出异常而是将指针值NULL
    m_ThreadData = new (std::nothrow) THREAD_DATA[m_ThreadCount];		// ** 初始化内存
    if (!m_ThreadHandles || !m_ThreadData)
    {
        return ProcessFailure(nullptr, L"Failed to allocate array for threads", L"Error", E_OUTOFMEMORY);
    }

    // Create appropriate # of threads for duplication
	// 根据线程数m_ThreadCount, 定义线程数据数组m_ThreadData[]和线程句柄数组m_ThreadHandles[]
    DUPL_RETURN Ret = DUPL_RETURN_SUCCESS;
    for (UINT i = 0; i < m_ThreadCount; ++i)
    {
        m_ThreadData[i].UnexpectedErrorEvent = UnexpectedErrorEvent;
        m_ThreadData[i].ExpectedErrorEvent = ExpectedErrorEvent;
        m_ThreadData[i].TerminateThreadsEvent = TerminateThreadsEvent;
        m_ThreadData[i].Output = (SingleOutput < 0) ? i : SingleOutput;
        m_ThreadData[i].TexSharedHandle = SharedHandle;
        m_ThreadData[i].OffsetX = DesktopDim->left;
        m_ThreadData[i].OffsetY = DesktopDim->top;
        m_ThreadData[i].PtrInfo = &m_PtrInfo;

        RtlZeroMemory(&m_ThreadData[i].DxRes, sizeof(DX_RESOURCES));
        Ret = InitializeDx(&m_ThreadData[i].DxRes);											// 调用this->InitializeDx()初始化D3D资源, 在下面定义
        if (Ret != DUPL_RETURN_SUCCESS)
        {
            return Ret;
        }

        DWORD ThreadId;
        m_ThreadHandles[i] = CreateThread(nullptr, 0, DDProc, &m_ThreadData[i], 0, &ThreadId);	// 创建线程就在这里了, CreateThread()定义在<Windows.h>中，函数原型如下 :
																																						// --- 输入 :	_In_opt_		LPSECURITY_ATTRIBUTES			...,		---- 句柄能否被继承, 默认不继承
																																						// 					_In_				SIZE_T										...,		---- 如果为0, 线程使用默认大小的栈
																																						// 					_In_				LPTHREAD_START_ROUTINE	...,		---- 线程的起始地址, 通常是函数
																																						// 					_In_opt_		LPVOID									...,		---- 传递给线程的参数
																																						// 					_In_				DWORD									...,		---- 如果为0, 线程在创建后立即运行
																																						//					_Out_opt_		LPDWORD								...,		---- 线程句柄
        if (m_ThreadHandles[i] == nullptr)
        {
            return ProcessFailure(nullptr, L"Failed to create thread", L"Error", E_FAIL);
        }
    }

    return Ret;
}

//
// Get DX_RESOURCES
// 初始化D3D资源
// --- 功能 ：	创建设备、顶点着色器、输入布局、像素着色器、采样器
//
DUPL_RETURN THREADMANAGER::InitializeDx(_Out_ DX_RESOURCES* Data)
{
    HRESULT hr = S_OK;

    // Driver types supported
	// 支持的驱动类型
    D3D_DRIVER_TYPE DriverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT NumDriverTypes = ARRAYSIZE(DriverTypes);		// ARRAYSIZE定义在<Windows.h>, 用于获取数组大小

    // Feature levels supported
	// D3D的兼容版本
    D3D_FEATURE_LEVEL FeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1
    };
    UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

    D3D_FEATURE_LEVEL FeatureLevel;

    // Create device
	// 创建设备
    for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex)
    {
        hr = D3D11CreateDevice(nullptr, DriverTypes[DriverTypeIndex], nullptr, 0, FeatureLevels, NumFeatureLevels,
                                D3D11_SDK_VERSION, &Data->Device, &FeatureLevel, &Data->Context);		// 创建设备, D3D11CreateDevice()定义在<d3d11.h>, 函数原型如下 ： 
																																							// --- 参数 :	_In_opt_		IDXGIAdapter*					...,		---- 视频适配器, 为NULL时则使用IDXGIFactory1枚举的第一个适配器	
																																							//										D3D_DRIVER_TYPE			...,		---- 驱动类型
																																							//										HMODULE						...,		---- 如果驱动类型为_SOFTWARE, 则此项不能为NULL
																																							//										UINT								...,		---- 创建设备的FLAG, enum类型, 可以按位或
																																							//					_In_opt_		D3D_FEATURE_LEVEL*		...,		---- 兼容版本数组
																																							//										UINT								...,		---- 兼容版本数
																																							//										UINT								...,		---- SDK版本, D3D11_SDK_VERSION
																																							//					_Out_opt_		ID3D11Device**				...,		---- 设备
																																							//					_Out_opt_		D3D_FEATURE_LEVEL*		...,		---- 当前兼容版本
																																							//					_Out_opt_		ID3D11DeviceContext**	...		---- 设备上下文
        if (SUCCEEDED(hr))
        {
            // Device creation success, no need to loop anymore
			// 设备已经匹配了驱动类型和D3D版本，无需继续循环
            break;
        }
    }
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to create device in InitializeDx", L"Error", hr);
    }

    // VERTEX shader
	// 顶点着色器
    UINT Size = ARRAYSIZE(g_VS);
    hr = Data->Device->CreateVertexShader(g_VS, Size, nullptr, &Data->VertexShader);
    if (FAILED(hr))
    {
        return ProcessFailure(Data->Device, L"Failed to create vertex shader in InitializeDx", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    // Input layout
	// 输入布局
    D3D11_INPUT_ELEMENT_DESC Layout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    UINT NumElements = ARRAYSIZE(Layout);
    hr = Data->Device->CreateInputLayout(Layout, NumElements, g_VS, Size, &Data->InputLayout);
    if (FAILED(hr))
    {
        return ProcessFailure(Data->Device, L"Failed to create input layout in InitializeDx", L"Error", hr, SystemTransitionsExpectedErrors);
    }
    Data->Context->IASetInputLayout(Data->InputLayout);

    // Pixel shader
	// 像素着色器
    Size = ARRAYSIZE(g_PS);
    hr = Data->Device->CreatePixelShader(g_PS, Size, nullptr, &Data->PixelShader);
    if (FAILED(hr))
    {
        return ProcessFailure(Data->Device, L"Failed to create pixel shader in InitializeDx", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    // Set up sampler
	// 取样器
    D3D11_SAMPLER_DESC SampDesc;
    RtlZeroMemory(&SampDesc, sizeof(SampDesc));
    SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SampDesc.MinLOD = 0;
    SampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = Data->Device->CreateSamplerState(&SampDesc, &Data->SamplerLinear);
    if (FAILED(hr))
    {
        return ProcessFailure(Data->Device, L"Failed to create sampler state in InitializeDx", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Getter for the PTR_INFO structure
// 获取鼠标指针信息
//
PTR_INFO* THREADMANAGER::GetPointerInfo()
{
    return &m_PtrInfo;
}

//
// Waits infinitely for all spawned threads to terminate
// 等待，直到所有的线程终止
//
void THREADMANAGER::WaitForThreadTermination()
{
    if (m_ThreadCount != 0)
    {
        WaitForMultipleObjectsEx(m_ThreadCount, m_ThreadHandles, TRUE, INFINITE, FALSE);	// 等待内核对象WaitForMultipleObjectsEx(), 定义在<Windows.h>
																																					// --- 本例中 ：	bWaitAll = TRUE, 
																																					//						dwMilliSeconds = INFINITE,
																																					//						bAlert = FALSE
    }
}
