-- DMG Finder-window layout for the Fluent-QT Gallery disk image.
-- CPack's DragNDrop generator runs this script while the image is mounted and passes the
-- volume name as the first argument. It sets the window geometry, switches to icon view,
-- applies the .background image, and positions the app bundle next to the /Applications
-- symlink so the mounted DMG reads as a drag-to-install layout.
-- zh_CN: Fluent-QT Gallery 磁盘镜像的 Finder 窗口布局。CPack DragNDrop 在镜像挂载时运行本脚本，
-- 并把卷名作为第一个参数传入：设置窗口几何、切换图标视图、套用 .background 背景图，
-- 并把 .app 摆在 /Applications 软链旁边，形成"拖入安装"的引导界面。
on run argv
    set volumeName to item 1 of argv
    tell application "Finder"
        tell disk volumeName
            open
            set theWindow to container window
            set current view of theWindow to icon view
            set toolbar visible of theWindow to false
            set statusbar visible of theWindow to false
            -- {left, top, right, bottom}; content area is 640x400 plus the ~22px title bar.
            set the bounds of theWindow to {220, 160, 860, 582}
            set theOptions to the icon view options of theWindow
            set arrangement of theOptions to not arranged
            set icon size of theOptions to 128
            set text size of theOptions to 13
            -- CPack copies CPACK_DMG_BACKGROUND_IMAGE into the image as .background/background.<ext>,
            -- always renamed to "background" regardless of the source filename.
            set background picture of theOptions to file ".background:background.tiff"
            -- Finder renders an icon's graphic centered ~50px BELOW its position value (the label
            -- sits under the icon), so to line the icons up with the arrow baked at y~200 in the
            -- background image we set y to ~150. zh_CN: Finder 实际把图标画在 position 值下方约 50px
            -- 处(标签在图标下面),所以要和背景里 y≈200 的箭头对齐,这里把 y 设成约 150。
            set position of item "Fluent-QT Gallery.app" of theWindow to {160, 150}
            set position of item "Applications" of theWindow to {480, 150}
            -- Park the .background folder far off-canvas so it is not visible even for users who
            -- have enabled "show all files" in Finder (the invisible flag below only hides it for
            -- default Finder settings). zh_CN: 把 .background 挪到画布外,即使用户开了"显示所有文件"
            -- 也看不到(下方的不可见标记只对默认设置生效)。
            try
                set position of item ".background" of theWindow to {980, 760}
            end try
            close
            open
            update without registering applications
            delay 1
        end tell
    end tell
    -- Tidy the volume so the mounted window is clean: flag the .background folder invisible and
    -- drop the .fseventsd / .Trashes scaffolding the build-time mount left behind. The final image
    -- is read-only, so once removed here these do not come back. (A user who has enabled
    -- "show all files" in Finder may still glimpse .background; that is their setting, not the DMG.)
    -- zh_CN: 清理卷，让挂载窗口干净：把 .background 标记为不可见，删掉构建挂载残留的
    -- .fseventsd / .Trashes。最终镜像是只读的，删掉后不会再生成。（用户若开了 Finder
    -- "显示所有文件"，仍可能瞥见 .background，那是其设置而非 DMG 本身的问题。）
    set volPath to quoted form of ("/Volumes/" & volumeName)
    do shell script "/usr/bin/chflags hidden " & volPath & "/.background || true"
    do shell script "/bin/rm -rf " & volPath & "/.fseventsd " & volPath & "/.Trashes || true"
end run
