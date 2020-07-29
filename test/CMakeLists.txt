find_program(BASH_PROGRAM bash)

set(TEST_BINARY ${PROJECT_BINARY_DIR}/bin/catimg)
set(TEST_SCRIPT ${PROJECT_SOURCE_DIR}/test/test-script.sh)

# pictures dimension : 64x64 pixel
# with `-r 1`: max width -> 128, max height -> 64
# with `-r 2`: max width -> 64, max height -> 32
set(TEST_PICTURE ${PROJECT_SOURCE_DIR}/test-images/mewtwo-front.png)

if (BASH_PROGRAM)
	# test for floating point expection introducted between commit 922986 and 860e13b
	add_test(NAME mewto-front-r1-H80 COMMAND ${TEST_BINARY} -r 1 -H 80 ${TEST_PICTURE})
	add_test(NAME mewto-front-r2-H80 COMMAND ${TEST_BINARY} -r 2 -H 80 ${TEST_PICTURE})

  # test if the console output got the right number of rows and columns
  # TEST_SCRIPT call: "<cmd line to invoke>" <expected output width> <expected output height>
  # FIXME: width is not check right now; don't know how if '-r 2'

	add_test(NAME mewto-front-r1-w64 COMMAND ${TEST_SCRIPT} "${TEST_BINARY} -r 1 -w 64 ${TEST_PICTURE}" 64 32)
	add_test(NAME mewto-front-r2-w64 COMMAND ${TEST_SCRIPT} "${TEST_BINARY} -r 2 -w 64 ${TEST_PICTURE}" 32 16)

	add_test(NAME mewto-front-r1-H64 COMMAND ${TEST_SCRIPT} "${TEST_BINARY} -r 1 -H 64 ${TEST_PICTURE}" 128 64)
	add_test(NAME mewto-front-r2-H64 COMMAND ${TEST_SCRIPT} "${TEST_BINARY} -r 2 -H 64 ${TEST_PICTURE}" 64 32)

  # testing max+1 height
	add_test(NAME mewto-front-r1-H65 COMMAND ${TEST_SCRIPT} "${TEST_BINARY} -r 1 -H 65 ${TEST_PICTURE}" 128 64)
	add_test(NAME mewto-front-r2-H65 COMMAND ${TEST_SCRIPT} "${TEST_BINARY} -r 2 -H 65 ${TEST_PICTURE}" 64 32)

  # test with much too high width
	add_test(NAME mewto-front-r1-w100k COMMAND ${TEST_SCRIPT} "${TEST_BINARY} -r 1 -w 100000 ${TEST_PICTURE}" 128 64)
	add_test(NAME mewto-front-r2-w100k COMMAND ${TEST_SCRIPT} "${TEST_BINARY} -r 2 -w 100000 ${TEST_PICTURE}" 64 32)

  # test with much too high height
	add_test(NAME mewto-front-r1-H100k COMMAND ${TEST_SCRIPT} "${TEST_BINARY} -r 1 -H 100000 ${TEST_PICTURE}" 128 64)
	add_test(NAME mewto-front-r2-H100k COMMAND ${TEST_SCRIPT} "${TEST_BINARY} -r 2 -H 100000 ${TEST_PICTURE}" 64 32)
endif (BASH_PROGRAM)
