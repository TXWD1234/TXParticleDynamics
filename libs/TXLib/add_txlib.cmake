include_guard(GLOBAL)
#[[
if using LOCAL_DIR, the parameter VERSION will be ignored
if not using LOCAL_DIR, the parameter BIN_DIR will be ignored
]]
function(tx_add_txlib)
	message(STATUS "add_txlib.cmake: Adding TXLib")

	set(options "")
    set(oneValueArgs LOCAL_DIR BIN_DIR VERSION)
    set(multiValueArgs MODULES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	set(TXLib_SOURCE_DIR "")
	set(TXLib_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

	# resolve BIN_DIR
	if(ARG_BIN_DIR)
		message(VERBOSE "add_txlib.cmake: Using specialized bin dir: ${ARG_BIN_DIR}")
		set(TXLib_BINARY_DIR "${ARG_BIN_DIR}/TXLib")
	else()
		message(VERBOSE "add_txlib.cmake: Using default bin dir: \${CMAKE_CURRENT_BINARY_DIR}: ${CMAKE_CURRENT_BINARY_DIR}")
		set(TXLib_BINARY_DIR "${TXLib_BINARY_DIR}/TXLib")
	endif()

	# prepare source
	set(REQUIRE_REMOTE_DOWNLOAD TRUE)

	if(ARG_LOCAL_DIR) # use local dir
		message(VERBOSE "add_txlib.cmake: Using local TXLib: ${ARG_LOCAL_DIR}")
		set(REQUIRE_REMOTE_DOWNLOAD FALSE)
		if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_LOCAL_DIR}") # look for in project dir first (relative path)
			set(TXLib_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_LOCAL_DIR}")
		elseif(EXISTS "${ARG_LOCAL_DIR}")
			set(TXLib_SOURCE_DIR "${ARG_LOCAL_DIR}")
		else()
			# intented fallback to remote download
			message(VERBOSE "add_txlib.cmake: LOCAL_DIR path not found! downloading TXLib.")
			set(REQUIRE_REMOTE_DOWNLOAD TRUE)
		endif()
	endif()

	if(REQUIRE_REMOTE_DOWNLOAD) # fetch from remote
		message(VERBOSE "add_txlib.cmake: Fetching TXLib from GitHub")		
	
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
			SOURCE_SUBDIR "cmake"
		)
		# use `SOURCE_SUBDIR` to work around the MakeAvailable's auto `add_subdirectory`

		FetchContent_MakeAvailable(txlib)
		FetchContent_GetProperties(txlib)

		set(TXLib_SOURCE_DIR "${txlib_SOURCE_DIR}")
		set(TXLib_BINARY_DIR "${txlib_BINARY_DIR}")
	endif()
	
	# <-------------------------------------------------
	# TXLib/cmake/installation is locally exist,
	# TXLib_SOURCE_DIR and TXLib_BINARY_DIR exist,
	# installation shall began:
	message(STATUS "add_txlib.cmake: Sources are ready.")

	# set the essential variables
	# they are the parameters of setup.cmake
	set(TXLib_INSTALLATION_MODULES "${ARG_MODULES}")

	# call ${TXLib_SOURCE_DIR}/cmake/installation/setup.cmake
	# setup begins
	include("${TXLib_SOURCE_DIR}/cmake/installation/setup.cmake")
	
endfunction()