# 每个基础组件测试都是独立可执行文件 便于 CTest 单独报告与设置超时
function(add_component_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE rysual_core)
    target_compile_options(${name} PRIVATE -Wall -Wextra)

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 30)
endfunction()

# add_component_test()

