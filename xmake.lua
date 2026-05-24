add_rules("mode.debug", "mode.release")

target("SimpleLine")
    set_kind("binary")
    add_files("src/main.c")
    if is_config("toolchain","msvc") then
        add_cflags("/utf-8")
    end

task("run_test")
    set_category("run")
    on_run(function ()
        import("core.project.project")
        local target = project.target("SimpleLine")
        local binary = target:targetfile()
        os.exec("python3 test/test_basic.py", {envs = {TEST_BINARY = binary}})
    end)
    set_menu {
        usage = "xmake run_test",
        description = "Run Python PTY-based tests",
        options = {}
    }

--
-- If you want to known more usage about xmake, please see https://xmake.io
--
