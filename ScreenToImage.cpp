#include <iostream>
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <iostream>
#include <unordered_map>

#pragma comment(lib, "Gdiplus.lib")
using namespace Gdiplus;

// 전역 변수
ULONG_PTR gdiplusToken;

void InitGDIPlus() {
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
}

void ShutdownGDIPlus() {
	GdiplusShutdown(gdiplusToken);
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j) {
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;
		}
	}

	free(pImageCodecInfo);
	return -1;
}

// 모니터 정보를 저장할 구조체
struct MonitorInfo {
	int x, y, width, height;
	float ratio;
};

// EnumDisplayMonitors 콜백 함수
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	std::vector<MonitorInfo>* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(dwData);
	MONITORINFOEX mi;
	mi.cbSize = sizeof(MONITORINFOEX);
	if (GetMonitorInfo(hMonitor, &mi)) {
		MonitorInfo info{};
		info.x = mi.rcMonitor.left;
		info.y = mi.rcMonitor.top;

		DEVMODE devmode = {};
		devmode.dmSize = sizeof(DEVMODE);
		EnumDisplaySettings(mi.szDevice, ENUM_CURRENT_SETTINGS, &devmode);
		info.width = devmode.dmPelsWidth;
		info.height = devmode.dmPelsHeight;

		float monitorWidth = mi.rcMonitor.right - mi.rcMonitor.left;
		info.ratio = devmode.dmPelsWidth / monitorWidth;

		//std::cout << "L: " << mi.rcMonitor.left << ", T: " << mi.rcMonitor.top << ", R: " << mi.rcMonitor.right << ", B: " << mi.rcMonitor.bottom << std::endl;
		//std::cout << "pW:" << devmode.dmPelsWidth << ", pH:" << devmode.dmPelsHeight << std::endl;
		//std::cout << "Ratio:" << devmode.dmPelsWidth / monitorWidth << std::endl;
		//std::cout << "===================================" << std::endl;
		monitors->push_back(info);
	}
	return TRUE;
}

MonitorInfo GetBoundingMonitors(std::vector<MonitorInfo> monitors) {
	// 모든 모니터를 감싸는 가상의 사각형 계산
	int minX = monitors[0].x;
	int minY = monitors[0].y;
	int maxX = monitors[0].x + monitors[0].width;
	int maxY = monitors[0].y + monitors[0].height;

	for (const auto& monitor : monitors) {
		if (monitor.x < minX) minX = monitor.x;
		if (monitor.y < minY) minY = monitor.y;
		if (monitor.x + monitor.width > maxX) maxX = monitor.x + monitor.width;
		if (monitor.y + monitor.height > maxY) maxY = monitor.y + monitor.height;

		//std::cout << "Monitor: (" << monitor.x << ", " << monitor.y << ") " << monitor.width << "x" << monitor.height << std::endl;
	}

	//std::cout << "Bounding Rectangle: (" << minX << ", " << minY << ") - (" << maxX << ", " << maxY << ")" << std::endl;
	//std::cout << "Bounding Size: " << (maxX - minX) << "x" << (maxY - minY) << std::endl;

	int totalWidth = maxX - minX;
	int totalHeight = maxY - minY;
	MonitorInfo info{ minX, minY, totalWidth, totalHeight };
	return info;
}

std::vector<MonitorInfo> CalcForMainScreen(std::vector<MonitorInfo> monitors) {
	float minRatio = 9999;
	for (const auto& monitor : monitors) {
		if (minRatio > monitor.ratio) minRatio = monitor.ratio;
	}
	//std::cout << "minRatio: " << minRatio << std::endl;

	for (auto& monitor : monitors) {
		float calcRatio = monitor.ratio != minRatio ? 1 : minRatio;
		monitor.x *= calcRatio;
		monitor.y *= calcRatio;
	}

	return monitors;
}

void CaptureAllScreens(const std::wstring& filename, const std::wstring& mimetype) {
	std::vector<MonitorInfo> monitors;

	// 모든 모니터 열거
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));
	if (monitors.empty()) {
		std::cout << "모니터를 찾을 수 없습니다." << std::endl;
		return;
	}

	monitors = CalcForMainScreen(monitors);

	MonitorInfo bound = GetBoundingMonitors(monitors);

	// 비트맵 생성
	HDC hScreenDC = GetDC(nullptr);
	HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
	HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, bound.width, bound.height);
	SelectObject(hMemoryDC, hBitmap);

	// 각 디스플레이에서 비트맵을 복사
	int offsetX = 0;
	for (const auto& monitor : monitors) {
		BitBlt(hMemoryDC, monitor.x - bound.x, monitor.y - bound.y, monitor.width, monitor.height, hScreenDC, monitor.x, monitor.y, SRCCOPY);
	}

	// 저장 형식 지정
	Bitmap bitmap(hBitmap, nullptr);
	CLSID clsid;
	GetEncoderClsid(mimetype.c_str(), &clsid);

	// 파일 저장
	bitmap.Save(filename.c_str(), &clsid, nullptr);

	// 리소스 정리
	DeleteObject(hBitmap);
	DeleteDC(hMemoryDC);
	ReleaseDC(nullptr, hScreenDC);

	// 결과 출력
	std::cout << "ltrb: " << bound.x << ", " << bound.y << ", " << bound.x + bound.width << ", " << bound.y + bound.height << std::endl;
	std::cout << "xywh: " << bound.x << ", " << bound.y << ", " << bound.width << ", " << bound.height << std::endl;
}

// 이미지 확장자에 해당하는 MIME 타입을 반환하는 함수
std::wstring getMimeType(const std::string& extension) {
	// 이미지 확장자와 MIME 타입을 매핑한 맵
	std::unordered_map<std::string, std::wstring> mimeTypes = {
		{".png", L"image/png"},
		{".jpg", L"image/jpeg"},
		{".jpeg", L"image/jpeg"},
		{".bmp", L"image/bmp"},
		{".gif", L"image/gif"}
	};

	auto it = mimeTypes.find(extension);
	if (it != mimeTypes.end()) {
		return it->second;  // 확장자에 맞는 MIME 타입 반환
	}
	return L"unknown/unknown";  // 유효한 확장자가 아닐 경우
}

// 이미지 확장자 목록
bool isValidImageExtension(const std::string& filename) {
	// 확장자 목록
	std::string validExtensions[] = { ".png", ".jpg", ".jpeg", ".bmp", ".gif" };

	for (const std::string& ext : validExtensions) {
		if (filename.size() >= ext.size() &&
			filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0) {
			return true;
		}
	}
	return false;
}


// 파일명과 확장자 및 MIME 타입을 반환하는 함수
bool getFilenameAndExtension(const std::string& input, std::wstring& filename, std::string& extension, std::wstring& mimeType) {
	// 인자가 없으면 기본값 "screenshot.png"을 사용
	std::string filenameInput = input.empty() ? "screenshot.png" : input;

	// 확장자가 없거나 이미지 확장자가 아니면 false 반환
	size_t dotPos = filenameInput.rfind('.');
	if (dotPos == std::string::npos) {
		return false;  // 확장자가 없으면 false 반환
	}

	extension = filenameInput.substr(dotPos); // 확장자 부분만 추출
	if (!isValidImageExtension(filenameInput)) {
		return false;  // 유효한 이미지 확장자가 아니면 false 반환
	}
	//mbstowcs(&filename[0], filenameInput.c_str(), filenameInput.length());
	size_t convertedChars = 0;
	wchar_t wstr[100]; // 충분한 크기의 배열 할당
	mbstowcs_s(&convertedChars, wstr, filenameInput.length() + 1, filenameInput.c_str(), _TRUNCATE);
	filename = wstr;

	// MIME 타입을 가져옵니다
	mimeType = getMimeType(extension);

	return true;
}

int main(int argc, char* argv[])
{
	std::string extension;
	std::wstring filename;
	std::wstring mimeType;

	// 인자가 없거나 하나만 주어졌을 때 처리
	std::string input = (argc == 1) ? "" : argv[1];

	if (!getFilenameAndExtension(input, filename, extension, mimeType)) {
		std::cerr << "Error: Invalid input or invalid image extension." << std::endl;
		return 1;
	}
	std::wcout << "Filename: " << filename << std::endl;
	std::cout << "Extension: " << extension << std::endl;
	std::wcout << L"MIME Type: " << mimeType << std::endl;

	InitGDIPlus();
	CaptureAllScreens(filename, mimeType);
	ShutdownGDIPlus();
	return 0;
}