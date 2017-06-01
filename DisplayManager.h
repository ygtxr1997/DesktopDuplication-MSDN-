// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

// --- 显示管理工具
// --- 包括D3D资源的初始化(包括显卡管线的初始化)和帧的处理(脏矩形算法)

#ifndef _DISPLAYMANAGER_H_
#define _DISPLAYMANAGER_H_

#include "CommonTypes.h"

//
// Handles the task of processing frames
// 处理帧的任务
//
class DISPLAYMANAGER
{
    public:
        DISPLAYMANAGER();
        ~DISPLAYMANAGER();
        void InitD3D(DX_RESOURCES* Data);	// 初始化D3D资源
        ID3D11Device* GetDevice();				// 获取D3D设备
        DUPL_RETURN ProcessFrame(_In_ FRAME_DATA* Data, _Inout_ ID3D11Texture2D* SharedSurf, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc);	// 处理帧
        void CleanRefs();									// 清空引用计数

    private:
    // methods
        DUPL_RETURN CopyDirty(_In_ ID3D11Texture2D* SrcSurface, _Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(DirtyCount) RECT* DirtyBuffer, UINT DirtyCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc);										// 复制脏矩形
        DUPL_RETURN CopyMove(_Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(MoveCount) DXGI_OUTDUPL_MOVE_RECT* MoveBuffer, UINT MoveCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, INT TexWidth, INT TexHeight);	// 复制移动矩形
        void SetDirtyVert(_Out_writes_(NUMVERTICES) VERTEX* Vertices, _In_ RECT* Dirty, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ D3D11_TEXTURE2D_DESC* FullDesc, _In_ D3D11_TEXTURE2D_DESC* ThisDesc);								// 设置脏矩形顶点
        void SetMoveRect(_Out_ RECT* SrcRect, _Out_ RECT* DestRect, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ DXGI_OUTDUPL_MOVE_RECT* MoveRect, INT TexWidth, INT TexHeight);																													// 设置移动矩形

    // variables
        ID3D11Device* m_Device;								// 设备
        ID3D11DeviceContext* m_DeviceContext;		// 设备上下文
        ID3D11Texture2D* m_MoveSurf;						// 移动表面
        ID3D11VertexShader* m_VertexShader;			// 顶点着色器
        ID3D11PixelShader* m_PixelShader;				// 像素着色器
        ID3D11InputLayout* m_InputLayout;				// 输入布局
        ID3D11RenderTargetView* m_RTV;					// 渲染目标视图
        ID3D11SamplerState* m_SamplerLinear;			// 采样器
        BYTE* m_DirtyVertexBufferAlloc;						// 脏顶点缓冲区首地址
        UINT m_DirtyVertexBufferAllocSize;					// 脏顶点缓冲区大小
};

#endif
