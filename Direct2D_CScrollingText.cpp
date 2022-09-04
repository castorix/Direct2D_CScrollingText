#include <windows.h>
#include <tchar.h>

//#include <commctrl.h>
//#pragma comment (lib, "comctl32")

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "CScrollingText.h"

HINSTANCE hInst;
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int nWidth = 678, nHeight = 694;

ID2D1Factory* m_pD2DFactory = NULL;
ID2D1Factory1* m_pD2DFactory1 = NULL;
IDWriteFactory* m_pDWriteFactory = NULL;
IWICImagingFactory* m_pWICImagingFactory = NULL;

HRESULT CreateD2D1Factory();
HRESULT CreateDWriteFactory();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	hInst = hInstance;
	WNDCLASSEX wcex =
	{
		sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInst, LoadIcon(NULL, IDI_APPLICATION),
		LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1), NULL, TEXT("WindowClass"), NULL,
	};
	if (!RegisterClassEx(&wcex))
		return MessageBox(NULL, TEXT("Cannot register class !"), TEXT("Error"), MB_ICONERROR | MB_OK);
	int nX = (GetSystemMetrics(SM_CXSCREEN) - nWidth) / 2, nY = (GetSystemMetrics(SM_CYSCREEN) - nHeight) / 2;
	HWND hWnd = CreateWindowEx(0, wcex.lpszClassName, TEXT("Direct2D Scrolling text"), WS_OVERLAPPEDWINDOW, nX, nY, nWidth, nHeight, NULL, NULL, hInst, NULL);
	if (!hWnd)
		return MessageBox(NULL, TEXT("Cannot create window !"), TEXT("Error"), MB_ICONERROR | MB_OK);
	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{	
	static CScrollingText *pSt1 = NULL, *pSt2 = NULL, *pSt3 = NULL, *pSt4 = NULL;
	//int wmId, wmEvent;
	switch (message)
	{
	case WM_CREATE:
	{
		HRESULT hr = CoInitialize(NULL);
		if (SUCCEEDED(hr))
		{
			hr = CreateD2D1Factory();
			if (SUCCEEDED(hr))
			{
				hr = CreateDWriteFactory();
				if (SUCCEEDED(hr))
				{
					hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pWICImagingFactory));
					if (SUCCEEDED(hr))
					{
						pSt1 = new CScrollingText(hInst, hWnd, 29, 31, 602, 78);
						pSt1->Initialize(m_pD2DFactory1, m_pDWriteFactory, m_pWICImagingFactory, L"This is a text with shadow", L"Times New roman", 35, 602, 78, 0, false, false, true);
						pSt1->SetTextColor(D2D1::ColorF::Blue);
						pSt1->SetGradientBackground(D2D1::ColorF::Yellow, D2D1::ColorF::Red);
						pSt1->SetSpeed(1.5);

						pSt2 = new CScrollingText(hInst, hWnd, 29, 144, 602, 174);
						pSt2->Initialize(m_pD2DFactory1, m_pDWriteFactory, m_pWICImagingFactory, L"Text with Gabriola font and fading, speed = 0.5", L"Gabriola", 48, 602, 174, 0, false, false, false, true);
						pSt2->SetGradientBackground(D2D1::ColorF::Magenta, D2D1::ColorF::DarkBlue);
						pSt2->SetTextColor(D2D1::ColorF::Yellow);
						pSt2->SetSpeed(0.5);

						pSt3 = new CScrollingText(hInst, hWnd, 29, 353, 602, 66);
						pSt3->Initialize(m_pD2DFactory1, m_pDWriteFactory, m_pWICImagingFactory, L"This is a bold text, speed = 3", L"Great Vibes", 36, 602, 66, 0, true);
						pSt3->SetGradientBackground(D2D1::ColorF::Lime, D2D1::ColorF::Orange);
						pSt3->SetTextColor(D2D1::ColorF::Green);
						pSt3->SetSpeed(3);

						pSt4 = new CScrollingText(hInst, hWnd, 29, 454, 602, 167);
						WCHAR wsText1[] = { 0xD83D,  0xDE00, 0x00 };
						WCHAR wsText2[] = { 0xD83D,  0xDE02, 0x00 };
						WCHAR wsText[260];
						lstrcpy(wsText, wsText1);
						lstrcat(wsText, L"Shadow Segoe UI Emoji");
						lstrcat(wsText, wsText2);
						lstrcat(wsText, L"Vertical scrolling");
						pSt4->Initialize(m_pD2DFactory1, m_pDWriteFactory, m_pWICImagingFactory, wsText, L"Segoe UI Emoji", 40, 602, 167, 1, false, false, true);
						pSt4->SetBitmapBackgroundFromURL(L"https://i.ibb.co/mNvT0VJ/Flowers-Background.jpg");
					}
				}
			}		
		}
		return 0;
	}
	break;
	case WM_DESTROY:
	{		
		SafeRelease(&m_pWICImagingFactory);
		SafeRelease(&m_pDWriteFactory);
		SafeRelease(&m_pD2DFactory1);
		SafeRelease(&m_pD2DFactory);
		CoUninitialize();
	/*	delete pSt1;
		delete pSt2;
		delete pSt3;
		delete pSt4;*/
		PostQuitMessage(0);
		return 0;
	}
	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

HRESULT CreateD2D1Factory()
{
	HRESULT hr = S_OK;
	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));
	options.debugLevel = D2D1_DEBUG_LEVEL::D2D1_DEBUG_LEVEL_INFORMATION;
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, options, &m_pD2DFactory);
	//m_pD2DFactory1 = static_cast<ID2D1Factory1*>(m_pD2DFactory);
	hr = m_pD2DFactory->QueryInterface((ID2D1Factory1**)&m_pD2DFactory1);
	return hr;
}

HRESULT CreateDWriteFactory()
{
	HRESULT hr = S_OK;
	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE::DWRITE_FACTORY_TYPE_SHARED, __uuidof(m_pDWriteFactory), reinterpret_cast<IUnknown**>(&m_pDWriteFactory));
	return hr;
}

