#include <iostream>
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <iostream>
#include <unordered_map>

#pragma comment(lib, "Gdiplus.lib")
using namespace Gdiplus;

// ���� ����
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

// ����� ������ ������ ����ü
struct MonitorInfo {
	int x, y, width, height;
	float ratio;
};

// EnumDisplayMonitors �ݹ� �Լ�
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
	// ��� ����͸� ���δ� ������ �簢�� ���
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

	// ��� ����� ����
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));
	if (monitors.empty()) {
		std::cout << "����͸� ã�� �� �����ϴ�." << std::endl;
		return;
	}

	monitors = CalcForMainScreen(monitors);

	MonitorInfo bound = GetBoundingMonitors(monitors);

	// ��Ʈ�� ����
	HDC hScreenDC = GetDC(nullptr);
	HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
	HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, bound.width, bound.height);
	SelectObject(hMemoryDC, hBitmap);

	// �� ���÷��̿��� ��Ʈ���� ����
	int offsetX = 0;
	for (const auto& monitor : monitors) {
		BitBlt(hMemoryDC, monitor.x - bound.x, monitor.y - bound.y, monitor.width, monitor.height, hScreenDC, monitor.x, monitor.y, SRCCOPY);
	}

	// ���� ���� ����
	Bitmap bitmap(hBitmap, nullptr);
	CLSID clsid;
	GetEncoderClsid(mimetype.c_str(), &clsid);

	// ���� ����
	bitmap.Save(filename.c_str(), &clsid, nullptr);

	// ���ҽ� ����
	DeleteObject(hBitmap);
	DeleteDC(hMemoryDC);
	ReleaseDC(nullptr, hScreenDC);

	// ��� ���
	std::cout << "ltrb: " << bound.x << ", " << bound.y << ", " << bound.x + bound.width << ", " << bound.y + bound.height << std::endl;
	std::cout << "xywh: " << bound.x << ", " << bound.y << ", " << bound.width << ", " << bound.height << std::endl;
}

// �̹��� Ȯ���ڿ� �ش��ϴ� MIME Ÿ���� ��ȯ�ϴ� �Լ�
std::wstring getMimeType(const std::string& extension) {
	// �̹��� Ȯ���ڿ� MIME Ÿ���� ������ ��
	std::unordered_map<std::string, std::wstring> mimeTypes = {
		{".png", L"image/png"},
		{".jpg", L"image/jpeg"},
		{".jpeg", L"image/jpeg"},
		{".bmp", L"image/bmp"},
		{".gif", L"image/gif"}
	};

	auto it = mimeTypes.find(extension);
	if (it != mimeTypes.end()) {
		return it->second;  // Ȯ���ڿ� �´� MIME Ÿ�� ��ȯ
	}
	return L"unknown/unknown";  // ��ȿ�� Ȯ���ڰ� �ƴ� ���
}

// �̹��� Ȯ���� ���
bool isValidImageExtension(const std::string& filename) {
	// Ȯ���� ���
	std::string validExtensions[] = { ".png", ".jpg", ".jpeg", ".bmp", ".gif" };

	for (const std::string& ext : validExtensions) {
		if (filename.size() >= ext.size() &&
			filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0) {
			return true;
		}
	}
	return false;
}


// ���ϸ�� Ȯ���� �� MIME Ÿ���� ��ȯ�ϴ� �Լ�
bool getFilenameAndExtension(const std::string& input, std::wstring& filename, std::string& extension, std::wstring& mimeType) {
	// ���ڰ� ������ �⺻�� "screenshot.png"�� ���
	std::string filenameInput = input.empty() ? "screenshot.png" : input;

	// Ȯ���ڰ� ���ų� �̹��� Ȯ���ڰ� �ƴϸ� false ��ȯ
	size_t dotPos = filenameInput.rfind('.');
	if (dotPos == std::string::npos) {
		return false;  // Ȯ���ڰ� ������ false ��ȯ
	}

	extension = filenameInput.substr(dotPos); // Ȯ���� �κи� ����
	if (!isValidImageExtension(filenameInput)) {
		return false;  // ��ȿ�� �̹��� Ȯ���ڰ� �ƴϸ� false ��ȯ
	}
	//mbstowcs(&filename[0], filenameInput.c_str(), filenameInput.length());
	size_t convertedChars = 0;
	wchar_t wstr[100]; // ����� ũ���� �迭 �Ҵ�
	mbstowcs_s(&convertedChars, wstr, filenameInput.length() + 1, filenameInput.c_str(), _TRUNCATE);
	filename = wstr;

	// MIME Ÿ���� �����ɴϴ�
	mimeType = getMimeType(extension);

	return true;
}

int main(int argc, char* argv[])
{
	std::string extension;
	std::wstring filename;
	std::wstring mimeType;

	// ���ڰ� ���ų� �ϳ��� �־����� �� ó��
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