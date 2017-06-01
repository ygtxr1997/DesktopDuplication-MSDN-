// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

// --- 线程管理工具，类似线程池
// --- 包括初始化D3D资源、获取鼠标指针信息、开始和停止线程等方法

#ifndef _THREADMANAGER_H_
#define _THREADMANAGER_H_

#include "CommonTypes.h"

class THREADMANAGER
{
    public:
        THREADMANAGER();
        ~THREADMANAGER();
        void Clean();											// 清空线程？
        DUPL_RETURN Initialize(INT SingleOutput, UINT OutputCount, HANDLE UnexpectedErrorEvent, HANDLE ExpectedErrorEvent, HANDLE TerminateThreadsEvent, HANDLE SharedHandle, _In_ RECT* DesktopDim);		// 线程管理工具(线程池)的初始化
        PTR_INFO* GetPointerInfo();				// 获取鼠标指针信息
        void WaitForThreadTermination();		// 等待线程终止

    private:
        DUPL_RETURN InitializeDx(_Out_ DX_RESOURCES* Data);			// 初始化D3D资源
        void CleanDx(_Inout_ DX_RESOURCES* Data);								// 清空D3D资源

        PTR_INFO m_PtrInfo;																	// 成员，鼠标指针信息
        UINT m_ThreadCount;																	// 成员，线程数
        _Field_size_(m_ThreadCount) HANDLE* m_ThreadHandles;			// 成员，线程句柄数组，元素为句柄
        _Field_size_(m_ThreadCount) THREAD_DATA* m_ThreadData;	// 成员，线程数据数组，元素为线程数据，线程数据THREAD_DATA在<CommonTypes.h>定义
};

#endif
