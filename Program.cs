using System;
using System.Threading;
using System.Windows.Forms;

namespace AudioDeviceSwitcher
{
    static class Program
    {
        private static Mutex? _mutex;

        [STAThread]
        static void Main()
        {
            // 创建互斥锁，确保只有一个实例运行
            bool createdNew;
            _mutex = new Mutex(true, "AudioDeviceSwitcher_SingleInstance_Mutex", out createdNew);

            if (!createdNew)
            {
                // 已经有实例在运行
                MessageBox.Show(
                    "AudioDeviceSwitcher 已经在运行中！\n\n请在系统托盘查找图标。",
                    "提示",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information
                );
                return;
            }

            // 启用高 DPI 支持
            Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new TrayApplicationContext());

            // 释放互斥锁
            _mutex?.ReleaseMutex();
            _mutex?.Dispose();
        }
    }
}

