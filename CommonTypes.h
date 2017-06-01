// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

// --- 程序依赖的库都在这里声明
// --- 各种通用的类型都在此#define或者typedef
// --- 还声明了一些异常处理用到的数组

#ifndef _COMMONTYPES_H_
#define _COMMONTYPES_H_

#include <windows.h>		// Windows必备库
#include <d3d11.h>			// Direct3D 11
#include <dxgi1_2.h>			// DirectX 图形基础架构
#include <sal.h>					// 微软大大弄出来的标准注释语言
#include <new>					// new一个对象
#include <warning.h>			// 包含一些常用的#pragma (warning)
#include <DirectXMath.h>	// 图形应用程序数学库

#include "PixelShader.h"		// 像素绘图头文件
#include "VertexShader.h"	// 顶点绘图头文件

#define NUMVERTICES 6		// 为什么平面会有6个顶点？猜想会用到3D模型
#define BPP         4				// 像素深度: 每个像素用的位数

#define OCCLUSION_STATUS_MSG WM_USER		// 用于防止用户ID和系统ID发送的消息冲突，微软大大非常喜欢定义自己的宏

extern HRESULT SystemTransitionsExpectedErrors[];		/* 用于异常处理的5个数组 */
extern HRESULT CreateDuplicationExpectedErrors[];
extern HRESULT FrameInfoExpectedErrors[];
extern HRESULT AcquireFrameExpectedError[];
extern HRESULT EnumOutputsExpectedErrors[];

typedef _Return_type_success_(return == DUPL_RETURN_SUCCESS) enum		// DUPL_RETURN 相当于 HRESULT，表示一个函数的执行结果(成功、预期内异常、意外异常)
{
    DUPL_RETURN_SUCCESS             = 0,
    DUPL_RETURN_ERROR_EXPECTED      = 1,
    DUPL_RETURN_ERROR_UNEXPECTED    = 2
}DUPL_RETURN;

_Post_satisfies_(return != DUPL_RETURN_SUCCESS)
DUPL_RETURN ProcessFailure(_In_opt_ ID3D11Device* Device, _In_ LPCWSTR Str, _In_ LPCWSTR Title, HRESULT hr, _In_opt_z_ HRESULT* ExpectedErrors = nullptr);	// 用于显示抛出异常的函数

void DisplayMsg(_In_ LPCWSTR Str, _In_ LPCWSTR Title, HRESULT hr);		// 显示消息

//
// Holds info about the pointer/cursor
// 保存鼠标指针信息
//
typedef struct _PTR_INFO
{
    _Field_size_bytes_(BufferSize) BYTE* PtrShapeBuffer;	// 自定义，鼠标形状缓冲区
    DXGI_OUTDUPL_POINTER_SHAPE_INFO ShapeInfo;	// dxgi1_2.h，包括形状、宽高、角度、焦点
    POINT Position;															// 鼠标位置
    bool Visible;																	// 鼠标是否可见
    UINT BufferSize;															// 缓冲区大小
    UINT WhoUpdatedPositionLast;									// 谁更新了鼠标位置？
    LARGE_INTEGER LastTimeStamp;								// 时间戳
} PTR_INFO;

//
// Structure that holds D3D resources not directly tied to any one thread
// 保存D3D资源相关(不绑定线程)
//
typedef struct _DX_RESOURCES
{
    ID3D11Device* Device;							// 设备
    ID3D11DeviceContext* Context;				// 设备上下文
    ID3D11VertexShader* VertexShader;		// 顶点着色器
    ID3D11PixelShader* PixelShader;				// 像素着色器
    ID3D11InputLayout* InputLayout;			// 输入布局
    ID3D11SamplerState* SamplerLinear;		// 采样器
} DX_RESOURCES;

//
// Structure to pass to a new thread
// 传递给线程的结构
//
typedef struct _THREAD_DATA
{
    // Used to indicate abnormal error condition
	// 非预期异常
    HANDLE UnexpectedErrorEvent;

    // Used to indicate a transition event occurred e.g. PnpStop, PnpStart, mode change, TDR, desktop switch and the application needs to recreate the duplication interface
	// 预期异常
    HANDLE ExpectedErrorEvent;

    // Used by WinProc to signal to threads to exit
	// WinProc是Windows窗口的回调函数，TerminateThreadsEvent用来被WinProc调用以终止线程
    HANDLE TerminateThreadsEvent;

    HANDLE TexSharedHandle;		// 纹理共享的句柄？
    UINT Output;							// 输出数
    INT OffsetX;								// X校正值
    INT OffsetY;								// Y校正值
    PTR_INFO* PtrInfo;					// 前面定义，鼠标指针信息
    DX_RESOURCES DxRes;			// 前面定义，包含一些D3D资源相关的变量
} THREAD_DATA;

//
// FRAME_DATA holds information about an acquired frame
// FRAME_DATA帧信息，保存一张捕获到的帧
//
typedef struct _FRAME_DATA
{
    ID3D11Texture2D* Frame;																																							// d3d11.h, D3D的2D纹理，可以理解为一张图
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;																																	// dxgi1_2.h, 包含上次出现的时间、鼠标更新的时间、鼠标指针位置、缓冲区大小等信息
    _Field_size_bytes_((MoveCount * sizeof(DXGI_OUTDUPL_MOVE_RECT)) + (DirtyCount * sizeof(RECT))) BYTE* MetaData;			// 元数据MetaData，占用内存大小=移动矩形的大小+脏矩形的大小
    UINT DirtyCount;																																											// 脏矩形数
    UINT MoveCount;																																										// 移动矩形数
} FRAME_DATA;

//
// A vertex with a position and texture coordinate
// 顶点Vertex, 包含3D位置和2D纹理坐标
//
typedef struct _VERTEX
{
    DirectX::XMFLOAT3 Pos;					// 3D坐标，这或许是前面宏定义定点数为6的原因
    DirectX::XMFLOAT2 TexCoord;		// 2D纹理坐标，也就是桌面这个surface
} VERTEX;

#endif
