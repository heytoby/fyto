# fyto 1.0.0.66

主窗口边缘、底栏高度与默认应用设置三项收尾修复。

- 主窗口：DWM 的 1px 边框不再绘制（DWMWA_COLOR_NONE），图片横向贴到窗口左右边缘；WS_THICKFRAME 与边缘拖拽缩放保留。
- 底栏：修掉 `_viv_viewport_rect` 之后重复再减一次底栏高度的问题。底栏恢复 24px 单行，标准/极简窗高固定相差 60px（标题 36 + 底栏 24），切模式不再每次少 24px；3:2 图片不再左右各留 18px。
- 默认应用：设置里的「图片格式关联」对话框顶部新增「一次性设为默认…」，打开系统「默认应用」的 fyto 页面；逐格式勾选保留为次要入口。ShellExecute 失败时给出「设置 → 应用 → 默认应用 → fyto」的手动路径。
- 注册表 `HKLM\SOFTWARE\fyto\Capabilities\FileAssociations` 已注册全部 9 个扩展名（.bmp .gif .ico .jpeg .jpg .png .tif .tiff .webp），安装包无需改动。
