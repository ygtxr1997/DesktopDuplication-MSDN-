// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "DisplayManager.h"
using namespace DirectX;

//
// Constructor NULLs out vars
// 初始化列表构造函数
//
DISPLAYMANAGER::DISPLAYMANAGER() : m_Device(nullptr),
                                   m_DeviceContext(nullptr),
                                   m_MoveSurf(nullptr),
                                   m_VertexShader(nullptr),
                                   m_PixelShader(nullptr),
                                   m_InputLayout(nullptr),
                                   m_RTV(nullptr),
                                   m_SamplerLinear(nullptr),
                                   m_DirtyVertexBufferAlloc(nullptr),
                                   m_DirtyVertexBufferAllocSize(0)
{
}

//
// Destructor calls CleanRefs to destroy everything
// 清空引用计数以析构
//
DISPLAYMANAGER::~DISPLAYMANAGER()
{
    CleanRefs();

    if (m_DirtyVertexBufferAlloc)
    {
        delete [] m_DirtyVertexBufferAlloc;
        m_DirtyVertexBufferAlloc = nullptr;
    }
}

//
// Initialize D3D variables
// 初始化D3D变量
//
void DISPLAYMANAGER::InitD3D(DX_RESOURCES* Data)		// DX_RESOURCES定义在<CommonTypes.h>中
{
    m_Device = Data->Device;
    m_DeviceContext = Data->Context;
    m_VertexShader = Data->VertexShader;
    m_PixelShader = Data->PixelShader;
    m_InputLayout = Data->InputLayout;
    m_SamplerLinear = Data->SamplerLinear;

    m_Device->AddRef();					// 增加引用计数
    m_DeviceContext->AddRef();
    m_VertexShader->AddRef();
    m_PixelShader->AddRef();
    m_InputLayout->AddRef();
    m_SamplerLinear->AddRef();
}

//
// Process a given frame and its metadata
// 根据所给的Data(FRAME_DATA*类型, 包括2D纹理、DXGI_OUTDUPL_FRAME_INFO、元数据)处理这个帧
// --- 说明:	主要用到了DISPLAYMANAGER::CopyMove()和CopyDirty()两个private方法
//
DUPL_RETURN DISPLAYMANAGER::ProcessFrame(_In_ FRAME_DATA* Data, _Inout_ ID3D11Texture2D* SharedSurf, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc)
{
    DUPL_RETURN Ret = DUPL_RETURN_SUCCESS;

    // Process dirties and moves
	// 处理脏区域和移动区域
    if (Data->FrameInfo.TotalMetadataBufferSize)
    {
        D3D11_TEXTURE2D_DESC Desc;
        Data->Frame->GetDesc(&Desc);	// ID3D11Texture2D::GetDesc(), 获取Texture2D的描述信息(属性)

        if (Data->MoveCount)
        {
            Ret = CopyMove(SharedSurf, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(Data->MetaData), Data->MoveCount, OffsetX, OffsetY, DeskDesc, Desc.Width, Desc.Height);	// DISPLAYMANAGER::CopyMove()定义在<DisplayManager.h>中, 可在本文件中查看函数体
            if (Ret != DUPL_RETURN_SUCCESS)
            {
                return Ret;
            }
        }

        if (Data->DirtyCount)
        {
            Ret = CopyDirty(Data->Frame, SharedSurf, reinterpret_cast<RECT*>(Data->MetaData + (Data->MoveCount * sizeof(DXGI_OUTDUPL_MOVE_RECT))), Data->DirtyCount, OffsetX, OffsetY, DeskDesc);	// DISPLAYMANAGER::CopyDirty()定义在<DisplayManager.h>中, 可在本文件中查看函数体
        }
    }

    return Ret;
}

//
// Returns D3D device being used
// 返回正在使用的D3D设备
//
ID3D11Device* DISPLAYMANAGER::GetDevice()
{
    return m_Device;
}

//
// Set appropriate source and destination rects for move rects
// 根据朝向等信息, 设置合适的源移动矩形和目标移动矩形
//
void DISPLAYMANAGER::SetMoveRect(_Out_ RECT* SrcRect, _Out_ RECT* DestRect, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ DXGI_OUTDUPL_MOVE_RECT* MoveRect, INT TexWidth, INT TexHeight)
{
    switch (DeskDesc->Rotation)
    {
        case DXGI_MODE_ROTATION_UNSPECIFIED:	// 未加规定的朝向
        case DXGI_MODE_ROTATION_IDENTITY:			// 默认朝向
        {
            SrcRect->left = MoveRect->SourcePoint.x;
            SrcRect->top = MoveRect->SourcePoint.y;
            SrcRect->right = MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left;
            SrcRect->bottom = MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top;

            *DestRect = MoveRect->DestinationRect;
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE90:		// 旋转90°
        {
            SrcRect->left = TexHeight - (MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top);
            SrcRect->top = MoveRect->SourcePoint.x;
            SrcRect->right = TexHeight - MoveRect->SourcePoint.y;
            SrcRect->bottom = MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left;

            DestRect->left = TexHeight - MoveRect->DestinationRect.bottom;
            DestRect->top = MoveRect->DestinationRect.left;
            DestRect->right = TexHeight - MoveRect->DestinationRect.top;
            DestRect->bottom = MoveRect->DestinationRect.right;
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE180:		// 旋转180°
        {
            SrcRect->left = TexWidth - (MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left);
            SrcRect->top = TexHeight - (MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top);
            SrcRect->right = TexWidth - MoveRect->SourcePoint.x;
            SrcRect->bottom = TexHeight - MoveRect->SourcePoint.y;

            DestRect->left = TexWidth - MoveRect->DestinationRect.right;
            DestRect->top = TexHeight - MoveRect->DestinationRect.bottom;
            DestRect->right = TexWidth - MoveRect->DestinationRect.left;
            DestRect->bottom =  TexHeight - MoveRect->DestinationRect.top;
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE270:		// 旋转270°
        {
            SrcRect->left = MoveRect->SourcePoint.x;
            SrcRect->top = TexWidth - (MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left);
            SrcRect->right = MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top;
            SrcRect->bottom = TexWidth - MoveRect->SourcePoint.x;

            DestRect->left = MoveRect->DestinationRect.top;
            DestRect->top = TexWidth - MoveRect->DestinationRect.right;
            DestRect->right = MoveRect->DestinationRect.bottom;
            DestRect->bottom =  TexWidth - MoveRect->DestinationRect.left;
            break;
        }
        default:
        {
            RtlZeroMemory(DestRect, sizeof(RECT));
            RtlZeroMemory(SrcRect, sizeof(RECT));
            break;
        }
    }
}

//
// Copy move rectangles
// 复制移动矩形
//
DUPL_RETURN DISPLAYMANAGER::CopyMove(_Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(MoveCount) DXGI_OUTDUPL_MOVE_RECT* MoveBuffer, UINT MoveCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, INT TexWidth, INT TexHeight)
{
    D3D11_TEXTURE2D_DESC FullDesc;
    SharedSurf->GetDesc(&FullDesc);	// FullDesc就是SharedSurf的描述信息(属性)

    // Make new intermediate surface to copy into for moving
	// 为复制移动部分 创建新的中间层
    if (!m_MoveSurf)
    {
        D3D11_TEXTURE2D_DESC MoveDesc;
        MoveDesc = FullDesc;
        MoveDesc.Width = DeskDesc->DesktopCoordinates.right - DeskDesc->DesktopCoordinates.left;
        MoveDesc.Height = DeskDesc->DesktopCoordinates.bottom - DeskDesc->DesktopCoordinates.top;
        MoveDesc.BindFlags = D3D11_BIND_RENDER_TARGET;	// D3D11_BIND_FLAG, 代表绑定到显卡管线的方式, 参考"显卡渲染流水线"
        MoveDesc.MiscFlags = 0;
        HRESULT hr = m_Device->CreateTexture2D(&MoveDesc, nullptr, &m_MoveSurf);	// ID3D11Device::CreateTexture2D(), 创建Texture2D到m_MoveSurf

        if (FAILED(hr))
        {
            return ProcessFailure(m_Device, L"Failed to create staging texture for move rects", L"Error", hr, SystemTransitionsExpectedErrors);
        }
    }

	// 开始复制
    for (UINT i = 0; i < MoveCount; ++i)
    {
        RECT SrcRect;
        RECT DestRect;

        SetMoveRect(&SrcRect, &DestRect, DeskDesc, &(MoveBuffer[i]), TexWidth, TexHeight);	// DISPLAYMANAGER::SetMoveRect(), 从MoveBuffer并根据DeskDesc初始化SrcRect和DestRect

        // Copy rect out of shared surface
		// 从共享表面中复制rect
        D3D11_BOX Box;		// D3D11盒子模型
        Box.left = SrcRect.left + DeskDesc->DesktopCoordinates.left - OffsetX;
        Box.top = SrcRect.top + DeskDesc->DesktopCoordinates.top - OffsetY;
        Box.front = 0;
        Box.right = SrcRect.right + DeskDesc->DesktopCoordinates.left - OffsetX;
        Box.bottom = SrcRect.bottom + DeskDesc->DesktopCoordinates.top - OffsetY;
        Box.back = 1;
        m_DeviceContext->CopySubresourceRegion(m_MoveSurf, 0, SrcRect.left, SrcRect.top, 0, SharedSurf, 0, &Box);		// ID3D11DeviceContext::CopySubresourceRegion(), 复制源资源到目的资源的区域, 参数均为_in_
																																															// --- 参数说明:		m_MoveSurf				目的资源, 移动表面(中间层)
																																															//							0								目的子资源编号为0
																																															//							SrcRect.left				目的区域的x坐标
																																															//							SrcRect.top				目的区域的y坐标
																																															//							0								目的区域的z坐标
																																															//							SharedSurf				源资源, 共享表面
																																															//							0								源子资源的编号为0
																																															//							&Box						被复制的范围就是Box的范围

        // Copy back to shared surface
		// 复制回共享表面
        Box.left = SrcRect.left;
        Box.top = SrcRect.top;
        Box.front = 0;
        Box.right = SrcRect.right;
        Box.bottom = SrcRect.bottom;
        Box.back = 1;
        m_DeviceContext->CopySubresourceRegion(SharedSurf, 0, DestRect.left + DeskDesc->DesktopCoordinates.left - OffsetX, DestRect.top + DeskDesc->DesktopCoordinates.top - OffsetY, 0, m_MoveSurf, 0, &Box);
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Sets up vertices for dirty rects for rotated desktops
// 设置脏矩形的顶点, 支持桌面旋转
//
#pragma warning(push)
#pragma warning(disable:__WARNING_USING_UNINIT_VAR) // false positives in SetDirtyVert due to tool bug

void DISPLAYMANAGER::SetDirtyVert(_Out_writes_(NUMVERTICES) VERTEX* Vertices, _In_ RECT* Dirty, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ D3D11_TEXTURE2D_DESC* FullDesc, _In_ D3D11_TEXTURE2D_DESC* ThisDesc)	// VERTEX定义在<CommonTypes.h>中; 参数包括脏矩形范围、XY校正、输出设备描述、2个Texture2D描述(分别为FullDesc和ThisDesc)
{
    INT CenterX = FullDesc->Width / 2;	// 屏幕中心X
    INT CenterY = FullDesc->Height / 2;	// 屏幕中心Y

    INT Width = DeskDesc->DesktopCoordinates.right - DeskDesc->DesktopCoordinates.left;
    INT Height = DeskDesc->DesktopCoordinates.bottom - DeskDesc->DesktopCoordinates.top;

    // Rotation compensated destination rect
	// 目的脏矩形
    RECT DestDirty = *Dirty;

    // Set appropriate coordinates compensated for rotation
    switch (DeskDesc->Rotation)
    {
        case DXGI_MODE_ROTATION_ROTATE90:		// 旋转90°
        {
            DestDirty.left = Width - Dirty->bottom;
            DestDirty.top = Dirty->left;
            DestDirty.right = Width - Dirty->top;
            DestDirty.bottom = Dirty->right;

            Vertices[0].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[1].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE180:		// 旋转180°
        {
            DestDirty.left = Width - Dirty->right;
            DestDirty.top = Height - Dirty->bottom;
            DestDirty.right = Width - Dirty->left;
            DestDirty.bottom = Height - Dirty->top;

            Vertices[0].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[1].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE270:		// 旋转270°
        {
            DestDirty.left = Dirty->top;
            DestDirty.top = Height - Dirty->right;
            DestDirty.right = Dirty->bottom;
            DestDirty.bottom = Height - Dirty->left;

            Vertices[0].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[1].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            break;
        }
        default:
            assert(false); // drop through
        case DXGI_MODE_ROTATION_UNSPECIFIED:	// 未定义角度
        case DXGI_MODE_ROTATION_IDENTITY:			// 默认 
        {
			// Vertices[i].TexCoord代表这个点的2D纹理坐标
			// XMFLOAT2()定义在<DirectXMath.h>, 代表2D坐标
            Vertices[0].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));	
            Vertices[1].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            break;
        }
    }

    // Set positions
    Vertices[0].Pos = XMFLOAT3((DestDirty.left + DeskDesc->DesktopCoordinates.left - OffsetX - CenterX) / static_cast<FLOAT>(CenterX),
                             -1 * (DestDirty.bottom + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY),
                             0.0f);
    Vertices[1].Pos = XMFLOAT3((DestDirty.left + DeskDesc->DesktopCoordinates.left - OffsetX - CenterX) / static_cast<FLOAT>(CenterX),
                             -1 * (DestDirty.top + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY),
                             0.0f);
    Vertices[2].Pos = XMFLOAT3((DestDirty.right + DeskDesc->DesktopCoordinates.left - OffsetX - CenterX) / static_cast<FLOAT>(CenterX),
                             -1 * (DestDirty.bottom + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY),
                             0.0f);
    Vertices[3].Pos = Vertices[2].Pos;
    Vertices[4].Pos = Vertices[1].Pos;
    Vertices[5].Pos = XMFLOAT3((DestDirty.right + DeskDesc->DesktopCoordinates.left - OffsetX - CenterX) / static_cast<FLOAT>(CenterX),
                             -1 * (DestDirty.top + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY),
                             0.0f);

    Vertices[3].TexCoord = Vertices[2].TexCoord;
    Vertices[4].TexCoord = Vertices[1].TexCoord;
}

#pragma warning(pop) // re-enable __WARNING_USING_UNINIT_VAR

//
// Copies dirty rectangles
// 复制脏矩形
//
DUPL_RETURN DISPLAYMANAGER::CopyDirty(_In_ ID3D11Texture2D* SrcSurface, _Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(DirtyCount) RECT* DirtyBuffer, UINT DirtyCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc)
{
    HRESULT hr;

    D3D11_TEXTURE2D_DESC FullDesc;
    SharedSurf->GetDesc(&FullDesc);

    D3D11_TEXTURE2D_DESC ThisDesc;
    SrcSurface->GetDesc(&ThisDesc);

    if (!m_RTV)
    {
        hr = m_Device->CreateRenderTargetView(SharedSurf, nullptr, &m_RTV);		// ID3D11Device::CreateRenderTargetView(), 为资源数据创建渲染目标视图
																																//	--- 原型 :	_in_		ID3D11Resouce*										---- 要绘制的资源					
																																//					_in_		D3D11_RENDER_TARGET_VIEW_DESC*	---- 如果为NULL, 则访问所有资源
																																//					_out_	ID3D11RenderTargetView**						---- 渲染目标视图的首地址
        if (FAILED(hr))
        {
            return ProcessFailure(m_Device, L"Failed to create render target view for dirty rects", L"Error", hr, SystemTransitionsExpectedErrors);
        }
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC ShaderDesc;							// 着色器描述(属性)
    ShaderDesc.Format = ThisDesc.Format;													// DXGI_FORMAT
    ShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;	// 维度为2D
	ShaderDesc.Texture2D.MostDetailedMip = ThisDesc.MipLevels - 1;		// 设定mipmap边界值(= mipmap最大值 - 1), 参见"mipmap"
    ShaderDesc.Texture2D.MipLevels = ThisDesc.MipLevels;						// 设定mipmap最大值

    // Create new shader resource view
	// 创建新的着色器资源视图
    ID3D11ShaderResourceView* ShaderResource = nullptr;
    hr = m_Device->CreateShaderResourceView(SrcSurface, &ShaderDesc, &ShaderResource);		// ID3D11Device::CreateShaderResourceView(), 为资源数据创建着色器资源
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create shader resource view for dirty rects", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    FLOAT BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};																						// 混色参数
    m_DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);									// 设置OM(output-merger)的混合状态
    m_DeviceContext->OMSetRenderTargets(1, &m_RTV, nullptr);													// 设置OM的渲染目标
    m_DeviceContext->VSSetShader(m_VertexShader, nullptr, 0);													// 设置顶点着色器
    m_DeviceContext->PSSetShader(m_PixelShader, nullptr, 0);														// 设置像素着色器
    m_DeviceContext->PSSetShaderResources(0, 1, &ShaderResource);											// 设置像素着色器资源 
    m_DeviceContext->PSSetSamplers(0, 1, &m_SamplerLinear);														// 设置采样器
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);		// 将底层原生数据用三角形序列解析

    // Create space for vertices for the dirty rects if the current space isn't large enough
	// 如果脏矩形的顶点空间不够, 则需要重新开辟空间
    UINT BytesNeeded = sizeof(VERTEX) * NUMVERTICES * DirtyCount;
    if (BytesNeeded > m_DirtyVertexBufferAllocSize)
    {
        if (m_DirtyVertexBufferAlloc)
        {
            delete [] m_DirtyVertexBufferAlloc;
        }

        m_DirtyVertexBufferAlloc = new (std::nothrow) BYTE[BytesNeeded];
        if (!m_DirtyVertexBufferAlloc)
        {
            m_DirtyVertexBufferAllocSize = 0;
            return ProcessFailure(nullptr, L"Failed to allocate memory for dirty vertex buffer.", L"Error", E_OUTOFMEMORY);
        }

        m_DirtyVertexBufferAllocSize = BytesNeeded;
    }

    // Fill them in
	// 向开辟的空间DirtyVertex添加内容
    VERTEX* DirtyVertex = reinterpret_cast<VERTEX*>(m_DirtyVertexBufferAlloc);
    for (UINT i = 0; i < DirtyCount; ++i, DirtyVertex += NUMVERTICES)
    {
        SetDirtyVert(DirtyVertex, &(DirtyBuffer[i]), OffsetX, OffsetY, DeskDesc, &FullDesc, &ThisDesc);
    }

    // Create vertex buffer
	// 创建顶点缓冲
    D3D11_BUFFER_DESC BufferDesc;
    RtlZeroMemory(&BufferDesc, sizeof(BufferDesc));
    BufferDesc.Usage = D3D11_USAGE_DEFAULT;
    BufferDesc.ByteWidth = BytesNeeded;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// 管线的绑定类型是VERTEX_BUFFER
    BufferDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    RtlZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = m_DirtyVertexBufferAlloc;

    ID3D11Buffer* VertBuf = nullptr;
    hr = m_Device->CreateBuffer(&BufferDesc, &InitData, &VertBuf);		// 创建Id3D11Buffer*类型 
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create vertex buffer in dirty rect processing", L"Error", hr, SystemTransitionsExpectedErrors);
    }
    UINT Stride = sizeof(VERTEX);
    UINT Offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &VertBuf, &Stride, &Offset);	// 设定输入汇编层次(IA)的顶点缓冲

    D3D11_VIEWPORT VP;
    VP.Width = static_cast<FLOAT>(FullDesc.Width);
    VP.Height = static_cast<FLOAT>(FullDesc.Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    m_DeviceContext->RSSetViewports(1, &VP);												// 设定管线的光栅 

    m_DeviceContext->Draw(NUMVERTICES * DirtyCount, 0);							// 创建原画

    VertBuf->Release();
    VertBuf = nullptr;

    ShaderResource->Release();
    ShaderResource = nullptr;

    return DUPL_RETURN_SUCCESS;
}

//
// Clean all references
// 清空所有引用计数
//
void DISPLAYMANAGER::CleanRefs()
{
    if (m_DeviceContext)
    {
        m_DeviceContext->Release();
        m_DeviceContext = nullptr;
    }

    if (m_Device)
    {
        m_Device->Release();
        m_Device = nullptr;
    }

    if (m_MoveSurf)
    {
        m_MoveSurf->Release();
        m_MoveSurf = nullptr;
    }

    if (m_VertexShader)
    {
        m_VertexShader->Release();
        m_VertexShader = nullptr;
    }

    if (m_PixelShader)
    {
        m_PixelShader->Release();
        m_PixelShader = nullptr;
    }

    if (m_InputLayout)
    {
        m_InputLayout->Release();
        m_InputLayout = nullptr;
    }

    if (m_SamplerLinear)
    {
        m_SamplerLinear->Release();
        m_SamplerLinear = nullptr;
    }

    if (m_RTV)
    {
        m_RTV->Release();
        m_RTV = nullptr;
    }
}
