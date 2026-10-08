# if using `LOCAL_DIR`, the parameter `VERSION` will be ignored
function(tx_add_txlib)
	message(STATUS "TXLib: Adding TXLib")

	set(options "")
    set(oneValueArgs LOCAL_DIR BIN_DIR VERSION)
    set(multiValueArgs COMPONENTS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	set(TX_SOURCE_DIR "")
	set(TX_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

	# resolve BIN_DIR
	if(ARG_BIN_DIR)
		message(STATUS "TXLib: Using specialized bin dir")
		set(TX_BINARY_DIR "${ARG_BIN_DIR}")
	else()
		message(STATUS "TXLib: Using default bin dir: \${CMAKE_CURRENT_BINARY_DIR}: ${CMAKE_CURRENT_BINARY_DIR}")
	endif()

	# prepare source
	set(REQUIRE_REMOTE_DOWNLOAD TRUE)

	if(ARG_LOCAL_DIR) # use local dir
		message(STATUS "TXLib: Using local TXLib")
		set(REQUIRE_REMOTE_DOWNLOAD FALSE)
		if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_LOCAL_DIR}/CMakeLists.txt") # look for in project dir first (relative path)
			set(TX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_LOCAL_DIR}")
		elseif(EXISTS "${ARG_LOCAL_DIR}/CMakeLists.txt")
			set(TX_SOURCE_DIR "${ARG_LOCAL_DIR}")
		else()
			# intented fallback to remote download
			message(STATUS "TXLib: LOCAL_DIR path not found! downloading TXLib.")
			set(REQUIRE_REMOTE_DOWNLOAD TRUE)
		endif()
		set(TX_BINARY_DIR "${TX_BINARY_DIR}/libs/TXLib")
	endif()

	if(REQUIRE_REMOTE_DOWNLOAD) # fetch from remote
		message(STATUS "TXLib: Fetching TXLib from GitHub")		
	
		# resolve version
		if(ARG_VERSION)
			set(GIT_TAG_VAL "${ARG_VERSION}")
		else()
			set(GIT_TAG_VAL "main")
		endif()

		# fetch content
		include(FetchContent)

		FetchContent_Declare(
			txlib
			GIT_REPOSITORY "https://github.com/TXWD1234/TXLib.git"
			GIT_TAG "${GIT_TAG_VAL}"
			SOURCE_SUBDIR "TXCMakeUtils"
		)
		# use `SOURCE_SUBDIR` to work around the MakeAvailable's auto `add_subdirectory`

		FetchContent_MakeAvailable(txlib)
		FetchContent_GetProperties(txlib)

		set(TX_SOURCE_DIR "${txlib_SOURCE_DIR}")
		set(TX_BINARY_DIR "${txlib_BINARY_DIR}")
	endif()
	
	# resolve components
	if(ARG_COMPONENTS) # if given components
		foreach(comp IN LISTS ARG_COMPONENTS)
			if(EXISTS "${TX_SOURCE_DIR}/${comp}/CMakeLists.txt")
				message(STATUS "TXLib: Adding component [${comp}]")
				add_subdirectory("${TX_SOURCE_DIR}/${comp}" "${TX_BINARY_DIR}/${comp}")
			else()
				message(WARNING "TXLib: Component [${comp}] not found in source!")
			endif()
		endforeach()
	else() # add the whole library
		message(STATUS "TXLib: Adding TXLib")
		add_subdirectory("${TX_SOURCE_DIR}" "${TX_BINARY_DIR}")
	endif()
	
	set(TXLib_SOURCE_DIR "${TX_SOURCE_DIR}" PARENT_SCOPE)
endfunction()