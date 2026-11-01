set(VLOGSIM_TESTS
    test_lexer
    test_parser
    test_eval_adder
    test_eval_mux
    test_seq_evaluator
    test_vcd_writer
    test_flattener
    test_four_state
    test_four_state_evaluator
    test_testbench_runner
    test_levelized_evaluator
    test_code_generator
)

foreach(test_name ${VLOGSIM_TESTS})
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE vlogsim_core)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# test_code_generator shells out to the system compiler to build and run
# generated code; point it at the exact compiler/standard this project
# itself was configured with, rather than guessing a "c++" on PATH.
target_compile_definitions(test_code_generator PRIVATE
    VLOGSIM_CXX_COMPILER="${CMAKE_CXX_COMPILER}"
    VLOGSIM_CXX_STD_FLAG="-std=c++17"
)
