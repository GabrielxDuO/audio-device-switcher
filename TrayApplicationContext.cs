using System;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;
using AudioSwitcher.AudioApi;
using AudioSwitcher.AudioApi.CoreAudio;
using Microsoft.Win32;

namespace AudioDeviceSwitcher
{
    public class TrayApplicationContext : ApplicationContext
    {
        private NotifyIcon? _trayIcon;
        private CoreAudioController? _audioController;
        private Icon? _lightIcon;
        private Icon? _darkIcon;
        private bool _isDarkMode;
        private HiddenWindow? _hiddenHost; // 用于 MessageBox/ContextMenu 的不可见宿主窗体

        public TrayApplicationContext()
        {
            // 创建一个不可见的宿主窗体，用于 MessageBox 和 ContextMenu
            _hiddenHost = new HiddenWindow();
            _hiddenHost.CreateControl();

            InitializeIcons();
            InitializeAudioController();
            InitializeTrayIcon();
            UpdateTrayMenu();

            // 监听系统主题变化
            SystemEvents.UserPreferenceChanged += OnUserPreferenceChanged;
        }

        private void InitializeIcons()
        {
            try
            {
                // 从嵌入资源加载图标
                var assembly = System.Reflection.Assembly.GetExecutingAssembly();
                
                // 加载亮色图标
                using (var stream = assembly.GetManifestResourceStream("AudioDeviceSwitcher.icon.ico"))
                {
                    if (stream != null)
                    {
                        _lightIcon = new Icon(stream, new Size(256, 256));
                        Console.WriteLine("已加载嵌入的亮色图标");
                    }
                    else
                    {
                        _lightIcon = SystemIcons.Application;
                        Console.WriteLine("未找到嵌入的亮色图标，使用系统默认");
                    }
                }
                
                // 加载暗色图标
                using (var stream = assembly.GetManifestResourceStream("AudioDeviceSwitcher.icon-dark.ico"))
                {
                    if (stream != null)
                    {
                        _darkIcon = new Icon(stream, new Size(256, 256));
                        Console.WriteLine("已加载嵌入的暗色图标");
                    }
                    else
                    {
                        _darkIcon = _lightIcon;
                        Console.WriteLine("未找到嵌入的暗色图标，使用亮色图标");
                    }
                }

                _isDarkMode = IsSystemInDarkMode();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"图标加载失败: {ex.Message}");
                _lightIcon = SystemIcons.Application;
                _darkIcon = SystemIcons.Application;
            }
        }

        private void InitializeAudioController()
        {
            _audioController = new CoreAudioController();
        }

        private void InitializeTrayIcon()
        {
            _trayIcon = new NotifyIcon
            {
                Icon = (_isDarkMode ? _darkIcon : _lightIcon) ?? SystemIcons.Application,
                Text = "AudioDeviceSwitcher",
                Visible = true
            };

            _trayIcon.MouseClick += OnTrayIconClick;
        }

        private void OnTrayIconClick(object? sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left || e.Button == MouseButtons.Right)
            {
                UpdateTrayMenu();
                if (_hiddenHost != null && _hiddenHost.IsHandleCreated)
                {
                    var contextMenu = _trayIcon?.ContextMenuStrip;
                    if (contextMenu != null)
                    {
                        var location = Control.MousePosition;
                        contextMenu.Show(_hiddenHost, _hiddenHost.PointToClient(location));
                    }
                }
            }
        }

        private void UpdateTrayMenu()
        {
            var menu = new ContextMenuStrip();
            
            // 尝试启用暗黑模式渲染
            ApplyDarkModeToMenu(menu);

            // 播放设备
            var playbackMenu = new ToolStripMenuItem("播放设备");
            if (IsSystemInDarkMode()) playbackMenu.ForeColor = Color.White;
            AddDeviceMenuItems(playbackMenu, DeviceType.Playback, Role.Console);
            menu.Items.Add(playbackMenu);

            // 播放通信设备
            var commPlaybackMenu = new ToolStripMenuItem("播放通信设备");
            if (IsSystemInDarkMode()) commPlaybackMenu.ForeColor = Color.White;
            AddDeviceMenuItems(commPlaybackMenu, DeviceType.Playback, Role.Communications);
            menu.Items.Add(commPlaybackMenu);

            // 录制设备
            var captureMenu = new ToolStripMenuItem("录制设备");
            if (IsSystemInDarkMode()) captureMenu.ForeColor = Color.White;
            AddDeviceMenuItems(captureMenu, DeviceType.Capture, Role.Console);
            menu.Items.Add(captureMenu);

            // 录制通信设备
            var commCaptureMenu = new ToolStripMenuItem("录制通信设备");
            if (IsSystemInDarkMode()) commCaptureMenu.ForeColor = Color.White;
            AddDeviceMenuItems(commCaptureMenu, DeviceType.Capture, Role.Communications);
            menu.Items.Add(commCaptureMenu);

            menu.Items.Add(new ToolStripSeparator());

            // 刷新设备列表
            var refreshItem = new ToolStripMenuItem("刷新设备列表");
            if (IsSystemInDarkMode()) refreshItem.ForeColor = Color.White;
            refreshItem.Click += (s, e) =>
            {
                UpdateTrayMenu();
                _trayIcon?.ShowBalloonTip(1000, "刷新成功", "设备列表已更新", ToolTipIcon.Info);
            };
            menu.Items.Add(refreshItem);

            menu.Items.Add(new ToolStripSeparator());

            // 开机启动
            var startupItem = new ToolStripMenuItem("开机启动")
            {
                CheckOnClick = true,
                Checked = IsStartupEnabled(),
                ImageScaling = ToolStripItemImageScaling.None // 减少图像区域占用
            };
            if (IsSystemInDarkMode()) startupItem.ForeColor = Color.White;
            startupItem.Click += OnStartupItemClick;
            menu.Items.Add(startupItem);

            menu.Items.Add(new ToolStripSeparator());

            // 退出
            var exitItem = new ToolStripMenuItem("退出");
            if (IsSystemInDarkMode()) exitItem.ForeColor = Color.White;
            exitItem.Click += (s, e) => Application.Exit();
            menu.Items.Add(exitItem);

            if (_trayIcon != null)
            {
                _trayIcon.ContextMenuStrip = menu;
            }
        }

        private void AddDeviceMenuItems(ToolStripMenuItem parentMenu, DeviceType deviceType, Role role)
        {
            try
            {
                if (_audioController == null)
                {
                    var errorItem = new ToolStripMenuItem("音频控制器未初始化") { Enabled = false };
                    if (IsSystemInDarkMode()) errorItem.ForeColor = Color.White;
                    parentMenu.DropDownItems.Add(errorItem);
                    return;
                }

                var devices = _audioController.GetDevices(deviceType, DeviceState.Active);
                var defaultDevice = _audioController.GetDefaultDevice(deviceType, role);

                if (!devices.Any())
                {
                    var noDeviceItem = new ToolStripMenuItem("未找到设备") { Enabled = false };
                    if (IsSystemInDarkMode()) noDeviceItem.ForeColor = Color.White;
                    parentMenu.DropDownItems.Add(noDeviceItem);
                    return;
                }

                foreach (var device in devices)
                {
                    var item = new ToolStripMenuItem(device.FullName)
                    {
                        Checked = defaultDevice?.Id == device.Id,
                        Tag = new DeviceInfo { Device = device, Role = role }
                    };
                    
                    // 设置暗黑模式文字颜色
                    if (IsSystemInDarkMode())
                    {
                        item.ForeColor = Color.White;
                        item.BackColor = Color.FromArgb(32, 32, 32);
                    }
                    
                    item.Click += OnDeviceItemClick!;
                    parentMenu.DropDownItems.Add(item);
                }
            }
            catch (Exception ex)
            {
                var errorItem = new ToolStripMenuItem($"错误: {ex.Message}") { Enabled = false };
                if (IsSystemInDarkMode()) errorItem.ForeColor = Color.White;
                parentMenu.DropDownItems.Add(errorItem);
            }
        }

        private async void OnDeviceItemClick(object? sender, EventArgs e)
        {
            try
            {
                if (sender is not ToolStripMenuItem item || item.Tag is not DeviceInfo info)
                    return;

                await info.Device.SetAsDefaultAsync();
                
                if (info.Role == Role.Communications)
                {
                    await info.Device.SetAsDefaultCommunicationsAsync();
                }

                // 短暂延迟后刷新菜单
                await System.Threading.Tasks.Task.Delay(300);
                UpdateTrayMenu();

                _trayIcon?.ShowBalloonTip(2000, "切换成功", $"已切换到: {info.Device.FullName}", ToolTipIcon.Info);
            }
            catch (Exception ex)
            {
                ShowMessageBox($"切换设备失败：\n\n{ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void OnStartupItemClick(object? sender, EventArgs e)
        {
            if (sender is ToolStripMenuItem item)
            {
                SetStartup(item.Checked);
            }
        }

        private bool IsStartupEnabled()
        {
            try
            {
                using var key = Registry.CurrentUser.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Run", false);
                return key?.GetValue("AudioDeviceSwitcher") != null;
            }
            catch
            {
                return false;
            }
        }

        private void SetStartup(bool enable)
        {
            try
            {
                using var key = Registry.CurrentUser.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Run", true);
                if (enable)
                {
                    key?.SetValue("AudioDeviceSwitcher", Application.ExecutablePath);
                }
                else
                {
                    key?.DeleteValue("AudioDeviceSwitcher", false);
                }
            }
            catch (Exception ex)
            {
                ShowMessageBox($"设置开机启动失败：\n\n{ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private bool IsSystemInDarkMode()
        {
            try
            {
                using var key = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize");
                var value = key?.GetValue("SystemUsesLightTheme");
                return value is int intValue && intValue == 0;
            }
            catch
            {
                return false;
            }
        }

        private void OnUserPreferenceChanged(object sender, UserPreferenceChangedEventArgs e)
        {
            // 监听多个可能触发主题变化的事件类别
            if (e.Category == UserPreferenceCategory.General || 
                e.Category == UserPreferenceCategory.VisualStyle ||
                e.Category == UserPreferenceCategory.Color)
            {
                var wasDarkMode = _isDarkMode;
                var newDarkMode = IsSystemInDarkMode();
                
                if (wasDarkMode != newDarkMode && _trayIcon != null)
                {
                    _isDarkMode = newDarkMode;
                    
                    // 更新托盘图标
                    var newIcon = (_isDarkMode ? _darkIcon : _lightIcon) ?? SystemIcons.Application;
                    _trayIcon.Icon = newIcon;
                    
                    // 更新菜单以应用新主题
                    UpdateTrayMenu();
                    
                    // 显示通知
                    _trayIcon?.ShowBalloonTip(1000, "主题已切换", $"已切换到{(_isDarkMode ? "暗色" : "亮色")}模式", ToolTipIcon.Info);
                }
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                SystemEvents.UserPreferenceChanged -= OnUserPreferenceChanged;
                _trayIcon?.Dispose();
                _audioController?.Dispose();
                _lightIcon?.Dispose();
                _darkIcon?.Dispose();
                _hiddenHost?.Dispose();
            }
            base.Dispose(disposing);
        }

        /// <summary>
        /// 显示 MessageBox，确保高 DPI 适配
        /// </summary>
        private DialogResult ShowMessageBox(string text, string caption, MessageBoxButtons buttons, MessageBoxIcon icon)
        {
            return MessageBox.Show(_hiddenHost, text, caption, buttons, icon);
        }

        private void ApplyDarkModeToMenu(ContextMenuStrip menu)
        {
            if (!IsSystemInDarkMode()) return;

            try
            {
                // 设置暗色主题
                menu.Renderer = new DarkModeRenderer();
                menu.BackColor = Color.FromArgb(32, 32, 32);
                menu.ForeColor = Color.White;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"应用暗黑模式失败: {ex.Message}");
            }
        }

        private class DeviceInfo
        {
            public required IDevice Device { get; set; }
            public required Role Role { get; set; }
        }

        /// <summary>
        /// 不可见宿主窗体，用于承载 MessageBox 和 ContextMenu，避免任务栏图标闪烁
        /// </summary>
        private class HiddenWindow : Form
        {
            public HiddenWindow()
            {
                ShowInTaskbar = false;
                FormBorderStyle = FormBorderStyle.None;
                StartPosition = FormStartPosition.Manual;
                Size = new Size(0, 0);
                Opacity = 0;
                TopMost = false;
            }

            protected override bool ShowWithoutActivation => true;

            protected override void SetVisibleCore(bool value)
            {
                // 永远保持不可见
                base.SetVisibleCore(false);
            }

            protected override CreateParams CreateParams
            {
                get
                {
                    var cp = base.CreateParams;
                    cp.ExStyle |= 0x00000080; // WS_EX_TOOLWINDOW
                    cp.ExStyle &= ~0x00000008; // 移除 WS_EX_APPWINDOW
                    return cp;
                }
            }
        }

        // 自定义暗黑模式渲染器
        private class DarkModeRenderer : ToolStripProfessionalRenderer
        {
            public DarkModeRenderer() : base(new DarkModeColorTable()) { }

            protected override void OnRenderArrow(ToolStripArrowRenderEventArgs e)
            {
                e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
                var r = new Rectangle(e.ArrowRectangle.Location, e.ArrowRectangle.Size);
                r.Inflate(-2, -6);
                // 计算箭头的顶点，减小开口
                int verticalOffset = r.Height / 4; // 从 r.Height / 2 改为 r.Height / 4，开口更小
                
                e.Graphics.DrawLines(Pens.White, new Point[]{
                    new Point(r.Left, r.Top + verticalOffset),           // 上端点
                    new Point(r.Right, r.Top + r.Height / 2),            // 右端点（箭头尖）
                    new Point(r.Left, r.Top + r.Height - verticalOffset) // 下端点
                });
            }

            protected override void OnRenderItemCheck(ToolStripItemImageRenderEventArgs e)
            {
                e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
                var r = new Rectangle(e.ImageRectangle.Location, e.ImageRectangle.Size);
                r.Inflate(-6, -6); // 增加收缩量，使对勾更小更紧凑
                r.Offset(8, 0);   // 向右偏移
                
                using (var pen = new Pen(Color.White, 1.5f))
                {
                    e.Graphics.DrawLines(pen, new Point[]{
                        new Point(r.Left, r.Top + r.Height / 2),
                        new Point(r.Left + r.Width / 3, r.Bottom),
                        new Point(r.Right, r.Top)
                    });
                }
            }

            protected override void OnRenderImageMargin(ToolStripRenderEventArgs e)
            {
                // 减少图像边距宽度
                if (e.ToolStrip is ContextMenuStrip)
                {
                    // 不绘制图像边距背景，使其更紧凑
                    return;
                }
                base.OnRenderImageMargin(e);
            }
        }

        // 暗黑模式颜色表
        private class DarkModeColorTable : ProfessionalColorTable
        {
            public override Color MenuItemSelected => Color.FromArgb(62, 62, 64);
            public override Color MenuItemSelectedGradientBegin => Color.FromArgb(62, 62, 64);
            public override Color MenuItemSelectedGradientEnd => Color.FromArgb(62, 62, 64);
            public override Color MenuItemBorder => Color.FromArgb(62, 62, 64);
            public override Color MenuBorder => Color.FromArgb(45, 45, 48);
            public override Color ToolStripDropDownBackground => Color.FromArgb(32, 32, 32);
            public override Color ImageMarginGradientBegin => Color.FromArgb(32, 32, 32);
            public override Color ImageMarginGradientMiddle => Color.FromArgb(32, 32, 32);
            public override Color ImageMarginGradientEnd => Color.FromArgb(32, 32, 32);
            public override Color MenuItemPressedGradientBegin => Color.FromArgb(0, 122, 204);
            public override Color MenuItemPressedGradientEnd => Color.FromArgb(0, 122, 204);
            public override Color CheckBackground => Color.FromArgb(62, 62, 64);
            public override Color CheckSelectedBackground => Color.FromArgb(62, 62, 64);
            public override Color CheckPressedBackground => Color.FromArgb(62, 62, 64);
            public override Color SeparatorDark => Color.FromArgb(60, 60, 60);
            public override Color SeparatorLight => Color.FromArgb(60, 60, 60);
        }
    }
}

