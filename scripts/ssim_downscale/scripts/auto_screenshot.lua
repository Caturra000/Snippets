local utils = require 'mp.utils'

mp.register_event("file-loaded", function()
    -- 等待一帧渲染完成
    mp.add_timeout(0.3, function()
        local output = mp.get_opt("output_path")
        if output then
            -- 必须使用 "window" 而不是 "video"，才能捕获带有缩放和GLSL处理的画面
            mp.commandv("screenshot-to-file", output, "window")
        else
            mp.commandv("screenshot", "window")
        end
        mp.add_timeout(0.2, function()
            mp.command("quit")
        end)
    end)
end)