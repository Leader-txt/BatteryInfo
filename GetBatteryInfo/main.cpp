#include <windows.h>
#include <Poclass.h>
#include <setupapi.h>
#include <Batclass.h>
#include <stdio.h>
#include <powerbase.h>
#include <gl/glut.h>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <string>

using namespace std;

#define WINDOW_X 10
#define WINDOW_Y 10
#define WINDOW_W 250
#define WINDOW_H 110
#define LINE_W 230
#define LINE_H 80
#define LINE_X 10
#define LINE_Y 10
#define REFRESH 1000
#define MAXDATAS 50

DEFINE_GUID(GUID_DEVCLASS_BATTERY, 0x72631E54, 0x78A4, 0x11D0, 0xBC, 0xF7, 0x00, 0xAA, 0x00, 0xB7, 0xB3, 0x2A);

#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib,"PowrProf.lib")
#pragma comment(linker,"/subsystem:\"windows\" /entry:\"mainCRTStartup\"")

BATTERY_STATUS GetBatteryState(void)
{
	BATTERY_STATUS batteryStatus = { 0 };
	HDEVINFO hdev = ::SetupDiGetClassDevs(&GUID_DEVCLASS_BATTERY, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (INVALID_HANDLE_VALUE != hdev)
	{
		//搜索限制为最多 100 块电池
		for (int idev = 0; idev < 100; idev++)
		{
			SP_DEVICE_INTERFACE_DATA did = { 0 };
			did.cbSize = sizeof(did);

			if (::SetupDiEnumDeviceInterfaces(hdev, 0, &GUID_DEVCLASS_BATTERY, idev, &did))
			{
				DWORD cbRequired = 0;

				::SetupDiGetDeviceInterfaceDetail(hdev, &did, 0, 0, &cbRequired, 0);
				if (ERROR_INSUFFICIENT_BUFFER == ::GetLastError())
				{
					PSP_DEVICE_INTERFACE_DETAIL_DATA pdidd = (PSP_DEVICE_INTERFACE_DETAIL_DATA)::LocalAlloc(LPTR, cbRequired);
					if (pdidd)
					{
						pdidd->cbSize = sizeof(*pdidd);
						if (::SetupDiGetDeviceInterfaceDetail(hdev, &did, pdidd, cbRequired, &cbRequired, 0))
						{
							// 枚举电池, 索取信息
							HANDLE hBattery = ::CreateFile(pdidd->DevicePath,
								GENERIC_READ | GENERIC_WRITE,
								FILE_SHARE_READ | FILE_SHARE_WRITE,
								NULL,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL,
								NULL);
							if (INVALID_HANDLE_VALUE != hBattery)
							{
								// 向电池索取标签.
								BATTERY_QUERY_INFORMATION bqi = { 0 };

								DWORD dwWait = 0;
								DWORD dwOut;

								if (::DeviceIoControl(hBattery,
									IOCTL_BATTERY_QUERY_TAG,
									&dwWait,
									sizeof(dwWait),
									&bqi.BatteryTag,
									sizeof(bqi.BatteryTag),
									&dwOut,
									NULL)
									&& bqi.BatteryTag)
								{
									//查询电池状态
									BATTERY_WAIT_STATUS bws = { 0 };
									bws.BatteryTag = bqi.BatteryTag;
									BATTERY_STATUS bs = { 0 };

									if (::DeviceIoControl(hBattery,
										IOCTL_BATTERY_QUERY_STATUS,
										&bws,
										sizeof(bws),
										&bs,
										sizeof(bs),
										&dwOut,
										NULL))
									{
										batteryStatus.Capacity = bs.Capacity;
										batteryStatus.PowerState = bs.PowerState;
										batteryStatus.Voltage = bs.Voltage;
										batteryStatus.Rate = bs.Rate;
									}
								}
								::CloseHandle(hBattery);
							}
						}
						::LocalFree(pdidd);
					}
				}
			}
			else  if (ERROR_NO_MORE_ITEMS == ::GetLastError())
			{
				break;
			}
		}
		::SetupDiDestroyDeviceInfoList(hdev);
	}

	return batteryStatus;
}

void Initial(void)
{
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);          //设置窗口背景色为白色
	glMatrixMode(GL_PROJECTION);        //指定设定投影参数
	gluOrtho2D(0.0, WINDOW_W, 0.0, WINDOW_H);            //设置投影参数
}

vector<long> datas;
long Index = -1;

void myDisplay(void)
{
	LONG ntStatus = S_OK;
	SYSTEM_BATTERY_STATE pi = { 0 };

	ntStatus = CallNtPowerInformation(SystemBatteryState, NULL, 0, &pi, sizeof(pi));

	BATTERY_STATUS bs = GetBatteryState();
	string restPower = to_string(pi.RemainingCapacity / (float)pi.MaxCapacity * 100);
	restPower = restPower.substr(0, restPower.find('.') + 1 + 2) + "%";
	long rateVal = abs(bs.Rate);
	long caVal = bs.Rate > 0 ? pi.MaxCapacity - pi.RemainingCapacity : pi.RemainingCapacity;
	string minute = bs.Rate ? to_string((int)round(caVal % rateVal / (float)rateVal * 60)) : "00";
	minute = minute.length() == 1 ? "0" + minute : minute;
	string restTime = bs.Rate ? to_string(caVal / rateVal) + ":" + minute : "∞";
	glutSetWindowTitle((restPower + " Rest[" + restTime+"]").c_str());
	Index = (Index + 1) % MAXDATAS;
	if (datas.size() < MAXDATAS) {
		datas.push_back(bs.Rate);
	}
	else {
		datas[Index] = bs.Rate;
	}

	glClear(GL_COLOR_BUFFER_BIT);        //用当前背景色填充窗口
	glColor3f(0.0f, 0.0f, 0.0f);                  //设置当前的绘图颜色为黑色
	//draw rect
	glBegin(GL_LINE_LOOP);
	glVertex2i(LINE_X, LINE_Y);
	glVertex2i(LINE_X, LINE_Y + LINE_H);
	glVertex2i(LINE_X + LINE_W, LINE_Y + LINE_H);
	glVertex2i(LINE_X + LINE_W, LINE_Y);
	glEnd();
	//draw lines
	glBegin(GL_LINE_STRIP);
	float span = LINE_W / (float)MAXDATAS;
	long maxRate = 0;
	for (const auto& i : datas)
		if (abs(i) > maxRate)
			maxRate = abs(i);
	for (int i = 0; i < datas.size(); i++) {
		long value = datas[(Index + datas.size() - i) % datas.size()];
		if (value >= 0) {
			glColor3f(0.0f, 0.4f, 0.0f);
		}
		else {
			glColor3f(1.0f, 0.0f, 0.0f);
		}
		float y = LINE_Y + (maxRate ? abs(value) / (float)maxRate * LINE_H : 0);
		float x = LINE_X + i * span;
		glVertex2f(x, y);
	}
	glEnd();
	//draw text
	glColor3f(0.0f, 0.0f, 1.0f);
	glRasterPos2i(LINE_X, LINE_Y + LINE_H + 5);
	string info ="max:"+ to_string(maxRate) + "mW Cur:" + to_string(datas[Index]) + "mW";
	for (const char& c : info) {
		glutBitmapCharacter(GLUT_BITMAP_8_BY_13, c);
	}
	glutSwapBuffers();
}
void timer(int id) {
	glutPostRedisplay();
	glutTimerFunc(REFRESH, timer, 0);
}
int main(int argc, char* argv[])
{
	/*LONG ntStatus = S_OK;
	SYSTEM_BATTERY_STATE pi = { 0 };

	ntStatus = CallNtPowerInformation(SystemBatteryState, NULL, 0, &pi, sizeof(pi));
	if (0 == ntStatus)
	{
		printf("Battery: %d / %d\n", pi.RemainingCapacity, pi.MaxCapacity);
	}

	BATTERY_STATUS bs = GetBatteryState();
	printf("Capacity: %d mWh PowerState: %d Voltage: %d mv Rate: %d mWh\n", bs.Capacity, bs.PowerState, bs.Voltage, bs.Rate);
	*/
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE); //初始化窗口的显示模式
	glutInitWindowPosition(WINDOW_X, WINDOW_Y);                //设置窗口的位置
	glutInitWindowSize(WINDOW_W, WINDOW_H);                   //设置窗口的大小
	glutCreateWindow("BatteryInfo");       //创建窗口
	HWND hwnd = FindWindow(NULL, TEXT("BatteryInfo"));
	HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(101)); // 替换IDI_ICON为你的图标资源ID

	// 设置窗口类的图标属性
	SetClassLongPtr(hwnd, GCLP_HICON, (LONG_PTR)hIcon);

	// 刷新窗口以显示新图标
	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
	DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
	dwStyle &= ~WS_MAXIMIZEBOX; // 隐藏最大化按钮
	dwStyle &= ~WS_MINIMIZEBOX; // 隐藏最小化按钮
	//dwStyle &= ~WS_SYSMENU; //隐藏所有按钮
	SetWindowLong(hwnd, GWL_STYLE, dwStyle);
	SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); // 将窗口置顶
	//if (hwnd != NULL) {
	//	LONG style = GetWindowLong(hwnd, GWL_STYLE);
	//	SetWindowLong(hwnd, GWL_STYLE, style & ~WS_CAPTION); // 移除标题栏
	//}
	//glutMouseFunc(onMouse);
	//glutMotionFunc(onMotion);
	glutDisplayFunc(myDisplay);                  //设置当前窗口的显示回调函数
	Initial();                                     //完成窗口初始化
	glutTimerFunc(REFRESH, timer, 0);
	glutMainLoop();                              //启动主 GLUT事件处理循环
	return 0;
}